#include "host/ControlledVST3Bass.h"

namespace fishpond {
namespace {
juce::PluginDescription* findInstrumentDescription(juce::OwnedArray<juce::PluginDescription>& descriptions)
{
    for (auto* description : descriptions)
        if (description != nullptr && description->isInstrument)
            return description;
    return nullptr;
}
}

bool HostedInstrument::prepareBundle(const juce::File& bundle, std::string& diagnostic)
{
    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3.findAllTypesForFile(descriptions, bundle.getFullPathName());
    auto* description = findInstrumentDescription(descriptions);
    if (description == nullptr) {
        diagnostic = "FP_INSTRUMENT_DISCOVERY: selected bundle has no VST3 instrument class";
        return false;
    }

    juce::String loadError;
    auto instance = vst3.createInstanceFromDescription(*description, configuration.sampleRate,
                                                        configuration.blockSize, loadError);
    if (instance == nullptr) {
        diagnostic = "FP_INSTRUMENT_LOAD: " + loadError.toStdString();
        return false;
    }
    return prepareProcessor(std::move(instance), diagnostic);
}

bool HostedInstrument::commitAtBlockBoundary(std::string& diagnostic)
{
    return host.applyPreparedAtBlockBoundary(host.configurationVersion(), diagnostic);
}
}
