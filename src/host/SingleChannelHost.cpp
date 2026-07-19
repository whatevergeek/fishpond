#include "host/SingleChannelHost.h"

namespace fishpond {

bool SingleChannelHost::prepareInstrument(std::unique_ptr<juce::AudioProcessor> processor, std::string& diagnostic)
{
    if (processor == nullptr || ! processor->acceptsMidi()) {
        diagnostic = "FP_INSTRUMENT_INVALID: expected a MIDI-capable instrument";
        return false;
    }

    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    if (! processor->checkBusesLayoutSupported(layout) || ! processor->setBusesLayout(layout)) {
        diagnostic = "FP_INSTRUMENT_LAYOUT: stereo output is required";
        return false;
    }

    processor->setPlayConfigDetails(0, 2, audioConfiguration.sampleRate, audioConfiguration.blockSize);
    processor->prepareToPlay(audioConfiguration.sampleRate, audioConfiguration.blockSize);
    prepared = std::move(processor);
    diagnostic.clear();
    return true;
}

bool SingleChannelHost::applyPreparedAtBlockBoundary(std::uint64_t preparedConfigurationVersion,
                                                      std::string& diagnostic)
{
    if (prepared == nullptr) {
        diagnostic = "FP_GRAPH_MISSING: no prepared instrument graph";
        return false;
    }
    if (preparedConfigurationVersion != audioConfiguration.version) {
        diagnostic = "FP_GRAPH_STALE: audio configuration changed before graph commit";
        return false;
    }

    active = std::move(prepared);
    channelState = SingleChannelState::ready;
    diagnostic.clear();
    return true;
}

void SingleChannelHost::process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    if (active != nullptr)
        active->processBlock(audio, midi);
    else
        audio.clear();
}

} // namespace fishpond
