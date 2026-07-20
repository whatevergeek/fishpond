#include "host/SingleChannelHost.h"

namespace fishpond {

bool SingleChannelHost::prepareInstrument(std::unique_ptr<juce::AudioProcessor> processor, std::string& diagnostic)
{
    if (processor == nullptr || ! processor->acceptsMidi()) {
        diagnostic = "FP_INSTRUMENT_INVALID: expected a MIDI-capable instrument";
        return false;
    }

    auto layout = processor->getBusesLayout();
    if (layout.outputBuses.isEmpty()) {
        diagnostic = "FP_INSTRUMENT_LAYOUT: instrument has no main output bus";
        return false;
    }
    const auto setMainOutput = [&] (juce::AudioChannelSet channelSet) {
        auto candidate = layout;
        candidate.getChannelSet(false, 0) = channelSet;
        return processor->checkBusesLayoutSupported(candidate) && processor->setBusesLayout(candidate);
    };
    if (! setMainOutput(juce::AudioChannelSet::stereo())
        && ! setMainOutput(juce::AudioChannelSet::mono())) {
        diagnostic = "FP_INSTRUMENT_LAYOUT: mono or stereo output is required";
        return false;
    }

    const auto outputChannels = processor->getMainBusNumOutputChannels();
    processor->setPlayConfigDetails(0, outputChannels, audioConfiguration.sampleRate, audioConfiguration.blockSize);
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
    auto* retired = static_cast<juce::AudioProcessor*>(nullptr);
    if (! applyRawPreparedAtBlockBoundary(prepared.get(), preparedConfigurationVersion, retired, diagnostic))
        return false;

    prepared.release();
    std::unique_ptr<juce::AudioProcessor> retiredOwner(retired);
    return true;
}

bool SingleChannelHost::applyRawPreparedAtBlockBoundary(juce::AudioProcessor* processor,
                                                         std::uint64_t preparedConfigurationVersion,
                                                         juce::AudioProcessor*& retired,
                                                         std::string& diagnostic) noexcept
{
    retired = nullptr;
    if (processor == nullptr) {
        diagnostic = "FP_GRAPH_MISSING: no prepared instrument graph";
        return false;
    }
    if (preparedConfigurationVersion != audioConfiguration.version) {
        diagnostic = "FP_GRAPH_STALE: audio configuration changed before graph commit";
        return false;
    }

    const auto committed = applyRawPreparedAtBlockBoundaryNoDiagnostic(processor, preparedConfigurationVersion, retired);
    if (committed)
        diagnostic.clear();
    return committed;
}

bool SingleChannelHost::applyRawPreparedAtBlockBoundaryNoDiagnostic(juce::AudioProcessor* processor,
                                                                     std::uint64_t preparedConfigurationVersion,
                                                                     juce::AudioProcessor*& retired) noexcept
{
    retired = nullptr;
    if (processor == nullptr || preparedConfigurationVersion != audioConfiguration.version)
        return false;

    retired = active.release();
    active.reset(processor);
    activeOutputChannels = active->getMainBusNumOutputChannels();
    channelState = SingleChannelState::ready;
    return true;
}

void SingleChannelHost::reconfigureForDevice(AudioConfiguration configuration)
{
    audioConfiguration = configuration;
    if (active == nullptr)
        return;

    active->releaseResources();
    activeOutputChannels = active->getMainBusNumOutputChannels();
    active->setPlayConfigDetails(0, activeOutputChannels, audioConfiguration.sampleRate, audioConfiguration.blockSize);
    active->prepareToPlay(audioConfiguration.sampleRate, audioConfiguration.blockSize);
}

void SingleChannelHost::process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    if (active == nullptr) {
        audio.clear();
        return;
    }

    if (activeOutputChannels == 1) {
        auto* monoChannel = audio.getWritePointer(0);
        juce::AudioBuffer<float> monoAudio(&monoChannel, 1, audio.getNumSamples());
        monoAudio.clear();
        active->processBlock(monoAudio, midi);
        for (int channel = 1; channel < audio.getNumChannels(); ++channel)
            audio.copyFrom(channel, 0, monoAudio, 0, 0, audio.getNumSamples());
        return;
    }

    active->processBlock(audio, midi);
}

} // namespace fishpond
