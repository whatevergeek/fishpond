#include "host/ControlledVST3Bass.h"

namespace fishpond {
bool HostedInstrument::prepareBundle(const juce::File& bundle, std::string& diagnostic)
{
    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3.findAllTypesForFile(descriptions, bundle.getFullPathName());
    if (descriptions.size() != 1) {
        diagnostic = "FP_INSTRUMENT_DISCOVERY: expected exactly one VST3 instrument in selected bundle";
        return false;
    }

    juce::String loadError;
    auto instance = vst3.createInstanceFromDescription(*descriptions[0], configuration.sampleRate,
                                                        configuration.blockSize, loadError);
    if (instance == nullptr) {
        diagnostic = "FP_INSTRUMENT_LOAD: " + loadError.toStdString();
        return false;
    }
    return host.prepareInstrument(std::move(instance), diagnostic);
}

bool HostedInstrument::commitAtBlockBoundary(std::string& diagnostic)
{
    return host.applyPreparedAtBlockBoundary(host.configurationVersion(), diagnostic);
}
}
