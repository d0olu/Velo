#include "persistence.hpp"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <cstring>

namespace elberr {

static const char MAGIC[] = "ELBERR01";
static const int32_t VERSION = 2;

Persistence::Persistence(const std::string& dir) {
    if (dir.empty()) {
        const char* home = getenv("HOME");
        dir_ = home ? std::string(home) + "/.elberr" : "/tmp/.elberr";
    } else {
        dir_ = dir;
    }
    memoryPath_ = dir_ + "/memory.bin";
}

void Persistence::ensureDir() {
    mkdir(dir_.c_str(), 0700);
}

bool Persistence::memoryExists() const {
    struct stat st;
    return stat(memoryPath_.c_str(), &st) == 0;
}

void Persistence::writeString(std::ostream& os, const std::string& s) {
    int32_t len = static_cast<int32_t>(s.size());
    os.write(reinterpret_cast<const char*>(&len), 4);
    if (len > 0) os.write(s.data(), len);
}

std::string Persistence::readString(std::istream& is) {
    int32_t len = 0;
    is.read(reinterpret_cast<char*>(&len), 4);
    if (len <= 0 || len > 10000000) return "";
    std::string s(len, '\0');
    is.read(&s[0], len);
    return s;
}

void Persistence::writeDouble(std::ostream& os, double d) {
    os.write(reinterpret_cast<const char*>(&d), 8);
}

double Persistence::readDouble(std::istream& is) {
    double d = 0;
    is.read(reinterpret_cast<char*>(&d), 8);
    return d;
}

void Persistence::writeInt64(std::ostream& os, int64_t v) {
    os.write(reinterpret_cast<const char*>(&v), 8);
}

int64_t Persistence::readInt64(std::istream& is) {
    int64_t v = 0;
    is.read(reinterpret_cast<char*>(&v), 8);
    return v;
}

void Persistence::writeInt32(std::ostream& os, int32_t v) {
    os.write(reinterpret_cast<const char*>(&v), 4);
}

int32_t Persistence::readInt32(std::istream& is) {
    int32_t v = 0;
    is.read(reinterpret_cast<char*>(&v), 4);
    return v;
}

bool Persistence::save(const BeliefBase& base,
                        const LanguageModel& lang,
                        const std::vector<std::pair<std::string, bool>>& goals,
                        int64_t totalCycles,
                        const std::vector<std::string>& events) {
    ensureDir();

    // Write to temp file, then rename (atomic)
    std::string tmpPath = memoryPath_ + ".tmp";
    std::ofstream ofs(tmpPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Persistence] Failed to open " << tmpPath << "\n";
        return false;
    }

    // Magic + version
    ofs.write(MAGIC, 8);
    writeInt32(ofs, VERSION);

    // Total cycles
    writeInt64(ofs, totalCycles);

    // Beliefs
    auto& entries = base.entries();
    writeInt32(ofs, static_cast<int32_t>(entries.size()));
    for (auto& [atom, entry] : entries) {
        writeString(ofs, atom);
        writeInt32(ofs, static_cast<int32_t>(entry.status));
        writeDouble(ofs, entry.confidence);
        writeInt64(ofs, entry.timestamp);
    }

    // Rules
    auto& rules = base.rules();
    writeInt32(ofs, static_cast<int32_t>(rules.size()));
    for (auto& rule : rules) {
        writeString(ofs, rule.condition ? rule.condition->toString() : "");
        writeString(ofs, rule.conclusion);
        writeInt32(ofs, static_cast<int32_t>(rule.conclusionStatus));
        writeDouble(ofs, rule.strength);
    }

    // Vocabulary
    auto& vocab = lang.vocabulary();
    writeInt32(ofs, static_cast<int32_t>(vocab.size()));
    for (auto& w : vocab) {
        writeString(ofs, w);
    }

    // Patterns
    auto& patterns = lang.patterns();
    writeInt32(ofs, static_cast<int32_t>(patterns.size()));
    for (auto& p : patterns) {
        writeString(ofs, p);
    }

