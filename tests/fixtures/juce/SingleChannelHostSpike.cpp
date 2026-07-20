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

class MonoFixtureInstrument final : public juce::AudioProcessor {
public:
    MonoFixtureInstrument()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)) {}

    const juce::String getName() const override { return "Mono Fixture"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
        buffer.setSample(0, 1, 0.25f);
    }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
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

    fishpond::SingleChannelHost monoHost({ 48'000.0, 64, 1 });
    const auto monoPrepared = monoHost.prepareInstrument(std::make_unique<MonoFixtureInstrument>(), diagnostic);
    const auto monoAccepted = monoHost.applyPreparedAtBlockBoundary(1, diagnostic);
    juce::AudioBuffer<float> monoAudio(2, 64);
    juce::MidiBuffer monoMidi;
    monoHost.process(monoAudio, monoMidi);
    const auto duplicatedMono = std::abs(monoAudio.getSample(0, 1) - 0.25f) < 0.001f
                            && std::abs(monoAudio.getSample(1, 1) - 0.25f) < 0.001f;

    return require(prepared, "fixture preparation failed")
        && require(accepted && host.state() == fishpond::SingleChannelState::ready,
                   "prepared fixture was not accepted")
        && require(producedAudio, "MIDI did not reach the active fixture graph")
        && require(replacementPrepared && staleRejected, "stale graph was accepted")
        && require(retainedGraph, "stale rejection did not retain the active graph")
        && require(monoPrepared && monoAccepted && duplicatedMono,
                   "mono fixture was not duplicated into the stereo host mix") ? 0 : 1;
}
