#pragma once
#include <string>
#include <vector>

namespace elberr {

struct AgentGoal {
    std::string id;
    std::string description;
    double priority = 0.5;
    bool achieved = false;
    std::string searchQuery;                // for web exploration
    std::vector<std::string> seedURLs;      // specific URLs to start from
    std::vector<std::string> requiredAtoms; // atoms that must be KNOWN/BELIEVED
};

enum class DriveType {
    CURIOSITY,        // want to learn new things
    SOCIALITY,        // want to respond to chat
    CONSERVATISM,     // want to think about what I know
    RESTLESSNESS,     // want to do something unexpected
    SELF_IMPROVEMENT  // want to understand and improve own code
};

struct Drive {
    DriveType type;
    double intensity = 0.0;
    const char* name() const;
};

enum class ActionType {
    LEARN_FROM_WEB,
    REASON,
    RESPOND_TO_CHAT,
    EXPLORE_CONCEPT,
    SELF_MODIFY,     // read/analyze/patch own source code
    IDLE
};

struct WillDecision {
    ActionType action = ActionType::IDLE;
    DriveType dominantDrive = DriveType::CURIOSITY;
    double driveIntensity = 0.0;
    bool spontaneous = false;
    std::string reason;
};

} // namespace elberr
