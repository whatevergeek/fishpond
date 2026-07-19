#include "runtime/AudioEventDispatcher.h"
#include "runtime/NoteEventProducer.h"

#include <iostream>
#include <string>

namespace {
fishpond::NoteEvent event(std::uint64_t frame, std::uint64_t sequence)
{
    return { 42, frame, sequence, 128, 60, 100, 1, fishpond::NoteEventType::noteOn };
}
}

int main(int argc, char** argv)
{
    const std::string test = argc == 2 ? argv[1] : "";
    fishpond::NoteEventQueue<2> queue;
    fishpond::NoteEvent received;

    if (test == "capacity")
        return queue.tryPush(event(10, 1)) && queue.tryPush(event(20, 2))
            && ! queue.tryPush(event(30, 3)) && queue.sizeApproximate() == 2 ? 0 : 1;

    if (test == "ordering") {
        if (! queue.tryPush(event(100, 5)) || ! queue.tryPush(event(120, 6)))
            return 1;
        if (! queue.tryPop(received) || received.targetSampleFrame != 100 || received.sequence != 5)
            return 1;
        return queue.tryPop(received) && received.targetSampleFrame == 120 && received.sequence == 6
            && ! queue.tryPop(received) ? 0 : 1;
    }

    if (test == "panic") {
        if (! queue.tryPush(event(100, 1)) || ! queue.tryPush(event(120, 2)))
            return 1;
        std::uint64_t observedGeneration = 0;
        queue.requestPanic();
        if (! queue.consumePanic(observedGeneration) || observedGeneration != 1)
            return 1;
        queue.discardPendingFromConsumer();
        return ! queue.tryPop(received) && ! queue.consumePanic(observedGeneration) ? 0 : 1;
    }

    if (test == "dispatch") {
        fishpond::NoteEventQueue<4> dispatchQueue;
        fishpond::AudioEventDispatcher<4> dispatcher;
        if (! dispatchQueue.tryPush(event(115, 2)) || ! dispatchQueue.tryPush(event(110, 5))
            || ! dispatchQueue.tryPush(event(110, 3)) || ! dispatchQueue.tryPush(event(140, 4)))
            return 1;
        std::uint64_t panicGeneration = 0;
        std::uint64_t emittedSequences[3] {};
        std::uint32_t offsets[3] {};
        std::size_t emitted = 0;
        const auto result = dispatcher.drain(dispatchQueue, 100, 20, panicGeneration,
            [&] (const fishpond::NoteEvent& ready, std::uint32_t offset) {
                emittedSequences[emitted] = ready.sequence;
                offsets[emitted++] = offset;
            });
        return result.dispatched == 3 && result.late == 0 && ! result.panic
            && emitted == 3 && emittedSequences[0] == 3 && emittedSequences[1] == 5
            && emittedSequences[2] == 2 && offsets[0] == 10 && offsets[1] == 10 && offsets[2] == 15
            && dispatchQueue.tryPop(received) && received.sequence == 4 ? 0 : 1;
    }

    if (test == "note-reservation") {
        fishpond::NoteEventQueue<2> noteQueue;
        fishpond::NoteEventProducer<2> producer(noteQueue);
        const fishpond::ScheduledNote note { 7, 100, 48, 60, 90, 1 };
        if (producer.schedule(note) != fishpond::NoteSubmitResult::queued
            || producer.schedule(note) != fishpond::NoteSubmitResult::queueFull)
            return 1;
        if (! noteQueue.tryPop(received) || received.type != fishpond::NoteEventType::noteOn
            || received.targetSampleFrame != 100 || received.sequence != 0)
            return 1;
        return noteQueue.tryPop(received) && received.type == fishpond::NoteEventType::noteOff
            && received.targetSampleFrame == 148 && received.sequence == 1 ? 0 : 1;
    }

    if (test == "note-validation") {
        fishpond::NoteEventQueue<2> noteQueue;
        fishpond::NoteEventProducer<2> producer(noteQueue);
        return producer.schedule({ 0, 100, 48, 60, 90, 1 }) == fishpond::NoteSubmitResult::invalid
            && producer.schedule({ 7, 100, 0, 60, 90, 1 }) == fishpond::NoteSubmitResult::invalid
            && producer.schedule({ 7, 100, 48, 60, 90, 17 }) == fishpond::NoteSubmitResult::invalid ? 0 : 1;
    }

    return 2;
}
