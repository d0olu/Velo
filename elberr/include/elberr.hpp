#pragma once
#include <string>

namespace elberr {

// Main facade — just starts the agent
void runAgent(const std::string& goal, int port);

} // namespace elberr
