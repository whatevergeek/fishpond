#include "AssistantGateway.h"

#include <regex>
#include <set>
#include <thread>

namespace fishpond {
namespace {

juce::var makeObject()
{
    return juce::var(new juce::DynamicObject());
}

juce::DynamicObject* objectFor(juce::var& value)
{
    return value.getDynamicObject();
}

juce::String requestInstructions(const AssistantSessionContext& context)
{
    juce::StringArray targets;
    for (const auto& target : context.readyTargets)
        targets.add("\"" + target + "\"");

    return "You are Fishpond's live-coding assistant. Produce concise Fishpond Python source that "
           "matches the musician's request: playable patterns for musical requests and valid control "
           "statements for stop/control requests. Return JSON with exactly two string fields: code and explanation. "
           "Use only documented Fishpond syntax: named players Pa through Pz; n(notes, target=..., p=..., dur=...); "
           "player.stop() to stop one player; silence() to stop all players; panic() for emergency clearing; "
           "clock.bpm and master.volume. Notes use names such as C3, rests use ., and braces make chords. "
           "For a per-player stop request, inspect non-comment player pattern assignments in the current Live Coding buffer "
           "and the scheduler snapshot. Emit one standalone `Pname.stop()` line for each distinct relevant player, "
           "including editor-defined players even if playback is currently stopped; ignore repeated assignments, "
           "comments, and existing stop lines. Do not substitute silence() when the musician asks for individual stops. "
           "Use silence() only when asked to stop all players, and panic() only for an explicit emergency stop. "
           "If no player names can be identified, ask which players to stop instead of inventing names. "
           "Assign each player only once in a suggestion: another assignment replaces its pattern. "
           "For sequential notes, put space-separated notes inside one n() string; braces mean simultaneous notes. "
           "Use distinct player names for independent simultaneous patterns. "
           "For every n() pattern statement, use only a ready target from this application context: [" + targets.joinIntoString(", ") + "]. "
           "The current tempo is " + juce::String(context.tempoBpm, 0) + " BPM. "
           "Editor focus snapshot:\n" + context.editorContext + "\n"
           "Playback snapshot:\n" + context.playbackContext + "\n"
           "For 'this', use the selected text, or the cursor line if nothing is selected. "
           "When the musician names a player such as Pg, inspect that player's n() pattern in the Live Coding buffer and use it as the source, even when it is not currently active. "
           "The scheduler snapshot describes what is actually queued or playing; it supplements the source code and does not replace it. "
           "If the named source player has no pattern in the buffer, use the scheduler snapshot if it has that player; otherwise ask which progression to follow. "
           "For chord-aware accompaniment, parse each chord group in the source pattern, align new notes with those chord steps and rests, and choose chord tones that fit each chord. "
           "Vary the phrase across chord changes instead of repeating one arpeggio or one tonal shape over the entire progression. "
           "If the scheduler says Playing: none, do not claim the source is currently sounding; it can still be used as the requested written progression. "
           "Preserve other active players by omitting them from the snippet, not by reassigning them. "
           "Return only the requested changed patterns, not a copy of the entire editor. "
           "Do not copy silence(), panic(), or existing player.stop() lines from context unless they match the request. "
           "Queued patterns are not active yet. "
           "The current Live Coding buffer is:\n```python\n" + context.liveCode + "\n```\n"
           "The most recent evaluator output is:\n```text\n" + context.diagnostics + "\n```\n"
           "If the musician asks for help fixing code, use that buffer and diagnostic to return the smallest useful corrected snippet. "
           "Before responding, review the snippet as Fishpond source: return source code only in the code field, "
           "never a JSON object or Markdown fence; balance quotes, parentheses, and braces. Pattern statements "
           "must use target= with one of the ready targets; control-only statements such as Pname.stop(), "
           "silence(), and panic() do not need a target. "
           "The application will insert the code only if the musician chooses Insert; never say that "
           "you evaluated code, loaded a plugin, saved a file, or changed audio settings.";
}

juce::var suggestionSchema()
{
    auto schema = makeObject();
    auto* schemaObject = objectFor(schema);
    schemaObject->setProperty("type", "object");
    schemaObject->setProperty("additionalProperties", false);

    auto properties = makeObject();
    auto code = makeObject();
    objectFor(code)->setProperty("type", "string");
    auto explanation = makeObject();
    objectFor(explanation)->setProperty("type", "string");
    objectFor(properties)->setProperty("code", code);
    objectFor(properties)->setProperty("explanation", explanation);
    schemaObject->setProperty("properties", properties);

    juce::Array<juce::var> required;
    required.add("code");
    required.add("explanation");
    schemaObject->setProperty("required", juce::var(required));
    return schema;
}

juce::String extractOpenAIText(const juce::var& response)
{
    const auto* root = response.getDynamicObject();
    if (root == nullptr)
        return {};

    const auto output = root->getProperty("output");
    const auto* outputArray = output.getArray();
    if (outputArray == nullptr)
        return {};

    for (const auto& item : *outputArray) {
        const auto* message = item.getDynamicObject();
        if (message == nullptr || message->getProperty("type").toString() != "message")
            continue;
        const auto content = message->getProperty("content");
        const auto* contentArray = content.getArray();
        if (contentArray == nullptr)
            continue;
        for (const auto& part : *contentArray) {
            const auto* contentObject = part.getDynamicObject();
            if (contentObject != nullptr && contentObject->hasProperty("text"))
                return contentObject->getProperty("text").toString();
        }
    }
    return {};
}

juce::String jsonObjectFromProviderText(juce::String text)
{
    text = text.trim();
    if (text.startsWith("```")) {
        const auto firstLineEnd = text.indexOfChar('\n');
        if (firstLineEnd >= 0)
            text = text.substring(firstLineEnd + 1);
        const auto closingFence = text.lastIndexOf("```");
        if (closingFence >= 0)
            text = text.substring(0, closingFence);
    }

    const auto objectStart = text.indexOfChar('{');
    const auto objectEnd = text.lastIndexOfChar('}');
    return objectStart >= 0 && objectEnd > objectStart
        ? text.substring(objectStart, objectEnd + 1)
        : text;
}

juce::String fencedCodeFromProviderText(const juce::String& text)
{
    const auto openingFence = text.indexOf("```");
    if (openingFence < 0)
        return {};

    const auto firstLineEnd = text.indexOfChar(openingFence, '\n');
    if (firstLineEnd < 0)
        return {};
    const auto closingFence = text.indexOf(firstLineEnd + 1, "```");
    if (closingFence < 0)
        return {};
    return text.substring(firstLineEnd + 1, closingFence).trim();
}

bool looksLikeFishpondCode(const juce::String& text)
{
    return text.contains(">>") || text.contains("silence(") || text.contains("panic(")
        || text.contains(".stop()") || text.contains("clock.bpm") || text.contains("master.volume");
}

bool isPlayerControlIntent(const juce::String& text)
{
    static const std::regex controlIntent(R"(\b(stop|silence|panic)\b)", std::regex::icase);
    return std::regex_search(text.toStdString(), controlIntent);
}

juce::String reviewFishpondCode(const juce::String& code)
{
    if (code.trimStart().startsWithChar('{'))
        return "response is a JSON object rather than Fishpond source";
    if (! looksLikeFishpondCode(code))
        return "response does not contain a supported Fishpond statement";

    std::vector<std::pair<juce::juce_wchar, int>> delimiters;
    juce::juce_wchar quote {};
    bool escaped = false, comment = false;
    int line = 1, quoteLine = 1;
    juce::String statements;
    for (const auto character : code) {
        if (comment) {
            if (character == '\n') { comment = false; ++line; statements += "\n"; }
            else statements += " ";
            continue;
        }
        if (quote != 0) {
            statements += " ";
            if (character == '\n') ++line;
            if (escaped) { escaped = false; continue; }
            if (character == '\\') { escaped = true; continue; }
            if (character == quote) quote = 0;
            continue;
        }
        if (character == '#') { comment = true; statements += " "; continue; }
        if (character == '\'' || character == '"') {
            quote = character; quoteLine = line; statements += " "; continue;
        }
        statements += juce::String::charToString(character);
        if (character == '\n') ++line;
        if (character == '(' || character == '{' || character == '[')
            delimiters.push_back({ character, line });
        if (character == ')' || character == '}' || character == ']') {
            const auto expected = character == ')' ? '(' : character == '}' ? '{' : '[';
            if (delimiters.empty() || delimiters.back().first != expected)
                return "mismatched closing delimiter on line " + juce::String(line);
            delimiters.pop_back();
        }
    }
    if (quote != 0)
        return "unclosed string starting on line " + juce::String(quoteLine);
    if (!delimiters.empty())
        return "unclosed delimiter starting on line " + juce::String(delimiters.back().second);
    if (statements.contains(">> n(") && ! statements.contains("target="))
        return "pattern statement has no target=";
    const std::regex assignment(R"(\b(P[a-z])\s*>>)");
    const auto source = statements.toStdString();
    std::set<std::string> players;
    for (auto it = std::sregex_iterator(source.begin(), source.end(), assignment);
         it != std::sregex_iterator(); ++it)
        if (!players.insert((*it)[1].str()).second)
            return "a player is assigned more than once; combine sequential notes in one pattern string";
    return {};
}

juce::String jsonStringField(const juce::String& text, const juce::String& field)
{
    const std::regex pattern("\\\"" + field.toStdString() + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
    std::smatch match;
    const auto source = text.toStdString();
    if (! std::regex_search(source, match, pattern))
        return {};

    juce::var decoded;
    const auto escapedValue = juce::String(match[1].str());
    if (juce::JSON::parse("[\"" + escapedValue + "\"]", decoded).failed()
        || decoded.getArray() == nullptr || decoded.getArray()->isEmpty())
        return {};
    return decoded.getArray()->getReference(0).toString();
}

AssistantSuggestion parseSuggestion(const juce::String& text)
{
    juce::var parsed;
    if (juce::JSON::parse(jsonObjectFromProviderText(text), parsed).failed()) {
        const auto fencedCode = fencedCodeFromProviderText(text);
        if (fencedCode.isNotEmpty()) {
            const auto nestedSuggestion = parseSuggestion(fencedCode);
            if (nestedSuggestion.succeeded)
                return nestedSuggestion;
            const auto code = jsonStringField(fencedCode, "code").trim();
            if (looksLikeFishpondCode(code)) {
                if (const auto failure = reviewFishpondCode(code); failure.isNotEmpty())
                    return { false, {}, {}, "FP_ASSISTANT_REVIEW_FAILED: " + failure };
                return { true, code, jsonStringField(fencedCode, "explanation").trim(), {} };
            }
        }
        if (fencedCode.contains("\"code\""))
            return { false, {}, {}, "FP_ASSISTANT_REVIEW_FAILED: provider returned an unreadable JSON code envelope" };
        if (fencedCode.isNotEmpty() && looksLikeFishpondCode(fencedCode)) {
            if (const auto failure = reviewFishpondCode(fencedCode); failure.isNotEmpty())
                return { false, {}, {}, "FP_ASSISTANT_REVIEW_FAILED: " + failure };
            return { true, fencedCode, "Suggested pattern. Review before inserting.", {} };
        }
        return { false, {}, {}, "FP_ASSISTANT_RESPONSE_INVALID: provider did not return JSON or a fenced code block" };
    }
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return { false, {}, {}, "FP_ASSISTANT_RESPONSE_INVALID: expected a JSON object" };

    auto code = object->getProperty("code").toString().trim();
    auto explanation = object->getProperty("explanation").toString().trim();
    for (const auto& field : { "snippet", "pattern", "content", "response", "output" }) {
        if (code.isNotEmpty())
            break;
        const auto value = object->getProperty(field);
        if (const auto* nested = value.getDynamicObject()) {
            code = nested->getProperty("code").toString().trim();
            if (explanation.isEmpty())
                explanation = nested->getProperty("explanation").toString().trim();
        } else if (value.isString()) {
            const auto candidate = value.toString().trim();
            const auto fencedCode = fencedCodeFromProviderText(candidate);
            if (fencedCode.isNotEmpty())
                code = fencedCode;
            else if (looksLikeFishpondCode(candidate))
                code = candidate;
        }
    }
    if (code.isEmpty())
        code = fencedCodeFromProviderText(text);
    if (code.isNotEmpty() && ! looksLikeFishpondCode(code))
        code.clear();
    if (code.isEmpty())
        return { false, {}, {}, "FP_ASSISTANT_RESPONSE_INVALID: provider returned JSON without a usable code field" };

    if (explanation.isEmpty())
        explanation = "Generated by the selected provider.";
    if (const auto reviewFailure = reviewFishpondCode(code); reviewFailure.isNotEmpty())
        return { false, {}, {}, "FP_ASSISTANT_REVIEW_FAILED: " + reviewFailure };
    return { true, code, explanation, {} };
}

AssistantSuggestion failed(const juce::String& diagnostic)
{
    return { false, {}, {}, diagnostic };
}

juce::String ollamaTagsEndpoint(const juce::String& generateEndpoint)
{
    const auto apiPosition = generateEndpoint.indexOf("/api/");
    if (apiPosition < 0)
        return generateEndpoint.trim().trimCharactersAtEnd("/") + "/api/tags";
    return generateEndpoint.substring(0, apiPosition) + "/api/tags";
}

bool usesResponsesAPI(const AssistantProvider provider)
{
    return provider != AssistantProvider::ollama;
}

} // namespace

juce::String AssistantGateway::defaultEndpoint(const AssistantProvider provider)
{
    switch (provider) {
        case AssistantProvider::openAI: return "https://api.openai.com/v1/responses";
        case AssistantProvider::ollama: return "http://127.0.0.1:11434/api/generate";
        case AssistantProvider::localResponses: return "http://127.0.0.1:8787/v1/responses";
    }
    return {};
}

juce::String AssistantGateway::defaultModel(const AssistantProvider provider)
{
    switch (provider) {
        case AssistantProvider::openAI: return "gpt-6-astra";
        case AssistantProvider::ollama: return {};
        case AssistantProvider::localResponses: return "codex-local";
    }
    return {};
}

void AssistantGateway::requestAsync(AssistantGatewayConfig config, AssistantSessionContext context,
                                    std::vector<AssistantChatTurn> conversationHistory,
                                    juce::String musicalIntent, Completion completion) const
{
    std::thread([config = std::move(config), context = std::move(context),
                 conversationHistory = std::move(conversationHistory), musicalIntent = std::move(musicalIntent),
                 completion = std::move(completion)] () mutable {
        const auto suggestion = request(config, context, conversationHistory, musicalIntent);
        juce::MessageManager::callAsync([completion = std::move(completion), suggestion] {
            completion(suggestion);
        });
    }).detach();
}

void AssistantGateway::listOllamaModelsAsync(juce::String generateEndpoint,
                                              OllamaModelsCompletion completion) const
{
    std::thread([generateEndpoint = std::move(generateEndpoint), completion = std::move(completion)] () mutable {
        juce::String diagnostic;
        auto models = listOllamaModels(generateEndpoint, diagnostic);
        juce::MessageManager::callAsync([completion = std::move(completion), models = std::move(models), diagnostic] () mutable {
            completion(std::move(models), diagnostic);
        });
    }).detach();
}

std::vector<juce::String> AssistantGateway::listOllamaModels(const juce::String& generateEndpoint,
                                                              juce::String& diagnostic)
{
    const auto endpoint = ollamaTagsEndpoint(generateEndpoint);
    int statusCode {};
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(10'000)
        .withStatusCode(&statusCode);
    auto stream = juce::URL(endpoint).createInputStream(options);
    if (stream == nullptr) {
        diagnostic = "FP_ASSISTANT_OLLAMA_MODELS_NETWORK_FAILED: could not reach " + endpoint;
        return {};
    }

    const auto responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300) {
        diagnostic = "FP_ASSISTANT_OLLAMA_MODELS_HTTP_" + juce::String(statusCode);
        return {};
    }

    const auto response = juce::JSON::parse(responseText);
    const auto* root = response.getDynamicObject();
    const auto* models = root != nullptr ? root->getProperty("models").getArray() : nullptr;
    if (models == nullptr) {
        diagnostic = "FP_ASSISTANT_OLLAMA_MODELS_INVALID: expected an Ollama models response";
        return {};
    }

    std::vector<juce::String> names;
    names.reserve(static_cast<std::size_t>(models->size()));
    for (const auto& model : *models) {
        const auto* modelObject = model.getDynamicObject();
        const auto name = modelObject != nullptr ? modelObject->getProperty("name").toString().trim() : juce::String();
        if (name.isNotEmpty())
            names.push_back(name);
    }
    if (names.empty())
        diagnostic = "FP_ASSISTANT_OLLAMA_MODELS_EMPTY: Ollama has no installed models";
    return names;
}

AssistantSuggestion AssistantGateway::request(const AssistantGatewayConfig& config,
                                              const AssistantSessionContext& context,
                                              const std::vector<AssistantChatTurn>& conversationHistory,
                                              const juce::String& musicalIntent)
{
    if (musicalIntent.trim().isEmpty())
        return failed("FP_ASSISTANT_PROMPT_REQUIRED: describe the musical idea you want to make");
    if (context.readyTargets.empty() && ! isPlayerControlIntent(musicalIntent))
        return failed("FP_ASSISTANT_TARGET_REQUIRED: load an instrument before generating code");
    if (config.endpoint.trim().isEmpty() || config.model.trim().isEmpty())
        return failed("FP_ASSISTANT_CONFIG_REQUIRED: set an endpoint and model");

    auto apiKey = config.apiKey.trim();
    if (config.provider == AssistantProvider::localResponses)
        apiKey.clear();
    if (config.provider == AssistantProvider::openAI && apiKey.isEmpty()) {
        apiKey = juce::SystemStats::getEnvironmentVariable("FISHPOND_OPENAI_API_KEY", {});
        if (apiKey.isEmpty())
            apiKey = juce::SystemStats::getEnvironmentVariable("OPENAI_API_KEY", {});
    }
    if (config.provider == AssistantProvider::openAI && apiKey.isEmpty())
        return failed("FP_ASSISTANT_API_KEY_REQUIRED: enter an OpenAI API key or set FISHPOND_OPENAI_API_KEY");

    const auto instructions = requestInstructions(context);
    auto body = makeObject();
    auto* requestObject = objectFor(body);
    requestObject->setProperty("model", config.model.trim());

    if (usesResponsesAPI(config.provider)) {
        requestObject->setProperty("instructions", instructions);
        juce::Array<juce::var> input;
        const auto appendMessage = [&input] (const juce::String& role, const juce::String& content) {
            auto message = makeObject();
            objectFor(message)->setProperty("role", role);
            objectFor(message)->setProperty("content", content);
            input.add(message);
        };
        for (const auto& turn : conversationHistory)
            appendMessage(turn.role, turn.content);
        appendMessage("user", musicalIntent.trim());
        requestObject->setProperty("input", juce::var(input));
        requestObject->setProperty("store", false);

        auto format = makeObject();
        auto* formatObject = objectFor(format);
        formatObject->setProperty("type", "json_schema");
        formatObject->setProperty("name", "fishpond_suggestion");
        formatObject->setProperty("strict", true);
        formatObject->setProperty("schema", suggestionSchema());
        auto text = makeObject();
        objectFor(text)->setProperty("format", format);
        requestObject->setProperty("text", text);
    } else {
        juce::String conversation;
        for (const auto& turn : conversationHistory)
            conversation += turn.role.toUpperCase() + ":\n" + turn.content + "\n\n";
        const auto historySection = conversation.isEmpty()
            ? juce::String()
            : "\n\nRecent conversation for follow-up context:\n" + conversation.trimEnd();
        requestObject->setProperty("prompt", instructions + historySection
                                   + "\n\nCurrent musician request:\n" + musicalIntent.trim());
        requestObject->setProperty("stream", false);
        requestObject->setProperty("think", false);
        requestObject->setProperty("format", suggestionSchema());
    }

    auto headers = juce::String("Content-Type: application/json\r\n");
    if (! apiKey.isEmpty() && config.provider != AssistantProvider::localResponses)
        headers += "Authorization: Bearer " + apiKey + "\r\n";

    int statusCode {};
    auto url = juce::URL(config.endpoint.trim()).withPOSTData(juce::JSON::toString(body, false));
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
        .withExtraHeaders(headers)
        .withConnectionTimeoutMs(60'000)
        .withStatusCode(&statusCode);
    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return failed("FP_ASSISTANT_NETWORK_FAILED: could not reach " + config.endpoint.trim());

    const auto responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300) {
        const auto response = juce::JSON::parse(responseText);
        const auto* errorRoot = response.getDynamicObject();
        const auto error = errorRoot != nullptr ? errorRoot->getProperty("error") : juce::var();
        const auto* errorObject = error.getDynamicObject();
        const auto message = errorObject != nullptr ? errorObject->getProperty("message").toString() : responseText.substring(0, 300);
        return failed("FP_ASSISTANT_HTTP_" + juce::String(statusCode) + ": " + message);
    }

    const auto response = juce::JSON::parse(responseText);
    const auto suggestionText = usesResponsesAPI(config.provider)
        ? extractOpenAIText(response)
        : response.getDynamicObject() != nullptr ? response.getDynamicObject()->getProperty("response").toString() : juce::String();
    if (suggestionText.isEmpty())
        return failed("FP_ASSISTANT_RESPONSE_INVALID: provider returned no generated text");
    const auto suggestion = parseSuggestion(suggestionText);
    if (! suggestion.succeeded
        && (suggestion.diagnostic.startsWith("FP_ASSISTANT_REVIEW_FAILED:")
            || suggestion.diagnostic.startsWith("FP_ASSISTANT_RESPONSE_INVALID:"))
        && ! musicalIntent.contains("[Fishpond review retry]")) {
        return request(config, context, conversationHistory, musicalIntent
                       + "\n\n[Fishpond review retry] Your previous suggestion was rejected: "
                       + suggestion.diagnostic
                       + ". Return one corrected Fishpond source snippet with balanced syntax."
                       + "\nPrevious response (data to correct):\n" + suggestionText.substring(0, 8000));
    }
    return suggestion;
}

} // namespace fishpond
