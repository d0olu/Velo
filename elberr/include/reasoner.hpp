#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include "belief_base.hpp"

namespace elberr {

struct ReasonResult {
    EStatus status = EStatus::UNKNOWN;
    double confidence = 0.0;
    std::vector<std::string> trace;
};

// Recursive reasoner with backward chaining, forward chaining,
// epistemic operators K()/B(), cycle detection via RAII guard
class RecursiveReasoner {
public:
    explicit RecursiveReasoner(BeliefBase& base) : base_(base) {}

    // Main query: ask about a formula string
    ReasonResult ask(const std::string& formulaStr);

    // Forward chaining: fire all rules, expand base
    int forwardChain();

    void setTrace(bool on) { trace_ = on; }
    bool tracing() const { return trace_; }

    static const int MAX_DEPTH = 16;

private:
    BeliefBase& base_;
    bool trace_ = false;

    // RAII guard for cycle detection
    struct VisitGuard {
        std::unordered_set<std::string>& visited;
        std::string key;
        VisitGuard(std::unordered_set<std::string>& v, const std::string& k)
            : visited(v), key(k) { visited.insert(k); }
        ~VisitGuard() { visited.erase(key); }
    };

    ReasonResult askRec(const FormulaPtr& f, int depth, std::unordered_set<std::string>& visited);
    ReasonResult tryBackwardChain(const std::string& atom, int depth, std::unordered_set<std::string>& visited);

    static double clampConf(double c) {
        return std::max(0.0, std::min(1.0, c));
    }
};

} // namespace elberr
