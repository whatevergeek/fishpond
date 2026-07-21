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

struct PendingInstrumentPattern {
    std::size_t playerIndex {};
    std::uint64_t channelId {};
    std::vector<std::vector<int>> noteSteps;
    double periodBeats {};
    double durationBeats {};
    int velocity {};
};

juce::String instrumentName(std::size_t slotIndex)
{
    return "Instrument " + juce::String(static_cast<int>(slotIndex + 1)).paddedLeft('0', 2);
}

juce::File defaultVst3Folder()
{
   #if JUCE_MAC
    auto folder = juce::File("/Library/Audio/Plug-Ins/VST3");
    if (! folder.exists())
        folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                     .getChildFile("Library/Audio/Plug-Ins/VST3");
    return folder;
   #elif JUCE_WINDOWS
    return juce::File("C:\\Program Files\\Common Files\\VST3");
   #elif JUCE_LINUX
    auto folder = juce::File("/usr/lib/vst3");
    if (! folder.exists())
        folder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3");
    return folder;
   #else
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
   #endif
}

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
    liveCodingEditor.loadContent("# Shift+Return: evaluate the current line, or the selected lines\n"
                                 "# {C3 E3 G3} plays a chord; silence() stops all players\n"
                                 "Pa >> n(\"{C2 E2 G2}\", target=\"instrument_01\", p=0.5)\n");
    liveCodingDocument.setSavePoint();
    liveCodingEditor.addKeyListener(this);
    diagnostics.setMultiLine(true);
    diagnostics.setReadOnly(true);
    diagnostics.setScrollbarsShown(true);
    diagnostics.setText("Ready. Load an instrument in Instruments, then evaluate a pattern.", juce::dontSendNotification);
    liveCodingPanel.addAndMakeVisible(liveCodingEditor);
    liveCodingPanel.addAndMakeVisible(diagnostics);
    instrumentsPanel.setText("Instruments - four independent instrument slots", juce::dontSendNotification);
    instrumentsPanel.setJustificationType(juce::Justification::centred);

    for (std::size_t slotIndex = 0; slotIndex < instrumentSlots.size(); ++slotIndex) {
        auto& slot = instrumentSlots[slotIndex];
        const auto name = instrumentName(slotIndex);
        const auto channel = channels.add(name.toStdString());
        jassert(channel.has_value());
        slot.channelId = channel->id;
        slot.instrument = std::make_unique<fishpond::HostedInstrument>(currentAudioConfiguration());
        slot.chooseButton.setButtonText("Load Inst " + juce::String(static_cast<int>(slotIndex + 1)).paddedLeft('0', 2) + " VST3...");
        slot.openEditorButton.setButtonText("Open Inst " + juce::String(static_cast<int>(slotIndex + 1)).paddedLeft('0', 2) + " UI");
        slot.channelName.setText(name, juce::dontSendNotification);
        slot.renameButton.setButtonText("Rename");
        slot.chooseButton.onClick = [this, slotIndex] { chooseInstrument(slotIndex); };
        slot.openEditorButton.onClick = [this, slotIndex] { openInstrumentEditor(slotIndex); };
        slot.renameButton.onClick = [this, slotIndex] { renameInstrumentChannel(slotIndex); };
        instrumentsPanel.addAndMakeVisible(slot.chooseButton);
        instrumentsPanel.addAndMakeVisible(slot.openEditorButton);
        instrumentsPanel.addAndMakeVisible(slot.channelName);
        instrumentsPanel.addAndMakeVisible(slot.renameButton);
    }

    tabs.addTab("Live Coding", juce::Colours::darkgrey, &liveCodingPanel, false);
    tabs.addTab("Instruments", juce::Colours::darkgrey, &instrumentsPanel, false);
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
    masterVolumeLabel.setJustificationType(juce::Justification::centredRight);
    masterVolumeSlider.setRange(-60.0, 0.0, 0.5);
    masterVolumeSlider.setValue(0.0, juce::dontSendNotification);
    masterVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    masterVolumeSlider.setTextValueSuffix(" dB");
    masterVolumeSlider.setDoubleClickReturnValue(true, 0.0);
    masterVolumeSlider.onValueChange = [this] {
        masterGain.store(juce::Decibels::decibelsToGain(static_cast<float>(masterVolumeSlider.getValue())),
                         std::memory_order_release);
    };
    startTimerHz(30);
    addAndMakeVisible(tabs);
    addAndMakeVisible(deviceStatus);
    addAndMakeVisible(startStopButton);
    addAndMakeVisible(tempoLabel);
    addAndMakeVisible(masterVolumeLabel);
    addAndMakeVisible(masterVolumeSlider);
    setSize(1040, 700);
}

