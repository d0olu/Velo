#include "autonomous_explorer.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>

namespace elberr {

AutonomousExplorer::AutonomousExplorer(LearnCallback cb)
    : callback_(std::move(cb)) {}

AutonomousExplorer::~AutonomousExplorer() {
    stop();
}

void AutonomousExplorer::addSeeds(const std::vector<std::string>& urls) {
    std::lock_guard<std::mutex> lock(queueMtx_);
    for (auto& url : urls) {
        if (visited_.count(url) == 0 && urlQueue_.size() < MAX_QUEUE) {
            urlQueue_.push(url);
        }
    }
}

void AutonomousExplorer::addSearchQuery(const std::string& query, const std::string& lang) {
    // Search Wikipedia for the query and add results as seeds
    auto urls = fetcher_.searchWikipedia(query, lang);
    if (!urls.empty()) {
        addSeeds(urls);
        std::cout << "[Explorer] Search '" << query << "' found " << urls.size() << " pages\n";
    } else {
        std::cout << "[Explorer] Search '" << query << "' returned no results\n";
    }
}

void AutonomousExplorer::start() {
    if (running_.load()) return;
    stopFlag_ = false;
    running_ = true;
    thread_ = std::thread(&AutonomousExplorer::explorerLoop, this);
    std::cout << "[Explorer] Started background exploration\n";
}

void AutonomousExplorer::stop() {
    if (!running_.load()) return;
    stopFlag_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
    std::cout << "[Explorer] Stopped\n";
}

size_t AutonomousExplorer::queueSize() const {
    // Not locking for a rough estimate
    return urlQueue_.size();
}

void AutonomousExplorer::explorerLoop() {
    while (!stopFlag_.load()) {
        std::string url;
        {
            std::lock_guard<std::mutex> lock(queueMtx_);
            if (urlQueue_.empty()) {
                // Nothing to explore — wait
                // Don't hold lock while sleeping
            } else {
                url = urlQueue_.front();
                urlQueue_.pop();

                if (visited_.count(url) > 0) {
                    url.clear();
                }
            }
        }

        if (url.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Mark as visited
        {
            std::lock_guard<std::mutex> lock(queueMtx_);
            visited_.insert(url);

            // Prevent unbounded growth
            if (visited_.size() > MAX_VISITED) {
                // Keep second half
                std::unordered_set<std::string> newVisited;
                size_t skip = visited_.size() / 2;
                size_t count = 0;
                for (auto& v : visited_) {
                    if (count++ >= skip) newVisited.insert(v);
                }
                visited_ = std::move(newVisited);
            }
        }

        std::cout << "[Explorer] Fetching: " << url << "\n";
        auto result = fetcher_.fetch(url);

        if (!result.ok) {
            std::cout << "[Explorer] FAIL: " << result.error << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        pagesFetched_++;

        // Build PageInfo
        PageInfo page;
        page.url = url;
        page.text = result.body;
        page.links = result.links;
        page.sentences = splitSentences(result.body);

        std::cout << "[Explorer] OK: " << page.sentences.size() << " sentences, "
                  << page.links.size() << " links\n";

        // Callback to agent
        if (callback_) {
            callback_(page);
        }

        // Add interesting links to queue
        {
            std::lock_guard<std::mutex> lock(queueMtx_);
            for (auto& link : result.links) {
                if (visited_.count(link) == 0 && urlQueue_.size() < MAX_QUEUE) {
                    double score = priorityScore(link);
                    if (score > 0.3) {
                        urlQueue_.push(link);
                    }
                }
            }
        }

        // Rate limit: don't hammer servers
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

double AutonomousExplorer::priorityScore(const std::string& url) const {
    double score = 0.5;

    // Prefer Wikipedia, educational sites
    if (url.find("wikipedia.org") != std::string::npos) score += 0.3;
    if (url.find("arxiv.org") != std::string::npos) score += 0.2;
    if (url.find(".edu") != std::string::npos) score += 0.15;
    if (url.find("britannica.com") != std::string::npos) score += 0.15;

    // Avoid login, signup, social media
    if (url.find("login") != std::string::npos) score -= 0.5;
    if (url.find("signup") != std::string::npos) score -= 0.5;
    if (url.find("facebook.com") != std::string::npos) score -= 0.5;
    if (url.find("twitter.com") != std::string::npos) score -= 0.4;
    if (url.find("instagram.com") != std::string::npos) score -= 0.4;

    // Avoid non-text resources
    if (url.find(".jpg") != std::string::npos || url.find(".png") != std::string::npos ||
        url.find(".gif") != std::string::npos || url.find(".pdf") != std::string::npos ||
        url.find(".mp3") != std::string::npos || url.find(".mp4") != std::string::npos) {
        score -= 0.5;
    }

    return score;
}

std::vector<std::string> AutonomousExplorer::splitSentences(const std::string& text) const {
    std::vector<std::string> sentences;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i) {
        current += text[i];
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
            // Trim
            size_t start = current.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                std::string s = current.substr(start);
                // Only add meaningful sentences (>10 chars, <500 chars)
                if (s.size() > 10 && s.size() < 500) {
                    sentences.push_back(s);
                }
            }
            current.clear();
        }
    }

    // Limit total sentences per page
    if (sentences.size() > 200) sentences.resize(200);

    return sentences;
}

} // namespace elberr
