#pragma once
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "web_fetcher.hpp"

namespace elberr {

struct PageInfo {
    std::string url;
    std::string text;
    std::vector<std::string> links;
    std::vector<std::string> sentences;
};

// Autonomous web explorer that runs in a background thread
// Crawls pages, extracts text, calls back with learned content
class AutonomousExplorer {
public:
    using LearnCallback = std::function<void(const PageInfo&)>;

    explicit AutonomousExplorer(LearnCallback cb);
    ~AutonomousExplorer();

    // Add seed URLs to explore
    void addSeeds(const std::vector<std::string>& urls);

    // Add a search query (uses Wikipedia search API)
    void addSearchQuery(const std::string& query, const std::string& lang = "ru");

    // Start/stop background exploration
    void start();
    void stop();

    bool isRunning() const { return running_.load(); }
    size_t pagesFetched() const { return pagesFetched_.load(); }
    size_t queueSize() const;

private:
    LearnCallback callback_;
    WebFetcher fetcher_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    std::atomic<size_t> pagesFetched_{0};

    std::queue<std::string> urlQueue_;
    std::unordered_set<std::string> visited_;
    std::mutex queueMtx_;

    static const size_t MAX_VISITED = 50000;
    static const size_t MAX_QUEUE = 1000;

    void explorerLoop();
    double priorityScore(const std::string& url) const;
    std::vector<std::string> splitSentences(const std::string& text) const;
};

} // namespace elberr
