#include "MainComponent.h"

#include <algorithm>

// Each message owns its document and controls for the lifetime of the conversation.
class AssistantChatTranscript final : public juce::Component {
    class Message final : public juce::Component {
    public:
        Message(juce::String text, bool isUser) : body(std::move(text)), user(isUser), editor(document, &tokeniser)
        {
            editor.setReadOnly(true);
            editor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            editor.setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xff11181d));
            addChildComponent(editor);
            addChildComponent(copy);
            addChildComponent(insert);
            copy.setTooltip("Copy this code block");
            insert.setTooltip("Insert this code at the Live Coding cursor; does not run it");
            copy.onClick = [this] { juce::SystemClipboard::copyTextToClipboard(snippet); copy.setButtonText("Copied"); };
            insert.onClick = [this] { if (onInsert) onInsert(snippet); };
        }

        void resolve(const juce::String& text, const juce::String& code)
        {
            pending = false;
            body = text;
            snippet = code;
            document.replaceAllContent(code);
            editor.setVisible(code.isNotEmpty());
            copy.setVisible(code.isNotEmpty());
            insert.setVisible(code.isNotEmpty());
        }

        bool isUserMessage() const { return user; }
        bool isPending() const { return pending; }
        juce::String historyContent() const
        {
            auto content = body;
            if (snippet.isNotEmpty())
                content += "\n```python\n" + snippet + "\n```";
            return content;
        }

        int measure(int width)
        {
            const int inset = user ? juce::jmin(70, width / 5) : 0;
            bubble = { inset + 6, 0, width - inset - 12, 0 };
            juce::AttributedString attributed;
            attributed.append(body, juce::Font(14.0f), juce::Colour(0xffe0e7eb));
            layout.createLayout(attributed, static_cast<float>(juce::jmax(30, bubble.getWidth() - 24)));
            textHeight = juce::jmax(22, static_cast<int>(std::ceil(layout.getHeight())));
            codeHeight = snippet.isEmpty() ? 0 : juce::jlimit(66, 240, document.getNumLines() * 18 + 26);
            const int height = 30 + textHeight + (codeHeight ? 40 + codeHeight : 10);
            bubble.setHeight(height);
            return height + 16;
        }

        void resized() override
        {
            const int top = 30 + textHeight;
            copy.setBounds(bubble.getRight() - 152, top + 5, 64, 26);
            insert.setBounds(bubble.getRight() - 82, top + 5, 70, 26);
            editor.setBounds(bubble.getX() + 10, top + 36, bubble.getWidth() - 20, codeHeight);
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colour(user ? 0xff283e49 : 0xff20282e));
            g.fillRoundedRectangle(bubble.toFloat(), 10.0f);
            g.setColour(juce::Colour(0xff8fa7b3));
            g.setFont(11.0f);
            g.drawText(user ? "YOU" : pending ? "FISHPOND  /  THINKING" : "FISHPOND", bubble.getX() + 12, 8, bubble.getWidth() - 60, 16, juce::Justification::centredLeft);
            layout.draw(g, juce::Rectangle<float>(static_cast<float>(bubble.getX() + 12), 30.0f,
                                                static_cast<float>(bubble.getWidth() - 24), static_cast<float>(textHeight)));
            if (pending) {
                for (int i = 0; i < 3; ++i) {
                    g.setColour(juce::Colour(0xff8fa7b3).withAlpha(i == phase % 3 ? 0.85f : 0.35f));
                    g.fillEllipse(static_cast<float>(bubble.getRight() - 42 + i * 10), 13.0f, 5.0f, 5.0f);
                }
            }
        }

        std::function<void(const juce::String&)> onInsert;
        bool pending = false;
        int phase = 0;
    private:
        juce::String body, snippet;
        bool user;
        juce::TextLayout layout;
        juce::Rectangle<int> bubble;
        int textHeight = 0, codeHeight = 0;
        juce::CodeDocument document;
        FishpondCodeTokeniser tokeniser;
        juce::CodeEditorComponent editor;
        juce::TextButton copy { "Copy" }, insert { "Insert" };
    };
