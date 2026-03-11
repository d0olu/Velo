#include "elberr.hpp"
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string goal;
    int port = 8080;

    if (argc >= 2) goal = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);

    elberr::runAgent(goal, port);
    return 0;
}
