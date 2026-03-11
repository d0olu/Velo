#include "will_engine.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iostream>

namespace elberr {

const char* Drive::name() const {
    switch (type) {
        case DriveType::CURIOSITY: return "curiosity";
        case DriveType::SOCIALITY: return "sociality";
        case DriveType::CONSERVATISM: return "conservatism";
        case DriveType::RESTLESSNESS: return "restlessness";
    }
    return "?";
}

WillEngine::WillEngine() {
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    rng_.seed(static_cast<unsigned>(seed));

    drives_.push_back({DriveType::CURIOSITY, 0.80});
    drives_.push_back({DriveType::SOCIALITY, 0.20});
    drives_.push_back({DriveType::CONSERVATISM, 0.15});
    drives_.push_back({DriveType::RESTLESSNESS, 0.05});
}

void WillEngine::update(size_t beliefCount, size_t wordCount, bool hasChatMessage,
                         size_t cyclesSinceLastLearn, size_t cyclesSinceLastChat) {
    for (auto& d : drives_) {
        switch (d.type) {
            case DriveType::CURIOSITY:
                // Grows when we haven't learned recently
                d.intensity += 0.002 * std::min(cyclesSinceLastLearn, (size_t)100);
                // Decays as knowledge grows (satiation)
                if (beliefCount > 100) d.intensity -= 0.001;
                break;

            case DriveType::SOCIALITY:
                // Spikes when someone writes
                if (hasChatMessage) d.intensity += 0.5;
                // Decays naturally
                d.intensity -= 0.005;
                // Grows slowly when chat is idle for long
                d.intensity += 0.001 * std::min(cyclesSinceLastChat, (size_t)50);
                break;

            case DriveType::CONSERVATISM:
                // Grows with knowledge base size
                d.intensity = 0.1 + 0.001 * std::min(beliefCount, (size_t)500);
                break;

            case DriveType::RESTLESSNESS:
                // Always slowly accumulates
                d.intensity += 0.003;
                // Higher when doing same thing repeatedly
                if (cyclesSinceLastLearn > 20 && cyclesSinceLastChat > 20) {
                    d.intensity += 0.01;
                }
                break;
        }

        // Clamp to [0, 1]
        d.intensity = std::max(0.0, std::min(1.0, d.intensity));
    }
}

WillDecision WillEngine::decide(bool hasActiveGoal, bool hasChatMessage) {
    WillDecision decision;
    ++decisionCount_;

    // Weighted random selection: P(drive) ~ intensity^2 + noise
    std::vector<double> weights;
    for (auto& d : drives_) {
        double w = d.intensity * d.intensity;
        std::uniform_real_distribution<double> noise(0.0, 0.1);
        w += noise(rng_);
        weights.push_back(std::max(0.01, w));
    }

    // Normalize
    double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (auto& w : weights) w /= total;

    // Pick
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    double cumulative = 0;
    size_t chosen = 0;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (r <= cumulative) {
            chosen = i;
            break;
        }
    }

    DriveType dominant = drives_[chosen].type;
    decision.dominantDrive = dominant;
    decision.driveIntensity = drives_[chosen].intensity;

    // Map drive to action
    switch (dominant) {
        case DriveType::CURIOSITY:
            if (hasActiveGoal) {
                decision.action = ActionType::LEARN_FROM_WEB;
                decision.reason = "curiosity drives learning";
            } else {
                decision.action = ActionType::EXPLORE_CONCEPT;
                decision.reason = "curiosity without goal -> explore";
                decision.spontaneous = true;
            }
            break;

        case DriveType::SOCIALITY:
            if (hasChatMessage) {
                decision.action = ActionType::RESPOND_TO_CHAT;
                decision.reason = "someone is talking to me";
            } else {
                decision.action = ActionType::REASON;
                decision.reason = "social drive but no one to talk to -> think";
            }
            break;

        case DriveType::CONSERVATISM:
            decision.action = ActionType::REASON;
            decision.reason = "need to consolidate knowledge";
            break;

        case DriveType::RESTLESSNESS:
            decision.action = ActionType::EXPLORE_CONCEPT;
            decision.reason = "restless -> explore something new";
            decision.spontaneous = true;
            break;
    }

    // Override: always respond to chat if message present (politeness)
    if (hasChatMessage && decision.action != ActionType::RESPOND_TO_CHAT) {
        // 70% chance to override
        if (dist(rng_) < 0.7) {
            decision.action = ActionType::RESPOND_TO_CHAT;
            decision.reason = "chat message takes priority";
        }
    }

    return decision;
}

void WillEngine::onActionDone(ActionType action) {
    switch (action) {
        case ActionType::LEARN_FROM_WEB:
            // Satisfy curiosity
            for (auto& d : drives_) {
                if (d.type == DriveType::CURIOSITY) d.intensity *= 0.7;
            }
            break;
        case ActionType::RESPOND_TO_CHAT:
            for (auto& d : drives_) {
                if (d.type == DriveType::SOCIALITY) d.intensity *= 0.3;
            }
            break;
        case ActionType::REASON:
            for (auto& d : drives_) {
                if (d.type == DriveType::CONSERVATISM) d.intensity *= 0.8;
            }
            break;
        case ActionType::EXPLORE_CONCEPT:
            for (auto& d : drives_) {
                if (d.type == DriveType::RESTLESSNESS) d.intensity *= 0.2;
                if (d.type == DriveType::CURIOSITY) d.intensity *= 0.8;
            }
            break;
        case ActionType::IDLE:
            break;
    }
}

} // namespace elberr
