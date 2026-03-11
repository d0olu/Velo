#include "elberr.hpp"
#include "agent.hpp"
#include <csignal>
#include <iostream>

namespace elberr {

static Agent* globalAgent = nullptr;

static void signalHandler(int sig) {
    (void)sig;
    std::cout << "\n[ELBERR] Shutting down...\n";
    if (globalAgent) globalAgent->stop();
}

void runAgent(const std::string& goal, int port) {
    std::string actualGoal = goal.empty()
        ? "\xD1\x87\xD0\xB8\xD1\x82\xD0\xB0\xD1\x82\xD1\x8C \xD1\x82\xD0\xB5\xD0\xBA\xD1\x81\xD1\x82\xD1\x8B, \xD0\xBD\xD0\xB0\xD0\xBA\xD0\xB0\xD0\xBF\xD0\xBB\xD0\xB8\xD0\xB2\xD0\xB0\xD1\x82\xD1\x8C \xD1\x81\xD0\xBB\xD0\xBE\xD0\xB2\xD0\xB0 \xD0\xB8 \xD1\x81\xD0\xBC\xD1\x8B\xD1\x81\xD0\xBB\xD1\x8B, \xD0\xBD\xD0\xB0\xD1\x83\xD1\x87\xD0\xB8\xD1\x82\xD1\x8C\xD1\x81\xD1\x8F \xD0\xB3\xD0\xBE\xD0\xB2\xD0\xBE\xD1\x80\xD0\xB8\xD1\x82\xD1\x8C"
        : goal;
    // Default goal: "читать тексты, накапливать слова и смыслы, научиться говорить"

    Agent agent(actualGoal, port);
    globalAgent = &agent;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    agent.run();

    globalAgent = nullptr;
}

} // namespace elberr
