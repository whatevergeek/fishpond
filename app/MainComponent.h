#pragma once

#include "AudioShell.h"
#include "runtime/Runtime.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component, private juce::AudioIODeviceCallback {
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
                                          int outputChannels, int samples,
                                          const juce::AudioIODeviceCallbackContext&) override;

    fishpond::AudioShell audioShell;
    fishpond::Runtime runtime;
    juce::AudioDeviceManager deviceManager;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component liveCodingPanel;
    juce::TextEditor liveCodingEditor;
    juce::TextButton executeButton { "Execute selection" };
    juce::TextEditor diagnostics;
    juce::Label mixerPlaceholder;
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "120 BPM" };
};
