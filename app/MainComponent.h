#pragma once

#include "AudioShell.h"
#include "LiveCodingEditor.h"
#include "host/ControlledVST3Bass.h"
#include "host/PreparedGraphHandoff.h"
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
    void newLiveCodingFile();
    void openLiveCodingFile();
    void saveLiveCodingFile();
    void saveLiveCodingFileAs();

private:
    static constexpr std::size_t instrumentSlotCount = 4;

    struct InstrumentSlot {
        std::unique_ptr<fishpond::HostedInstrument> instrument;
        fishpond::PreparedGraphHandoff<juce::AudioProcessor, 4> handoff;
        std::uint64_t channelId {};
        std::atomic<bool> loading {};
        std::atomic<std::uint64_t> submittedLoadId {};
        std::atomic<std::uint64_t> committedLoadId {};
        std::atomic<std::uint64_t> rejectedLoadId {};
        juce::String pendingBundleName;
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
    void finishInstrumentLoad(std::size_t slotIndex, std::uint64_t loadId,
                              const juce::String& bundleName, bool submitted,
                              const juce::String& diagnostic);
    void commitPendingInstrumentWhileStopped(std::size_t slotIndex);
    fishpond::AudioConfiguration currentAudioConfiguration() const noexcept;
    void openInstrumentEditor(std::size_t slotIndex);
    void renameInstrumentChannel(std::size_t slotIndex);
    void loadLiveCodingFile(const juce::File& file);
    void saveLiveCodingFileTo(juce::File file);
    void confirmDiscardLiveCodingChanges(const juce::String& action, std::function<void()> continuation);
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
    std::atomic<float> masterGain { 1.0f };
    double tempoBpm { 120.0 };
    // Even values are stable snapshots; an odd value means device configuration is changing.
    std::atomic<std::uint64_t> audioConfigurationVersion { 2 };
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
    juce::File liveCodingFile;
    std::unique_ptr<juce::FileChooser> liveCodingFileChooser;
    juce::TextEditor diagnostics;
    juce::Label instrumentsPanel;
    juce::Label deviceStatus;
    juce::TextButton startStopButton { "Start audio" };
    juce::Label tempoLabel { {}, "clock.tempo = 120" };
    juce::Label masterVolumeLabel { {}, "Master volume" };
    juce::Slider masterVolumeSlider;
};