void MainComponent::chooseInstrument(std::size_t slotIndex)
{
    auto& slot = instrumentSlots[slotIndex];
    const auto name = instrumentName(slotIndex);
    slot.chooser = std::make_unique<juce::FileChooser>("Select " + name + " VST3", defaultVst3Folder(), "*.vst3");
    slot.chooser->launchAsync(juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories,
        [this, slotIndex] (const juce::FileChooser& chooser) {
            const auto bundle = chooser.getResult();
            instrumentSlots[slotIndex].chooser.reset();
            if (bundle != juce::File())
                loadInstrumentBundle(slotIndex, bundle);
        });
}

void MainComponent::loadInstrumentBundle(std::size_t slotIndex, const juce::File& bundle)
{
    const auto name = instrumentName(slotIndex);
    auto& slot = instrumentSlots[slotIndex];
    if (slot.loading.exchange(true, std::memory_order_acq_rel)) {
        instrumentsPanel.setText(name + " is already loading", juce::dontSendNotification);
        return;
    }

    slot.editorWindow.reset();
    slot.chooseButton.setEnabled(false);
    slot.openEditorButton.setEnabled(false);
    const auto configuration = currentAudioConfiguration();
    const auto loadId = slot.submittedLoadId.fetch_add(1, std::memory_order_acq_rel) + 1;
    const auto bundleName = bundle.getFileName();
    const juce::Component::SafePointer<MainComponent> safeThis(this);

    instrumentsPanel.setText(name + " loading: " + bundleName, juce::dontSendNotification);
    auto format = std::make_shared<juce::VST3PluginFormat>();
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions, bundle.getFullPathName());
    juce::PluginDescription* description = nullptr;
    for (auto* candidate : descriptions)
        if (candidate != nullptr && candidate->isInstrument) {
            description = candidate;
            break;
        }
    if (description == nullptr) {
        finishInstrumentLoad(slotIndex, loadId, bundleName, false,
                             "FP_INSTRUMENT_DISCOVERY: selected bundle has no VST3 instrument class");
        return;
    }

    // JUCE delivers this callback on its message thread. Some commercial VST3s
    // require creation, bus negotiation, and prepareToPlay to happen there, but
    // it is still entirely outside the audio callback and audio keeps rendering.
    format->createPluginInstanceAsync(*description, configuration.sampleRate, configuration.blockSize,
        [safeThis, slotIndex, loadId, bundleName, configuration, format]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& loadError) {
            if (safeThis == nullptr)
                return;

            std::string preparationDiagnostic;
            auto submitted = instance != nullptr;
            if (! submitted) {
                preparationDiagnostic = "FP_INSTRUMENT_LOAD: " + loadError.toStdString();
            } else {
                fishpond::HostedInstrument preparedInstrument(configuration);
                submitted = preparedInstrument.prepareProcessor(std::move(instance), preparationDiagnostic);
                if (submitted) {
                    auto* processor = preparedInstrument.releasePrepared();
                    submitted = safeThis->instrumentSlots[slotIndex].handoff.submit(
                        { processor, configuration.version, loadId });
                    if (! submitted) {
                        delete processor;
                        preparationDiagnostic = "FP_GRAPH_QUEUE_FULL: instrument replacement queue is full";
                    }
                }
            }

            safeThis->finishInstrumentLoad(slotIndex, loadId, bundleName, submitted,
                                           juce::String(preparationDiagnostic));
        });
}

