#include "MainComponent.h"

class FishpondApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Fishpond"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow,
                             private juce::MenuBarModel {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setMenuBar(this);
            centreWithSize(1040, 700);
            setVisible(true);
        }

        ~MainWindow() override { setMenuBar(nullptr); }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    private:
        enum MenuItem {
            newFile = 1,
            openFile,
            saveFile,
            saveFileAs
        };

        juce::StringArray getMenuBarNames() override { return { "File" }; }

        juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override
        {
            juce::PopupMenu menu;
            if (menuIndex != 0)
                return menu;

            const auto addFileItem = [&menu] (int itemID, const juce::String& name, juce::KeyPress shortcut) {
                juce::PopupMenu::Item item(name);
                item.itemID = itemID;
                item.shortcutKeyDescription = shortcut.getTextDescriptionWithIcons();
                menu.addItem(std::move(item));
            };

            const auto command = juce::ModifierKeys::commandModifier;
            addFileItem(newFile, "New", juce::KeyPress('n', command, 0));
            addFileItem(openFile, "Open...", juce::KeyPress('o', command, 0));
            menu.addSeparator();
            addFileItem(saveFile, "Save", juce::KeyPress('s', command, 0));
            addFileItem(saveFileAs, "Save As...", juce::KeyPress('s', command | juce::ModifierKeys::shiftModifier, 0));
            return menu;
        }

        void menuItemSelected(int menuItemID, int) override
        {
            auto* editor = dynamic_cast<MainComponent*>(getContentComponent());
            if (editor == nullptr)
                return;
            switch (menuItemID) {
                case newFile: editor->newLiveCodingFile(); break;
                case openFile: editor->openLiveCodingFile(); break;
                case saveFile: editor->saveLiveCodingFile(); break;
                case saveFileAs: editor->saveLiveCodingFileAs(); break;
                default: break;
            }
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(FishpondApplication)
