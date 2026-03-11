#include "reasoner.hpp"
#include <iostream>
#include <algorithm>

namespace elberr {

ReasonResult RecursiveReasoner::ask(const std::string& formulaStr) {
    FormulaParser parser;
    FormulaPtr f;
    try {
        f = parser.parse(formulaStr);
    } catch (const std::exception& e) {
        ReasonResult r;
        r.trace.push_back("Parse error: " + std::string(e.what()));
        return r;
    }

    std::unordered_set<std::string> visited;
    return askRec(f, 0, visited);
}

ReasonResult RecursiveReasoner::askRec(const FormulaPtr& f, int depth,
                                        std::unordered_set<std::string>& visited) {
    ReasonResult result;
    if (!f || depth > MAX_DEPTH) {
        result.status = EStatus::UNKNOWN;
        result.trace.push_back("Max depth or null formula");
        return result;
    }

    std::string key = f->toString();

    // Cycle detection with RAII guard
    if (visited.count(key)) {
        result.status = EStatus::UNKNOWN;
        result.trace.push_back("Cycle detected: " + key);
        return result;
    }
    VisitGuard guard(visited, key);

    if (trace_) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "[ask d=" << depth << "] " << key << "\n";
    }

    switch (f->type) {
        case FormulaType::ATOM: {
            // Step 1: Direct lookup
            auto* entry = base_.lookup(f->atom);
            if (entry) {
                result.status = entry->status;
                result.confidence = entry->confidence;
                result.trace.push_back("Found: " + f->atom + " = " +
                    statusToStr(entry->status));
                return result;
            }
            // Step 4: Backward chaining — try rules
            result = tryBackwardChain(f->atom, depth, visited);
            if (result.status != EStatus::UNKNOWN) return result;

            // Step 5: Check for contradiction
            if (base_.has("~" + f->atom)) {
                result.status = EStatus::CONTRADICTED;
                result.confidence = base_.confidenceOf("~" + f->atom);
                result.trace.push_back("Contradicted: ~" + f->atom + " exists");
                return result;
            }

            // Step 6: Unknown
            result.status = EStatus::UNKNOWN;
            result.trace.push_back("Unknown: " + f->atom);
            return result;
        }

        case FormulaType::NOT: {
            // Step 2: Decomposition — NOT
            auto sub = askRec(f->left, depth + 1, visited);
            if (sub.status == EStatus::KNOWN) {
                result.status = EStatus::CONTRADICTED;
                result.confidence = sub.confidence;
            } else if (sub.status == EStatus::UNKNOWN) {
                result.status = EStatus::UNKNOWN;
                result.confidence = 0.0;
            } else {
                result.status = EStatus::BELIEVED;
                result.confidence = 1.0 - sub.confidence;
            }
            result.trace = sub.trace;
            result.trace.push_back("NOT applied");
            return result;
        }

        case FormulaType::AND: {
            auto l = askRec(f->left, depth + 1, visited);
            auto r = askRec(f->right, depth + 1, visited);
            if (l.status == EStatus::UNKNOWN || r.status == EStatus::UNKNOWN) {
                result.status = EStatus::UNKNOWN;
            } else if (l.status == EStatus::KNOWN && r.status == EStatus::KNOWN) {
                result.status = EStatus::KNOWN;
            } else {
                result.status = EStatus::BELIEVED;
            }
            result.confidence = clampConf(l.confidence * r.confidence);
            result.trace = l.trace;
            result.trace.insert(result.trace.end(), r.trace.begin(), r.trace.end());
            result.trace.push_back("AND combined");
            return result;
        }

        case FormulaType::OR: {
            auto l = askRec(f->left, depth + 1, visited);
            auto r = askRec(f->right, depth + 1, visited);
            if (l.status == EStatus::KNOWN || r.status == EStatus::KNOWN) {
                result.status = EStatus::KNOWN;
                result.confidence = std::max(l.confidence, r.confidence);
            } else if (l.status == EStatus::BELIEVED || r.status == EStatus::BELIEVED) {
                result.status = EStatus::BELIEVED;
                result.confidence = clampConf(l.confidence + r.confidence -
                                              l.confidence * r.confidence);
            } else {
                result.status = EStatus::UNKNOWN;
            }
            result.trace.push_back("OR combined");
            return result;
        }

        case FormulaType::IMPLIES: {
            // p -> q  ≡  ~p | q
            auto l = askRec(f->left, depth + 1, visited);
            auto r = askRec(f->right, depth + 1, visited);
            if (l.status == EStatus::KNOWN && l.confidence > 0.5) {
                result = r;
            } else {
                result.status = EStatus::BELIEVED;
                result.confidence = clampConf(1.0 - l.confidence + l.confidence * r.confidence);
            }
            result.trace.push_back("IMPLIES evaluated");

            // Side effect: if antecedent is known, create rule for forward chaining
            if (l.status == EStatus::KNOWN || l.status == EStatus::BELIEVED) {
                std::vector<std::string> rightAtoms;
                f->right->collectAtoms(rightAtoms);
                if (!rightAtoms.empty()) {
                    Rule rule;
                    rule.condition = f->left;
                    rule.conclusion = rightAtoms[0];
                    rule.conclusionStatus = r.status;
                    rule.strength = clampConf(l.confidence * 0.8);
                    base_.addRule(std::move(rule));
                }
            }
            return result;
        }

        case FormulaType::BICONDI: {
            auto l = askRec(f->left, depth + 1, visited);
            auto r = askRec(f->right, depth + 1, visited);
            if (l.status == r.status && l.status != EStatus::UNKNOWN) {
                result.status = EStatus::BELIEVED;
                result.confidence = clampConf(l.confidence * r.confidence);
            } else {
                result.status = EStatus::UNKNOWN;
            }
            result.trace.push_back("BICONDI evaluated");
            return result;
        }

        case FormulaType::KNOWS: {
            // Step 3: Modal operator K(φ) — "does the agent KNOW φ?"
            auto sub = askRec(f->left, depth + 1, visited);
            if (sub.status == EStatus::KNOWN && sub.confidence >= 0.7) {
                result.status = EStatus::KNOWN;
                result.confidence = sub.confidence;
            } else {
                result.status = EStatus::UNKNOWN;
                result.confidence = 0.0;
            }
            result.trace = sub.trace;
            result.trace.push_back("K() checked: " +
                std::string(sub.status == EStatus::KNOWN ? "YES" : "NO"));
            return result;
        }

        case FormulaType::BELIEVES: {
            // B(φ) — "does the agent BELIEVE φ?"
            auto sub = askRec(f->left, depth + 1, visited);
            if (sub.status == EStatus::KNOWN || sub.status == EStatus::BELIEVED) {
                result.status = EStatus::KNOWN;
                result.confidence = sub.confidence;
            } else {
                result.status = EStatus::UNKNOWN;
                result.confidence = 0.0;
            }
            result.trace = sub.trace;
            result.trace.push_back("B() checked: " +
                std::string(result.status == EStatus::KNOWN ? "YES" : "NO"));
            return result;
        }
    }

