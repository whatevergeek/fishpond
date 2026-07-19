#include <juce_audio_utils/juce_audio_utils.h>

namespace {

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Button::Listener
{
public:
    MainComponent()
        : deviceSelector(deviceManager, 0, 0, 2, 2, false, false, true, false)
    {
        addAndMakeVisible(transportButton);
        addAndMakeVisible(statusLabel);
        addAndMakeVisible(deviceSelector);

        transportButton.setButtonText("Start transport");
        transportButton.addListener(this);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setText("Stereo master: ready", juce::dontSendNotification);

        setSize(720, 480);
        setAudioChannels(0, 2);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int, double) override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();
    }

    void releaseResources() override {}

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(16);
        auto header = bounds.removeFromTop(36);
        transportButton.setBounds(header.removeFromLeft(170));
        statusLabel.setBounds(header);
        deviceSelector.setBounds(bounds.withTrimmedTop(12));
    }

private:
    void buttonClicked(juce::Button* button) override
    {
        if (button != &transportButton)
            return;

        transportRunning = !transportRunning;
        transportButton.setButtonText(transportRunning ? "Stop transport" : "Start transport");
        statusLabel.setText(transportRunning ? "Stereo master: transport running"
                                             : "Stereo master: ready",
                            juce::dontSendNotification);
    }

    juce::TextButton transportButton;
    juce::Label statusLabel;
    juce::AudioDeviceSelectorComponent deviceSelector;
    bool transportRunning = false;
};

class FishpondApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Fishpond"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Desktop::getInstance().getDefaultLookAndFeel()
                                       .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace

START_JUCE_APPLICATION(FishpondApplication)
