#include "language_model.hpp"
#include "russian_grammar.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace elberr {

static bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::vector<std::string> LanguageModel::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        // Strip trailing punctuation
        while (!word.empty() && (word.back() == '.' || word.back() == ',' ||
               word.back() == '!' || word.back() == '?' || word.back() == ':' ||
               word.back() == ';')) {
            word.pop_back();
        }
        if (!word.empty()) tokens.push_back(word);
    }
    return tokens;
}

void LanguageModel::learnFromText(const std::string& text) {
    // 1. Add all words to vocabulary
    auto words = tokenize(text);
    for (auto& w : words) {
        vocab_.insert(w);
    }

    // 2. Extract sentence patterns
    // Split into sentences
    std::string sentence;
    for (size_t i = 0; i < text.size(); ++i) {
        sentence += text[i];
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
            extractPatterns(sentence);
            sentence.clear();
        }
    }
    if (!sentence.empty()) extractPatterns(sentence);

    // 3. Build word associations (bigrams)
    for (size_t i = 0; i + 1 < words.size(); ++i) {
        wordAssociations_[words[i]].push_back(words[i + 1]);
    }
}

void LanguageModel::addWord(const std::string& word) {
    vocab_.insert(word);
}

void LanguageModel::extractPatterns(const std::string& sentence) {
    auto words = tokenize(sentence);
    if (words.size() < 3) return;

    // Look for "X является Y", "X — это Y", "X есть Y"
    for (size_t i = 0; i + 2 < words.size(); ++i) {
        // "является" pattern
        if (words[i + 1] == "\xD1\x8F\xD0\xB2\xD0\xBB\xD1\x8F\xD0\xB5\xD1\x82\xD1\x81\xD1\x8F") {
            addPattern("X \xD1\x8F\xD0\xB2\xD0\xBB\xD1\x8F\xD0\xB5\xD1\x82\xD1\x81\xD1\x8F Y");
        }
        // "есть" pattern
        if (words[i + 1] == "\xD0\xB5\xD1\x81\xD1\x82\xD1\x8C") {
            addPattern("X \xD0\xB5\xD1\x81\xD1\x82\xD1\x8C Y");
        }
        // "это" pattern
        if (words[i + 1] == "\xD1\x8D\xD1\x82\xD0\xBE" ||
            (i + 2 < words.size() && words[i + 1] == "\xe2\x80\x94" &&
             words[i + 2] == "\xD1\x8D\xD1\x82\xD0\xBE")) {
            addPattern("X \xe2\x80\x94 \xD1\x8D\xD1\x82\xD0\xBE Y");
        }
        // "is" / "are" for English text
        if (words[i + 1] == "is" || words[i + 1] == "are") {
            addPattern("X " + words[i + 1] + " Y");
        }
    }

    // Generic patterns: if sentence has > 4 words, extract template
    if (words.size() >= 4) {
        // Replace first noun-like word with X, last with Y
        std::string pattern;
        bool replacedFirst = false;
        for (size_t i = 0; i < words.size(); ++i) {
            if (!replacedFirst && RussianGrammar::isCyrillic(words[i]) && words[i].size() > 4) {
                pattern += "X ";
                replacedFirst = true;
            } else if (i == words.size() - 1 && replacedFirst && words[i].size() > 4) {
                pattern += "Y";
            } else {
                pattern += words[i] + " ";
            }
        }
        // Only add if has both X and Y
        if (pattern.find("X") != std::string::npos && pattern.find("Y") != std::string::npos) {
            addPattern(pattern);
        }
    }
}

void LanguageModel::addPattern(const std::string& p) {
    // Deduplicate
    for (auto& existing : patterns_) {
        if (existing == p) return;
    }
    if (patterns_.size() < 5000) {
        patterns_.push_back(p);
    }
}

std::string LanguageModel::generateResponse(const std::string& topic,
                                              const std::vector<std::string>& knownFacts) const {
    if (!canSpeak()) {
        // Babble stage — repeat known words
        if (vocab_.size() < 5) return "...";
        std::string response;
        int count = 0;
        for (auto& w : vocab_) {
            if (count++ > 3) break;
            response += w + " ";
        }
        return response;
    }

    // Try to find a pattern and fill it
    std::string best = findPattern(topic, knownFacts);
    if (!best.empty()) return best;

    // Fallback: build response from associations
    std::string response;
    auto it = wordAssociations_.find(topic);
    if (it != wordAssociations_.end() && !it->second.empty()) {
        response = topic;
        std::string current = topic;
        for (int i = 0; i < 8; ++i) {
            auto ait = wordAssociations_.find(current);
            if (ait == wordAssociations_.end() || ait->second.empty()) break;
            // Pick first association (deterministic for now)
            current = ait->second[i % ait->second.size()];
            response += " " + current;
        }
        return response;
    }

    // Last resort: mention what we know about the topic
    for (auto& fact : knownFacts) {
        if (fact.find(RussianGrammar::transliterate(topic)) != std::string::npos ||
            fact.find(topic) != std::string::npos) {
            return topic + ": " + fact;
        }
    }

    return topic + "... " + generateStatus(knownFacts.size(), vocab_.size());
}

std::string LanguageModel::findPattern(const std::string& topic,
                                        const std::vector<std::string>& facts) const {
    if (patterns_.empty()) return "";

    // Find a related fact for Y
    std::string yValue;
    for (auto& fact : facts) {
        if (fact.find(RussianGrammar::transliterate(topic)) != std::string::npos) {
            // Extract the other part of the fact
            yValue = fact;
            break;
        }
    }
    if (yValue.empty() && !facts.empty()) {
        yValue = facts[0];
    }

    // Fill first available pattern
    for (auto& pat : patterns_) {
        std::string filled = pat;
        auto xpos = filled.find("X");
        if (xpos != std::string::npos) {
            filled.replace(xpos, 1, topic);
        }
        auto ypos = filled.find("Y");
        if (ypos != std::string::npos && !yValue.empty()) {
            filled.replace(ypos, 1, yValue);
            return filled;
        }
    }

    return "";
}

std::string LanguageModel::generateStatus(size_t factCount, size_t wCount) const {
    // Pre-speech status
    if (wCount < 10) {
        return "[" + std::to_string(wCount) + " words learned]";
    }
    if (wCount < 30) {
        return "[learning... " + std::to_string(wCount) + " words, " +
               std::to_string(factCount) + " facts]";
    }
    return "[vocabulary: " + std::to_string(wCount) + ", facts: " +
           std::to_string(factCount) + ", patterns: " + std::to_string(patterns_.size()) + "]";
}

bool LanguageModel::canSpeak() const {
    return vocab_.size() >= 30 && patterns_.size() >= 3;
}

size_t LanguageModel::wordCount() const { return vocab_.size(); }
size_t LanguageModel::patternCount() const { return patterns_.size(); }

} // namespace elberr
