#pragma once

#include "host/SingleChannelHost.h"

namespace fishpond {
class HostedInstrument {
public:
    explicit HostedInstrument(AudioConfiguration initialConfiguration)
        : configuration(initialConfiguration), host(configuration) {}

    bool prepareBundle(const juce::File& bundle, std::string& diagnostic);
    bool commitAtBlockBoundary(std::string& diagnostic);
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
