#pragma once

#include "AudioShell.h"
#include "host/ControlledVST3Bass.h"
#include "mixer/ChannelRegistry.h"
#include "runtime/AsyncBassScheduler.h"
#include "runtime/PythonExecutionWorker.h"
#include "runtime/AudioEventDispatcher.h"
#include "runtime/Runtime.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <atomic>
#include <array>
#include <vector>

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::Timer,
                            private juce::KeyListener {
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void executeEditorText(const juce::String& source);
    void loadBassBundle(const juce::File& bundle);
    void loadLeadBundle(const juce::File& bundle);
    void openBassEditor();
    void openLeadEditor();
    void renameBassChannel();
    void renameLeadChannel();
    void timerCallback() override;
    void updateSchedulerTiming();
    juce::String currentLine() const;
    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
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
    std::array<juce::MidiBuffer, 2> channelMidi;
    std::array<juce::AudioBuffer<float>, 2> channelAudio;
    std::unique_ptr<fishpond::HostedInstrument> bass;
    std::unique_ptr<fishpond::HostedInstrument> lead;
    fishpond::ChannelRegistry channels;
    std::uint64_t bassChannelId {};
    std::uint64_t leadChannelId {};
    juce::AudioDeviceManager deviceManager;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component liveCodingPanel;
    juce::TextEditor liveCodingEditor;
    juce::TextEditor diagnostics;
    juce::Label mixerPlaceholder;
    juce::TextButton chooseBassButton { "Load Inst 01 VST3..." };
    juce::TextButton openBassEditorButton { "Open Inst 01 UI" };
    juce::TextEditor bassChannelName { "Instrument 01" };
    juce::TextButton renameBassButton { "Rename" };
    juce::TextButton chooseLeadButton { "Load Inst 02 VST3..." };
    juce::TextButton openLeadEditorButton { "Open Inst 02 UI" };
    juce::TextEditor leadChannelName { "Instrument 02" };
    juce::TextButton renameLeadButton { "Rename" };
    std::unique_ptr<juce::FileChooser> bassPluginChooser;
    std::unique_ptr<juce::FileChooser> leadPluginChooser;
    std::unique_ptr<juce::DocumentWindow> bassEditorWindow;
    std::unique_ptr<juce::DocumentWindow> leadEditorWindow;
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "120 BPM" };
    juce::Slider tempoSlider;
};
