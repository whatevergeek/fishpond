#pragma once

#include "AudioShell.h"
#include "host/ControlledVST3Bass.h"
#include "runtime/PythonExecutionWorker.h"
#include "runtime/Runtime.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::KeyListener,
                            private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void executeEditorText(const juce::String& source);
    void timerCallback() override;
    juce::String currentCodeBlock() const;
    juce::String currentLine() const;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
                                          int outputChannels, int samples,
                                          const juce::AudioIODeviceCallbackContext&) override;

    fishpond::AudioShell audioShell;
    fishpond::Runtime runtime;
    fishpond::PythonExecutionWorker pythonWorker;
    std::unique_ptr<fishpond::ControlledVST3Bass> bass;
    juce::AudioDeviceManager deviceManager;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component liveCodingPanel;
    juce::TextEditor liveCodingEditor;
    juce::TextButton executeButton { "Execute block" };
    juce::TextEditor diagnostics;
    juce::Label mixerPlaceholder;
    juce::TextButton loadBassButton { "Load Bass fixture" };
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "120 BPM" };
};
