#pragma once

#include "host/SingleChannelHost.h"

namespace fishpond {
class ControlledVST3Bass {
public:
    explicit ControlledVST3Bass(AudioConfiguration initialConfiguration)
        : configuration(initialConfiguration), host(configuration) {}

    bool prepareBundle(const juce::File& bundle, std::string& diagnostic);
    bool commitAtBlockBoundary(std::string& diagnostic);
    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) { host.process(audio, midi); }
    SingleChannelState state() const noexcept { return host.state(); }

private:
    AudioConfiguration configuration;
    SingleChannelHost host;
};
}
