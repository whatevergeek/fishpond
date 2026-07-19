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
        Command command { CommandType::replace };
        command.playerIndex = playerIndex;
        command.notes = std::move(notes);
        command.periodBeats = periodBeats;
        command.velocity = velocity;
        command.durationBeats = durationBeats;
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
    enum class CommandType { timing, replace, remove, clear };
    struct Command {
        CommandType type;
        std::size_t playerIndex {};
        SchedulerTiming timing {};
        std::vector<int> notes;
        double periodBeats {};
        std::uint8_t velocity { 100 };
        double durationBeats { 1.0 };
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
                else if (command.type == CommandType::replace) {
                    queue.requestPanic();
                    scheduler.replace(command.playerIndex, command.notes, command.periodBeats, command.velocity,
                                      command.durationBeats, renderFrame.load(std::memory_order_acquire));
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
