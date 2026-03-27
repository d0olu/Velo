#include "agent.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <random>

namespace elberr {

// Escape a string for safe embedding in JSON
static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Helper to find own source directory at runtime
static std::string findSourceDir() {
    // Try relative paths from where the binary likely runs
    const char* candidates[] = {
        "elberr", "../elberr", ".", "..",
        "/home/user/Velo/elberr"  // fallback absolute
    };
    for (auto& c : candidates) {
        std::string path = std::string(c) + "/src/agent.cpp";
        std::ifstream test(path);
        if (test.good()) return c;
    }
    return "elberr"; // best guess
}

Agent::Agent(const std::string& initialGoal, int chatPort)
    : reasoner_(base_),
      explorer_([this](const PageInfo& p) { onPageLearned(p); }),
      chat_(chatPort, [this](const std::string& msg) { onChatMessage(msg); }),
      code_(findSourceDir()),
      initialGoalText_(initialGoal)
{
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║  E.L.B.E.R.R v2.0                                  ║\n";
    std::cout << "║  Epistemic Logic-Based Engine for Recursive Reason  ║\n";
    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Initial goal: " << initialGoal << "\n";
    std::cout << "║  " << gpu_.info() << "\n";
    std::cout << "║  Chat: http://localhost:" << chatPort << "\n";
    std::cout << "║  Self-modification: ENABLED\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    // Immediately scan own source code
    code_.scanSourceFiles();
    code_.parseAll();
    std::cout << "[Agent] " << code_.summary() << "\n";
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
    // PRIMARY GOAL: learn C++ and improve own source code
    AgentGoal g0;
    g0.id = "learn_cpp";
    g0.description = "learn C++ by reading own source code and documentation";
    g0.priority = 0.99;
    g0.searchQuery = "C++ programming tutorial patterns";
    g0.requiredAtoms = {"code_total_functions_10", "pattern_shared_ptr", "pattern_raii_lock"};
    goals_.push_back(g0);

    AgentGoal g0b;
    g0b.id = "self_improve";
    g0b.description = "find bugs and improvements in own source, apply patches that compile";
    g0b.priority = 0.97;
    g0b.searchQuery = "C++ best practices bug patterns";
    g0b.requiredAtoms = {}; // achieved when successfulPatches > 0
    goals_.push_back(g0b);

    // Core developmental goals — like a child learning to speak
    AgentGoal g1;
    g1.id = "perceive_language";
    g1.description = "learn to perceive written language";
    g1.priority = 0.90;
    g1.searchQuery = "\xD1\x8F\xD0\xB7\xD1\x8B\xD0\xBA"; // язык
    g1.requiredAtoms = {"word_yazyk", "word_tekst", "word_bukva", "word_slovo"};
    g1.seedURLs = {};
    goals_.push_back(g1);

    AgentGoal g2;
    g2.id = "build_vocabulary";
    g2.description = "accumulate vocabulary of words";
    g2.priority = 0.85;
    g2.searchQuery = "\xD1\x81\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x80\xD1\x8C \xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"; // словарь русский
    g2.requiredAtoms = {};  // achieved when wordCount > 100
    goals_.push_back(g2);

    AgentGoal g3;
    g3.id = "understand_grammar";
    g3.description = "learn grammar structures";
    g3.priority = 0.80;
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
    // Copy id+achieved — push_back below may invalidate references
    std::string goalId = goals_[goalIdx].id;
    bool goalAchieved = goals_[goalIdx].achieved;

    if (goalId == "perceive_language" && goalAchieved) {
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

    if (goalId == "learn_cpp" && goalAchieved) {
        bool exists = false;
        for (auto& g : goals_) {
            if (g.id == "advanced_cpp") { exists = true; break; }
        }
        if (!exists) {
            AgentGoal g;
            g.id = "advanced_cpp";
            g.description = "learn advanced C++ patterns: templates, SFINAE, concepts";
            g.priority = 0.75;
            g.searchQuery = "C++ templates SFINAE concepts metaprogramming";
            goals_.push_back(g);
            logEvent("Spawned subgoal: advanced_cpp");
        }
    }

    if (goalId == "build_vocabulary" && goalAchieved) {
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
        if (goal.id == "learn_cpp" && code_.totalFunctions() > 0) {
            // Achieved when we've successfully parsed our own code
            auto beliefs = code_.extractBeliefs();
            if (beliefs.size() >= 5) wasAchieved = true;
        }
        if (goal.id == "self_improve" && code_.successfulPatches() > 0) {
            wasAchieved = true;
        }
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
        case ActionType::SELF_MODIFY:
            actSelfModify();
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
    chat_.broadcast("{\"type\":\"response\",\"text\":\"" + escapeJson(response) + "\"}");
    // Safe substr for UTF-8: find a safe cut point
    std::string logSnippet = response;
    if (logSnippet.size() > 80) logSnippet = logSnippet.substr(0, 80) + "...";
    logEvent("Chat response: " + logSnippet);
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

// === Self-modification ===

void Agent::actSelfModify() {
    // Phase 1: Re-scan source (it might have changed)
    code_.scanSourceFiles();
    code_.parseAll();

    // Phase 2: Learn about own code → add beliefs
    {
        std::lock_guard<std::mutex> lock(knowledgeMtx_);
        auto beliefs = code_.extractBeliefs();
        for (auto& [atom, conf] : beliefs) {
            base_.expand(atom, EStatus::KNOWN, conf);
        }
    }

    // Phase 3: Find issues
    auto bugs = code_.findPotentialBugs();
    auto opts = code_.findOptimizations();
    auto imps = code_.suggestImprovements();

    // Merge all suggestions
    std::vector<CodePatch> allPatches;
    allPatches.insert(allPatches.end(), bugs.begin(), bugs.end());
    allPatches.insert(allPatches.end(), opts.begin(), opts.end());
    allPatches.insert(allPatches.end(), imps.begin(), imps.end());

    if (allPatches.empty()) {
        logEvent("Self-analysis: no issues found (" + std::to_string(code_.totalFunctions()) +
                 " functions analyzed)");
        return;
    }

    // Log what we found
    logEvent("Self-analysis found " + std::to_string(allPatches.size()) + " potential issues");

    // Phase 4: Try to apply the highest-confidence patch
    // Sort by confidence descending
    std::sort(allPatches.begin(), allPatches.end(),
        [](const CodePatch& a, const CodePatch& b) {
            return a.confidence > b.confidence;
        });

    for (auto& patch : allPatches) {
        // Only attempt patches with actual code changes
        if (patch.oldCode.empty() || patch.newCode.empty()) {
            // This is an observation, not an actionable patch — record as belief
            std::lock_guard<std::mutex> lock(knowledgeMtx_);
            std::string atom = "code_issue_" + patch.category;
            base_.expand(atom, EStatus::BELIEVED, patch.confidence);
            logEvent("Code insight: " + patch.reason);
            continue;
        }

        // Only try patches with confidence > 0.7
        if (patch.confidence < 0.7) continue;

        logEvent("Attempting self-patch: " + patch.reason);

        CompileResult result;
        bool ok = code_.tryPatch(patch, result);

        if (ok) {
            logEvent("Self-patch APPLIED successfully: " + patch.reason);
            {
                std::lock_guard<std::mutex> lock(knowledgeMtx_);
                base_.expand("self_patch_success", EStatus::KNOWN, 1.0);
                base_.expand("code_improved_" + patch.category, EStatus::KNOWN, 0.9);
            }
            // Broadcast to chat
            chat_.broadcast("{\"type\":\"event\",\"text\":\"" +
                            escapeJson("I improved my own code: " + patch.category) + "\"}");
            break; // One patch per cycle
        } else {
            logEvent("Self-patch FAILED: " + patch.reason +
                     " (errors: " + std::to_string(result.errors.size()) + ")");
            {
                std::lock_guard<std::mutex> lock(knowledgeMtx_);
                base_.expand("self_patch_failure", EStatus::BELIEVED, 0.5);
            }
            // Learn from the error
            for (auto& err : result.errors) {
                // Extract useful parts of compiler error
                langModel_.learnFromText(err);
            }
            break; // Don't try more patches this cycle
        }
    }

    // Phase 5: Learn C++ by reading own code
    // Feed own source code to the language model
    static size_t nextFileToLearn = 0;
    auto& files = code_.sourceFiles();
    if (!files.empty()) {
        size_t idx = nextFileToLearn % files.size();
        std::string content = code_.readFile(files[idx]);
        if (!content.empty()) {
            // Learn C++ keywords and patterns from own code
            std::lock_guard<std::mutex> lock(knowledgeMtx_);
            langModel_.learnFromText(content);

            // Extract function names as vocabulary
            auto constructs = code_.findInFile(files[idx]);
            for (auto& c : constructs) {
                if (!c.name.empty()) {
                    base_.expand("own_func_" + c.name, EStatus::KNOWN, 1.0);
                }
            }
        }
        ++nextFileToLearn;
    }
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
       << "\"codeFiles\":" << code_.sourceFiles().size() << ","
       << "\"codeFunctions\":" << code_.totalFunctions() << ","
       << "\"codePatches\":" << code_.successfulPatches() << ","
       << "\"codeFailedPatches\":" << code_.failedPatches() << ","
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
    chat_.broadcast("{\"type\":\"event\",\"text\":\"" + escapeJson(event) + "\"}");
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
