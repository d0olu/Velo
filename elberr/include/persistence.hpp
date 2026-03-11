#pragma once
#include <string>
#include "belief_base.hpp"
#include "language_model.hpp"

namespace elberr {

// Persistence: save/load agent memory to disk
// Binary format: header + beliefs + rules + vocabulary + patterns + goals + events
class Persistence {
public:
    explicit Persistence(const std::string& dir = "");

    // Save full agent state
    bool save(const BeliefBase& base,
              const LanguageModel& lang,
              const std::vector<std::pair<std::string, bool>>& goals,
              int64_t totalCycles,
              const std::vector<std::string>& events);

    // Load agent state
    bool load(BeliefBase& base,
              LanguageModel& lang,
              std::vector<std::pair<std::string, bool>>& goals,
              int64_t& totalCycles,
              std::vector<std::string>& events);

    bool memoryExists() const;
    std::string memoryPath() const { return memoryPath_; }

private:
    std::string dir_;
    std::string memoryPath_;

    void ensureDir();

    // Binary helpers
    void writeString(std::ostream& os, const std::string& s);
    std::string readString(std::istream& is);
    void writeDouble(std::ostream& os, double d);
    double readDouble(std::istream& is);
    void writeInt64(std::ostream& os, int64_t v);
    int64_t readInt64(std::istream& is);
    void writeInt32(std::ostream& os, int32_t v);
    int32_t readInt32(std::istream& is);
};

} // namespace elberr
