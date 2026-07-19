#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <cstdint>
#include <memory>
#include <string>

namespace fishpond {

struct AudioConfiguration {
    double sampleRate {};
    int blockSize {};
    std::uint64_t version {};
};

enum class SingleChannelState { empty, ready, failed };

// The management side owns preparation. applyPreparedAtBlockBoundary() is the
// sole audio-side transition and receives a fully prepared processor.
class SingleChannelHost {
public:
    explicit SingleChannelHost(AudioConfiguration configuration) : audioConfiguration(configuration) {}

    bool prepareInstrument(std::unique_ptr<juce::AudioProcessor> processor, std::string& diagnostic);
    bool applyPreparedAtBlockBoundary(std::uint64_t preparedConfigurationVersion, std::string& diagnostic);
    juce::AudioProcessor* releasePrepared() noexcept { return prepared.release(); }
    bool applyRawPreparedAtBlockBoundary(juce::AudioProcessor* processor,
                                         std::uint64_t preparedConfigurationVersion,
                                         juce::AudioProcessor*& retired,
                                         std::string& diagnostic) noexcept;
    juce::AudioProcessor* releaseActiveForWorker() noexcept { return active.release(); }
    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi);

    SingleChannelState state() const noexcept { return channelState; }
    std::uint64_t configurationVersion() const noexcept { return audioConfiguration.version; }
    void reconfigure(AudioConfiguration configuration) noexcept { audioConfiguration = configuration; }

private:
    AudioConfiguration audioConfiguration;
    std::unique_ptr<juce::AudioProcessor> prepared;
    std::unique_ptr<juce::AudioProcessor> active;
    SingleChannelState channelState { SingleChannelState::empty };
};

} // namespace fishpond