void MainComponent::finishInstrumentLoad(std::size_t slotIndex, std::uint64_t loadId,
                                         const juce::String& bundleName, bool submitted,
                                         const juce::String& diagnostic)
{
    auto& slot = instrumentSlots[slotIndex];
    slot.loading.store(false, std::memory_order_release);
    slot.chooseButton.setEnabled(true);

    const auto name = instrumentName(slotIndex);
    if (! submitted) {
        slot.openEditorButton.setEnabled(slot.instrument->state() == fishpond::SingleChannelState::ready);
        instrumentsPanel.setText(diagnostic, juce::dontSendNotification);
        return;
    }

    slot.pendingBundleName = bundleName;
    if (audioShell.state() != fishpond::AudioShellState::running)
        commitPendingInstrumentWhileStopped(slotIndex);

    if (slot.committedLoadId.load(std::memory_order_acquire) == loadId) {
        slot.pendingBundleName.clear();
        slot.openEditorButton.setEnabled(true);
        instrumentsPanel.setText(name + " ready: " + bundleName, juce::dontSendNotification);
    } else {
        instrumentsPanel.setText(name + " prepared: swaps at the next audio block", juce::dontSendNotification);
    }
}

void MainComponent::commitPendingInstrumentWhileStopped(std::size_t slotIndex)
{
    auto& slot = instrumentSlots[slotIndex];
    fishpond::PreparedGraphCommand<juce::AudioProcessor> command;
    while (slot.handoff.tryTake(command)) {
        juce::AudioProcessor* retired = nullptr;
        std::string diagnostic;
        if (slot.instrument->applyRawPreparedAtBlockBoundary(command.graph, command.configurationVersion,
                                                              retired, diagnostic)) {
            std::unique_ptr<juce::AudioProcessor> retiredOwner(retired);
            slot.committedLoadId.store(command.commandId, std::memory_order_release);
        } else {
            std::unique_ptr<juce::AudioProcessor> rejectedOwner(command.graph);
            slot.rejectedLoadId.store(command.commandId, std::memory_order_release);
        }
    }
}

fishpond::AudioConfiguration MainComponent::currentAudioConfiguration() const noexcept
{
    for (;;) {
        const auto before = audioConfigurationVersion.load(std::memory_order_acquire);
        if ((before & 1U) != 0)
            continue;
        const auto sampleRate = activeSampleRate.load(std::memory_order_acquire);
        const auto blockSize = activeBlockSize.load(std::memory_order_acquire);
        if (before == audioConfigurationVersion.load(std::memory_order_acquire))
            return { sampleRate, static_cast<int>(blockSize), before };
    }
}

void MainComponent::openInstrumentEditor(std::size_t slotIndex)
{
    auto& slot = instrumentSlots[slotIndex];
    if (slot.editorWindow != nullptr) {
        slot.editorWindow->setVisible(true);
        slot.editorWindow->toFront(true);
        return;
    }
    auto* processor = slot.instrument != nullptr ? slot.instrument->activeProcessorForEditor() : nullptr;
    const auto name = instrumentName(slotIndex);
    if (processor == nullptr || slot.instrument->state() != fishpond::SingleChannelState::ready) {
        instrumentsPanel.setText("Load " + name + " before opening its UI", juce::dontSendNotification);
        return;
    }
    if (! processor->hasEditor()) {
        instrumentsPanel.setText("This VST3 does not provide a plugin UI", juce::dontSendNotification);
        return;
    }
    slot.editorWindow = std::make_unique<PluginEditorWindow>(*processor);
    slot.editorWindow->setVisible(true);
    slot.editorWindow->toFront(true);
}

void MainComponent::renameInstrumentChannel(std::size_t slotIndex)
{
    auto& slot = instrumentSlots[slotIndex];
    if (! channels.rename(slot.channelId, slot.channelName.getText().toStdString())) {
        instrumentsPanel.setText("FP_CHANNEL_NAME_INVALID: choose a unique non-empty name", juce::dontSendNotification);
        return;
    }
    const auto channel = channels.resolve(slot.channelName.getText().toStdString());
    instrumentsPanel.setText("Channel renamed: " + slot.channelName.getText() + " (target=\""
                                 + juce::String(channel->alias) + "\")", juce::dontSendNotification);
}

