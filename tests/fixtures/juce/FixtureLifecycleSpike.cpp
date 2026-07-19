#include "FixtureInstrument.h"

#include <cmath>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main()
{
    FixtureInstrument fixture;
    fixture.prepareToPlay(48'000.0, 64);

    juce::AudioBuffer<float> audio(2, 64);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    fixture.processBlock(audio, midi);

    const bool processed = std::abs(audio.getSample(0, 1)) > 0.001f
        && std::abs(audio.getSample(1, 1)) > 0.001f;
    fixture.releaseResources();

    return require(fixture.wasPrepared(), "fixture did not prepare")
        && require(processed, "fixture did not produce deterministic MIDI-triggered audio")
        && require(fixture.wasReleased(), "fixture did not release resources")
        ? 0 : 1;
}
