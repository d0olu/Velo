#include "belief_base.hpp"
#include <sstream>
#include <chrono>
#include <algorithm>

namespace elberr {

int64_t BeliefBase::now() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string BeliefBase::negateAtom(const std::string& atom) const {
    if (!atom.empty() && atom[0] == '~') return atom.substr(1);
    return "~" + atom;
}

void BeliefBase::expand(const std::string& atom, EStatus status, double confidence) {
    confidence = std::max(0.0, std::min(1.0, confidence));
    auto it = entries_.find(atom);
    if (it != entries_.end()) {
        // Update existing: take higher confidence
        if (confidence > it->second.confidence) {
            it->second.status = status;
            it->second.confidence = confidence;
            it->second.timestamp = now();
        }
    } else {
        EpistemicEntry e;
        e.atom = atom;
        e.status = status;
        e.confidence = confidence;
        e.timestamp = now();
        entries_[atom] = e;
    }
}

void BeliefBase::contract(const std::string& atom) {
    entries_.erase(atom);
}

void BeliefBase::revise(const std::string& atom, EStatus status, double confidence) {
    // AGM Levi Identity: K * φ = (K - ¬φ) + φ
    // 1. Contract the negation
    std::string neg = negateAtom(atom);
    contract(neg);

    // 2. Also remove anything that directly contradicts
    //    e.g. if revising "~rain", remove "rain"
    if (!atom.empty() && atom[0] == '~') {
        std::string pos = atom.substr(1);
        auto it = entries_.find(pos);
        if (it != entries_.end()) {
            // Mark as contradicted if it was strongly held
            if (it->second.confidence > 0.5) {
                it->second.status = EStatus::CONTRADICTED;
                it->second.confidence *= 0.3;
            } else {
                entries_.erase(it);
            }
        }
    }

    // 3. Expand with the new belief
    expand(atom, status, confidence);
}

const EpistemicEntry* BeliefBase::lookup(const std::string& atom) const {
    auto it = entries_.find(atom);
    return it != entries_.end() ? &it->second : nullptr;
}

bool BeliefBase::has(const std::string& atom) const {
    return entries_.count(atom) > 0;
}

EStatus BeliefBase::statusOf(const std::string& atom) const {
    auto* e = lookup(atom);
    return e ? e->status : EStatus::UNKNOWN;
}

double BeliefBase::confidenceOf(const std::string& atom) const {
    auto* e = lookup(atom);
    return e ? e->confidence : 0.0;
}

void BeliefBase::addRule(Rule rule) {
    if (rules_.size() >= MAX_RULES) {
        // Remove lowest-strength rule
        auto minIt = std::min_element(rules_.begin(), rules_.end(),
            [](const Rule& a, const Rule& b) { return a.strength < b.strength; });
        if (minIt != rules_.end() && minIt->strength < rule.strength) {
            *minIt = std::move(rule);
        }
        return;
    }
    rules_.push_back(std::move(rule));
}

std::string BeliefBase::dump() const {
    std::ostringstream ss;
    ss << "=== Belief Base (" << entries_.size() << " entries, "
       << rules_.size() << " rules) ===\n";
    for (auto& [atom, e] : entries_) {
        ss << "  " << atom << " : " << statusToStr(e.status)
           << " [" << e.confidence << "]\n";
    }
    for (size_t i = 0; i < rules_.size(); ++i) {
        ss << "  rule[" << i << "]: "
           << (rules_[i].condition ? rules_[i].condition->toString() : "?")
           << " -> " << rules_[i].conclusion << "\n";
    }
    return ss.str();
}

void BeliefBase::expandSafe(const std::string& atom, EStatus status, double confidence) {
    std::lock_guard<std::mutex> lock(mtx);
    expand(atom, status, confidence);
}

void BeliefBase::reviseSafe(const std::string& atom, EStatus status, double confidence) {
    std::lock_guard<std::mutex> lock(mtx);
    revise(atom, status, confidence);
}

} // namespace elberr