public:
    AssistantChatTranscript()
    {
        addMessage("Make something musical. Ask for a pattern, explore your code, or paste an error. Suggestions stay here until you choose to insert them.", false);
        firstConversationMessage = messages.size();
    }
    std::function<void(const juce::String&)> onApply;
    std::vector<fishpond::AssistantChatTurn> recentHistory() const
    {
        constexpr std::size_t maxMessages = 12;
        constexpr int maxCharacters = 16000;
        std::vector<fishpond::AssistantChatTurn> history;
        int characters = 0;

        for (auto index = messages.size(); index > firstConversationMessage && history.size() < maxMessages;) {
            --index;
            const auto& message = messages[index];
            if (message->isPending())
                continue;

            const auto content = message->historyContent().trim();
            if (content.isEmpty())
                continue;
            if (characters + content.length() > maxCharacters)
                break;

            history.push_back({ message->isUserMessage() ? "user" : "assistant", content });
            characters += content.length();
        }
        std::reverse(history.begin(), history.end());
        return history;
    }
    void beginTurn(const juce::String& user)
    {
        addMessage(user, true);
        addMessage("Working on your request...", false);
        messages.back()->pending = true;
        refresh();
    }
    void animatePending(int phase)
    {
        if (!messages.empty() && messages.back()->pending) {
            messages.back()->phase = phase;
            messages.back()->repaint();
        }
    }
    void resolveLatest(const juce::String& text, const juce::String& snippet)
    {
        if (!messages.empty()) messages.back()->resolve(text, snippet);
        refresh();
    }
    void clear()
    {
        for (const auto& message : messages)
            removeChildComponent(message.get());
        messages.clear();
        addMessage("Make something musical. Ask for a pattern, explore your code, or paste an error. Suggestions stay here until you choose to insert them.", false);
        firstConversationMessage = messages.size();
    }
    void setAvailableWidth(int width)
    {
        availableWidth = juce::jmax(180, width);
        refresh();
    }