    return result;
}

ReasonResult RecursiveReasoner::tryBackwardChain(const std::string& atom, int depth,
                                                   std::unordered_set<std::string>& visited) {
    ReasonResult best;
    best.status = EStatus::UNKNOWN;

    for (auto& rule : base_.rules()) {
        if (rule.conclusion != atom) continue;

        auto condAtoms = rule.conditionAtoms();
        if (condAtoms.empty()) continue;

        bool allSatisfied = true;
        double minConf = 1.0;
        std::vector<std::string> trace;

        for (auto& ca : condAtoms) {
            auto sub = askRec(Formula::makeAtom(ca), depth + 1, visited);
            if (sub.status == EStatus::UNKNOWN || sub.status == EStatus::CONTRADICTED) {
                allSatisfied = false;
                break;
            }
            minConf = std::min(minConf, sub.confidence);
            trace.insert(trace.end(), sub.trace.begin(), sub.trace.end());
        }

        if (allSatisfied) {
            double conf = clampConf(minConf * rule.strength);
            if (conf > best.confidence) {
                best.status = rule.conclusionStatus;
                best.confidence = conf;
                best.trace = trace;
                best.trace.push_back("Backward chain: " +
                    rule.condition->toString() + " -> " + atom);
            }
        }
    }

    // If we derived something, expand the base for future lookups
    if (best.status != EStatus::UNKNOWN) {
        base_.expand(atom, best.status, best.confidence);
    }

    return best;
}

int RecursiveReasoner::forwardChain() {
    int fired = 0;
    bool changed = true;
    int iterations = 0;

    while (changed && iterations < 100) {
        changed = false;
        ++iterations;

        for (auto& rule : base_.rules()) {
            if (base_.has(rule.conclusion)) continue;

            auto condAtoms = rule.conditionAtoms();
            bool allPresent = true;
            double minConf = 1.0;

            for (auto& ca : condAtoms) {
                auto* entry = base_.lookup(ca);
                if (!entry || entry->status == EStatus::UNKNOWN ||
                    entry->status == EStatus::CONTRADICTED) {
                    allPresent = false;
                    break;
                }
                minConf = std::min(minConf, entry->confidence);
            }

            if (allPresent) {
                double conf = clampConf(minConf * rule.strength);
                base_.expand(rule.conclusion, rule.conclusionStatus, conf);
                ++fired;
                changed = true;
                if (trace_) {
                    std::cout << "[FWD] " << rule.condition->toString()
                              << " -> " << rule.conclusion
                              << " (" << conf << ")\n";
                }
            }
        }
    }

    return fired;
}

} // namespace elberr
