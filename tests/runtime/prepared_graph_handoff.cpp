#include "host/PreparedGraphHandoff.h"

#include <iostream>
#include <memory>

namespace {
struct Graph {
    explicit Graph(int valueIn) : value(valueIn) {}
    ~Graph() { ++destroyed; }
    int value {};
    static int destroyed;
};

int Graph::destroyed = 0;

bool require(bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    const std::string test = argc > 1 ? argv[1] : "";
    fishpond::PreparedGraphHandoff<Graph, 4> handoff;

    if (test == "command") {
        auto graph = std::make_unique<Graph>(42);
        const auto accepted = handoff.submit({ graph.release(), 7, 99 });
        fishpond::PreparedGraphCommand<Graph> command;
        const auto taken = handoff.tryTake(command);
        std::unique_ptr<Graph> owner(command.graph);
        return require(accepted && taken, "prepared graph command did not cross the handoff")
            && require(command.configurationVersion == 7 && command.commandId == 99
                           && owner->value == 42,
                       "prepared graph command changed in transit") ? 0 : 1;
    }

    if (test == "retire") {
        Graph::destroyed = 0;
        auto graph = std::make_unique<Graph>(3);
        const auto retired = handoff.retireFromAudio(graph.release());
        auto reclaimed = handoff.reclaimOnWorker();
        const auto correctOwner = reclaimed != nullptr && reclaimed->value == 3;
        reclaimed.reset();
        return require(retired && correctOwner, "retired graph did not return to the worker")
            && require(Graph::destroyed == 1, "retired graph was not destroyed on the worker") ? 0 : 1;
    }

    std::cerr << "unknown test\n";
    return 2;
}
