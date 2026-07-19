#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace fishpond {

template <typename Graph>
struct PreparedGraphCommand {
    Graph* graph {};
    std::uint64_t configurationVersion {};
    std::uint64_t commandId {};
};

// Single producer: host-management worker. Single consumer: audio callback.
// The callback can also retire an old graph; only the worker destroys it.
template <typename Graph, std::size_t Capacity>
class PreparedGraphHandoff {
    static_assert(Capacity > 1, "handoff capacity must leave one sentinel slot");

public:
    bool submit(PreparedGraphCommand<Graph> command) noexcept
    {
        if (command.graph == nullptr)
            return false;

        const auto write = pendingWrite.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == pendingRead.load(std::memory_order_acquire))
            return false;

        pending[write] = command;
        pendingWrite.store(next, std::memory_order_release);
        return true;
    }

    bool tryTake(PreparedGraphCommand<Graph>& command) noexcept
    {
        const auto read = pendingRead.load(std::memory_order_relaxed);
        if (read == pendingWrite.load(std::memory_order_acquire))
            return false;

        command = pending[read];
        pendingRead.store(increment(read), std::memory_order_release);
        return true;
    }

    bool retireFromAudio(Graph* graph) noexcept
    {
        if (graph == nullptr)
            return true;

        const auto write = retiredWrite.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == retiredRead.load(std::memory_order_acquire))
            return false;

        retired[write] = graph;
        retiredWrite.store(next, std::memory_order_release);
        return true;
    }

    std::unique_ptr<Graph> reclaimOnWorker() noexcept
    {
        const auto read = retiredRead.load(std::memory_order_relaxed);
        if (read == retiredWrite.load(std::memory_order_acquire))
            return {};

        auto* graph = retired[read];
        retiredRead.store(increment(read), std::memory_order_release);
        return std::unique_ptr<Graph>(graph);
    }

private:
    static constexpr std::size_t increment(std::size_t index) noexcept
    {
        return (index + 1) % Capacity;
    }

    std::array<PreparedGraphCommand<Graph>, Capacity> pending {};
    std::array<Graph*, Capacity> retired {};
    std::atomic<std::size_t> pendingWrite {};
    std::atomic<std::size_t> pendingRead {};
    std::atomic<std::size_t> retiredWrite {};
    std::atomic<std::size_t> retiredRead {};
};

} // namespace fishpond
