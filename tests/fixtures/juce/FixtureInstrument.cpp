#include "FixtureInstrument.h"

#include <cmath>

FixtureInstrument::FixtureInstrument()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

const juce::String FixtureInstrument::getName() const { return "Fishpond Fixture Instrument"; }
void FixtureInstrument::prepareToPlay(double newSampleRate, int) { sampleRate = newSampleRate; prepared = true; released = false; }
void FixtureInstrument::releaseResources() { released = true; }
bool FixtureInstrument::isBusesLayoutSupported(const BusesLayout& layouts) const { return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
void FixtureInstrument::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOn()) {
            phase = 0.0;
            frequency = 440.0 * std::pow(2.0, (metadata.getMessage().getNoteNumber() - 69) / 12.0);
            remainingToneSamples = static_cast<int>(sampleRate * 0.25);
        }
    if (! prepared || released)
        return;
    for (int sample = 0; sample < buffer.getNumSamples() && remainingToneSamples > 0; ++sample) {
        const auto value = static_cast<float>(0.15 * std::sin(phase));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, value);
        phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
        --remainingToneSamples;
    }
}
bool FixtureInstrument::hasEditor() const { return false; }
juce::AudioProcessorEditor* FixtureInstrument::createEditor() { return nullptr; }
double FixtureInstrument::getTailLengthSeconds() const { return 0; }
bool FixtureInstrument::acceptsMidi() const { return true; }
bool FixtureInstrument::producesMidi() const { return false; }
bool FixtureInstrument::isMidiEffect() const { return false; }
int FixtureInstrument::getNumPrograms() { return 1; }
int FixtureInstrument::getCurrentProgram() { return 0; }
void FixtureInstrument::setCurrentProgram(int) {}
const juce::String FixtureInstrument::getProgramName(int) { return {}; }
void FixtureInstrument::changeProgramName(int, const juce::String&) {}
void FixtureInstrument::getStateInformation(juce::MemoryBlock&) {}
void FixtureInstrument::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FixtureInstrument(); }
