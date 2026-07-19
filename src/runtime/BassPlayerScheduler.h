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
    explicit BassPlayerScheduler(NoteEventQueue<Capacity>& queueToFill) : producer(queueToFill) {}

    bool setTiming(SchedulerTiming next) noexcept
    {
        if (next.sampleRate <= 0.0 || next.blockSize == 0 || next.bpm <= 0.0)
            return false;
        timing = next;
        if (noteCount != 0 && periodBeats > 0.0) {
            const auto frames = framesForPeriod(periodBeats);
            const auto duration = framesForPeriod(durationBeats);
            if (frames == 0 || duration == 0)
                return false;
            periodFrames = frames;
            durationFrames = duration;
        }
        return true;
    }

    bool replace(const std::vector<int>& nextNotes, double periodBeats, std::uint8_t nextVelocity,
                 double durationBeats, std::uint64_t renderFrame) noexcept
    {
        if (nextNotes.empty() || nextNotes.size() > notes.size() || periodBeats <= 0.0 || durationBeats <= 0.0)
            return false;

        const auto frames = framesForPeriod(periodBeats);
        const auto duration = framesForPeriod(durationBeats);
        if (frames == 0 || duration == 0)
            return false;

        for (std::size_t index = 0; index < nextNotes.size(); ++index) {
            if (nextNotes[index] < 0 || nextNotes[index] > 127)
                return false;
            notes[index] = static_cast<std::uint8_t>(nextNotes[index]);
        }
        noteCount = nextNotes.size();
        nextNote = 0;
        periodFrames = frames;
        durationFrames = duration;
        velocity = nextVelocity;
        this->periodBeats = periodBeats;
        this->durationBeats = durationBeats;
        nextFrame = renderFrame + timing.blockSize;
        return true;
    }

    void clear() noexcept
    {
        noteCount = 0;
        nextNote = 0;
        periodFrames = 0;
        periodBeats = 0.0;
        durationBeats = 0.0;
    }

    void pump(std::uint64_t renderFrame) noexcept
    {
        if (noteCount == 0 || periodFrames == 0)
            return;

        const auto horizon = renderFrame + static_cast<std::uint64_t>(timing.blockSize) * 4;
        while (nextFrame < horizon) {
            if (producer.schedule({ 1, nextFrame, durationFrames, notes[nextNote], velocity, 1 })
                != NoteSubmitResult::queued)
                return;
            nextNote = (nextNote + 1) % noteCount;
            nextFrame += periodFrames;
        }
    }

private:
    std::uint32_t framesForPeriod(double beats) const noexcept
    {
        const auto frames = static_cast<std::uint64_t>(std::llround(
            beats * timing.sampleRate * 60.0 / timing.bpm));
        return frames == 0 || frames > UINT32_MAX ? 0U : static_cast<std::uint32_t>(frames);
    }

    NoteEventProducer<Capacity> producer;
    SchedulerTiming timing;
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
}
