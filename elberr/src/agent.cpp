#include "agent.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <random>

namespace elberr {

Agent::Agent(const std::string& initialGoal, int chatPort)
    : reasoner_(base_),
      explorer_([this](const PageInfo& p) { onPageLearned(p); }),
      chat_(chatPort, [this](const std::string& msg) { onChatMessage(msg); }),
      initialGoalText_(initialGoal)
{
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║  E.L.B.E.R.R v2.0                                  ║\n";
    std::cout << "║  Epistemic Logic-Based Engine for Recursive Reason  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Initial goal: " << initialGoal << "\n";
    std::cout << "║  " << gpu_.info() << "\n";
    std::cout << "║  Chat: http://localhost:" << chatPort << "\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";
}

Agent::~Agent() {
    stop();
}

void Agent::stop() {
    stopFlag_ = true;
    explorer_.stop();
    chat_.stop();
    saveMemory();
}

// === Goal management ===

void Agent::seedGoals() {
    // Core developmental goals — like a child learning to speak
    AgentGoal g1;
    g1.id = "perceive_language";
    g1.description = "learn to perceive written language";
    g1.priority = 0.98;
    g1.searchQuery = "\xD1\x8F\xD0\xB7\xD1\x8B\xD0\xBA"; // язык
    g1.requiredAtoms = {"word_yazyk", "word_tekst", "word_bukva", "word_slovo"};
    g1.seedURLs = {};
    goals_.push_back(g1);

    AgentGoal g2;
    g2.id = "build_vocabulary";
    g2.description = "accumulate vocabulary of words";
    g2.priority = 0.95;
    g2.searchQuery = "\xD1\x81\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x80\xD1\x8C \xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"; // словарь русский
    g2.requiredAtoms = {};  // achieved when wordCount > 100
    goals_.push_back(g2);

    AgentGoal g3;
    g3.id = "understand_grammar";
    g3.description = "learn grammar structures";
    g3.priority = 0.90;
    g3.searchQuery = "\xD0\xB3\xD1\x80\xD0\xB0\xD0\xBC\xD0\xBC\xD0\xB0\xD1\x82\xD0\xB8\xD0\xBA\xD0\xB0 \xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xBE\xD0\xB3\xD0\xBE \xD1\x8F\xD0\xB7\xD1\x8B\xD0\xBA\xD0\xB0"; // грамматика русского языка
    g3.requiredAtoms = {};  // achieved when patternCount > 20
    goals_.push_back(g3);

    AgentGoal g4;
    g4.id = "learn_reasoning";
    g4.description = "learn to reason and draw conclusions";
    g4.priority = 0.80;
    g4.searchQuery = "\xD0\xBB\xD0\xBE\xD0\xB3\xD0\xB8\xD0\xBA\xD0\xB0 \xD1\x80\xD0\xB0\xD1\x81\xD1\x81\xD1\x83\xD0\xB6\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB5"; // логика рассуждение
    g4.requiredAtoms = {};  // achieved when rules > 50
    goals_.push_back(g4);

    AgentGoal g5;
    g5.id = "speak";
    g5.description = "learn to communicate in natural language";
    g5.priority = 0.70;
    g5.searchQuery = "";
    g5.requiredAtoms = {};  // achieved when canSpeak() is true
    goals_.push_back(g5);

    // Also add the user's custom goal if different
    if (!initialGoalText_.empty()) {
        AgentGoal ug;
        ug.id = "user_goal";
        ug.description = initialGoalText_;
        ug.priority = 0.85;
        ug.searchQuery = initialGoalText_;
        goals_.push_back(ug);
    }
}

void Agent::spawnSubGoals(int goalIdx) {
    if (goalIdx < 0 || goalIdx >= static_cast<int>(goals_.size())) return;
    auto& goal = goals_[goalIdx];

    if (goal.id == "perceive_language" && goal.achieved) {
        // Spawn: read literature
        bool exists = false;
        for (auto& g : goals_) {
            if (g.id == "read_literature") { exists = true; break; }
        }
        if (!exists) {
            AgentGoal g;
            g.id = "read_literature";
            g.description = "read Russian literature";
            g.priority = 0.75;
            g.searchQuery = "\xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB0\xD1\x8F \xD0\xBB\xD0\xB8\xD1\x82\xD0\xB5\xD1\x80\xD0\xB0\xD1\x82\xD1\x83\xD1\x80\xD0\xB0"; // русская литература
            goals_.push_back(g);
            logEvent("Spawned subgoal: read_literature");
        }
    }

    if (goal.id == "build_vocabulary" && goal.achieved) {
        bool exists = false;
        for (auto& g : goals_) {
            if (g.id == "learn_science") { exists = true; break; }
        }
        if (!exists) {
            AgentGoal g;
            g.id = "learn_science";
            g.description = "learn science concepts";
            g.priority = 0.65;
            g.searchQuery = "\xD0\xBD\xD0\xB0\xD1\x83\xD0\xBA\xD0\xB0"; // наука
            goals_.push_back(g);
            logEvent("Spawned subgoal: learn_science");
        }
    }
}

int Agent::findActiveGoal() const {
    int bestIdx = -1;
    double bestPriority = -1;
    for (int i = 0; i < static_cast<int>(goals_.size()); ++i) {
        if (!goals_[i].achieved && goals_[i].priority > bestPriority) {
            bestPriority = goals_[i].priority;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// === Callbacks ===

void Agent::onPageLearned(const PageInfo& page) {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    // Feed text to language model
    for (auto& sentence : page.sentences) {
        langModel_.learnFromText(sentence);

        // Morphological analysis for Russian text
        if (RussianGrammar::isCyrillic(sentence)) {
            auto morphs = grammar_.analyzeSentence(sentence);
            for (auto& m : morphs) {
                langModel_.addWord(m.original);
                if (!m.lemma.empty()) langModel_.addWord(m.lemma);

                // Add transliterated atoms to belief base
                std::string atom = "word_" + RussianGrammar::transliterate(m.lemma);
                base_.expand(atom, EStatus::BELIEVED, 0.6);
            }

            // Extract SVO triples → logical propositions
            auto triples = grammar_.extractSVO(morphs);
            for (auto& t : triples) {
                std::string prop = t.asProposition();
                if (!prop.empty()) {
                    base_.expand(prop, EStatus::BELIEVED, 0.5);
                }

                // Create implication rules from "X is Y" patterns
                if (t.verb == "is" && !t.subject.empty() && !t.object.empty()) {
                    Rule rule;
                    std::string sAtom = "word_" + RussianGrammar::transliterate(t.subject);
                    rule.condition = Formula::makeAtom(sAtom);
                    rule.conclusion = "word_" + RussianGrammar::transliterate(t.object);
                    rule.conclusionStatus = EStatus::BELIEVED;
                    rule.strength = 0.7;
                    base_.addRule(std::move(rule));
                }
            }
        } else {
            // English text — just extract words and basic facts
            std::istringstream iss(sentence);
            std::string word;
            while (iss >> word) {
                // Clean punctuation
                while (!word.empty() && !std::isalnum(word.back())) word.pop_back();
                if (word.size() > 2) {
                    // Convert to lowercase
                    std::string lower;
                    for (char c : word) lower += std::tolower(c);
                    langModel_.addWord(lower);
                }
            }
        }
    }

    cyclesSinceLearn_ = 0;
}

void Agent::onChatMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(chatMtx_);
    chatMessages_.push(msg);
}

// === Cognitive loop ===

void Agent::run() {
    // Load previous memory
    loadMemory();

    // Seed goals if fresh start
    if (goals_.empty()) {
        seedGoals();
        logEvent("Agent born — goals seeded");
    }

    // Start subsystems
    chat_.start();

    // Seed initial URLs from Wikipedia search
    {
        int activeIdx = findActiveGoal();
        if (activeIdx >= 0 && !goals_[activeIdx].searchQuery.empty()) {
            explorer_.addSearchQuery(goals_[activeIdx].searchQuery, "ru");
        }
    }
    explorer_.start();

    std::cout << "[Agent] Cognitive loop started\n";

    while (!stopFlag_.load()) {
        ++totalCycles_;

        // 1. PERCEIVE
        perceive();

        // 2. ASSESS goals
        assessGoals();

        // 3. DECIDE what to do
        WillDecision decision = decide();

        // 4. ACT
        act(decision);

        // 5. REMEMBER
        remember();

        // Periodic save
        if (totalCycles_ % 50 == 0) {
            saveMemory();
        }

        // Broadcast status to chat UI
        if (totalCycles_ % 5 == 0) {
            broadcastStatus();
        }

        // Don't spin too fast
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    saveMemory();
    std::cout << "[Agent] Stopped after " << totalCycles_ << " cycles\n";
}

void Agent::perceive() {
    // Check for incoming chat messages (done via callback)
    ++cyclesSinceLearn_;
    ++cyclesSinceChat_;
}

void Agent::assessGoals() {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    for (int i = 0; i < static_cast<int>(goals_.size()); ++i) {
        auto& goal = goals_[i];
        if (goal.achieved) continue;

        bool wasAchieved = false;

        // Check by required atoms
        if (!goal.requiredAtoms.empty()) {
            bool allPresent = true;
            for (auto& atom : goal.requiredAtoms) {
                if (!base_.has(atom)) { allPresent = false; break; }
            }
            if (allPresent) wasAchieved = true;
        }

        // Check special conditions
        if (goal.id == "build_vocabulary" && langModel_.wordCount() >= 100) {
            wasAchieved = true;
        }
        if (goal.id == "understand_grammar" && langModel_.patternCount() >= 20) {
            wasAchieved = true;
        }
        if (goal.id == "learn_reasoning" && base_.ruleCount() >= 50) {
            wasAchieved = true;
        }
        if (goal.id == "speak" && langModel_.canSpeak()) {
            wasAchieved = true;
        }

        if (wasAchieved && !goal.achieved) {
            goal.achieved = true;
            logEvent("Goal achieved: " + goal.id);
            std::cout << "[Agent] *** GOAL ACHIEVED: " << goal.id << " ***\n";

            // Spawn subgoals
            spawnSubGoals(i);
        }
    }
}

WillDecision Agent::decide() {
    bool hasChatMsg = false;
    {
        std::lock_guard<std::mutex> lock(chatMtx_);
        hasChatMsg = !chatMessages_.empty();
    }

    bool hasActiveGoal = (findActiveGoal() >= 0);

    // Update drives based on current state
    will_.update(base_.size(), langModel_.wordCount(), hasChatMsg,
                 cyclesSinceLearn_, cyclesSinceChat_);

    return will_.decide(hasActiveGoal, hasChatMsg);
}

void Agent::act(const WillDecision& decision) {
    switch (decision.action) {
        case ActionType::LEARN_FROM_WEB: {
            int idx = findActiveGoal();
            actLearnFromWeb(idx);
            break;
        }
        case ActionType::REASON:
            actReason();
            break;
        case ActionType::RESPOND_TO_CHAT:
            actRespondToChat();
            break;
        case ActionType::EXPLORE_CONCEPT:
            actExploreConcept();
            break;
        case ActionType::IDLE:
            break;
    }

    will_.onActionDone(decision.action);
}

void Agent::remember() {
    // Nothing special per cycle — persistence handles long-term memory
}

// === Actions ===

void Agent::actLearnFromWeb(int goalIdx) {
    if (goalIdx < 0 || goalIdx >= static_cast<int>(goals_.size())) return;

    // If explorer is idle, feed it new queries
    if (explorer_.queueSize() == 0) {
        std::string query = goals_[goalIdx].searchQuery;
        if (!query.empty()) {
            explorer_.addSearchQuery(query, "ru");
        }
        // Also try English
        if (!goals_[goalIdx].description.empty()) {
            explorer_.addSearchQuery(goals_[goalIdx].description, "en");
        }
    }
}

void Agent::actReason() {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    // Forward chain: derive new facts from rules
    int fired = reasoner_.forwardChain();
    if (fired > 0) {
        logEvent("Forward chain fired " + std::to_string(fired) + " rules");
    }

    // GPU batch propagation if enough data
    if (base_.size() > 10 && base_.ruleCount() > 5) {
        std::vector<double> confidences;
        std::vector<double> strengths;
        std::vector<std::vector<int>> ruleInputs;
        std::unordered_map<std::string, int> atomIndex;

        int idx = 0;
        for (auto& [atom, entry] : base_.entries()) {
            atomIndex[atom] = idx;
            confidences.push_back(entry.confidence);
            ++idx;
        }

        for (auto& rule : base_.rules()) {
            strengths.push_back(rule.strength);
            std::vector<int> inputs;
            for (auto& ca : rule.conditionAtoms()) {
                auto it = atomIndex.find(ca);
                if (it != atomIndex.end()) inputs.push_back(it->second);
            }
            ruleInputs.push_back(inputs);
        }

        gpu_.confidencePropagate(confidences, strengths, ruleInputs);
    }
}

void Agent::actRespondToChat() {
    std::string message;
    {
        std::lock_guard<std::mutex> lock(chatMtx_);
        if (chatMessages_.empty()) return;
        message = chatMessages_.front();
        chatMessages_.pop();
    }

    cyclesSinceChat_ = 0;

    std::string response;
    {
        std::lock_guard<std::mutex> lock(knowledgeMtx_);

        // If message is in Russian, analyze it and learn from it
        if (RussianGrammar::isCyrillic(message)) {
            langModel_.learnFromText(message);
            auto morphs = grammar_.analyzeSentence(message);
            for (auto& m : morphs) {
                if (!m.lemma.empty()) {
                    base_.expand("word_" + RussianGrammar::transliterate(m.lemma),
                                EStatus::BELIEVED, 0.7);
                }
            }
        } else {
            langModel_.learnFromText(message);
        }

        // Generate response
        std::vector<std::string> facts;
        for (auto& [atom, entry] : base_.entries()) {
            if (entry.confidence > 0.4) {
                facts.push_back(atom);
            }
        }

        // Extract topic from message
        std::string topic = message;
        // Try to find a content word
        auto words = grammar_.analyzeSentence(message);
        for (auto& w : words) {
            if (w.pos == POS::NOUN || w.pos == POS::VERB || w.pos == POS::ADJ) {
                topic = w.lemma;
                break;
            }
        }

        response = langModel_.generateResponse(topic, facts);
    }

    if (response.empty()) {
        response = langModel_.generateStatus(base_.size(), langModel_.wordCount());
    }

    // Send response
    chat_.broadcast("{\"type\":\"response\",\"text\":\"" + response + "\"}");
    logEvent("Chat response: " + response.substr(0, 50));
}

void Agent::actExploreConcept() {
    std::string conceptQuery = pickRandomConcept();
    if (!conceptQuery.empty()) {
        explorer_.addSearchQuery(conceptQuery, "ru");
        logEvent("Spontaneous exploration: " + conceptQuery);
    }
}

std::string Agent::pickRandomConcept() {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    if (base_.size() == 0) return "";

    // Pick a random atom from the belief base
    static std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, base_.size() - 1);
    size_t target = dist(rng);

    size_t i = 0;
    for (auto& [atom, entry] : base_.entries()) {
        if (i == target) {
            // Strip "word_" prefix and de-transliterate
            std::string query = atom;
            if (query.find("word_") == 0) query = query.substr(5);
            return query;
        }
        ++i;
    }
    return "";
}

// === UI ===

void Agent::broadcastStatus() {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    std::ostringstream ss;
    ss << "{\"type\":\"status\","
       << "\"cycle\":" << totalCycles_ << ","
       << "\"beliefs\":" << base_.size() << ","
       << "\"rules\":" << base_.ruleCount() << ","
       << "\"words\":" << langModel_.wordCount() << ","
       << "\"patterns\":" << langModel_.patternCount() << ","
       << "\"pages\":" << explorer_.pagesFetched() << ","
       << "\"canSpeak\":" << (langModel_.canSpeak() ? "true" : "false") << ","
       << "\"goals\":[";

    for (size_t i = 0; i < goals_.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"id\":\"" << goals_[i].id << "\","
           << "\"done\":" << (goals_[i].achieved ? "true" : "false") << ","
           << "\"pri\":" << goals_[i].priority << "}";
    }
    ss << "],\"drives\":[";

    auto& drives = will_.drives();
    for (size_t i = 0; i < drives.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"name\":\"" << drives[i].name() << "\","
           << "\"val\":" << drives[i].intensity << "}";
    }
    ss << "]}";

    chat_.broadcastStatus(ss.str());
}

// === Persistence ===

void Agent::saveMemory() {
    std::lock_guard<std::mutex> lock(knowledgeMtx_);

    std::vector<std::pair<std::string, bool>> goalPairs;
    for (auto& g : goals_) {
        goalPairs.push_back({g.id, g.achieved});
    }

    persistence_.save(base_, langModel_, goalPairs, totalCycles_, eventLog_);
}

void Agent::loadMemory() {
    if (!persistence_.memoryExists()) {
        std::cout << "[Memory] First run — agent born.\n";
        return;
    }

    std::vector<std::pair<std::string, bool>> goalPairs;
    persistence_.load(base_, langModel_, goalPairs, totalCycles_, eventLog_);

    // Restore goal states
    seedGoals();
    for (auto& [id, achieved] : goalPairs) {
        for (auto& g : goals_) {
            if (g.id == id) {
                g.achieved = achieved;
                break;
            }
        }
    }

    std::cout << "[Memory] Agent age: " << totalCycles_ << " cycles, "
              << eventLog_.size() << " events\n";
}

void Agent::logEvent(const std::string& event) {
    std::string entry = "[" + std::to_string(totalCycles_) + "] " + event;
    eventLog_.push_back(entry);
    std::cout << entry << "\n";

    // Also broadcast as chat event
    chat_.broadcast("{\"type\":\"event\",\"text\":\"" + event + "\"}");
}

std::vector<std::string> Agent::getURLsForQuery(const std::string& query) {
    // Fallback seed URLs based on common topics
    std::vector<std::string> urls;
    if (query.find("\xD1\x8F\xD0\xB7\xD1\x8B\xD0\xBA") != std::string::npos) {
        urls.push_back("https://ru.wikipedia.org/wiki/%D0%AF%D0%B7%D1%8B%D0%BA");
        urls.push_back("https://ru.wikipedia.org/wiki/%D0%A0%D1%83%D1%81%D1%81%D0%BA%D0%B8%D0%B9_%D1%8F%D0%B7%D1%8B%D0%BA");
    }
    if (query.find("\xD0\xBD\xD0\xB0\xD1\x83\xD0\xBA\xD0\xB0") != std::string::npos) {
        urls.push_back("https://ru.wikipedia.org/wiki/%D0%9D%D0%B0%D1%83%D0%BA%D0%B0");
    }
    return urls;
}

} // namespace elberr