    // Goals
    writeInt32(ofs, static_cast<int32_t>(goals.size()));
    for (auto& [id, achieved] : goals) {
        writeString(ofs, id);
        writeInt32(ofs, achieved ? 1 : 0);
    }

    // Events (last 1000)
    size_t eventStart = events.size() > 1000 ? events.size() - 1000 : 0;
    int32_t eventCount = static_cast<int32_t>(events.size() - eventStart);
    writeInt32(ofs, eventCount);
    for (size_t i = eventStart; i < events.size(); ++i) {
        writeString(ofs, events[i]);
    }

    ofs.close();

    // Atomic rename
    rename(tmpPath.c_str(), memoryPath_.c_str());

    std::cout << "[Memory] Saved: " << entries.size() << " beliefs, "
              << vocab.size() << " words, " << patterns.size() << " patterns\n";
    return true;
}

bool Persistence::load(BeliefBase& base,
                        LanguageModel& lang,
                        std::vector<std::pair<std::string, bool>>& goals,
                        int64_t& totalCycles,
                        std::vector<std::string>& events) {
    std::ifstream ifs(memoryPath_, std::ios::binary);
    if (!ifs) return false;

    // Check magic
    char magic[8];
    ifs.read(magic, 8);
    if (memcmp(magic, MAGIC, 8) != 0) {
        std::cerr << "[Memory] Invalid file format\n";
        return false;
    }

    int32_t version = readInt32(ifs);
    if (version > VERSION) {
        std::cerr << "[Memory] Unsupported version " << version << "\n";
        return false;
    }

    totalCycles = readInt64(ifs);

    // Beliefs
    int32_t numBeliefs = readInt32(ifs);
    for (int32_t i = 0; i < numBeliefs && ifs.good(); ++i) {
        std::string atom = readString(ifs);
        EStatus status = static_cast<EStatus>(readInt32(ifs));
        double confidence = readDouble(ifs);
        int64_t timestamp = readInt64(ifs);
        (void)timestamp;
        base.expand(atom, status, confidence);
    }

    // Rules
    int32_t numRules = readInt32(ifs);
    FormulaParser parser;
    for (int32_t i = 0; i < numRules && ifs.good(); ++i) {
        std::string condStr = readString(ifs);
        std::string conclusion = readString(ifs);
        EStatus cStatus = static_cast<EStatus>(readInt32(ifs));
        double strength = readDouble(ifs);

        Rule rule;
        try {
            if (!condStr.empty()) rule.condition = parser.parse(condStr);
        } catch (...) {}
        rule.conclusion = conclusion;
        rule.conclusionStatus = cStatus;
        rule.strength = strength;
        if (rule.condition) base.addRule(std::move(rule));
    }

    // Vocabulary
    int32_t numWords = readInt32(ifs);
    for (int32_t i = 0; i < numWords && ifs.good(); ++i) {
        std::string word = readString(ifs);
        lang.addWord(word);
    }

    // Patterns
    int32_t numPatterns = readInt32(ifs);
    for (int32_t i = 0; i < numPatterns && ifs.good(); ++i) {
        std::string pattern = readString(ifs);
        lang.addPattern(pattern);
    }

    // Goals
    int32_t numGoals = readInt32(ifs);
    for (int32_t i = 0; i < numGoals && ifs.good(); ++i) {
        std::string id = readString(ifs);
        bool achieved = readInt32(ifs) != 0;
        goals.push_back({id, achieved});
    }

    // Events
    int32_t numEvents = readInt32(ifs);
    for (int32_t i = 0; i < numEvents && ifs.good(); ++i) {
        events.push_back(readString(ifs));
    }

    std::cout << "[Memory] Loaded: " << numBeliefs << " beliefs, "
              << numWords << " words, " << numPatterns << " patterns, "
              << totalCycles << " cycles total\n";

    return true;
}

} // namespace elberr