void MainComponent::confirmDiscardLiveCodingChanges(const juce::String& action,
                                                    std::function<void()> continuation)
{
    if (! liveCodingDocument.hasChangedSinceSavePoint())
    {
        continuation();
        return;
    }

    juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                                       "Unsaved Live Coding text",
                                       "Discard unsaved changes and " + action + "?",
                                       "Discard", "Cancel", this,
                                       juce::ModalCallbackFunction::create(
                                           [continuation = std::move(continuation)] (int result) mutable {
                                               if (result != 0)
                                                   continuation();
                                           }));
}

void MainComponent::newLiveCodingFile()
{
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    confirmDiscardLiveCodingChanges("create a new file", [safeThis] {
        if (safeThis == nullptr)
            return;
        safeThis->liveCodingEditor.loadContent({});
        safeThis->liveCodingDocument.setSavePoint();
        safeThis->liveCodingFile = juce::File();
        safeThis->diagnostics.setText("New Live Coding file", juce::dontSendNotification);
    });
}

void MainComponent::openLiveCodingFile()
{
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    confirmDiscardLiveCodingChanges("open another file", [safeThis] {
        if (safeThis == nullptr)
            return;
        const auto initialFolder = safeThis->liveCodingFile.existsAsFile()
            ? safeThis->liveCodingFile.getParentDirectory()
            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        safeThis->liveCodingFileChooser = std::make_unique<juce::FileChooser>("Open Fishpond Live Coding file", initialFolder, "*.fp");
        safeThis->liveCodingFileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                                          | juce::FileBrowserComponent::canSelectFiles,
            [safeThis] (const juce::FileChooser& chooser) {
                const auto file = chooser.getResult();
                if (safeThis == nullptr)
                    return;
                safeThis->liveCodingFileChooser.reset();
                if (file != juce::File())
                    safeThis->loadLiveCodingFile(file);
            });
    });
}

void MainComponent::saveLiveCodingFile()
{
    if (liveCodingFile.existsAsFile()) {
        saveLiveCodingFileTo(liveCodingFile);
        return;
    }
    saveLiveCodingFileAs();
}

void MainComponent::saveLiveCodingFileAs()
{
    const auto initialFile = liveCodingFile.existsAsFile()
        ? liveCodingFile
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("untitled.fp");
    liveCodingFileChooser = std::make_unique<juce::FileChooser>("Save Fishpond Live Coding file", initialFile, "*.fp");
    liveCodingFileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                            | juce::FileBrowserComponent::canSelectFiles
                                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            liveCodingFileChooser.reset();
            if (file != juce::File())
                saveLiveCodingFileTo(file);
        });
}

void MainComponent::loadLiveCodingFile(const juce::File& file)
{
    auto stream = file.createInputStream();
    if (stream == nullptr) {
        diagnostics.setText("FP_FILE_OPEN_FAILED: " + file.getFullPathName(), juce::dontSendNotification);
        return;
    }

    liveCodingEditor.loadContent(stream->readEntireStreamAsString());
    liveCodingDocument.setSavePoint();
    liveCodingFile = file;
    diagnostics.setText("Opened " + file.getFileName(), juce::dontSendNotification);
}

void MainComponent::saveLiveCodingFileTo(juce::File file)
{
    if (! file.hasFileExtension("fp"))
        file = file.withFileExtension(".fp");
    if (! file.replaceWithText(liveCodingDocument.getAllContent())) {
        diagnostics.setText("FP_FILE_SAVE_FAILED: " + file.getFullPathName(), juce::dontSendNotification);
        return;
    }

    liveCodingFile = file;
    liveCodingDocument.setSavePoint();
    diagnostics.setText("Saved " + file.getFileName(), juce::dontSendNotification);
}