private:
    void addMessage(const juce::String& text, bool user)
    {
        auto message = std::make_unique<Message>(text, user);
        message->onInsert = [this](const juce::String& snippet) { if (onApply) onApply(snippet); };
        addAndMakeVisible(*message);
        messages.push_back(std::move(message));
        refresh();
    }
    void refresh()
    {
        int y = 12;
        for (auto& message : messages) {
            const int height = message->measure(availableWidth);
            message->setBounds(0, y, availableWidth, height);
            message->resized();
            message->repaint();
            y += height;
        }
        setSize(availableWidth, y);
    }
    std::vector<std::unique_ptr<Message>> messages;
    std::size_t firstConversationMessage {};
    int availableWidth = 300;
};

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
    liveCodingEditor.setClearOutputHandler([this] { clearOutput(); });
    diagnostics.setMultiLine(true);
    diagnostics.setReadOnly(true);
    diagnostics.setScrollbarsShown(true);
    diagnostics.setText("Ready. Load an instrument in Instruments, then evaluate a pattern.", juce::dontSendNotification);
    liveCodingPanel.addAndMakeVisible(liveCodingEditor);
    liveCodingPanel.addAndMakeVisible(diagnostics);
    assistantToggleButton.onClick = [this] {
        assistantPanelVisible = ! assistantPanelVisible;
        assistantPanel.setVisible(assistantPanelVisible);
        assistantToggleButton.setButtonText(assistantPanelVisible ? "Hide Assistant" : "Show Assistant");
        resized();
    };
    liveCodingPanel.addAndMakeVisible(assistantToggleButton);

    assistantProvider.addItem("OpenAI Responses API", 1);
    assistantProvider.addItem("Ollama local server", 2);
    assistantProvider.addItem("Local Responses API (Codex Bridge)", 3);
    assistantProvider.setSelectedId(2, juce::dontSendNotification);
    assistantProvider.onChange = [this] { updateAssistantProviderFields(); };
    assistantEndpoint.setTextToShowWhenEmpty("Provider endpoint", juce::Colours::grey);
    assistantModel.setTextToShowWhenEmpty("Model", juce::Colours::grey);
    assistantModelChoices.setTextWhenNothingSelected("Choose an installed Ollama model");
    assistantModelChoices.onChange = [this] {
        if (assistantModelChoices.getSelectedId() > 0)
            assistantModel.setText(assistantModelChoices.getText(), juce::dontSendNotification);
    };
    assistantApiKey.setTextToShowWhenEmpty("Uses FISHPOND_OPENAI_API_KEY or OPENAI_API_KEY when blank", juce::Colours::grey);
    assistantApiKey.setPasswordCharacter('*');
    assistantPrompt.setMultiLine(true);
    assistantPrompt.setReturnKeyStartsNewLine(true);
    assistantPrompt.addKeyListener(this);
    assistantPrompt.onTextChange = [this] {
        assistantGenerateButton.setEnabled(!assistantRequestInFlight && assistantPrompt.getText().trim().isNotEmpty());
    };
    assistantGenerateButton.setEnabled(false);
    assistantPromptLabel.setText("Message  /  Cmd+Return to send", juce::dontSendNotification);
    assistantPrompt.setTextToShowWhenEmpty("Ask for a pattern, explain an error, or request a fix...", juce::Colours::grey);
    assistantGenerateButton.onClick = [this] { requestAssistantSuggestion(); };
    assistantNewChatButton.setTooltip("Clear this conversation and start a new chat");
    assistantNewChatButton.onClick = [this] {
        if (assistantRequestInFlight)
            return;
        assistantConversation->clear();
        assistantPrompt.clear();
        assistantStatus.setText("Ready", juce::dontSendNotification);
        assistantStatus.setTooltip({});
        assistantGenerateButton.setEnabled(false);
        assistantConversationViewport.setViewPosition(0, 0);
    };
    assistantStatus.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    assistantRefreshModelsButton.onClick = [this] { requestOllamaModels(); };
    assistantConversation = std::make_unique<AssistantChatTranscript>();
    assistantConversation->onApply = [this](const juce::String& code) { insertAssistantSuggestion(code); };
    assistantTabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff171e23));
    assistantPrompt.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202b32));
    assistantPrompt.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff435b68));
    assistantPrompt.setIndents(10, 8);
    for (auto* component : { static_cast<juce::Component*>(&assistantProviderLabel),
                             static_cast<juce::Component*>(&assistantProvider),
                             static_cast<juce::Component*>(&assistantEndpointLabel),
                             static_cast<juce::Component*>(&assistantEndpoint),
                             static_cast<juce::Component*>(&assistantModelLabel),
                             static_cast<juce::Component*>(&assistantModel),
                             static_cast<juce::Component*>(&assistantModelChoices),
                             static_cast<juce::Component*>(&assistantApiKeyLabel),
                             static_cast<juce::Component*>(&assistantApiKey),
                             static_cast<juce::Component*>(&assistantApiKey) })
        assistantConfigPanel.addAndMakeVisible(*component);
    assistantConfigPanel.addAndMakeVisible(assistantRefreshModelsButton);
    assistantChatPanel.addAndMakeVisible(assistantConversationViewport);
    assistantConversationViewport.setViewedComponent(assistantConversation.get(), false);
    assistantChatPanel.addAndMakeVisible(assistantNewChatButton);
    for (auto* component : { static_cast<juce::Component*>(&assistantPromptLabel),
                             static_cast<juce::Component*>(&assistantPrompt),
                             static_cast<juce::Component*>(&assistantStatus),
                             static_cast<juce::Component*>(&assistantGenerateButton) })
        assistantChatPanel.addAndMakeVisible(*component);
    assistantTabs.addTab("Chat", juce::Colour(0xff171e23), &assistantChatPanel, false);
    assistantTabs.addTab("Config", juce::Colour(0xff171e23), &assistantConfigPanel, false);
    assistantPanel.addAndMakeVisible(assistantTabs);
    assistantPanelConstrainer.setMinimumSize(300, 240);
    assistantPanelConstrainer.setMaximumSize(720, 4000);
    assistantPanel.addAndMakeVisible(assistantResizeHandle);
    assistantPanel.addComponentListener(this);
    liveCodingPanel.addChildComponent(assistantPanel);
    assistantPanel.setVisible(false);
    updateAssistantProviderFields();

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

void MainComponent::loadInstrumentBundle(std::size_t slotIndex, const juce::File& bundle,
                                         const juce::MemoryBlock& pluginState)
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
        [safeThis, slotIndex, loadId, bundleName, bundle, pluginState, configuration, format]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& loadError) {
            if (safeThis == nullptr)
                return;

            std::string preparationDiagnostic;
            auto submitted = instance != nullptr;
            if (! submitted) {
                preparationDiagnostic = "FP_INSTRUMENT_LOAD: " + loadError.toStdString();
            } else {
                if (pluginState.getSize() > 0)
                    instance->setStateInformation(pluginState.getData(), static_cast<int>(pluginState.getSize()));
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

            if (submitted) {
                auto& slot = safeThis->instrumentSlots[slotIndex];
                slot.bundleFile = bundle;
                slot.savedPluginState = pluginState;
            }

            safeThis->finishInstrumentLoad(slotIndex, loadId, bundleName, submitted,
                                           juce::String(preparationDiagnostic));
        });
}

