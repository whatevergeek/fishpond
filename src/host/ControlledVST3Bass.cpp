#include "host/ControlledVST3Bass.h"

namespace fishpond {
bool ControlledVST3Bass::prepareBundle(const juce::File& bundle, std::string& diagnostic)
{
    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3.findAllTypesForFile(descriptions, bundle.getFullPathName());
    if (descriptions.size() != 1) {
        diagnostic = "FP_BASS_DISCOVERY: expected exactly one VST3 instrument in controlled bundle";
        return false;
    }

    juce::String loadError;
    auto instance = vst3.createInstanceFromDescription(*descriptions[0], configuration.sampleRate,
                                                        configuration.blockSize, loadError);
    if (instance == nullptr) {
        diagnostic = "FP_BASS_LOAD: " + loadError.toStdString();
        return false;
    }
    return host.prepareInstrument(std::move(instance), diagnostic);
}

bool ControlledVST3Bass::commitAtBlockBoundary(std::string& diagnostic)
{
    return host.applyPreparedAtBlockBoundary(host.configurationVersion(), diagnostic);
}
}
