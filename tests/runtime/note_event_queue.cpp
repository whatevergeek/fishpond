#include "runtime/AudioEventDispatcher.h"
#include "runtime/BassPlayerScheduler.h"
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
        fishpond::AudioEventDispatcher<2> dispatcher;
        fishpond::NoteEvent panicEvent;
        bool receivedPanicEvent = false;
        const auto result = dispatcher.drain(queue, 80, 64, observedGeneration,
            [&] (const fishpond::NoteEvent& ready, std::uint32_t offset) {
                panicEvent = ready;
                receivedPanicEvent = offset == 0;
            });
        if (! (result.panic && result.dispatched == 1 && receivedPanicEvent
               && panicEvent.type == fishpond::NoteEventType::allNotesOff
               && panicEvent.midiChannel == 1 && observedGeneration == 1))
            return 1;
        return ! queue.tryPop(received) && ! queue.consumePanic(observedGeneration) ? 0 : 1;
    }

    if (test == "dispatch") {
        fishpond::NoteEventQueue<4> dispatchQueue;
        fishpond::AudioEventDispatcher<4> dispatcher;
        if (! dispatchQueue.tryPush(event(115, 2)) || ! dispatchQueue.tryPush(event(110, 5))
            || ! dispatchQueue.tryPush(event(110, 3)) || ! dispatchQueue.tryPush(event(140, 4)))
            return 1;
        std::uint64_t panicGeneration = 0;
        std::uint64_t emittedSequences[4] {};
        std::uint32_t offsets[4] {};
        std::size_t emitted = 0;
        const auto result = dispatcher.drain(dispatchQueue, 100, 20, panicGeneration,
            [&] (const fishpond::NoteEvent& ready, std::uint32_t offset) {
                emittedSequences[emitted] = ready.sequence;
                offsets[emitted++] = offset;
            });
        if (! (result.dispatched == 3 && result.late == 0 && ! result.panic
               && emitted == 3 && emittedSequences[0] == 3 && emittedSequences[1] == 5
               && emittedSequences[2] == 2 && offsets[0] == 10 && offsets[1] == 10 && offsets[2] == 15))
            return 1;
        const auto later = dispatcher.drain(dispatchQueue, 120, 21, panicGeneration,
            [&] (const fishpond::NoteEvent& ready, std::uint32_t offset) {
                emittedSequences[emitted] = ready.sequence;
                offsets[emitted++] = offset;
            });
        return later.dispatched == 1 && emitted == 4 && emittedSequences[3] == 4 && offsets[3] == 20 ? 0 : 1;
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

    if (test == "scheduler-timing") {
        fishpond::NoteEventQueue<4> scheduledQueue;
        fishpond::BassPlayerScheduler<4> scheduler(scheduledQueue);
        if (! scheduler.setTiming({ 44'100.0, 128, 60.0 })
            || scheduler.setTiming({ 0.0, 128, 60.0 })
            || ! scheduler.replace(0, { 36, 48 }, 0.5, 100, 0.5, 1'000))
            return 1;
        scheduler.pump(1'000);
        if (! scheduledQueue.tryPop(received) || received.type != fishpond::NoteEventType::noteOn
            || received.targetSampleFrame != 1'128 || received.midiNote != 36)
            return 1;
        if (! scheduledQueue.tryPop(received) || received.type != fishpond::NoteEventType::noteOff
            || received.targetSampleFrame != 23'178 || received.midiNote != 36)
            return 1;
        fishpond::NoteEventQueue<2> tempoQueue;
        fishpond::BassPlayerScheduler<2> tempoScheduler(tempoQueue);
        if (! tempoScheduler.setTiming({ 48'000.0, 256, 120.0 })
            || ! tempoScheduler.replace(0, { 60 }, 1.0, 100, 1.0, 0))
            return 1;
        tempoScheduler.pump(0);
        if (! tempoQueue.tryPop(received) || received.targetSampleFrame != 256)
            return 1;
        if (! tempoQueue.tryPop(received) || received.targetSampleFrame != 24'256)
            return 1;
        if (! tempoScheduler.setTiming({ 48'000.0, 256, 60.0 }))
            return 1;
        tempoScheduler.pump(24'000);
        if (! tempoQueue.tryPop(received) || received.targetSampleFrame != 24'256)
            return 1;
        return tempoQueue.tryPop(received) && received.targetSampleFrame == 72'256 ? 0 : 1;
    }

    if (test == "scheduler-multiple") {
        fishpond::NoteEventQueue<8> scheduledQueue;
        fishpond::BassPlayerScheduler<8> scheduler(scheduledQueue);
        if (! scheduler.setTiming({ 48'000.0, 256, 120.0 })
            || ! scheduler.replace(0, { 36 }, 1.0, 100, 0.5, 0)
            || ! scheduler.replace(1, { 48 }, 0.5, 80, 0.25, 0))
            return 1;
        scheduler.pump(0);
        fishpond::AudioEventDispatcher<8> dispatcher;
        std::uint64_t panicGeneration = 0;
        fishpond::NoteEvent dispatched[4] {};
        std::size_t count = 0;
        const auto result = dispatcher.drain(scheduledQueue, 0, 13'000, panicGeneration,
            [&] (const fishpond::NoteEvent& event, std::uint32_t) { dispatched[count++] = event; });
        return result.dispatched == 4 && count == 4
            && dispatched[0].channelId == 1 && dispatched[0].type == fishpond::NoteEventType::noteOn
            && dispatched[1].channelId == 2 && dispatched[1].type == fishpond::NoteEventType::noteOn
            && dispatched[2].channelId == 2 && dispatched[2].type == fishpond::NoteEventType::noteOff
            && dispatched[3].channelId == 1 && dispatched[3].type == fishpond::NoteEventType::noteOff ? 0 : 1;
    }

    if (test == "scheduler-remove") {
        fishpond::NoteEventQueue<8> scheduledQueue;
        fishpond::BassPlayerScheduler<8> scheduler(scheduledQueue);
        if (! scheduler.setTiming({ 48'000.0, 256, 120.0 })
            || ! scheduler.replace(0, { 36 }, 1.0, 100, 0.5, 0)
            || ! scheduler.replace(1, { 48 }, 1.0, 100, 0.5, 0)
            || ! scheduler.remove(0))
            return 1;
        scheduler.pump(0);
        fishpond::NoteEvent first, second;
        return scheduledQueue.tryPop(first) && scheduledQueue.tryPop(second)
            && first.channelId == 2 && second.channelId == 2
            && first.type == fishpond::NoteEventType::noteOn
            && second.type == fishpond::NoteEventType::noteOff
            && ! scheduledQueue.tryPop(received) ? 0 : 1;
    }

    if (test == "scheduler-rests") {
        fishpond::NoteEventQueue<4> scheduledQueue;
        fishpond::BassPlayerScheduler<4> scheduler(scheduledQueue);
        if (! scheduler.setTiming({ 48'000.0, 256, 120.0 })
            || ! scheduler.replace(0, { -1, 36 }, 1.0, 100, 0.5, 0))
            return 1;
        scheduler.pump(0);
        if (scheduledQueue.tryPop(received))
            return 1;
        scheduler.pump(24'000);
        return scheduledQueue.tryPop(received) && received.type == fishpond::NoteEventType::noteOn
            && received.targetSampleFrame == 24'256 && received.midiNote == 36
            && scheduledQueue.tryPop(received) && received.type == fishpond::NoteEventType::noteOff
            && received.targetSampleFrame == 36'256 ? 0 : 1;
    }

    if (test == "scheduler-bar-quantization") {
        fishpond::NoteEventQueue<8> scheduledQueue;
        fishpond::BassPlayerScheduler<8> scheduler(scheduledQueue);
        if (! scheduler.setTiming({ 48'000.0, 256, 120.0 })
            || scheduler.nextBarFrame(1) != 96'000
            || scheduler.nextBarFrame(95'999) != 96'000
            || scheduler.nextBarFrame(96'000) != 192'000
            || ! scheduler.replaceAtFrame(0, { 36 }, 1.0, 100, 0.5, scheduler.nextBarFrame(30'000))
            || ! scheduler.replaceAtFrame(1, { 48 }, 1.0, 100, 0.5, scheduler.nextBarFrame(70'000)))
            return 1;
        scheduler.pump(95'000);
        bool playerOneStartsOnBar {};
        bool playerTwoStartsOnBar {};
        for (int index = 0; index < 4; ++index) {
            if (! scheduledQueue.tryPop(received))
                return 1;
            if (received.type != fishpond::NoteEventType::noteOn)
                continue;
            playerOneStartsOnBar = playerOneStartsOnBar
                || (received.channelId == 1 && received.targetSampleFrame == 96'000);
            playerTwoStartsOnBar = playerTwoStartsOnBar
                || (received.channelId == 2 && received.targetSampleFrame == 96'000);
        }
        return playerOneStartsOnBar && playerTwoStartsOnBar ? 0 : 1;
    }

    if (test == "scheduler-validation-profile") {
        fishpond::NoteEventQueue<8192> scheduledQueue;
        fishpond::BassPlayerScheduler<8192> scheduler(scheduledQueue);
        fishpond::AudioEventDispatcher<8192> dispatcher;
        if (! scheduler.setTiming({ 48'000.0, 256, 120.0 })
            || ! scheduler.replace(0, { 36, 38 }, 0.5, 100, 0.25, 0)
            || ! scheduler.replace(1, { 48 }, 1.0, 80, 0.5, 0))
            return 1;
        std::uint64_t panicGeneration = 0;
        std::uint64_t frame = 0;
        std::uint32_t on[2] {}, off[2] {};
        std::uint32_t late {};
        bool panic {};
        const auto drain = [&] {
            const auto result = dispatcher.drain(scheduledQueue, frame, 256, panicGeneration,
                [&] (const fishpond::NoteEvent& event, std::uint32_t) {
                    const auto index = static_cast<std::size_t>(event.channelId - 1);
                    if (index < 2)
                        (event.type == fishpond::NoteEventType::noteOn ? on[index] : off[index])++;
                });
            late += result.late;
            panic = panic || result.panic;
        };
        for (; frame < 5'760'000; frame += 256) {
            scheduler.pump(frame);
            drain();
            if (scheduledQueue.sizeApproximate() >= scheduledQueue.capacity())
                return 1;
        }
        scheduler.clear();
        for (int block = 0; block < 64; ++block, frame += 256)
            drain();
        return ! panic && late == 0 && on[0] > 0 && on[1] > 0 && on[0] == off[0] && on[1] == off[1]
            && scheduledQueue.sizeApproximate() == 0 ? 0 : 1;
    }

    return 2;
}
