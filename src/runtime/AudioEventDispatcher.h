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
            return { 0, 0, true };
        }

        const auto blockEnd = blockStart + blockFrames;
        std::size_t count = 0;
        NoteEvent event;
        while (count < Capacity && queue.tryPeek(event) && event.targetSampleFrame < blockEnd) {
            queue.tryPop(events[count++]);
        }

        for (std::size_t index = 1; index < count; ++index) {
            auto candidate = events[index];
            auto position = index;
            while (position > 0 && precedes(candidate, events[position - 1])) {
                events[position] = events[position - 1];
                --position;
            }
            events[position] = candidate;
        }

        EventDrainResult result;
        for (std::size_t index = 0; index < count; ++index) {
            const auto& ready = events[index];
            const auto late = ready.targetSampleFrame < blockStart;
            const auto offset = late ? 0U : static_cast<std::uint32_t>(ready.targetSampleFrame - blockStart);
            handler(ready, offset);
            ++result.dispatched;
            result.late += late ? 1U : 0U;
        }
        return result;
    }

private:
    static bool precedes(const NoteEvent& left, const NoteEvent& right) noexcept
    {
        return left.targetSampleFrame < right.targetSampleFrame
            || (left.targetSampleFrame == right.targetSampleFrame && left.sequence < right.sequence);
    }

    std::array<NoteEvent, Capacity> events {};
};
}
