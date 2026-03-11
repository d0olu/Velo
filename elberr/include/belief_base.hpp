#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "epistemic.hpp"

namespace elberr {

// AGM-compatible belief base
// Supports: expansion (+), contraction (-), revision (*)
// Levi identity: K * φ = (K - ¬φ) + φ
class BeliefBase {
public:
    // Basic operations
    void expand(const std::string& atom, EStatus status, double confidence);
    void contract(const std::string& atom);
    void revise(const std::string& atom, EStatus status, double confidence = 1.0);

    // Query
    const EpistemicEntry* lookup(const std::string& atom) const;
    bool has(const std::string& atom) const;
    EStatus statusOf(const std::string& atom) const;
    double confidenceOf(const std::string& atom) const;

    // Rules
    void addRule(Rule rule);
    const std::vector<Rule>& rules() const { return rules_; }
    size_t ruleCount() const { return rules_.size(); }

    // Iteration
    const std::unordered_map<std::string, EpistemicEntry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }

    // Dump
    std::string dump() const;

    // Thread-safe wrappers
    void expandSafe(const std::string& atom, EStatus status, double confidence);
    void reviseSafe(const std::string& atom, EStatus status, double confidence = 1.0);

    mutable std::mutex mtx;

private:
    std::unordered_map<std::string, EpistemicEntry> entries_;
    std::vector<Rule> rules_;
    static const size_t MAX_RULES = 2000;

    int64_t now() const;
    std::string negateAtom(const std::string& atom) const;
};

} // namespace elberr
