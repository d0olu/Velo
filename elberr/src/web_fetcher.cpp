#include "web_fetcher.hpp"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace elberr {

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

WebFetcher::WebFetcher() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

WebFetcher::~WebFetcher() {
    curl_global_cleanup();
}

FetchResult WebFetcher::fetch(const std::string& url) {
    FetchResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "Failed to init curl";
        return result;
    }

    std::string rawBody;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rawBody);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    // Limit download size to 2MB
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, 2 * 1024 * 1024L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return result;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    result.httpCode = static_cast<int>(httpCode);
    curl_easy_cleanup(curl);

    if (httpCode != 200) {
        result.error = "HTTP " + std::to_string(httpCode);
        return result;
    }

    result.ok = true;
    result.body = stripHTML(rawBody);
    result.links = extractLinks(rawBody, url);
    return result;
}

std::vector<std::string> WebFetcher::searchWikipedia(const std::string& query,
                                                       const std::string& lang) {
    std::vector<std::string> urls;

    // Use Wikipedia API opensearch
    std::string apiUrl = "https://" + lang + ".wikipedia.org/w/api.php?action=opensearch&search=";

    // URL-encode query
    CURL* curl = curl_easy_init();
    if (!curl) return urls;

    char* encoded = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.size()));
    if (encoded) {
        apiUrl += encoded;
        curl_free(encoded);
    }
    apiUrl += "&limit=5&format=json";

    std::string rawBody;
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rawBody);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return urls;

    // Parse opensearch JSON: ["query", ["title1","title2"], ["desc1","desc2"], ["url1","url2"]]
    // Extract URLs from the 4th array
    // Simple parser: find last array of strings starting with http
    size_t pos = 0;
    int arrayCount = 0;
    while (pos < rawBody.size()) {
        if (rawBody[pos] == '[') {
            ++arrayCount;
            if (arrayCount == 4) {
                // Parse URLs from this array
                ++pos;
                while (pos < rawBody.size() && rawBody[pos] != ']') {
                    if (rawBody[pos] == '"') {
                        ++pos;
                        std::string url;
                        while (pos < rawBody.size() && rawBody[pos] != '"') {
                            url += rawBody[pos++];
                        }
                        if (!url.empty() && url.substr(0, 4) == "http") {
                            urls.push_back(url);
                        }
                        if (pos < rawBody.size()) ++pos; // skip closing quote
                    } else {
                        ++pos;
                    }
                }
                break;
            }
        }
        ++pos;
    }

    return urls;
}

std::string WebFetcher::stripHTML(const std::string& html) {
    std::string text;
    text.reserve(html.size() / 2);

    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    std::string tagName;

    for (size_t i = 0; i < html.size(); ++i) {
        if (html[i] == '<') {
            inTag = true;
            tagName.clear();
            continue;
        }

        if (inTag) {
            if (html[i] == '>') {
                inTag = false;
                // Check tag name
                std::string lower;
                for (char c : tagName) lower += std::tolower(c);
                if (lower == "script") inScript = true;
                else if (lower == "/script") inScript = false;
                else if (lower == "style") inStyle = true;
                else if (lower == "/style") inStyle = false;
                else if (lower == "br" || lower == "br/" || lower == "p" ||
                         lower == "/p" || lower == "div" || lower == "/div" ||
                         lower == "li" || lower == "/li" || lower == "h1" ||
                         lower == "h2" || lower == "h3" || lower == "h4") {
                    text += '\n';
                }
            } else {
                if (tagName.size() < 20) tagName += html[i];
            }
            continue;
        }

        if (inScript || inStyle) continue;

        // Decode common HTML entities
        if (html[i] == '&') {
            std::string entity;
            size_t j = i + 1;
            while (j < html.size() && j < i + 10 && html[j] != ';' && html[j] != '<')
                entity += html[j++];
            if (j < html.size() && html[j] == ';') {
                if (entity == "amp") { text += '&'; i = j; continue; }
                if (entity == "lt") { text += '<'; i = j; continue; }
                if (entity == "gt") { text += '>'; i = j; continue; }
                if (entity == "quot") { text += '"'; i = j; continue; }
                if (entity == "apos") { text += '\''; i = j; continue; }
                if (entity == "nbsp") { text += ' '; i = j; continue; }
                if (entity == "#160") { text += ' '; i = j; continue; }
                // Skip unknown entities
                i = j;
                continue;
            }
        }

        text += html[i];
    }

    // Collapse multiple whitespace
    std::string clean;
    clean.reserve(text.size());
    bool lastSpace = false;
    for (char c : text) {
        if (c == '\n') {
            if (!lastSpace) clean += '\n';
            lastSpace = true;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            if (!lastSpace) clean += ' ';
            lastSpace = true;
        } else {
            clean += c;
            lastSpace = false;
        }
    }

    // Truncate to reasonable size
    if (clean.size() > 100000) clean.resize(100000);

    return clean;
}

std::vector<std::string> WebFetcher::extractLinks(const std::string& html,
                                                    const std::string& baseUrl) {
    std::vector<std::string> links;
    std::string lower = html;

    // Find all href="..." patterns
    size_t pos = 0;
    while (pos < lower.size()) {
        pos = lower.find("href=\"", pos);
        if (pos == std::string::npos) break;
        pos += 6;
        size_t end = lower.find('"', pos);
        if (end == std::string::npos) break;

        std::string href = html.substr(pos, end - pos);
        pos = end;

        // Skip anchors, javascript, mailto
        if (href.empty() || href[0] == '#' || href[0] == '{' ||
            href.find("javascript:") == 0 || href.find("mailto:") == 0) continue;

        // Resolve relative URLs
        std::string resolved = resolveUrl(baseUrl, href);
        if (!resolved.empty() && resolved.find("http") == 0) {
            links.push_back(resolved);
        }
    }

    // Deduplicate
    std::sort(links.begin(), links.end());
    links.erase(std::unique(links.begin(), links.end()), links.end());

    // Limit
    if (links.size() > 200) links.resize(200);

    return links;
}

std::string WebFetcher::resolveUrl(const std::string& base, const std::string& relative) {
    if (relative.find("http://") == 0 || relative.find("https://") == 0) {
        return relative;
    }
    if (relative.find("//") == 0) {
        return "https:" + relative;
    }

    // Extract base components
    size_t protoEnd = base.find("://");
    if (protoEnd == std::string::npos) return "";
    protoEnd += 3;

    size_t hostEnd = base.find('/', protoEnd);
    if (hostEnd == std::string::npos) hostEnd = base.size();

    std::string origin = base.substr(0, hostEnd);

    if (relative[0] == '/') {
        return origin + relative;
    }

    // Relative path
    size_t lastSlash = base.rfind('/');
    if (lastSlash != std::string::npos && lastSlash > protoEnd) {
        return base.substr(0, lastSlash + 1) + relative;
    }

    return origin + "/" + relative;
}

} // namespace elberr
