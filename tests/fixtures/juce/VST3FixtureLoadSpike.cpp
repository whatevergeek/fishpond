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
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3.findAllTypesForFile(descriptions, FISHPOND_FIXTURE_VST3_PATH);

    juce::String loadError;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    if (descriptions.size() == 1)
        instance = vst3.createInstanceFromDescription(*descriptions[0], 48'000.0, 64, loadError);

    fishpond::SingleChannelHost host({ 48'000.0, 64, 1 });
    std::string diagnostic;
    const auto prepared = host.prepareInstrument(std::move(instance), diagnostic);
    const auto accepted = host.applyPreparedAtBlockBoundary(1, diagnostic);

    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    host.process(audio, midi);

    return require(descriptions.size() == 1, "controlled VST3 bundle was not discovered")
        && require(loadError.isEmpty(), "controlled VST3 bundle failed to instantiate")
        && require(prepared && accepted, "loaded VST3 was not prepared and committed")
        && require(std::abs(audio.getSample(0, 0) - 0.125f) < 0.0001f,
                   "MIDI did not reach the loaded VST3") ? 0 : 1;
}
