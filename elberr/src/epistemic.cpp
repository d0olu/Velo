#include "epistemic.hpp"
#include <algorithm>

namespace elberr {

const char* statusToStr(EStatus s) {
    switch (s) {
        case EStatus::KNOWN: return "KNOWN";
        case EStatus::BELIEVED: return "BELIEVED";
        case EStatus::UNKNOWN: return "UNKNOWN";
        case EStatus::CONTRADICTED: return "CONTRADICTED";
    }
    return "?";
}

EStatus strToStatus(const std::string& s) {
    if (s == "KNOWN") return EStatus::KNOWN;
    if (s == "BELIEVED") return EStatus::BELIEVED;
    if (s == "CONTRADICTED") return EStatus::CONTRADICTED;
    return EStatus::UNKNOWN;
}

std::vector<std::string> Rule::conditionAtoms() const {
    std::vector<std::string> atoms;
    if (condition) condition->collectAtoms(atoms);
    return atoms;
}

} // namespace elberr
