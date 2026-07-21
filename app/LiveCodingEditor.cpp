#include "LiveCodingEditor.h"

namespace {
bool isIdentifierCharacter(juce::juce_wchar character)
{
    return juce::CharacterFunctions::isLetterOrDigit(character) || character == '_';
}

bool isKeyword(const juce::String& word)
{
    static const juce::StringArray keywords { "False", "None", "True", "and", "as", "assert", "async",
                                               "await", "break", "case", "class", "continue", "def", "del",
                                               "elif", "else", "except", "finally", "for", "from", "global", "if",
                                               "import", "in", "is", "lambda", "match", "nonlocal", "not", "or",
                                               "pass", "raise", "return", "try", "while", "with", "yield" };
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

bool isStringPrefixCharacter(juce::juce_wchar character)
{
    return character == 'r' || character == 'R' || character == 'b' || character == 'B'
        || character == 'u' || character == 'U' || character == 'f' || character == 'F';
}

bool looksLikePythonString(juce::CodeDocument::Iterator source)
{
    const auto first = source.peekNextChar();
    if (first == '\'' || first == '"')
        return true;
    if (! isStringPrefixCharacter(first))
        return false;

    auto prefixLength = 0;
    while (prefixLength < 3 && isStringPrefixCharacter(source.peekNextChar())) {
        source.skip();
        ++prefixLength;
    }

    const auto quote = source.peekNextChar();
    return quote == '\'' || quote == '"';
}

void consumePythonString(juce::CodeDocument::Iterator& source)
{
    auto quote = source.nextChar();
    if (isStringPrefixCharacter(quote)) {
        auto prefixLength = 1;
        while (prefixLength < 3 && isStringPrefixCharacter(source.peekNextChar())) {
            source.skip();
            ++prefixLength;
        }
        quote = source.nextChar();
    }

    auto quoteProbe = source;
    const auto tripleQuoted = quoteProbe.peekNextChar() == quote
                           && (quoteProbe.skip(), quoteProbe.peekNextChar() == quote);
    if (tripleQuoted) {
        source.skip();
        if (source.peekNextChar() == quote)
            source.skip();
    }

    while (! source.isEOF()) {
        const auto character = source.nextChar();
        if (character == '\\' && ! source.isEOF()) {
            source.skip();
            continue;
        }
        if (character != quote)
            continue;
        if (! tripleQuoted)
            break;
        if (source.peekNextChar() != quote)
            continue;
        source.skip();
        if (source.peekNextChar() == quote) {
            source.skip();
            break;
        }
    }
}

void consumePythonNumber(juce::CodeDocument::Iterator& source)
{
    const auto first = source.peekNextChar();
    if (first == '.') {
        source.skip();
        while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '_')
            source.skip();
    } else {
        source.skip();
        if (first == '0' && (source.peekNextChar() == 'x' || source.peekNextChar() == 'X'
                             || source.peekNextChar() == 'o' || source.peekNextChar() == 'O'
                             || source.peekNextChar() == 'b' || source.peekNextChar() == 'B')) {
            source.skip();
            while (juce::CharacterFunctions::isLetterOrDigit(source.peekNextChar()) || source.peekNextChar() == '_')
                source.skip();
            return;
        }
        while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '_')
            source.skip();
        if (source.peekNextChar() == '.') {
            source.skip();
            while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '_')
                source.skip();
        }
    }

    if (source.peekNextChar() == 'e' || source.peekNextChar() == 'E') {
        source.skip();
        if (source.peekNextChar() == '+' || source.peekNextChar() == '-')
            source.skip();
        while (juce::CharacterFunctions::isDigit(source.peekNextChar()) || source.peekNextChar() == '_')
            source.skip();
    }
    if (source.peekNextChar() == 'j' || source.peekNextChar() == 'J')
        source.skip();
}

bool isFunctionCall(juce::CodeDocument::Iterator source)
{
    source.skipWhitespace();
    return source.peekNextChar() == '(';
}

bool startsPythonNumber(juce::CodeDocument::Iterator source)
{
    if (juce::CharacterFunctions::isDigit(source.peekNextChar()))
        return true;
    if (source.peekNextChar() != '.')
        return false;
    source.skip();
    return juce::CharacterFunctions::isDigit(source.peekNextChar());
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

    if (looksLikePythonString(source)) {
        consumePythonString(source);
        return tokenType_string;
    }

    if (startsPythonNumber(source)) {
        consumePythonNumber(source);
        return tokenType_number;
    }

    if (firstCharacter == '@') {
        source.skip();
        return tokenType_decorator;
    }

    if (juce::CharacterFunctions::isLetter(firstCharacter) || firstCharacter == '_') {
        const auto followsDecorator = source.peekPreviousChar() == '@';
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
        if (followsDecorator || isFunctionCall(source))
            return tokenType_function;
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
    scheme.set("Decorator", juce::Colour(0xffc678dd));
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

void LiveCodingEditor::setClearOutputHandler(std::function<void()> handler)
{
    clearOutputHandler = std::move(handler);
}

bool LiveCodingEditor::keyPressed(const juce::KeyPress& key)
{
    const auto character = juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode()));
    if (key.getModifiers().isCommandDown() && character == 'k' && clearOutputHandler != nullptr) {
        clearOutputHandler();
        return true;
    }

    return juce::CodeEditorComponent::keyPressed(key);
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
