#include "FixtureInstrument.h"
#include "host/SingleChannelHost.h"

#include <cmath>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main()
{
    fishpond::SingleChannelHost host({ 48'000.0, 64, 1 });
    std::string diagnostic;

    const auto prepared = host.prepareInstrument(std::make_unique<FixtureInstrument>(), diagnostic);
    const auto accepted = host.applyPreparedAtBlockBoundary(1, diagnostic);

    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    host.process(audio, midi);
    const auto producedAudio = std::abs(audio.getSample(0, 1)) > 0.001f;

    host.reconfigure({ 48'000.0, 64, 2 });
    const auto replacementPrepared = host.prepareInstrument(std::make_unique<FixtureInstrument>(), diagnostic);
    const auto staleRejected = ! host.applyPreparedAtBlockBoundary(1, diagnostic);

    juce::AudioBuffer<float> retainedAudio(2, 64);
    juce::MidiBuffer retainedMidi;
    retainedMidi.addEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), 0);
    host.process(retainedAudio, retainedMidi);
    const auto retainedGraph = std::abs(retainedAudio.getSample(0, 1)) > 0.001f;

    return require(prepared, "fixture preparation failed")
        && require(accepted && host.state() == fishpond::SingleChannelState::ready,
                   "prepared fixture was not accepted")
        && require(producedAudio, "MIDI did not reach the active fixture graph")
        && require(replacementPrepared && staleRejected, "stale graph was accepted")
        && require(retainedGraph, "stale rejection did not retain the active graph") ? 0 : 1;
}
