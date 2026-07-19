#include "FixtureInstrument.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

float peakLevel(const juce::AudioBuffer<float>& audio)
{
    auto peak = 0.0f;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        peak = std::max(peak, std::abs(audio.getSample(0, sample)));
    return peak;
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
    const auto fullVelocityLevel = peakLevel(audio);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.2f), 0);
    fixture.processBlock(audio, midi);
    const auto lowVelocityLevel = peakLevel(audio);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    fixture.processBlock(audio, midi);
    const bool releasedByNoteOff = std::abs(audio.getSample(0, 1)) < 0.000001f;
    fixture.releaseResources();

    return require(fixture.wasPrepared(), "fixture did not prepare")
        && require(processed, "fixture did not produce deterministic MIDI-triggered audio")
        && require(fullVelocityLevel <= 0.15f, "fixture velocity exceeded full-scale amplitude")
        && require(lowVelocityLevel < fullVelocityLevel * 0.25f, "fixture did not honor MIDI velocity")
        && require(releasedByNoteOff, "fixture did not honor MIDI note-off")
        && require(fixture.wasReleased(), "fixture did not release resources")
        ? 0 : 1;
}
