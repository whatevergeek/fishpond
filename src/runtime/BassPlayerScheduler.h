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

    std::uint64_t nextBarFrame(std::uint64_t renderFrame) const noexcept
    {
        const auto barFrames = framesForPeriod(4.0);
        if (barFrames == 0)
            return 0;
        return (renderFrame / barFrames + 1) * static_cast<std::uint64_t>(barFrames);
    }

    bool setTiming(SchedulerTiming next) noexcept
    {
        if (next.sampleRate <= 0.0 || next.blockSize == 0 || next.bpm <= 0.0)
            return false;
        timing = next;
        for (auto& player : players)
            if (player.stepCount != 0 && ! updatePlayerTiming(player))
                return false;
        return true;
    }

    bool replace(std::size_t playerIndex, const std::vector<int>& nextNotes, double periodBeats, std::uint8_t nextVelocity,
                 double durationBeats, std::uint64_t renderFrame) noexcept
    {
        return replaceAtFrame(playerIndex, nextNotes, periodBeats, nextVelocity, durationBeats,
                              renderFrame + timing.blockSize, static_cast<std::uint64_t>(playerIndex + 1));
    }

    bool replaceAtFrame(std::size_t playerIndex, const std::vector<int>& nextNotes, double periodBeats,
                        std::uint8_t nextVelocity, double durationBeats, std::uint64_t startFrame,
                        std::uint64_t channelId = 0) noexcept
    {
        std::vector<std::vector<int>> steps;
        steps.reserve(nextNotes.size());
        for (const auto note : nextNotes)
            steps.push_back({ note });
        return replaceStepsAtFrame(playerIndex, steps, periodBeats, nextVelocity, durationBeats, startFrame, channelId);
    }

    bool replaceStepsAtFrame(std::size_t playerIndex, const std::vector<std::vector<int>>& nextSteps, double periodBeats,
                             std::uint8_t nextVelocity, double durationBeats, std::uint64_t startFrame,
                             std::uint64_t channelId = 0) noexcept
    {
        if (playerIndex >= players.size() || nextSteps.empty() || nextSteps.size() > players[playerIndex].steps.size()
            || periodBeats <= 0.0 || durationBeats <= 0.0)
            return false;

        const auto frames = framesForPeriod(periodBeats);
        const auto duration = framesForPeriod(durationBeats);
        if (frames == 0 || duration == 0)
            return false;

        for (std::size_t stepIndex = 0; stepIndex < nextSteps.size(); ++stepIndex) {
            if (nextSteps[stepIndex].empty() || nextSteps[stepIndex].size() > players[playerIndex].steps[stepIndex].size())
                return false;
            for (std::size_t noteIndex = 0; noteIndex < nextSteps[stepIndex].size(); ++noteIndex) {
                if (nextSteps[stepIndex][noteIndex] < -1 || nextSteps[stepIndex][noteIndex] > 127)
                    return false;
                players[playerIndex].steps[stepIndex][noteIndex] = static_cast<std::int16_t>(nextSteps[stepIndex][noteIndex]);
            }
            players[playerIndex].stepNoteCounts[stepIndex] = nextSteps[stepIndex].size();
        }
        auto& player = players[playerIndex];
        player.stepCount = nextSteps.size();
        player.nextStep = 0;
        player.periodFrames = frames;
        player.durationFrames = duration;
        player.velocity = nextVelocity;
        player.periodBeats = periodBeats;
        player.durationBeats = durationBeats;
        player.channelId = channelId == 0 ? static_cast<std::uint64_t>(playerIndex + 1) : channelId;
        player.nextFrame = startFrame;
        return true;
    }

    void clear() noexcept
    {
        players = {};
    }

    // A normal per-player stop leaves any already-reserved note-off in the
    // queue to release naturally, while preventing future pattern steps.
    bool remove(std::size_t playerIndex) noexcept
    {
        if (playerIndex >= players.size())
            return false;
        players[playerIndex] = {};
        return true;
    }

    void pump(std::uint64_t renderFrame) noexcept
    {
        const auto horizon = renderFrame + static_cast<std::uint64_t>(timing.blockSize) * 4;
        for (;;) {
            auto* next = nextPlayerBefore(horizon);
            if (next == nullptr)
                return;
            std::vector<std::uint8_t> notes;
            const auto noteCount = next->stepNoteCounts[next->nextStep];
            notes.reserve(noteCount);
            for (std::size_t index = 0; index < noteCount; ++index)
                if (next->steps[next->nextStep][index] >= 0)
                    notes.push_back(static_cast<std::uint8_t>(next->steps[next->nextStep][index]));
            if (! notes.empty() && producer.scheduleGroup(notes, next->channelId, next->nextFrame,
                                                           next->durationFrames, next->velocity) != NoteSubmitResult::queued)
                return;
            next->nextStep = (next->nextStep + 1) % next->stepCount;
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
        std::array<std::array<std::int16_t, 128>, 128> steps {};
        std::array<std::size_t, 128> stepNoteCounts {};
        std::size_t stepCount {};
        std::size_t nextStep {};
        std::uint32_t periodFrames {};
        std::uint32_t durationFrames {};
        std::uint8_t velocity { 100 };
        double periodBeats {};
        double durationBeats {};
        std::uint64_t channelId { 1 };
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
            if (player.stepCount != 0 && player.periodFrames != 0 && player.nextFrame < horizon
                && (result == nullptr || player.nextFrame < result->nextFrame))
                result = &player;
        return result;
    }

    NoteEventProducer<Capacity> producer;
    SchedulerTiming timing;
    std::array<Player, maxPlayers> players {};
};
}
