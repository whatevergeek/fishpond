#pragma once

#include "AudioShell.h"
#include "host/ControlledVST3Bass.h"
#include "runtime/AsyncBassScheduler.h"
#include "runtime/PythonExecutionWorker.h"
#include "runtime/AudioEventDispatcher.h"
#include "runtime/Runtime.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <atomic>
#include <functional>
#include <vector>

class LiveCodingEditor final : public juce::TextEditor {
public:
    std::function<bool(const juce::KeyPress&)> onShortcut;

    bool keyPressed(const juce::KeyPress& key) override
    {
        return onShortcut != nullptr && onShortcut(key) ? true : juce::TextEditor::keyPressed(key);
    }
};

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void executeEditorText(const juce::String& source);
    void loadBassBundle(const juce::File& bundle);
    void openBassEditor();
    void timerCallback() override;
    void updateSchedulerTiming();
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
    fishpond::RuntimeNoteEventQueue noteQueue;
    fishpond::AudioEventDispatcher<8192> noteDispatcher;
    std::atomic<std::uint64_t> renderFrame {};
    fishpond::AsyncBassScheduler<8192> bassScheduler { noteQueue, renderFrame };
    std::atomic<double> activeSampleRate { 48'000.0 };
    std::atomic<std::uint32_t> activeBlockSize { 512 };
    std::uint64_t observedPanic {};
    juce::MidiBuffer bassMidi;
    std::unique_ptr<fishpond::ControlledVST3Bass> bass;
    juce::AudioDeviceManager deviceManager;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component liveCodingPanel;
    LiveCodingEditor liveCodingEditor;
    juce::TextButton executeButton { "Execute block" };
    juce::TextEditor diagnostics;
    juce::Label mixerPlaceholder;
    juce::TextButton loadBassButton { "Load Bass fixture" };
    juce::TextButton chooseBassButton { "Load VST3..." };
    juce::TextButton openBassEditorButton { "Open plugin UI" };
    std::unique_ptr<juce::FileChooser> bassPluginChooser;
    std::unique_ptr<juce::DocumentWindow> bassEditorWindow;
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "120 BPM" };
    juce::Slider tempoSlider;
};
