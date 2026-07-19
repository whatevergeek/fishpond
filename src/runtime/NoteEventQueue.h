#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace fishpond {
enum class NoteEventType : std::uint8_t {
    noteOn,
    noteOff,
    allNotesOff,
};

// Fixed-size data only: it crosses from the Python runtime producer to the
// audio consumer without retaining Python objects or allocating memory.
struct NoteEvent {
    std::uint64_t channelId {};
    std::uint64_t targetSampleFrame {};
    std::uint64_t sequence {};
    std::uint32_t durationFrames {};
    std::uint8_t midiNote {};
    std::uint8_t velocity {};
    std::uint8_t midiChannel {};
    NoteEventType type { NoteEventType::noteOn };
};

// Single producer (Python runtime) / single consumer (audio callback) ring.
// The extra slot makes full and empty states unambiguous.
template <std::size_t Capacity>
class NoteEventQueue {
public:
    static_assert(Capacity > 0, "event queue requires capacity");

    bool tryPush(const NoteEvent& event) noexcept
    {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == readIndex.load(std::memory_order_acquire))
            return false;
        slots[write] = event;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    bool tryPop(NoteEvent& event) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire))
            return false;
        event = slots[read];
        readIndex.store(increment(read), std::memory_order_release);
        return true;
    }

    bool tryPeek(NoteEvent& event) const noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire))
            return false;
        event = slots[read];
        return true;
    }

    void requestPanic() noexcept
    {
        panicGeneration.fetch_add(1, std::memory_order_release);
    }

    bool consumePanic(std::uint64_t& observedGeneration) noexcept
    {
        const auto requested = panicGeneration.load(std::memory_order_acquire);
        if (requested == observedGeneration)
            return false;
        observedGeneration = requested;
        return true;
    }

    // Consumer-only: an audio callback can discard pending work after panic.
    void discardPendingFromConsumer() noexcept
    {
        readIndex.store(writeIndex.load(std::memory_order_acquire), std::memory_order_release);
    }

    std::size_t sizeApproximate() const noexcept
    {
        const auto read = readIndex.load(std::memory_order_acquire);
        const auto write = writeIndex.load(std::memory_order_acquire);
        return write >= read ? write - read : slotCount - read + write;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t slotCount = Capacity + 1;

    static constexpr std::size_t increment(std::size_t index) noexcept
    {
        return index + 1 == slotCount ? 0 : index + 1;
    }

    std::array<NoteEvent, slotCount> slots {};
    alignas(64) std::atomic<std::size_t> writeIndex {};
    alignas(64) std::atomic<std::size_t> readIndex {};
    std::atomic<std::uint64_t> panicGeneration {};
};

using RuntimeNoteEventQueue = NoteEventQueue<8192>;
}
