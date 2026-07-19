#pragma once

#include "runtime/NoteEventProducer.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fishpond {
struct SchedulerTiming {
    double sampleRate { 48'000.0 };
    std::uint32_t blockSize { 512 };
    double bpm { 120.0 };
};

// Non-real-time player scheduler. It is the sole producer for its note queue;
// the audio callback only advances the observed render frame and consumes it.
template <std::size_t Capacity>
class BassPlayerScheduler {
public:
    static constexpr std::size_t maxPlayers = 26;

    explicit BassPlayerScheduler(NoteEventQueue<Capacity>& queueToFill) : producer(queueToFill) {}

    bool setTiming(SchedulerTiming next) noexcept
    {
        if (next.sampleRate <= 0.0 || next.blockSize == 0 || next.bpm <= 0.0)
            return false;
        timing = next;
        for (auto& player : players)
            if (player.noteCount != 0 && ! updatePlayerTiming(player))
                return false;
        return true;
    }

    bool replace(std::size_t playerIndex, const std::vector<int>& nextNotes, double periodBeats, std::uint8_t nextVelocity,
                 double durationBeats, std::uint64_t renderFrame) noexcept
    {
        if (playerIndex >= players.size() || nextNotes.empty() || nextNotes.size() > players[playerIndex].notes.size()
            || periodBeats <= 0.0 || durationBeats <= 0.0)
            return false;

        const auto frames = framesForPeriod(periodBeats);
        const auto duration = framesForPeriod(durationBeats);
        if (frames == 0 || duration == 0)
            return false;

        for (std::size_t index = 0; index < nextNotes.size(); ++index) {
            if (nextNotes[index] < 0 || nextNotes[index] > 127)
                return false;
            players[playerIndex].notes[index] = static_cast<std::uint8_t>(nextNotes[index]);
        }
        auto& player = players[playerIndex];
        player.noteCount = nextNotes.size();
        player.nextNote = 0;
        player.periodFrames = frames;
        player.durationFrames = duration;
        player.velocity = nextVelocity;
        player.periodBeats = periodBeats;
        player.durationBeats = durationBeats;
        player.nextFrame = renderFrame + timing.blockSize;
        return true;
    }

    void clear() noexcept
    {
        players = {};
    }

    void pump(std::uint64_t renderFrame) noexcept
    {
        const auto horizon = renderFrame + static_cast<std::uint64_t>(timing.blockSize) * 4;
        for (;;) {
            auto* next = nextPlayerBefore(horizon);
            if (next == nullptr)
                return;
            if (producer.schedule({ static_cast<std::uint64_t>(next - players.data() + 1), next->nextFrame,
                                    next->durationFrames, next->notes[next->nextNote], next->velocity, 1 })
                != NoteSubmitResult::queued)
                return;
            next->nextNote = (next->nextNote + 1) % next->noteCount;
            next->nextFrame += next->periodFrames;
        }
    }

private:
    std::uint32_t framesForPeriod(double beats) const noexcept
    {
        const auto frames = static_cast<std::uint64_t>(std::llround(
            beats * timing.sampleRate * 60.0 / timing.bpm));
        return frames == 0 || frames > UINT32_MAX ? 0U : static_cast<std::uint32_t>(frames);
    }

    struct Player {
        std::array<std::uint8_t, 128> notes {};
        std::size_t noteCount {};
        std::size_t nextNote {};
        std::uint32_t periodFrames {};
        std::uint32_t durationFrames {};
        std::uint8_t velocity { 100 };
        double periodBeats {};
        double durationBeats {};
        std::uint64_t nextFrame {};
    };

    bool updatePlayerTiming(Player& player) noexcept
    {
        const auto frames = framesForPeriod(player.periodBeats);
        const auto duration = framesForPeriod(player.durationBeats);
        if (frames == 0 || duration == 0)
            return false;
        player.periodFrames = frames;
        player.durationFrames = duration;
        return true;
    }

    Player* nextPlayerBefore(std::uint64_t horizon) noexcept
    {
        Player* result = nullptr;
        for (auto& player : players)
            if (player.noteCount != 0 && player.periodFrames != 0 && player.nextFrame < horizon
                && (result == nullptr || player.nextFrame < result->nextFrame))
                result = &player;
        return result;
    }

    NoteEventProducer<Capacity> producer;
    SchedulerTiming timing;
    std::array<Player, maxPlayers> players {};
};
}
