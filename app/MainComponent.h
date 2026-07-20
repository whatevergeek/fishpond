#pragma once

#include "AudioShell.h"
#include "LiveCodingEditor.h"
#include "host/ControlledVST3Bass.h"
#include "mixer/ChannelRegistry.h"
#include "runtime/AsyncBassScheduler.h"
#include "runtime/AudioEventDispatcher.h"
#include "runtime/PythonExecutionWorker.h"
#include "runtime/Runtime.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <atomic>
#include <memory>
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
    static constexpr std::size_t instrumentSlotCount = 4;

    struct InstrumentSlot {
        std::unique_ptr<fishpond::HostedInstrument> instrument;
        std::uint64_t channelId {};
        juce::TextButton chooseButton;
        juce::TextButton openEditorButton;
        juce::TextEditor channelName;
        juce::TextButton renameButton;
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<juce::DocumentWindow> editorWindow;
    };

    void executeEditorText(const juce::String& source);
    void chooseInstrument(std::size_t slotIndex);
    void loadInstrumentBundle(std::size_t slotIndex, const juce::File& bundle);
    void openInstrumentEditor(std::size_t slotIndex);
    void renameInstrumentChannel(std::size_t slotIndex);
    void timerCallback() override;
    void updateSchedulerTiming();
    juce::Range<int> editorEvaluationRange() const;
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
    std::array<InstrumentSlot, instrumentSlotCount> instrumentSlots;
    std::array<juce::MidiBuffer, instrumentSlotCount> channelMidi;
    std::array<juce::AudioBuffer<float>, instrumentSlotCount> channelAudio;
    fishpond::ChannelRegistry channels;
    juce::AudioDeviceManager deviceManager;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component liveCodingPanel;
    juce::CodeDocument liveCodingDocument;
    FishpondCodeTokeniser liveCodingTokeniser;
    LiveCodingEditor liveCodingEditor { liveCodingDocument, liveCodingTokeniser };
    juce::TextEditor diagnostics;
    juce::Label instrumentsPanel;
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "120 BPM" };
    juce::Slider tempoSlider;
};
