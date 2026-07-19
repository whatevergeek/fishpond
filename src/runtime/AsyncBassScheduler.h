#pragma once

#include "runtime/BassPlayerScheduler.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace fishpond {
// Owns the scheduling loop on a non-real-time thread. UI/Python-completion
// code submits commands; it never becomes a second queue producer.
template <std::size_t Capacity>
class AsyncBassScheduler {
public:
    struct Pattern {
        std::size_t playerIndex {};
        std::uint64_t channelId { 1 };
        std::vector<std::vector<int>> noteSteps;
        double periodBeats {};
        std::uint8_t velocity { 100 };
        double durationBeats { 1.0 };
    };

    AsyncBassScheduler(NoteEventQueue<Capacity>& queueToFill, const std::atomic<std::uint64_t>& renderFrameToRead)
        : queue(queueToFill), renderFrame(renderFrameToRead), worker([this] { run(); }) {}

    ~AsyncBassScheduler()
    {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        wake.notify_one();
        worker.join();
    }

    AsyncBassScheduler(const AsyncBassScheduler&) = delete;
    AsyncBassScheduler& operator=(const AsyncBassScheduler&) = delete;

    void setTiming(SchedulerTiming timing)
    {
        Command command { CommandType::timing };
        command.timing = std::move(timing);
        submit(std::move(command));
    }
    void replace(std::size_t playerIndex, std::vector<int> notes, double periodBeats, std::uint8_t velocity,
                 double durationBeats)
    {
        std::vector<std::vector<int>> noteSteps;
        noteSteps.reserve(notes.size());
        for (const auto note : notes)
            noteSteps.push_back({ note });
        replaceAll({ { playerIndex, 1, std::move(noteSteps), periodBeats, velocity, durationBeats } });
    }
    void replaceAll(std::vector<Pattern> patterns)
    {
        Command command { CommandType::replaceAll };
        command.patterns = std::move(patterns);
        submit(std::move(command));
    }
    void clear() { submit({ CommandType::clear }); }
    void remove(std::size_t playerIndex)
    {
        Command command { CommandType::remove };
        command.playerIndex = playerIndex;
        submit(std::move(command));
    }

private:
    enum class CommandType { timing, replaceAll, remove, clear };
    struct Command {
        CommandType type;
        std::size_t playerIndex {};
        SchedulerTiming timing {};
        std::vector<Pattern> patterns;
    };

    void submit(Command command)
    {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if (stopping)
                return;
            commands.push_back(std::move(command));
        }
        wake.notify_one();
    }

    void run()
    {
        BassPlayerScheduler<Capacity> scheduler(queue);
        for (;;) {
            std::deque<Command> pending;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait_for(lock, std::chrono::milliseconds(10), [this] { return stopping || ! commands.empty(); });
                if (stopping)
                    return;
                pending.swap(commands);
            }
            for (const auto& command : pending) {
                if (command.type == CommandType::timing) {
                    scheduler.setTiming(command.timing);
                }
                else if (command.type == CommandType::replaceAll) {
                    const auto startFrame = scheduler.nextBarFrame(renderFrame.load(std::memory_order_acquire));
                    for (const auto& pattern : command.patterns)
                        scheduler.replaceStepsAtFrame(pattern.playerIndex, pattern.noteSteps, pattern.periodBeats, pattern.velocity,
                                                      pattern.durationBeats, startFrame, pattern.channelId);
                } else if (command.type == CommandType::remove) {
                    scheduler.remove(command.playerIndex);
                } else {
                    scheduler.clear();
                    queue.requestPanic();
                }
            }
            scheduler.pump(renderFrame.load(std::memory_order_acquire));
        }
    }

    NoteEventQueue<Capacity>& queue;
    const std::atomic<std::uint64_t>& renderFrame;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Command> commands;
    bool stopping {};
    std::thread worker;
};
}
