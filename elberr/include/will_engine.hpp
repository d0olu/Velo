#pragma once
#include <vector>
#include <random>
#include "agent_types.hpp"

namespace elberr {

// Will engine: drives compete for attention, creating spontaneous behavior
// Each drive has an intensity that grows/decays based on agent state
class WillEngine {
public:
    WillEngine();

    // Update drives based on current state
    void update(size_t beliefCount, size_t wordCount, bool hasChatMessage,
                size_t cyclesSinceLastLearn, size_t cyclesSinceLastChat);

    // Choose action based on weighted random selection of drives
    WillDecision decide(bool hasActiveGoal, bool hasChatMessage);

    // Get current drive states
    const std::vector<Drive>& drives() const { return drives_; }

    // Decay after action
    void onActionDone(ActionType action);

private:
    std::vector<Drive> drives_;
    std::mt19937 rng_;
    size_t decisionCount_ = 0;
};

} // namespace elberr
