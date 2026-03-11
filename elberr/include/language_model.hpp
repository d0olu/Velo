#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace elberr {

// Simple pattern-based language model that learns from text
// Extracts sentence patterns like "X является Y", "X есть Y"
// Generates responses by filling patterns with known facts
class LanguageModel {
public:
    // Learn from raw text (extract words, patterns)
    void learnFromText(const std::string& text);

    // Learn a single word
    void addWord(const std::string& word);

    // Generate a response about a topic using learned patterns
    std::string generateResponse(const std::string& topic,
                                 const std::vector<std::string>& knownFacts) const;

    // Generate a greeting or status message
    std::string generateStatus(size_t factCount, size_t wordCount) const;

    // Can the model speak yet?
    bool canSpeak() const;

    // Stats
    size_t wordCount() const;
    size_t patternCount() const;

    // Access for persistence
    const std::unordered_set<std::string>& vocabulary() const { return vocab_; }
    const std::vector<std::string>& patterns() const { return patterns_; }
    void setVocabulary(const std::unordered_set<std::string>& v) { vocab_ = v; }
    void addPattern(const std::string& p);

    mutable std::mutex mtx;

private:
    std::unordered_set<std::string> vocab_;
    std::vector<std::string> patterns_;  // "X является Y", "X — это Y"
    std::unordered_map<std::string, std::vector<std::string>> wordAssociations_;

    // Extract patterns from a sentence
    void extractPatterns(const std::string& sentence);
    // Split text into words
    std::vector<std::string> tokenize(const std::string& text) const;
    // Find best matching pattern for a topic
    std::string findPattern(const std::string& topic,
                           const std::vector<std::string>& facts) const;
};

} // namespace elberr
