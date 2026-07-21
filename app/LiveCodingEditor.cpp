#include "LiveCodingEditor.h"

namespace {
bool isIdentifierCharacter(juce::juce_wchar character)
{
    return juce::CharacterFunctions::isLetterOrDigit(character) || character == '_';
}

bool isKeyword(const juce::String& word)
{
    static const juce::StringArray keywords { "and", "as", "async", "await", "break", "class", "continue",
                                               "def", "elif", "else", "except", "False", "finally", "for",
                                               "from", "if", "import", "in", "is", "lambda", "None", "not", "or",
                                               "pass", "return", "True", "try", "while", "with", "yield" };
    return keywords.contains(word);
}

bool isFunction(const juce::String& word)
{
    static const juce::StringArray functions { "n", "silence", "panic", "print", "range", "len" };
    return functions.contains(word);
}

bool isRuntimeObject(const juce::String& word)
{
    static const juce::StringArray objects { "clock", "master" };
    return objects.contains(word);
}

bool isRuntimeMember(const juce::String& word)
{
    static const juce::StringArray members { "bpm", "tempo", "volume" };
    return members.contains(word);
}
}

int FishpondCodeTokeniser::readNextToken(juce::CodeDocument::Iterator& source)
{
    source.skipWhitespace();
    const auto firstCharacter = source.peekNextChar();

    if (firstCharacter == 0)
        return tokenType_error;

    if (firstCharacter == '#') {
        source.skipToEndOfLine();
        return tokenType_comment;
    }

    if (firstCharacter == '\'' || firstCharacter == '"') {
        const auto quote = source.nextChar();
        while (! source.isEOF()) {
            const auto character = source.nextChar();
            if (character == '\\' && ! source.isEOF())
                source.skip();
            else if (character == quote)
                break;
        }
        return tokenType_string;
    }

    if (juce::CharacterFunctions::isDigit(firstCharacter)) {
        while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '.')
            source.skip();
        return tokenType_number;
    }

    if (juce::CharacterFunctions::isLetter(firstCharacter) || firstCharacter == '_') {
        juce::String word;
        while (isIdentifierCharacter(source.peekNextChar()))
            word += source.nextChar();

        if (isKeyword(word))
            return tokenType_keyword;
        if (isFunction(word))
            return tokenType_function;
        if (isRuntimeObject(word))
            return tokenType_runtimeObject;
        if (isRuntimeMember(word))
            return tokenType_runtimeMember;
        if (word.length() == 2 && word[0] == 'P' && word[1] >= 'a' && word[1] <= 'z')
            return tokenType_player;
        return tokenType_identifier;
    }

    source.skip();
    if (firstCharacter == '(' || firstCharacter == ')' || firstCharacter == '[' || firstCharacter == ']'
        || firstCharacter == '{' || firstCharacter == '}')
        return tokenType_bracket;
    if (firstCharacter == ',' || firstCharacter == ':' || firstCharacter == ';' || firstCharacter == '.')
        return tokenType_punctuation;
    return tokenType_operator;
}

juce::CodeEditorComponent::ColourScheme FishpondCodeTokeniser::getDefaultColourScheme()
{
    juce::CodeEditorComponent::ColourScheme scheme;
    scheme.set("Error", juce::Colour(0xffe06c75));
    scheme.set("Comment", juce::Colour(0xff7d8799));
    scheme.set("Keyword", juce::Colour(0xffc678dd));
    scheme.set("Function", juce::Colour(0xff61afef));
    scheme.set("RuntimeObject", juce::Colour(0xff56b6c2));
    scheme.set("RuntimeMember", juce::Colour(0xffe5c07b));
    scheme.set("Player", juce::Colour(0xffe5c07b));
    scheme.set("Number", juce::Colour(0xffd19a66));
    scheme.set("String", juce::Colour(0xff98c379));
    scheme.set("Operator", juce::Colour(0xff56b6c2));
    scheme.set("Bracket", juce::Colour(0xffabb2bf));
    scheme.set("Punctuation", juce::Colour(0xffabb2bf));
    scheme.set("Identifier", juce::Colour(0xffabb2bf));
    return scheme;
}

LiveCodingEditor::LiveCodingEditor(juce::CodeDocument& document, juce::CodeTokeniser& tokeniser)
    : CodeEditorComponent(document, &tokeniser)
{
    setColourScheme(tokeniser.getDefaultColourScheme());
    setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xff26343a));
    setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colour(0xfff1f1f1));
    // Normal text selection must remain readable for editing and copying. Execution
    // feedback is painted separately as a short translucent overlay below.
    setColour(juce::CodeEditorComponent::highlightColourId, juce::Colour(0xff356f8c));
    setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xff1b2529));
    setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(0xff7d8799));
    setLineNumbersShown(true);
    setFont(juce::Font(juce::FontOptions(15.0f).withName("Menlo")));
}

void LiveCodingEditor::flashRange(juce::Range<int> range)
{
    if (range.isEmpty())
        return;

    flashingRange = range;
    repaint();
    startTimer(200);
}

void LiveCodingEditor::paintOverChildren(juce::Graphics& graphics)
{
    if (flashingRange.isEmpty())
        return;

    const auto& document = getDocument();
    const auto firstLine = juce::CodeDocument::Position(document, flashingRange.getStart()).getLineNumber();
    const auto lastCharacter = juce::jmax(flashingRange.getStart(), flashingRange.getEnd() - 1);
    const auto lastLine = juce::CodeDocument::Position(document, lastCharacter).getLineNumber();

    graphics.setColour(juce::Colour(0x33ffad42));
    for (auto line = firstLine; line <= lastLine; ++line) {
        const auto lineStart = getCharacterBounds({ document, line, 0 });
        graphics.fillRect(lineStart.getX(), lineStart.getY(), getWidth() - lineStart.getX(), getLineHeight());
    }
}

void LiveCodingEditor::timerCallback()
{
    stopTimer();
    flashingRange = {};
    repaint();
}
