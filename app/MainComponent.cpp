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
    std::uint64_t channelId {};
    std::vector<std::vector<int>> noteSteps;
    double periodBeats {};
    double durationBeats {};
    int velocity {};
};

std::optional<std::size_t> singlePlayerStopIndex(const juce::String& command)
{
    juce::String player;
    if (command.startsWith("silence(P") && command.endsWithChar(')'))
        player = command.substring(8, command.length() - 1);
    else if (command.endsWith(".stop()"))
        player = command.dropLastCharacters(7);
    if (player.length() != 2 || player[0] != 'P' || player[1] < 'a' || player[1] > 'z')
        return std::nullopt;
    return static_cast<std::size_t>(player[1] - 'a');
}
}

MainComponent::MainComponent()
{
    liveCodingEditor.setMultiLine(true, true);
    liveCodingEditor.setReturnKeyStartsNewLine(true);
    liveCodingEditor.setText("# Shift+Return: evaluate the current line, or the selected lines\n# {C3 E3 G3} plays a chord; silence() stops all players\nPa >> n(\"{C2 E2 G2}\", target=\"instrument_01\", p=0.5)\n");
    liveCodingEditor.addKeyListener(this);
    diagnostics.setMultiLine(true);
    diagnostics.setReadOnly(true);
    diagnostics.setText("Ready. Load an instrument in Instruments, then evaluate a pattern.", juce::dontSendNotification);
    liveCodingPanel.addAndMakeVisible(liveCodingEditor);
    liveCodingPanel.addAndMakeVisible(diagnostics);
    mixerPlaceholder.setText("Instruments - two instrument slots", juce::dontSendNotification);
    mixerPlaceholder.setJustificationType(juce::Justification::centred);
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
        bassPluginChooser = std::make_unique<juce::FileChooser>("Select Instrument 01 VST3", vst3Folder, "*.vst3");
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
    mixerPlaceholder.addAndMakeVisible(chooseBassButton);
    openBassEditorButton.onClick = [this] { openBassEditor(); };
    const auto bassChannel = channels.add("Instrument 01");
    jassert(bassChannel.has_value());
    bassChannelId = bassChannel->id;
    bassChannelName.setText("Instrument 01", juce::dontSendNotification);
    renameBassButton.onClick = [this] { renameBassChannel(); };
    const auto leadChannel = channels.add("Instrument 02");
    jassert(leadChannel.has_value());
    leadChannelId = leadChannel->id;
    leadChannelName.setText("Instrument 02", juce::dontSendNotification);
    chooseLeadButton.onClick = [this] {
        leadPluginChooser = std::make_unique<juce::FileChooser>("Select Instrument 02 VST3", juce::File(), "*.vst3");
        leadPluginChooser->launchAsync(juce::FileBrowserComponent::openMode
                                           | juce::FileBrowserComponent::canSelectFiles
                                           | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& chooser) {
                const auto bundle = chooser.getResult();
                leadPluginChooser.reset();
                if (bundle != juce::File())
                    loadLeadBundle(bundle);
            });
    };
    renameLeadButton.onClick = [this] { renameLeadChannel(); };
    mixerPlaceholder.addAndMakeVisible(openBassEditorButton);
    mixerPlaceholder.addAndMakeVisible(bassChannelName);
    mixerPlaceholder.addAndMakeVisible(renameBassButton);
    mixerPlaceholder.addAndMakeVisible(chooseLeadButton);
    openLeadEditorButton.onClick = [this] { openLeadEditor(); };
    mixerPlaceholder.addAndMakeVisible(openLeadEditorButton);
    mixerPlaceholder.addAndMakeVisible(leadChannelName);
    mixerPlaceholder.addAndMakeVisible(renameLeadButton);
    tabs.addTab("Live Coding", juce::Colours::darkgrey, &liveCodingPanel, false);
    tabs.addTab("Instruments", juce::Colours::darkgrey, &mixerPlaceholder, false);

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
    for (auto& midi : channelMidi)
        midi.ensureSize(4096);
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
            mixerPlaceholder.setText("Stop audio before loading Instrument 01", juce::dontSendNotification);
            return;
        }
        bassEditorWindow.reset();
        bass = std::make_unique<fishpond::HostedInstrument>(fishpond::AudioConfiguration { 48'000.0, 512, 1 });
        std::string diagnostic;
        const auto prepared = bass->prepareBundle(bundle, diagnostic);
        const auto committed = prepared && bass->commitAtBlockBoundary(diagnostic);
        mixerPlaceholder.setText(committed ? "Instrument 01 ready: " + bundle.getFileName() : diagnostic,
                                 juce::dontSendNotification);
}

void MainComponent::loadLeadBundle(const juce::File& bundle)
{
    if (audioShell.state() == fishpond::AudioShellState::running) {
        mixerPlaceholder.setText("Stop audio before loading Instrument 02", juce::dontSendNotification);
        return;
    }
    leadEditorWindow.reset();
    lead = std::make_unique<fishpond::HostedInstrument>(fishpond::AudioConfiguration { 48'000.0, 512, 1 });
    std::string diagnostic;
    const auto prepared = lead->prepareBundle(bundle, diagnostic);
    const auto committed = prepared && lead->commitAtBlockBoundary(diagnostic);
    mixerPlaceholder.setText(committed ? "Instrument 02 ready: " + bundle.getFileName() : diagnostic,
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
        mixerPlaceholder.setText("Load Instrument 01 before opening its UI", juce::dontSendNotification);
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

void MainComponent::openLeadEditor()
{
    if (leadEditorWindow != nullptr) {
        leadEditorWindow->setVisible(true);
        leadEditorWindow->toFront(true);
        return;
    }
    auto* processor = lead != nullptr ? lead->activeProcessorForEditor() : nullptr;
    if (processor == nullptr || lead->state() != fishpond::SingleChannelState::ready) {
        mixerPlaceholder.setText("Load Instrument 02 before opening its UI", juce::dontSendNotification);
        return;
    }
    if (! processor->hasEditor()) {
        mixerPlaceholder.setText("This VST3 does not provide a plugin UI", juce::dontSendNotification);
        return;
    }
    leadEditorWindow = std::make_unique<PluginEditorWindow>(*processor);
    leadEditorWindow->setVisible(true);
    leadEditorWindow->toFront(true);
}

void MainComponent::renameBassChannel()
{
    if (! channels.rename(bassChannelId, bassChannelName.getText().toStdString())) {
        mixerPlaceholder.setText("FP_CHANNEL_NAME_INVALID: choose a unique non-empty name", juce::dontSendNotification);
        return;
    }
    const auto channel = channels.resolve(bassChannelName.getText().toStdString());
    mixerPlaceholder.setText("Channel renamed: " + bassChannelName.getText() + " (target=\"" + juce::String(channel->alias) + "\")",
                             juce::dontSendNotification);
}

void MainComponent::renameLeadChannel()
{
    if (! channels.rename(leadChannelId, leadChannelName.getText().toStdString())) {
        mixerPlaceholder.setText("FP_CHANNEL_NAME_INVALID: choose a unique non-empty name", juce::dontSendNotification);
        return;
    }
    const auto channel = channels.resolve(leadChannelName.getText().toStdString());
    mixerPlaceholder.setText("Channel renamed: " + leadChannelName.getText() + " (target=\""
                                 + juce::String(channel->alias) + "\")", juce::dontSendNotification);
}

MainComponent::~MainComponent()
{
    stopTimer();
    liveCodingEditor.removeKeyListener(this);
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
            diagnostics.setText(command == "panic()" ? "Panic: cleared instrument events" : "Silenced active players",
                                juce::dontSendNotification);
            continue;
        }
        if (const auto playerIndex = singlePlayerStopIndex(command)) {
            bassScheduler.remove(*playerIndex);
            diagnostics.setText("Silenced P" + juce::String::charToString(static_cast<juce::juce_wchar>('a' + *playerIndex)),
                                juce::dontSendNotification);
            continue;
        }
        if (completion.source.find(">> n(") != std::string::npos) {
            std::vector<std::uint64_t> readyChannelIds;
            if (bass != nullptr && bass->state() == fishpond::SingleChannelState::ready)
                readyChannelIds.push_back(bassChannelId);
            if (lead != nullptr && lead->state() == fishpond::SingleChannelState::ready)
                readyChannelIds.push_back(leadChannelId);
            std::vector<PendingBassPattern> patterns;
            juce::StringArray lines;
            lines.addLines(juce::String(completion.source));

            for (const auto& line : lines) {
                const auto patternSource = line.toStdString();
                if (patternSource.find(">> n(") == std::string::npos)
                    continue;

                const auto validation = runtime.evaluateEditorText(patternSource, channels, readyChannelIds);
                if (! validation.accepted) {
                    diagnostics.setText(validation.diagnostic, juce::dontSendNotification);
                    patterns.clear();
                    break;
                }
                const auto targetMarker = patternSource.find("target=");
                const auto targetQuote = patternSource.find('"', targetMarker);
                const auto targetEnd = patternSource.find('"', targetQuote + 1);
                const auto channel = channels.resolve(patternSource.substr(targetQuote + 1, targetEnd - targetQuote - 1));

                const auto noteSteps = runtime.noteStepsFromEditorText(patternSource);
                const auto playerIndex = runtime.playerIndexFromEditorText(patternSource);
                const auto periodBeats = runtime.periodBeatsFromEditorText(patternSource);
                const auto durationBeats = runtime.durationBeatsFromEditorText(patternSource);
                const auto velocity = runtime.velocityFromEditorText(patternSource);
                if (! playerIndex || ! periodBeats || ! durationBeats || ! velocity) {
                    diagnostics.setText("FP_PATTERN_VALUE_INVALID: player, p, dur, and velocity must be valid", juce::dontSendNotification);
                    patterns.clear();
                    break;
                }
                if (! channel) {
                    diagnostics.setText("FP_TARGET_UNAVAILABLE: target has no ready instrument channel", juce::dontSendNotification);
                    patterns.clear();
                    break;
                }
                if (noteSteps.empty()) {
                    diagnostics.setText("FP_PATTERN_VALUE_INVALID: notes must use C3 or {C3 E3 G3} chord groups", juce::dontSendNotification);
                    patterns.clear();
                    break;
                }
                patterns.push_back({ *playerIndex, channel->id, noteSteps, *periodBeats, *durationBeats, *velocity });
            }

            if (patterns.empty())
                continue;

            std::vector<fishpond::AsyncBassScheduler<8192>::Pattern> scheduledPatterns;
            scheduledPatterns.reserve(patterns.size());
            for (auto& pattern : patterns)
                scheduledPatterns.push_back({ pattern.playerIndex, pattern.channelId, std::move(pattern.noteSteps), pattern.periodBeats,
                                              static_cast<std::uint8_t>(pattern.velocity), pattern.durationBeats });
            bassScheduler.replaceAll(std::move(scheduledPatterns));

            diagnostics.setText("Queued " + juce::String(static_cast<int>(patterns.size()))
                                    + (patterns.size() == 1 ? " instrument pattern for the next bar"
                                                            : " instrument patterns for the next bar"),
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

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* origin)
{
    if (origin != &liveCodingEditor || ! key.isKeyCode(juce::KeyPress::returnKey))
        return false;

    const auto shiftDown = key.getModifiers().isShiftDown()
                        || juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    if (! shiftDown)
        return false;

    const auto selectedText = liveCodingEditor.getHighlightedText();
    executeEditorText(selectedText.isNotEmpty() ? selectedText : currentLine());
    return true;
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
    chooseBassButton.setBounds(mixerActions.removeFromLeft(140));
    openBassEditorButton.setBounds(mixerActions.removeFromLeft(130));
    bassChannelName.setBounds(mixerActions.removeFromLeft(160));
    renameBassButton.setBounds(mixerActions.removeFromLeft(80));
    mixerActions = mixerArea.removeFromTop(32);
    chooseLeadButton.setBounds(mixerActions.removeFromLeft(140));
    openLeadEditorButton.setBounds(mixerActions.removeFromLeft(130));
    leadChannelName.setBounds(mixerActions.removeFromLeft(160));
    renameLeadButton.setBounds(mixerActions.removeFromLeft(80));
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
    for (auto& buffer : channelAudio)
        buffer.setSize(outputChannels, device->getCurrentBufferSizeSamples(), false, false, true);
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
    for (auto& midi : channelMidi)
        midi.clear();
    const auto start = renderFrame.load();
    noteDispatcher.drain(noteQueue, start, static_cast<std::uint32_t>(samples), observedPanic,
        [this] (const fishpond::NoteEvent& event, std::uint32_t offset) {
            if (event.type == fishpond::NoteEventType::allNotesOff && event.channelId == 0) {
                for (auto& midi : channelMidi) {
                    midi.addEvent(juce::MidiMessage::allNotesOff(event.midiChannel), static_cast<int>(offset));
                    midi.addEvent(juce::MidiMessage::allSoundOff(event.midiChannel), static_cast<int>(offset));
                }
                return;
            }
            auto* midi = event.channelId == bassChannelId ? &channelMidi[0]
                       : event.channelId == leadChannelId ? &channelMidi[1] : nullptr;
            if (midi == nullptr)
                return;
            if (event.type == fishpond::NoteEventType::noteOn)
                midi->addEvent(juce::MidiMessage::noteOn(event.midiChannel, event.midiNote,
                                                             event.velocity / 127.0f), static_cast<int>(offset));
            else if (event.type == fishpond::NoteEventType::noteOff)
                midi->addEvent(juce::MidiMessage::noteOff(event.midiChannel, event.midiNote), static_cast<int>(offset));
            else if (event.type == fishpond::NoteEventType::allNotesOff) {
                midi->addEvent(juce::MidiMessage::allNotesOff(event.midiChannel), static_cast<int>(offset));
                midi->addEvent(juce::MidiMessage::allSoundOff(event.midiChannel), static_cast<int>(offset));
            }
        });
    audio.clear();
    const std::array<fishpond::HostedInstrument*, 2> instruments { bass.get(), lead.get() };
    for (std::size_t index = 0; index < instruments.size(); ++index) {
        auto& instrumentAudio = channelAudio[index];
        instrumentAudio.clear();
        if (instruments[index] != nullptr)
            instruments[index]->process(instrumentAudio, channelMidi[index]);
        for (int channel = 0; channel < outputChannels; ++channel)
            audio.addFrom(channel, 0, instrumentAudio, channel, 0, samples);
    }
    renderFrame.store(start + samples);
}
