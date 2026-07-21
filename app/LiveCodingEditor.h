#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class FishpondCodeTokeniser final : public juce::CodeTokeniser {
public:
    enum TokenType {
        tokenType_error = 0,
        tokenType_comment,
        tokenType_keyword,
        tokenType_function,
        tokenType_runtimeObject,
        tokenType_runtimeMember,
        tokenType_player,
        tokenType_number,
        tokenType_string,
        tokenType_operator,
        tokenType_bracket,
        tokenType_punctuation,
        tokenType_identifier
    };

    int readNextToken(juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;
};

class LiveCodingEditor final : public juce::CodeEditorComponent,
                               private juce::Timer {
public:
    LiveCodingEditor(juce::CodeDocument& document, juce::CodeTokeniser& tokeniser);

    void flashRange(juce::Range<int> range);

private:
    void paintOverChildren(juce::Graphics& graphics) override;
    void timerCallback() override;

    juce::Range<int> flashingRange;
};
