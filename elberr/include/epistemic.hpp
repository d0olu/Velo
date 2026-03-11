#pragma once
#include <string>
#include <vector>
#include "formula.hpp"

namespace elberr {

enum class EStatus {
    KNOWN,         // certain knowledge
    BELIEVED,      // plausible belief
    UNKNOWN,       // no information
    CONTRADICTED   // conflicting evidence
};

const char* statusToStr(EStatus s);
EStatus strToStatus(const std::string& s);

struct EpistemicEntry {
    std::string atom;
    EStatus status = EStatus::UNKNOWN;
    double confidence = 0.0;
    int64_t timestamp = 0; // epoch ms when added

    bool operator==(const EpistemicEntry& o) const { return atom == o.atom; }
};

struct Rule {
    FormulaPtr condition;       // antecedent (conjunction of atoms)
    std::string conclusion;     // consequent atom
    EStatus conclusionStatus = EStatus::BELIEVED;
    double strength = 0.9;

    std::vector<std::string> conditionAtoms() const;
};

} // namespace elberr
