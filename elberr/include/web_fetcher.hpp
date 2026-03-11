#pragma once
#include <string>
#include <vector>
#include <functional>

namespace elberr {

struct FetchResult {
    bool ok = false;
    int httpCode = 0;
    std::string body;
    std::string error;
    std::vector<std::string> links;
};

// Real HTTP fetcher using libcurl
class WebFetcher {
public:
    WebFetcher();
    ~WebFetcher();

    // Fetch a URL, return cleaned text + extracted links
    FetchResult fetch(const std::string& url);

    // Wikipedia search API — returns list of page URLs
    std::vector<std::string> searchWikipedia(const std::string& query,
                                              const std::string& lang = "ru");

    void setUserAgent(const std::string& ua) { userAgent_ = ua; }
    void setTimeoutSec(long t) { timeoutSec_ = t; }

private:
    std::string userAgent_ = "ELBERR/2.0 (Epistemic Logic Bot)";
    long timeoutSec_ = 15;

    // Strip HTML tags, decode entities, extract text
    std::string stripHTML(const std::string& html);
    // Extract href links from HTML
    std::vector<std::string> extractLinks(const std::string& html,
                                           const std::string& baseUrl);
    // Resolve relative URL
    std::string resolveUrl(const std::string& base, const std::string& relative);
};

} // namespace elberr