void MainComponent::openInstrumentPreset()
{
    const auto initialFile = instrumentPresetFile.existsAsFile()
        ? instrumentPresetFile
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    instrumentPresetFileChooser = std::make_unique<juce::FileChooser>(
        "Open Fishpond Instruments", initialFile, "*.fpi");
    instrumentPresetFileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                                 | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            instrumentPresetFileChooser.reset();
            if (file != juce::File())
                loadInstrumentPresetFrom(file);
        });
}

void MainComponent::saveInstrumentPreset()
{
    if (instrumentPresetFile.existsAsFile())
        saveInstrumentPresetTo(instrumentPresetFile);
    else
        saveInstrumentPresetAs();
}

void MainComponent::saveInstrumentPresetAs()
{
    const auto initialFile = instrumentPresetFile.existsAsFile()
        ? instrumentPresetFile
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
              .getChildFile("Fishpond Instruments.fpi");
    instrumentPresetFileChooser = std::make_unique<juce::FileChooser>(
        "Save Fishpond Instruments", initialFile, "*.fpi");
    instrumentPresetFileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                                 | juce::FileBrowserComponent::canSelectFiles
                                                 | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            instrumentPresetFileChooser.reset();
            if (file != juce::File())
                saveInstrumentPresetTo(file);
        });
}

void MainComponent::saveInstrumentPresetTo(juce::File file)
{
    if (! file.hasFileExtension("fpi"))
        file = file.withFileExtension(".fpi");

    juce::Array<juce::var> slots;
    for (auto& slot : instrumentSlots) {
        auto entry = juce::var(new juce::DynamicObject());
        auto* object = entry.getDynamicObject();
        object->setProperty("channelName", slot.channelName.getText());
        object->setProperty("bundlePath", slot.bundleFile.getFullPathName());

        auto state = slot.savedPluginState;
        if (slot.instrument != nullptr && slot.instrument->state() == fishpond::SingleChannelState::ready) {
            if (auto* processor = slot.instrument->activeProcessorForEditor()) {
                juce::MemoryBlock currentState;
                processor->getStateInformation(currentState);
                if (currentState.getSize() > 0)
                    state = std::move(currentState);
            }
        }
        object->setProperty("pluginState", state.toBase64Encoding());
        slot.savedPluginState = state;
        slots.add(entry);
    }

    auto preset = juce::var(new juce::DynamicObject());
    auto* object = preset.getDynamicObject();
    object->setProperty("format", "Fishpond Instruments");
    object->setProperty("version", 1);
    object->setProperty("slots", juce::var(slots));
    if (! file.replaceWithText(juce::JSON::toString(preset, true))) {
        instrumentsPanel.setText("Could not save instrument setup: " + file.getFileName(), juce::dontSendNotification);
        return;
    }
    instrumentPresetFile = file;
    instrumentsPanel.setText("Saved instrument setup: " + file.getFileName(), juce::dontSendNotification);
}

