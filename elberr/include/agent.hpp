#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include "agent_types.hpp"
#include "belief_base.hpp"
#include "reasoner.hpp"
#include "russian_grammar.hpp"
#include "language_model.hpp"
#include "autonomous_explorer.hpp"
#include "gpu_engine.hpp"
#include "persistence.hpp"
#include "will_engine.hpp"
#include "chat_server.hpp"
#include "code_engine.hpp"

namespace elberr {

// Autonomous agent: cognitive loop, no commands, one initial goal
// Cycle: PERCEIVE → ASSESS → DECIDE → ACT → REMEMBER
class Agent {
public:
    Agent(const std::string& initialGoal, int chatPort);
    ~Agent();

    void run();  // blocking: runs cognitive loop forever
    void stop();

private:
    // Core modules
    BeliefBase base_;
    RecursiveReasoner reasoner_;
    RussianGrammar grammar_;
    LanguageModel langModel_;
    AutonomousExplorer explorer_;
    GPUEngine gpu_;
    Persistence persistence_;
    WillEngine will_;
    ChatServer chat_;
    CodeEngine code_;

    // Goals
    std::vector<AgentGoal> goals_;
    std::string initialGoalText_;

    // Incoming chat messages
    std::queue<std::string> chatMessages_;
    std::mutex chatMtx_;

    // State
    std::atomic<bool> stopFlag_{false};
    int64_t totalCycles_ = 0;
    size_t cyclesSinceLearn_ = 0;
    size_t cyclesSinceChat_ = 0;
    std::vector<std::string> eventLog_;
    std::mutex knowledgeMtx_;

    // Cognitive loop phases
    void perceive();
    void assessGoals();
    WillDecision decide();
    void act(const WillDecision& decision);
    void remember();

    // Actions
    void actLearnFromWeb(int goalIdx);
    void actReason();
    void actRespondToChat();
    void actExploreConcept();
    void actSelfModify();

    // Goal management
    void seedGoals();
    void spawnSubGoals(int goalIdx);
    int findActiveGoal() const;

    // Callbacks
    void onPageLearned(const PageInfo& page);
    void onChatMessage(const std::string& msg);

    // UI
    void broadcastStatus();

    // Persistence
    void saveMemory();
    void loadMemory();

    // Helpers
    std::string pickRandomConcept();
    void logEvent(const std::string& event);

    static std::vector<std::string> getURLsForQuery(const std::string& query);
};

} // namespace elberr
