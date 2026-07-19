#include "MainComponent.h"

MainComponent::MainComponent()
{
    liveCodingEditor.setMultiLine(true);
    liveCodingEditor.setText("# P1.1 audio shell\n# Live coding arrives in P1.2\n");
    mixerPlaceholder.setText("Mixer — one Bass channel arrives in P1.4", juce::dontSendNotification);
    mixerPlaceholder.setJustificationType(juce::Justification::centred);
    tabs.addTab("Live Coding", juce::Colours::darkgrey, &liveCodingEditor, false);
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
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto transport = area.removeFromTop(34);
    deviceStatus.setBounds(transport.removeFromLeft(500));
    tempoLabel.setBounds(transport.removeFromLeft(90));
    startStopButton.setBounds(transport.removeFromRight(120));
    tabs.setBounds(area);
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
