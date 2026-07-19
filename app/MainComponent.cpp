#include "MainComponent.h"

MainComponent::MainComponent()
{
    liveCodingEditor.setMultiLine(true, true);
    liveCodingEditor.setReturnKeyStartsNewLine(true);
    liveCodingEditor.setText("# Ctrl+Return: evaluate block\n# Shift+Return: evaluate line\n# silence() stops active players\nPa >> n(\"C2 C3\", target=\"bass\", p=0.5)\n");
    liveCodingEditor.addKeyListener(this);
    diagnostics.setMultiLine(true);
    diagnostics.setReadOnly(true);
    diagnostics.setText("Ready. Add an instrument channel in P1.4 before notes can be heard.", juce::dontSendNotification);
    executeButton.setButtonText("Execute block (Ctrl+Return)");
    executeButton.setTooltip("Evaluate the current code block (Ctrl+Return). Shift+Return evaluates the current line.");
    executeButton.onClick = [this] { executeEditorText(currentCodeBlock()); };
    liveCodingPanel.addAndMakeVisible(liveCodingEditor);
    liveCodingPanel.addAndMakeVisible(executeButton);
    liveCodingPanel.addAndMakeVisible(diagnostics);
    mixerPlaceholder.setText("Mixer - one Bass channel arrives in P1.4", juce::dontSendNotification);
    mixerPlaceholder.setJustificationType(juce::Justification::centred);
    loadBassButton.onClick = [this] {
        if (audioShell.state() == fishpond::AudioShellState::running) {
            mixerPlaceholder.setText("Stop audio before loading the Bass fixture", juce::dontSendNotification);
            return;
        }
        bass = std::make_unique<fishpond::ControlledVST3Bass>(fishpond::AudioConfiguration { 48'000.0, 512, 1 });
        std::string diagnostic;
        const auto prepared = bass->prepareBundle(juce::File(FISHPOND_CONTROLLED_BASS_PATH), diagnostic);
        const auto committed = prepared && bass->commitAtBlockBoundary(diagnostic);
        mixerPlaceholder.setText(committed ? "Bass - controlled VST3 fixture ready" : diagnostic,
                                 juce::dontSendNotification);
    };
    mixerPlaceholder.addAndMakeVisible(loadBassButton);
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

MainComponent::~MainComponent()
{
    stopTimer();
    liveCodingEditor.removeKeyListener(this);
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    const auto& modifiers = key.getModifiers();
    if (modifiers.isCtrlDown() && key.getKeyCode() == juce::KeyPress::returnKey) {
        executeEditorText(currentCodeBlock());
        return true;
    }
    if (modifiers.isShiftDown() && key.getKeyCode() == juce::KeyPress::returnKey) {
        executeEditorText(currentLine());
        return true;
    }
    return false;
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
            const auto validation = runtime.evaluateEditorText(completion.source, bassReady);
            if (validation.accepted) {
                const auto notes = runtime.notesFromEditorText(completion.source);
                const auto periodBeats = runtime.periodBeatsFromEditorText(completion.source);
                if (! periodBeats) {
                    diagnostics.setText("FP_PATTERN_VALUE_INVALID: p must be a positive number", juce::dontSendNotification);
                    continue;
                }
                bassScheduler.replace(notes, *periodBeats);
                diagnostics.setText("Playing " + juce::String(notes.size()) + " Bass-note pattern",
                                    juce::dontSendNotification);
            } else
                diagnostics.setText(validation.diagnostic, juce::dontSendNotification);
        } else {
            diagnostics.setText(completion.result.diagnostic, juce::dontSendNotification);
        }
    }
}

juce::String MainComponent::currentCodeBlock() const
{
    const auto source = liveCodingEditor.getText();
    const auto caret = liveCodingEditor.getCaretPosition();
    const auto blockStart = source.substring(0, caret).lastIndexOf("\n\n");
    const auto blockEnd = source.indexOf(caret, "\n\n");
    return source.substring(blockStart < 0 ? 0 : blockStart + 2,
                            blockEnd < 0 ? source.length() : blockEnd);
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
    auto actions = liveArea.removeFromTop(32);
    executeButton.setBounds(actions.removeFromLeft(160));
    diagnostics.setBounds(liveArea.removeFromBottom(84));
    liveCodingEditor.setBounds(liveArea);
    auto mixerArea = mixerPlaceholder.getLocalBounds().reduced(8);
    loadBassButton.setBounds(mixerArea.removeFromTop(32).removeFromLeft(160));
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
