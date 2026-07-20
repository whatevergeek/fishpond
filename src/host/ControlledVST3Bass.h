#pragma once

#include "host/SingleChannelHost.h"

namespace fishpond {
class HostedInstrument {
public:
    explicit HostedInstrument(AudioConfiguration initialConfiguration)
        : configuration(initialConfiguration), host(configuration) {}

    bool prepareBundle(const juce::File& bundle, std::string& diagnostic);
    bool prepareProcessor(std::unique_ptr<juce::AudioProcessor> processor, std::string& diagnostic)
    {
        return host.prepareInstrument(std::move(processor), diagnostic);
    }
    bool commitAtBlockBoundary(std::string& diagnostic);
    juce::AudioProcessor* releasePrepared() noexcept { return host.releasePrepared(); }
    bool applyRawPreparedAtBlockBoundary(juce::AudioProcessor* processor,
                                         std::uint64_t preparedConfigurationVersion,
                                         juce::AudioProcessor*& retired,
                                         std::string& diagnostic) noexcept
    {
        return host.applyRawPreparedAtBlockBoundary(processor, preparedConfigurationVersion, retired, diagnostic);
    }
    bool applyRawPreparedAtBlockBoundaryNoDiagnostic(juce::AudioProcessor* processor,
                                                     std::uint64_t preparedConfigurationVersion,
                                                     juce::AudioProcessor*& retired) noexcept
    {
        return host.applyRawPreparedAtBlockBoundaryNoDiagnostic(processor, preparedConfigurationVersion, retired);
    }
    void reconfigureForDevice(AudioConfiguration configuration) { host.reconfigureForDevice(configuration); }
    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) { host.process(audio, midi); }
    SingleChannelState state() const noexcept { return host.state(); }
    juce::AudioProcessor* activeProcessorForEditor() const noexcept { return host.activeProcessorForEditor(); }

private:
    AudioConfiguration configuration;
    SingleChannelHost host;
};

// P1 fixture compatibility only. New application code uses HostedInstrument.
using ControlledVST3Bass = HostedInstrument;
}
