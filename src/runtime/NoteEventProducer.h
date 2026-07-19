#pragma once

#include "runtime/NoteEventQueue.h"

#include <cstdint>

namespace fishpond {
enum class NoteSubmitResult {
    queued,
    invalid,
    queueFull,
};

struct ScheduledNote {
    std::uint64_t channelId {};
    std::uint64_t targetSampleFrame {};
    std::uint32_t durationFrames {};
    std::uint8_t midiNote {};
    std::uint8_t velocity { 100 };
    std::uint8_t midiChannel { 1 };
};

// Runtime-thread producer. A note-on is admitted only when its matching
// note-off can also be reserved, so an overflow cannot leave a note hanging.
template <std::size_t Capacity>
class NoteEventProducer {
public:
    explicit NoteEventProducer(NoteEventQueue<Capacity>& queueToFill) : queue(queueToFill) {}

    NoteSubmitResult schedule(const ScheduledNote& note) noexcept
    {
        if (note.channelId == 0 || note.durationFrames == 0 || note.midiChannel == 0 || note.midiChannel > 16)
            return NoteSubmitResult::invalid;
        if (queue.availableCapacityApproximate() < 2)
            return NoteSubmitResult::queueFull;

        const auto on = NoteEvent { note.channelId, note.targetSampleFrame, nextSequence++, note.durationFrames,
                                    note.midiNote, note.velocity, note.midiChannel, NoteEventType::noteOn };
        const auto off = NoteEvent { note.channelId, note.targetSampleFrame + note.durationFrames, nextSequence++, 0,
                                     note.midiNote, 0, note.midiChannel, NoteEventType::noteOff };
        if (! queue.tryPush(on) || ! queue.tryPush(off))
            return NoteSubmitResult::queueFull;
        return NoteSubmitResult::queued;
    }

private:
    NoteEventQueue<Capacity>& queue;
    std::uint64_t nextSequence {};
};
}
