#include "host/ControlledVST3Bass.h"

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
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    fishpond::ControlledVST3Bass bass({ 48'000.0, 64, 1 });
    std::string diagnostic;
    const auto prepared = bass.prepareBundle(juce::File(FISHPOND_FIXTURE_VST3_PATH), diagnostic);
    const auto accepted = bass.commitAtBlockBoundary(diagnostic);

    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    bass.process(audio, midi);

    return require(prepared && accepted, "loaded VST3 was not prepared and committed")
        && require(std::abs(audio.getSample(0, 1)) > 0.001f,
                   "MIDI did not reach the loaded VST3") ? 0 : 1;
}
