#include "MainComponent.h"

namespace {
class PluginEditorWindow final : public juce::DocumentWindow {
public:
    explicit PluginEditorWindow(juce::AudioProcessor& processor)
        : DocumentWindow(processor.getName(), juce::Colours::darkgrey, juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        auto* editor = processor.createEditorAndMakeActive();
        setContentOwned(editor, true);
        centreWithSize(editor->getWidth(), editor->getHeight());
    }

    void closeButtonPressed() override { setVisible(false); }
};

struct PendingBassPattern {
    std::size_t playerIndex {};
    std::vector<int> notes;
    double periodBeats {};
    double durationBeats {};
    int velocity {};
};
}

MainComponent::MainComponent()
{
    liveCodingEditor.setMultiLine(true, true);
    liveCodingEditor.setReturnKeyStartsNewLine(true);
    liveCodingEditor.setText("# Shift+Return: evaluate the current line, or the selected lines\n# silence() stops active players\nPa >> n(\"C2 C3\", target=\"bass\", p=0.5)\n");
    liveCodingEditor.onShortcut = [this] (const juce::KeyPress& key) {
        const auto& modifiers = key.getModifiers();
        if (modifiers.isShiftDown() && key.getKeyCode() == juce::KeyPress::returnKey) {
            const auto selectedText = liveCodingEditor.getHighlightedText();
            executeEditorText(selectedText.isNotEmpty() ? selectedText : currentLine());
            return true;
        }
        return false;
    };
    diagnostics.setMultiLine(true);
    diagnostics.setReadOnly(true);
    diagnostics.setText("Ready. Load the controlled Bass fixture in Mixer, then evaluate a pattern.", juce::dontSendNotification);
    liveCodingPanel.addAndMakeVisible(liveCodingEditor);
    liveCodingPanel.addAndMakeVisible(diagnostics);
    mixerPlaceholder.setText("Mixer - one controlled Bass VST3 channel", juce::dontSendNotification);
    mixerPlaceholder.setJustificationType(juce::Justification::centred);
    loadBassButton.onClick = [this] { loadBassBundle(juce::File(FISHPOND_CONTROLLED_BASS_PATH)); };
    chooseBassButton.onClick = [this] {
       #if JUCE_MAC
        auto vst3Folder = juce::File("/Library/Audio/Plug-Ins/VST3");
        if (! vst3Folder.exists())
            vst3Folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                             .getChildFile("Library/Audio/Plug-Ins/VST3");
       #elif JUCE_WINDOWS
        auto vst3Folder = juce::File("C:\\Program Files\\Common Files\\VST3");
       #elif JUCE_LINUX
        auto vst3Folder = juce::File("/usr/lib/vst3");
        if (! vst3Folder.exists())
            vst3Folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3");
       #else
        auto vst3Folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
       #endif
        bassPluginChooser = std::make_unique<juce::FileChooser>("Select a VST3 instrument", vst3Folder, "*.vst3");
        bassPluginChooser->launchAsync(juce::FileBrowserComponent::openMode
                                           | juce::FileBrowserComponent::canSelectFiles
                                           | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& chooser) {
                const auto bundle = chooser.getResult();
                bassPluginChooser.reset();
                if (bundle != juce::File())
                    loadBassBundle(bundle);
            });
    };
    mixerPlaceholder.addAndMakeVisible(loadBassButton);
    mixerPlaceholder.addAndMakeVisible(chooseBassButton);
    openBassEditorButton.onClick = [this] { openBassEditor(); };
    mixerPlaceholder.addAndMakeVisible(openBassEditorButton);
    tabs.addTab("Live Coding", juce::Colours::darkgrey, &liveCodingPanel, false);
    tabs.addTab("Mixer", juce::Colours::darkgrey, &mixerPlaceholder, false);

    startStopButton.onClick = [this] {
        if (audioShell.state() == fishpond::AudioShellState::running) {
            deviceManager.closeAudioDevice();
            startStopButton.setButtonText("Start audio");
            deviceStatus.setText("Audio engine stopped", juce::dontSendNotification);
        } else {
            const auto error = deviceManager.initialiseWithDefaultDevices(0, 2);
            if (error.isNotEmpty())
                deviceStatus.setText("Audio-device initialization failed: " + error, juce::dontSendNotification);
            else
                startStopButton.setButtonText("Stop audio");
        }
    };

    deviceStatus.setText("Audio engine stopped", juce::dontSendNotification);
    deviceManager.addAudioCallback(this);
    bassMidi.ensureSize(4096);
    tempoSlider.setRange(30.0, 300.0, 1.0);
    tempoSlider.setValue(120.0, juce::dontSendNotification);
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    tempoSlider.onValueChange = [this] {
        tempoLabel.setText(juce::String(tempoSlider.getValue(), 0) + " BPM", juce::dontSendNotification);
        if (! tempoSlider.isMouseButtonDown())
            updateSchedulerTiming();
    };
    tempoSlider.onDragEnd = [this] { updateSchedulerTiming(); };
    startTimerHz(30);
    addAndMakeVisible(tabs);
    addAndMakeVisible(deviceStatus);
    addAndMakeVisible(startStopButton);
    addAndMakeVisible(tempoLabel);
    addAndMakeVisible(tempoSlider);
    setSize(1040, 700);
}