MainComponent::~MainComponent()
{
    stopTimer();
    liveCodingEditor.removeKeyListener(this);
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    for (std::size_t slotIndex = 0; slotIndex < instrumentSlots.size(); ++slotIndex) {
        auto& slot = instrumentSlots[slotIndex];
        commitPendingInstrumentWhileStopped(slotIndex);
        while (slot.handoff.reclaimOnWorker() != nullptr) {}
    }
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
    for (std::size_t slotIndex = 0; slotIndex < instrumentSlots.size(); ++slotIndex) {
        auto& slot = instrumentSlots[slotIndex];
        while (slot.handoff.reclaimOnWorker() != nullptr) {}

        if (audioShell.state() != fishpond::AudioShellState::running)
            commitPendingInstrumentWhileStopped(slotIndex);

        if (slot.pendingBundleName.isEmpty())
            continue;

        const auto loadId = slot.submittedLoadId.load(std::memory_order_acquire);
        const auto name = instrumentName(slotIndex);
        if (slot.committedLoadId.load(std::memory_order_acquire) >= loadId) {
            instrumentsPanel.setText(name + " ready: " + slot.pendingBundleName, juce::dontSendNotification);
            slot.pendingBundleName.clear();
            slot.openEditorButton.setEnabled(true);
        } else if (slot.rejectedLoadId.load(std::memory_order_acquire) >= loadId) {
            instrumentsPanel.setText("FP_GRAPH_STALE: retry loading " + name, juce::dontSendNotification);
            slot.pendingBundleName.clear();
            slot.openEditorButton.setEnabled(slot.instrument->state() == fishpond::SingleChannelState::ready);
        }
    }

    fishpond::PythonExecutionCompletion completion;
    while (pythonWorker.tryTakeCompletion(completion)) {
        const auto command = juce::String(completion.source).trim();

        const auto showConsole = [this, &completion] (juce::String fallback) {
            auto output = juce::String(completion.result.consoleOutput);
            if (output.isEmpty())
                output = std::move(fallback);

            diagnostics.setText(output, juce::dontSendNotification);
            diagnostics.moveCaretToEnd(false);
        };

        if (! completion.result.accepted) {
            showConsole(completion.result.diagnostic);
            continue;
        }
        if (completion.result.changedTempoBpm) {
            tempoBpm = *completion.result.changedTempoBpm;
            tempoLabel.setText("clock.bpm = " + juce::String(tempoBpm, 0),
                               juce::dontSendNotification);
            updateSchedulerTiming();
        }
        if (completion.result.changedMasterVolumeDb) {
            const auto volumeDb = *completion.result.changedMasterVolumeDb;
            masterVolumeSlider.setValue(volumeDb, juce::dontSendNotification);
            masterGain.store(juce::Decibels::decibelsToGain(static_cast<float>(volumeDb)),
                             std::memory_order_release);
        }
        juce::StringArray commandLines;
        commandLines.addLines(juce::String(completion.source));
        bool globalStopRequested {};
        bool hasNonControlLine {};
        std::vector<std::size_t> stoppedPlayers;
        for (const auto& line : commandLines) {
            const auto trimmedLine = line.trim();
            if (trimmedLine.isEmpty() || trimmedLine.startsWithChar('#'))
                continue;
            if (trimmedLine == "silence()" || trimmedLine == "panic()") {
                globalStopRequested = true;
                continue;
            }
            if (const auto playerIndex = singlePlayerStopIndex(trimmedLine)) {
                stoppedPlayers.push_back(*playerIndex);
                continue;
            }
            hasNonControlLine = true;
        }
        if (! hasNonControlLine && globalStopRequested) {
            bassScheduler.clear();
            showConsole(command == "panic()" ? "Panic: cleared instrument events" : "Silenced active players");
            continue;
        }
        if (! hasNonControlLine && ! stoppedPlayers.empty()) {
            for (const auto playerIndex : stoppedPlayers)
                bassScheduler.remove(playerIndex);
            showConsole("Silenced " + juce::String(static_cast<int>(stoppedPlayers.size()))
                        + (stoppedPlayers.size() == 1 ? " player" : " players"));
            continue;
        }
        if (completion.source.find(">> n(") == std::string::npos) {
            showConsole(completion.result.diagnostic);
            continue;
        }

        std::vector<std::uint64_t> readyChannelIds;
        for (const auto& slot : instrumentSlots)
            if (slot.instrument != nullptr && slot.instrument->state() == fishpond::SingleChannelState::ready)
                readyChannelIds.push_back(slot.channelId);
        std::vector<PendingInstrumentPattern> patterns;
        juce::StringArray lines;
        lines.addLines(juce::String(completion.source));
        for (const auto& line : lines) {
            const auto patternSource = line.toStdString();
            if (patternSource.find(">> n(") == std::string::npos)
                continue;
            const auto validation = runtime.evaluateEditorText(patternSource, channels, readyChannelIds);
            if (! validation.accepted) {
                showConsole(validation.diagnostic);
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
                showConsole("FP_PATTERN_VALUE_INVALID: player, p, dur, and velocity must be valid");
                patterns.clear();
                break;
            }
            if (! channel) {
                showConsole("FP_TARGET_UNAVAILABLE: target has no ready instrument channel");
                patterns.clear();
                break;
            }
            if (noteSteps.empty()) {
                showConsole("FP_PATTERN_VALUE_INVALID: notes must use C3 or {C3 E3 G3} chord groups");
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
        showConsole("Queued " + juce::String(static_cast<int>(patterns.size()))
                    + (patterns.size() == 1 ? " instrument pattern for the next bar"
                                            : " instrument patterns for the next bar"));
    }
}

juce::Range<int> MainComponent::editorEvaluationRange() const
{
    const auto selectedRange = liveCodingEditor.getHighlightedRegion();
    const auto caretPosition = liveCodingEditor.getCaretPosition();
    const auto startPosition = juce::CodeDocument::Position(liveCodingDocument,
                                                             selectedRange.isEmpty() ? caretPosition : selectedRange.getStart());
    const auto endPosition = juce::CodeDocument::Position(liveCodingDocument,
                                                           selectedRange.isEmpty() ? caretPosition : selectedRange.getEnd());
    const auto firstLine = startPosition.getLineNumber();
    const auto lastLine = endPosition.getLineNumber();
    const auto rangeStart = juce::CodeDocument::Position(liveCodingDocument, firstLine, 0).getPosition();
    const auto rangeEnd = juce::CodeDocument::Position(liveCodingDocument, lastLine,
                                                        liveCodingDocument.getLine(lastLine).length()).getPosition();
    return { rangeStart, rangeEnd };
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* origin)
{
    if (origin != &liveCodingEditor)
        return false;

    const auto modifiers = key.getModifiers();
    if (modifiers.isCommandDown()) {
        const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
        if (character == 'n') {
            newLiveCodingFile();
            return true;
        }
        if (character == 'o') {
            openLiveCodingFile();
            return true;
        }
        if (character == 's') {
            if (modifiers.isShiftDown())
                saveLiveCodingFileAs();
            else
                saveLiveCodingFile();
            return true;
        }
    }

    if (! key.isKeyCode(juce::KeyPress::returnKey))
        return false;
    const auto shiftDown = key.getModifiers().isShiftDown()
                        || juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    if (! shiftDown)
        return false;
    const auto range = editorEvaluationRange();
    const auto source = liveCodingEditor.getTextInRange(range);
    liveCodingEditor.flashRange(range);
    executeEditorText(source);
    return true;
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto transport = area.removeFromTop(34);
    startStopButton.setBounds(transport.removeFromRight(120));
    transport.removeFromRight(28); // Fixed space separates the destructive audio action from level controls.
    masterVolumeSlider.setBounds(transport.removeFromRight(190));
    masterVolumeLabel.setBounds(transport.removeFromRight(105));
    transport.removeFromRight(18);
    tempoLabel.setBounds(transport.removeFromRight(165));
    deviceStatus.setBounds(transport);
    tabs.setBounds(area);
    auto liveArea = liveCodingPanel.getLocalBounds().reduced(8);
    diagnostics.setBounds(liveArea.removeFromBottom(84));
    liveCodingEditor.setBounds(liveArea);
    auto instrumentArea = instrumentsPanel.getLocalBounds().reduced(8);
    for (auto& slot : instrumentSlots) {
        auto actions = instrumentArea.removeFromTop(32);
        slot.chooseButton.setBounds(actions.removeFromLeft(160));
        slot.openEditorButton.setBounds(actions.removeFromLeft(145));
        slot.channelName.setBounds(actions.removeFromLeft(180));
        slot.renameButton.setBounds(actions.removeFromLeft(80));
    }
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
    audioConfigurationVersion.fetch_add(1, std::memory_order_acq_rel);
    activeSampleRate.store(device->getCurrentSampleRate(), std::memory_order_release);
    activeBlockSize.store(static_cast<std::uint32_t>(device->getCurrentBufferSizeSamples()), std::memory_order_release);
    const auto configurationVersion = audioConfigurationVersion.fetch_add(1, std::memory_order_acq_rel) + 1;
    const fishpond::AudioConfiguration configuration { device->getCurrentSampleRate(),
                                                        device->getCurrentBufferSizeSamples(), configurationVersion };
    for (auto& slot : instrumentSlots)
        slot.instrument->reconfigureForDevice(configuration);
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
                              activeBlockSize.load(std::memory_order_acquire), tempoBpm });
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
    for (auto& slot : instrumentSlots) {
        fishpond::PreparedGraphCommand<juce::AudioProcessor> command;
        while (slot.handoff.tryTake(command)) {
            juce::AudioProcessor* retired = nullptr;
            const auto committed = slot.instrument->applyRawPreparedAtBlockBoundaryNoDiagnostic(
                command.graph, command.configurationVersion, retired);
            auto* graphToRetire = committed ? retired : command.graph;
            // A slot accepts one replacement at a time, while the non-audio timer drains
            // retired graphs. Never destroy a plug-in instance from this callback.
            if (slot.handoff.retireFromAudio(graphToRetire)) {
                if (committed)
                    slot.committedLoadId.store(command.commandId, std::memory_order_release);
                else
                    slot.rejectedLoadId.store(command.commandId, std::memory_order_release);
            }
        }
    }
    for (auto& midi : channelMidi)
        midi.clear();
    const auto start = renderFrame.load();
    noteDispatcher.drain(noteQueue, start, static_cast<std::uint32_t>(samples), observedPanic,
        [this] (const fishpond::NoteEvent& event, std::uint32_t offset) {
            const auto addAllNotesOff = [offset] (juce::MidiBuffer& midi, std::uint8_t midiChannel) {
                midi.addEvent(juce::MidiMessage::allNotesOff(midiChannel), static_cast<int>(offset));
                midi.addEvent(juce::MidiMessage::allSoundOff(midiChannel), static_cast<int>(offset));
            };
            if (event.type == fishpond::NoteEventType::allNotesOff && event.channelId == 0) {
                for (auto& midi : channelMidi)
                    addAllNotesOff(midi, event.midiChannel);
                return;
            }
            std::size_t slotIndex = instrumentSlots.size();
            for (std::size_t index = 0; index < instrumentSlots.size(); ++index)
                if (instrumentSlots[index].channelId == event.channelId) {
                    slotIndex = index;
                    break;
                }
            if (slotIndex == instrumentSlots.size())
                return;
            auto& midi = channelMidi[slotIndex];
            if (event.type == fishpond::NoteEventType::noteOn)
                midi.addEvent(juce::MidiMessage::noteOn(event.midiChannel, event.midiNote,
                                                        event.velocity / 127.0f), static_cast<int>(offset));
            else if (event.type == fishpond::NoteEventType::noteOff)
                midi.addEvent(juce::MidiMessage::noteOff(event.midiChannel, event.midiNote), static_cast<int>(offset));
            else if (event.type == fishpond::NoteEventType::allNotesOff)
                addAllNotesOff(midi, event.midiChannel);
        });
    audio.clear();
    for (std::size_t index = 0; index < instrumentSlots.size(); ++index) {
        auto& instrumentAudio = channelAudio[index];
        instrumentAudio.clear();
        if (instrumentSlots[index].instrument != nullptr)
            instrumentSlots[index].instrument->process(instrumentAudio, channelMidi[index]);
        for (int channel = 0; channel < outputChannels; ++channel)
            audio.addFrom(channel, 0, instrumentAudio, channel, 0, samples);
    }
    audio.applyGain(masterGain.load(std::memory_order_acquire));
    renderFrame.store(start + samples);
}
