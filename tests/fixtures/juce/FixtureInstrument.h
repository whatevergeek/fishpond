#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class FixtureInstrument final : public juce::AudioProcessor {
public:
    FixtureInstrument();

    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;
    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool wasPrepared() const noexcept { return prepared; }
    bool wasReleased() const noexcept { return released; }

private:
    bool prepared = false;
    bool released = false;
};
