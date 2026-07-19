#pragma once

#include "runtime/NoteEventQueue.h"

#include <array>
#include <cstdint>

namespace fishpond {
struct EventDrainResult {
    std::uint32_t dispatched {};
    std::uint32_t late {};
    bool panic {};
};

// Audio-thread helper. It owns fixed storage and uses an insertion sort so no
// queue drain, ordering operation, or dispatch requires allocation.
template <std::size_t Capacity>
class AudioEventDispatcher {
public:
    template <typename Handler>
    EventDrainResult drain(NoteEventQueue<Capacity>& queue, std::uint64_t blockStart,
                           std::uint32_t blockFrames, std::uint64_t& observedPanic,
                           Handler&& handler) noexcept
    {
        if (queue.consumePanic(observedPanic)) {
            queue.discardPendingFromConsumer();
            pendingCount = 0;
            // A panic discards scheduled note-offs as well as note-ons. Emit an
            // explicit safety event so a hosted instrument cannot retain an
            // already-sounding note.
            handler(NoteEvent { 0, blockStart, 0, 0, 0, 0, 1, NoteEventType::allNotesOff }, 0);
            return { 1, 0, true };
        }

        const auto blockEnd = blockStart + blockFrames;
        NoteEvent event;
        while (pendingCount < Capacity && queue.tryPop(event))
            events[pendingCount++] = event;

        for (std::size_t index = 1; index < pendingCount; ++index) {
            auto candidate = events[index];
            auto position = index;
            while (position > 0 && precedes(candidate, events[position - 1])) {
                events[position] = events[position - 1];
                --position;
            }
            events[position] = candidate;
        }

        EventDrainResult result;
        std::size_t dispatched = 0;
        while (dispatched < pendingCount && events[dispatched].targetSampleFrame < blockEnd) {
            const auto& ready = events[dispatched++];
            const auto late = ready.targetSampleFrame < blockStart;
            const auto offset = late ? 0U : static_cast<std::uint32_t>(ready.targetSampleFrame - blockStart);
            handler(ready, offset);
            ++result.dispatched;
            result.late += late ? 1U : 0U;
        }
        for (std::size_t index = dispatched; index < pendingCount; ++index)
            events[index - dispatched] = events[index];
        pendingCount -= dispatched;
        return result;
    }

private:
    static bool precedes(const NoteEvent& left, const NoteEvent& right) noexcept
    {
        return left.targetSampleFrame < right.targetSampleFrame
            || (left.targetSampleFrame == right.targetSampleFrame && left.sequence < right.sequence);
    }

    std::array<NoteEvent, Capacity> events {};
    std::size_t pendingCount {};
};
}