void MainComponent::loadBassBundle(const juce::File& bundle)
{
        if (audioShell.state() == fishpond::AudioShellState::running) {
            mixerPlaceholder.setText("Stop audio before loading a Bass VST3", juce::dontSendNotification);
            return;
        }
        bassEditorWindow.reset();
        bass = std::make_unique<fishpond::ControlledVST3Bass>(fishpond::AudioConfiguration { 48'000.0, 512, 1 });
        std::string diagnostic;
        const auto prepared = bass->prepareBundle(bundle, diagnostic);
        const auto committed = prepared && bass->commitAtBlockBoundary(diagnostic);
        mixerPlaceholder.setText(committed ? "Bass VST3 ready: " + bundle.getFileName() : diagnostic,
                                 juce::dontSendNotification);
}

void MainComponent::openBassEditor()
{
    if (bassEditorWindow != nullptr) {
        bassEditorWindow->setVisible(true);
        bassEditorWindow->toFront(true);
        return;
    }
    auto* processor = bass != nullptr ? bass->activeProcessorForEditor() : nullptr;
    if (processor == nullptr || bass->state() != fishpond::SingleChannelState::ready) {
        mixerPlaceholder.setText("Load a Bass VST3 before opening its UI", juce::dontSendNotification);
        return;
    }
    if (! processor->hasEditor()) {
        mixerPlaceholder.setText("This VST3 does not provide a plugin UI", juce::dontSendNotification);
        return;
    }
    bassEditorWindow = std::make_unique<PluginEditorWindow>(*processor);
    bassEditorWindow->setVisible(true);
    bassEditorWindow->toFront(true);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

void MainComponent::executeEditorText(const juce::String& source)
{
    if (source.trim().isEmpty()) {
        diagnostics.setText("Nothing to evaluate", juce::dontSendNotification);
        return;
    }
    if (! pythonWorker.submit(source.toStdString())) {
        diagnostics.setText("FP_RUNTIME_BUSY: evaluation queue is full", juce::dontSendNotification);
        return;
    }
    diagnostics.setText("Evaluating on the Python runtime thread...", juce::dontSendNotification);
}

void MainComponent::timerCallback()
{
    fishpond::PythonExecutionCompletion completion;
    while (pythonWorker.tryTakeCompletion(completion)) {
        if (! completion.result.accepted) {
            diagnostics.setText(completion.result.diagnostic, juce::dontSendNotification);
            continue;
        }
        if (completion.result.changedTempoBpm) {
            tempoSlider.setValue(*completion.result.changedTempoBpm, juce::dontSendNotification);
            tempoLabel.setText(juce::String(*completion.result.changedTempoBpm, 0) + " BPM",
                               juce::dontSendNotification);
            updateSchedulerTiming();
        }
        const auto command = juce::String(completion.source).trim();
        if (command == "silence()" || command == "panic()") {
            bassScheduler.clear();
            diagnostics.setText(command == "panic()" ? "Panic: cleared Bass events" : "Silenced active players",
                                juce::dontSendNotification);
            continue;
        }
        if (completion.source.find(">> n(") != std::string::npos) {
            const auto bassReady = bass != nullptr && bass->state() == fishpond::SingleChannelState::ready;
            std::vector<PendingBassPattern> patterns;
            juce::StringArray lines;
            lines.addLines(juce::String(completion.source));

            for (const auto& line : lines) {
                const auto patternSource = line.toStdString();
                if (patternSource.find(">> n(") == std::string::npos)
                    continue;

                const auto validation = runtime.evaluateEditorText(patternSource, bassReady);
                if (! validation.accepted) {
                    diagnostics.setText(validation.diagnostic, juce::dontSendNotification);
                    patterns.clear();
                    break;
                }

                const auto notes = runtime.notesFromEditorText(patternSource);
                const auto playerIndex = runtime.playerIndexFromEditorText(patternSource);
                const auto periodBeats = runtime.periodBeatsFromEditorText(patternSource);
                const auto durationBeats = runtime.durationBeatsFromEditorText(patternSource);
                const auto velocity = runtime.velocityFromEditorText(patternSource);
                if (! playerIndex || ! periodBeats || ! durationBeats || ! velocity) {
                    diagnostics.setText("FP_PATTERN_VALUE_INVALID: player, p, dur, and velocity must be valid", juce::dontSendNotification);
                    patterns.clear();
                    break;
                }
                patterns.push_back({ *playerIndex, notes, *periodBeats, *durationBeats, *velocity });
            }

            if (patterns.empty())
                continue;

            for (const auto& pattern : patterns)
                bassScheduler.replace(pattern.playerIndex, pattern.notes, pattern.periodBeats,
                                      static_cast<std::uint8_t>(pattern.velocity), pattern.durationBeats);

            diagnostics.setText("Playing " + juce::String(static_cast<int>(patterns.size()))
                                    + (patterns.size() == 1 ? " Bass-note pattern" : " Bass-note patterns"),
                                juce::dontSendNotification);
        } else {
            diagnostics.setText(completion.result.diagnostic, juce::dontSendNotification);
        }
    }
}

juce::String MainComponent::currentLine() const
{
    const auto source = liveCodingEditor.getText();
    const auto caret = liveCodingEditor.getCaretPosition();
    const auto lineStart = source.substring(0, caret).lastIndexOfChar('\n');
    const auto lineEnd = source.indexOfChar(caret, '\n');
    return source.substring(lineStart < 0 ? 0 : lineStart + 1,
                            lineEnd < 0 ? source.length() : lineEnd);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto transport = area.removeFromTop(34);
    deviceStatus.setBounds(transport.removeFromLeft(500));
    tempoLabel.setBounds(transport.removeFromLeft(76));
    tempoSlider.setBounds(transport.removeFromLeft(180));
    startStopButton.setBounds(transport.removeFromRight(120));
    tabs.setBounds(area);
    auto liveArea = liveCodingPanel.getLocalBounds().reduced(8);
    diagnostics.setBounds(liveArea.removeFromBottom(84));
    liveCodingEditor.setBounds(liveArea);
    auto mixerArea = mixerPlaceholder.getLocalBounds().reduced(8);
    auto mixerActions = mixerArea.removeFromTop(32);
    loadBassButton.setBounds(mixerActions.removeFromLeft(160));
    chooseBassButton.setBounds(mixerActions.removeFromLeft(120));
    openBassEditorButton.setBounds(mixerActions.removeFromLeft(130));
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto outputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
    if (! audioShell.selectDevice(device->getName().toStdString(), outputChannels)) {
        deviceStatus.setText("Audio-device initialization failed: stereo output is required", juce::dontSendNotification);
        return;
    }

    audioShell.start();
    activeSampleRate.store(device->getCurrentSampleRate(), std::memory_order_release);
    activeBlockSize.store(static_cast<std::uint32_t>(device->getCurrentBufferSizeSamples()), std::memory_order_release);
    updateSchedulerTiming();
    deviceStatus.setText("Running " + device->getName() + " — "
                             + juce::String(device->getCurrentSampleRate(), 0) + " Hz, "
                             + juce::String(device->getCurrentBufferSizeSamples()) + " samples",
                         juce::dontSendNotification);
}

void MainComponent::updateSchedulerTiming()
{
    bassScheduler.setTiming({ activeSampleRate.load(std::memory_order_acquire),
                              activeBlockSize.load(std::memory_order_acquire), tempoSlider.getValue() });
}

void MainComponent::audioDeviceStopped()
{
    audioShell.stop();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
                                                      int outputChannels, int samples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    juce::AudioBuffer<float> audio(output, outputChannels, samples);
    bassMidi.clear();
    const auto start = renderFrame.load();
    noteDispatcher.drain(noteQueue, start, static_cast<std::uint32_t>(samples), observedPanic,
        [this] (const fishpond::NoteEvent& event, std::uint32_t offset) {
            if (event.type == fishpond::NoteEventType::noteOn)
                bassMidi.addEvent(juce::MidiMessage::noteOn(event.midiChannel, event.midiNote,
                                                             event.velocity / 127.0f), static_cast<int>(offset));
            else if (event.type == fishpond::NoteEventType::noteOff)
                bassMidi.addEvent(juce::MidiMessage::noteOff(event.midiChannel, event.midiNote), static_cast<int>(offset));
        });
    if (bass != nullptr)
        bass->process(audio, bassMidi);
    else
        audio.clear();
    renderFrame.store(start + samples);
}