void MainComponent::loadInstrumentPresetFrom(const juce::File& file)
{
    juce::var preset;
    const auto parseResult = juce::JSON::parse(file.loadFileAsString(), preset);
    const auto* presetObject = preset.getDynamicObject();
    if (parseResult.failed() || presetObject == nullptr
        || presetObject->getProperty("format").toString() != "Fishpond Instruments") {
        instrumentsPanel.setText("Could not read Fishpond instrument setup: " + file.getFileName(), juce::dontSendNotification);
        return;
    }

    const auto slotData = presetObject->getProperty("slots");
    const auto* entries = slotData.getArray();
    if (entries == nullptr || entries->size() != static_cast<int>(instrumentSlotCount)) {
        instrumentsPanel.setText("Instrument setup must contain four slots", juce::dontSendNotification);
        return;
    }

    instrumentPresetFile = file;
    for (int index = 0; index < entries->size(); ++index) {
        auto& slot = instrumentSlots[static_cast<std::size_t>(index)];
        const auto& entry = entries->getReference(index);
        const auto* entryObject = entry.getDynamicObject();
        if (entryObject == nullptr)
            continue;
        const auto channelName = entryObject->getProperty("channelName").toString().trim();
        if (channelName.isNotEmpty()) {
            slot.channelName.setText(channelName, juce::dontSendNotification);
            if (! channels.rename(slot.channelId, channelName.toStdString())) {
                instrumentsPanel.setText("Could not restore duplicate/invalid channel name: " + channelName,
                                         juce::dontSendNotification);
                continue;
            }
        }

        const juce::File bundle(entryObject->getProperty("bundlePath").toString());
        if (bundle == juce::File())
            continue;
        if (! bundle.exists()) {
            instrumentsPanel.setText("VST3 not found: " + bundle.getFullPathName(), juce::dontSendNotification);
            continue;
        }

        juce::MemoryBlock pluginState;
        const auto encodedState = entryObject->getProperty("pluginState").toString();
        if (encodedState.isNotEmpty() && ! pluginState.fromBase64Encoding(encodedState)) {
            instrumentsPanel.setText("Invalid plugin state for slot " + juce::String(index + 1),
                                     juce::dontSendNotification);
            continue;
        }
        loadInstrumentBundle(static_cast<std::size_t>(index), bundle, pluginState);
    }
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

void MainComponent::clearOutput()
{
    diagnostics.setText({}, juce::dontSendNotification);
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
    assistantPanel.removeComponentListener(this);
    assistantConversationViewport.setViewedComponent(nullptr, false);
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
    const auto contextNow = juce::Time::getMillisecondCounterHiRes();
    if (assistantPanelVisible && contextNow - assistantContextUpdatedAt >= 250.0) {
        assistantContextUpdatedAt = contextNow;
        const auto context = assistantSessionContext();
        assistantPromptLabel.setText(context.summary, juce::dontSendNotification);
        assistantPromptLabel.setTooltip(context.editorContext + "\n\n" + context.playbackContext);
    }

    // One quiet activity indicator; keep the composer and its actions stationary.
    if (assistantRequestInFlight && (++assistantThinkingTick % 15 == 0))
        assistantConversation->animatePending(assistantThinkingTick / 15);

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

fishpond::AssistantSessionContext MainComponent::assistantSessionContext() const
{
    fishpond::AssistantSessionContext context;
    context.tempoBpm = tempoBpm;
    context.liveCode = liveCodingDocument.getAllContent();
    context.diagnostics = diagnostics.getText();
    const auto caret = juce::CodeDocument::Position(liveCodingDocument, liveCodingEditor.getCaretPosition());
    const auto selected = liveCodingEditor.getHighlightedRegion();
    const auto start = juce::CodeDocument::Position(liveCodingDocument, selected.getStart());
    const auto end = juce::CodeDocument::Position(liveCodingDocument, selected.getEnd());
    context.editorContext = "Cursor (1-based): line " + juce::String(caret.getLineNumber() + 1)
        + ", column " + juce::String(caret.getIndexInLine() + 1)
        + "\nCursor line: " + liveCodingDocument.getLine(caret.getLineNumber())
        + "\nSelection: " + (selected.isEmpty() ? juce::String("none")
            : "lines " + juce::String(start.getLineNumber() + 1) + "-" + juce::String(end.getLineNumber() + 1)
                + "\n" + context.liveCode.substring(selected.getStart(), selected.getEnd()));
    const bool running = audioShell.state() == fishpond::AudioShellState::running;
    const auto snapshot = bassScheduler.snapshot();
    juce::StringArray playing, queued;
    context.playbackContext = running ? "Audio engine: running\n" : "Audio engine: stopped (no audible playback)\n";
    context.playbackContext += "Unprocessed scheduler commands: " + juce::String(static_cast<int>(snapshot.unprocessedCommands))
        + "\nScheduler-accepted patterns; MIDI notes use -1 for rests.\n";
    for (const auto& entry : snapshot.patterns) {
        const auto& pattern = entry.pattern;
        const auto player = "P" + juce::String::charToString(static_cast<juce::juce_wchar>('a' + pattern.playerIndex));
        const bool pending = snapshot.renderFrame < entry.startFrame;
        if (pending) queued.add(player);
        else if (running) playing.add(player);
        juce::String target = "channel " + juce::String(static_cast<juce::int64>(pattern.channelId));
        for (const auto& channel : channels.channels())
            if (channel.id == pattern.channelId) target = channel.alias;
        juce::StringArray steps;
        for (const auto& step : pattern.noteSteps) {
            juce::StringArray notes;
            for (const auto note : step) notes.add(juce::String(note));
            steps.add("[" + notes.joinIntoString(",") + "]");
        }
        context.playbackContext += player + " | " + (pending ? "queued for next bar" : running ? "active" : "paused")
            + " | target=" + target + " | MIDI steps=[" + steps.joinIntoString(",") + "]"
            + " | p=" + juce::String(pattern.periodBeats) + " | dur=" + juce::String(pattern.durationBeats)
            + " | velocity=" + juce::String(static_cast<int>(pattern.velocity)) + "\n";
    }
    context.playbackContext += "Active means scheduled playback, not proof of audible sound. Already queued notes may finish after replacement or stop.\n";
    context.summary = (selected.isEmpty() ? "Cursor L" + juce::String(caret.getLineNumber() + 1)
        : "Selected L" + juce::String(start.getLineNumber() + 1) + "-" + juce::String(end.getLineNumber() + 1))
        + " | " + (running ? "Playing: " + (playing.isEmpty() ? juce::String("none") : playing.joinIntoString(",")) : juce::String("Audio stopped"))
        + (queued.isEmpty() ? juce::String() : " | Queued: " + queued.joinIntoString(","));

    const auto& channelEntries = channels.channels();
    for (std::size_t slotIndex = 0; slotIndex < instrumentSlots.size() && slotIndex < channelEntries.size(); ++slotIndex)
        if (instrumentSlots[slotIndex].instrument != nullptr
            && instrumentSlots[slotIndex].instrument->state() == fishpond::SingleChannelState::ready)
        {
            context.readyTargets.push_back(channelEntries[slotIndex].alias);
            if (auto* processor = instrumentSlots[slotIndex].instrument->activeProcessorForEditor())
                context.playbackContext += "Ready instrument: " + juce::String(channelEntries[slotIndex].alias)
                    + " | plugin=" + processor->getName() + "\n";
        }
    return context;
}

void MainComponent::updateAssistantProviderFields()
{
    const auto selectedProvider = assistantProvider.getSelectedId();
    const auto isOllama = selectedProvider == 2;
    const auto isLocalResponses = selectedProvider == 3;
    const auto provider = isOllama ? fishpond::AssistantProvider::ollama
        : isLocalResponses ? fishpond::AssistantProvider::localResponses
        : fishpond::AssistantProvider::openAI;
    assistantEndpoint.setText(fishpond::AssistantGateway::defaultEndpoint(provider), juce::dontSendNotification);
    assistantModel.setText(fishpond::AssistantGateway::defaultModel(provider), juce::dontSendNotification);
    assistantModel.setVisible(! isOllama);
    assistantModelChoices.setVisible(isOllama);
    assistantRefreshModelsButton.setVisible(isOllama);
    assistantApiKey.setEnabled(! isOllama && ! isLocalResponses);
    assistantApiKeyLabel.setEnabled(! isOllama && ! isLocalResponses);
    assistantApiKey.setTextToShowWhenEmpty(isLocalResponses
        ? "Not required for Codex Bridge"
        : "Uses FISHPOND_OPENAI_API_KEY or OPENAI_API_KEY when blank", juce::Colours::grey);
    if (isOllama) {
        assistantApiKey.setText({}, juce::dontSendNotification);
        requestOllamaModels();
    } else if (isLocalResponses) {
        assistantApiKey.setText({}, juce::dontSendNotification);
    }
    resized();
}

void MainComponent::requestOllamaModels()
{
    if (assistantProvider.getSelectedId() != 2)
        return;

    const auto requestId = ++assistantModelListRequestId;
    assistantRefreshModelsButton.setEnabled(false);
    assistantModelChoices.clear(juce::dontSendNotification);
    assistantModelChoices.setTextWhenNothingSelected("Loading installed Ollama models...");
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    assistantGateway.listOllamaModelsAsync(assistantEndpoint.getText(),
        [safeThis, requestId] (std::vector<juce::String> models, juce::String diagnostic) {
            if (safeThis == nullptr || safeThis->assistantProvider.getSelectedId() != 2
                || safeThis->assistantModelListRequestId != requestId)
                return;

            safeThis->assistantRefreshModelsButton.setEnabled(true);
            safeThis->assistantModelChoices.clear(juce::dontSendNotification);
            for (std::size_t index = 0; index < models.size(); ++index)
                safeThis->assistantModelChoices.addItem(models[index], static_cast<int>(index + 1));

            if (models.empty()) {
                safeThis->assistantModelChoices.setTextWhenNothingSelected(diagnostic);
                safeThis->assistantModelChoices.setTooltip(diagnostic);
                return;
            }

            const auto currentModel = safeThis->assistantModel.getText();
            const auto selectedIndex = std::find(models.begin(), models.end(), currentModel);
            const auto selection = selectedIndex == models.end()
                ? 1
                : static_cast<int>(std::distance(models.begin(), selectedIndex) + 1);
            safeThis->assistantModelChoices.setSelectedId(selection, juce::sendNotification);
            safeThis->assistantModelChoices.setTooltip(juce::String(static_cast<int>(models.size()))
                                                           + " installed Ollama models available");
        });
}

void MainComponent::requestAssistantSuggestion()
{
    if (assistantRequestInFlight || assistantPrompt.getText().trim().isEmpty())
        return;
    const auto submittedPrompt = assistantPrompt.getText().trim();
    const auto conversationHistory = assistantConversation->recentHistory();

    const auto selectedProvider = assistantProvider.getSelectedId();
    const auto provider = selectedProvider == 2 ? fishpond::AssistantProvider::ollama
        : selectedProvider == 3 ? fishpond::AssistantProvider::localResponses
        : fishpond::AssistantProvider::openAI;
    fishpond::AssistantGatewayConfig config { provider, assistantEndpoint.getText(), assistantModel.getText(),
                                              assistantApiKey.getText() };
    assistantRequestInFlight = true;
    assistantNewChatButton.setEnabled(false);
    assistantThinkingTick = 0;
    assistantConversation->beginTurn(submittedPrompt);
    assistantPrompt.clear();
    assistantConversationViewport.setViewPosition(0, assistantConversation->getHeight());
    assistantGenerateButton.setEnabled(false);
    assistantGenerateButton.setButtonText("Send");
    assistantStatus.setText("You can keep editing", juce::dontSendNotification);
    assistantStatus.setTooltip({});

    const juce::Component::SafePointer<MainComponent> safeThis(this);
    assistantGateway.requestAsync(std::move(config), assistantSessionContext(), conversationHistory, submittedPrompt,
        [safeThis, submittedPrompt] (fishpond::AssistantSuggestion suggestion) {
            if (safeThis == nullptr)
                return;
            safeThis->assistantRequestInFlight = false;
            safeThis->assistantNewChatButton.setEnabled(true);
            safeThis->assistantGenerateButton.setEnabled(safeThis->assistantPrompt.getText().trim().isNotEmpty());
            safeThis->assistantGenerateButton.setButtonText("Send");
            if (! suggestion.succeeded) {
                const bool invalidResponse = suggestion.diagnostic.contains("FP_ASSISTANT_REVIEW_FAILED")
                    || suggestion.diagnostic.contains("FP_ASSISTANT_RESPONSE_INVALID");
                juce::String recovery;
                if (suggestion.diagnostic.startsWith("FP_ASSISTANT_TARGET_REQUIRED"))
                    recovery = "Load an instrument in the Instruments tab first, then send again. For drums, load a drum instrument.";
                else if (suggestion.diagnostic.startsWith("FP_ASSISTANT_CONFIG_REQUIRED"))
                    recovery = "Choose a model and endpoint in Config, then send again.";
                else if (suggestion.diagnostic.startsWith("FP_ASSISTANT_API_KEY_REQUIRED"))
                    recovery = "Add your OpenAI API key in Config, or select Ollama.";
                else if (suggestion.diagnostic.startsWith("FP_ASSISTANT_NETWORK_FAILED"))
                    recovery = "Couldn't reach the provider. Check that the server is running and the endpoint in Config is correct.";
                else if (invalidResponse)
                    recovery = "The model response couldn't be accepted as usable code. Try again or choose another model.";
                else
                    recovery = "The provider couldn't complete this request.";
                safeThis->assistantConversation->resolveLatest(recovery + "\n\nDetails: " + suggestion.diagnostic, {});
                // Restore only an empty composer: never replace a new draft typed while waiting.
                if (safeThis->assistantPrompt.getText().isEmpty())
                    safeThis->assistantPrompt.setText(submittedPrompt);
                safeThis->assistantGenerateButton.setEnabled(safeThis->assistantPrompt.getText().trim().isNotEmpty());
                safeThis->assistantConversationViewport.setViewPosition(0, safeThis->assistantConversation->getHeight());
                safeThis->assistantStatus.setText("Request failed", juce::dontSendNotification);
                safeThis->assistantStatus.setTooltip(suggestion.diagnostic);
                return;
            }
            safeThis->assistantConversation->resolveLatest(suggestion.explanation, suggestion.code);
            safeThis->assistantConversationViewport.setViewPosition(0, safeThis->assistantConversation->getHeight());
            safeThis->assistantStatus.setText("Response ready", juce::dontSendNotification);
        });
}

void MainComponent::insertAssistantSuggestion(const juce::String& code)
{
    if (code.isEmpty())
        return;
    liveCodingEditor.insertTextAtCaret(code);
    liveCodingEditor.grabKeyboardFocus();
    diagnostics.setText("Assistant suggestion inserted at the current cursor. Review it, then press Shift+Return to evaluate.",
                        juce::dontSendNotification);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* origin)
{
    if (origin == &assistantPrompt && key.getKeyCode() == juce::KeyPress::returnKey
        && key.getModifiers().isCommandDown()) {
        requestAssistantSuggestion();
        return true;
    }
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
    auto liveToolbar = liveArea.removeFromTop(28);
    assistantToggleButton.setBounds(liveToolbar.removeFromRight(150));
    liveArea.removeFromTop(6);
    diagnostics.setBounds(liveArea.removeFromBottom(84));
    liveArea.removeFromBottom(6);
    if (assistantPanelVisible) {
        const auto assistantWidth = juce::jlimit(300, juce::jmax(300, liveArea.getWidth() - 180), assistantPanelWidth);
        assistantPanel.setBounds(liveArea.removeFromRight(assistantWidth));
        liveArea.removeFromRight(8);
    } else {
        assistantPanel.setBounds({});
    }
    liveCodingEditor.setBounds(liveArea);

    assistantTabs.setBounds(assistantPanel.getLocalBounds());
    assistantResizeHandle.setBounds(0, 0, 8, assistantPanel.getHeight());

    auto assistantConfigArea = assistantConfigPanel.getLocalBounds().reduced(8);
    assistantProviderLabel.setBounds(assistantConfigArea.removeFromTop(20));
    assistantProvider.setBounds(assistantConfigArea.removeFromTop(28));
    assistantConfigArea.removeFromTop(6);
    assistantEndpointLabel.setBounds(assistantConfigArea.removeFromTop(20));
    assistantEndpoint.setBounds(assistantConfigArea.removeFromTop(28));
    assistantConfigArea.removeFromTop(6);
    assistantModelLabel.setBounds(assistantConfigArea.removeFromTop(20));
    auto assistantModelArea = assistantConfigArea.removeFromTop(28);
    assistantModel.setBounds(assistantModelArea);
    assistantRefreshModelsButton.setBounds(assistantModelArea.removeFromRight(78));
    assistantModelChoices.setBounds(assistantModelArea);
    assistantConfigArea.removeFromTop(6);
    assistantApiKeyLabel.setBounds(assistantConfigArea.removeFromTop(20));
    assistantApiKey.setBounds(assistantConfigArea.removeFromTop(28));

    auto assistantChatArea = assistantChatPanel.getLocalBounds().reduced(8);
    auto promptArea = assistantChatArea.removeFromBottom(120);
    auto chatHeader = assistantChatArea.removeFromTop(30);
    assistantNewChatButton.setBounds(chatHeader.removeFromRight(96).reduced(0, 2));
    assistantConversationViewport.setBounds(assistantChatArea);
    assistantConversation->setAvailableWidth(assistantConversationViewport.getWidth() - 16);
    assistantPromptLabel.setBounds(promptArea.removeFromTop(20));
    assistantPrompt.setBounds(promptArea.removeFromTop(64));
    promptArea.removeFromTop(6);
    auto assistantActions = promptArea;
    assistantStatus.setBounds(assistantActions.removeFromLeft(150));
    assistantGenerateButton.setBounds(assistantActions.removeFromRight(104));
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
    deviceStatus.setText("Running " + device->getName() + " - "
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

void MainComponent::componentMovedOrResized(juce::Component& component, bool, bool wasResized)
{
    if (&component != &assistantPanel || ! wasResized || ! assistantPanelVisible)
        return;

    assistantPanelWidth = assistantPanel.getWidth();
    resized();
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
