#include "MainComponent.h"

MainComponent::MainComponent()
{
    liveCodingEditor.setMultiLine(true);
    liveCodingEditor.setText("# Ctrl+Return: evaluate block\n# Shift+Return: evaluate line\nPa >> n(\"C2 C3\", target=\"bass\", p=0.5)\n");
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
    addAndMakeVisible(tabs);
    addAndMakeVisible(deviceStatus);
    addAndMakeVisible(startStopButton);
    addAndMakeVisible(tempoLabel);
    setSize(1040, 700);
}

MainComponent::~MainComponent()
{
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
    const auto result = runtime.evaluateEditorText(source.toStdString());
    diagnostics.setText(result.diagnostic, juce::dontSendNotification);
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
    tempoLabel.setBounds(transport.removeFromLeft(90));
    startStopButton.setBounds(transport.removeFromRight(120));
    tabs.setBounds(area);
    auto liveArea = liveCodingPanel.getLocalBounds().reduced(8);
    auto actions = liveArea.removeFromTop(32);
    executeButton.setBounds(actions.removeFromLeft(160));
    diagnostics.setBounds(liveArea.removeFromBottom(84));
    liveCodingEditor.setBounds(liveArea);
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
    deviceStatus.setText("Running " + device->getName() + " — "
                             + juce::String(device->getCurrentSampleRate(), 0) + " Hz, "
                             + juce::String(device->getCurrentBufferSizeSamples()) + " samples",
                         juce::dontSendNotification);
}

void MainComponent::audioDeviceStopped()
{
    audioShell.stop();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output,
                                                      int outputChannels, int samples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    for (int channel = 0; channel < outputChannels; ++channel)
        if (output[channel] != nullptr)
            juce::FloatVectorOperations::clear(output[channel], samples);
}
