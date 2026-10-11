#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <vector>

namespace fishpond {

enum class AssistantProvider {
    openAI,
    ollama,
    localResponses,
};

struct AssistantGatewayConfig {
    AssistantProvider provider { AssistantProvider::ollama };
    juce::String endpoint;
    juce::String model;
    juce::String apiKey;
};

struct AssistantSessionContext {
    double tempoBpm { 120.0 };
    std::vector<juce::String> readyTargets;
    juce::String liveCode;
    juce::String diagnostics;
    juce::String editorContext;
    juce::String playbackContext;
    juce::String summary;
};

struct AssistantChatTurn {
    juce::String role;
    juce::String content;
};

struct AssistantSuggestion {
    bool succeeded {};
    juce::String code;
    juce::String explanation;
    juce::String diagnostic;
};

// Makes a single suggestion off the UI and audio threads. The gateway never
// evaluates generated code and does not persist credentials.
class AssistantGateway {
public:
    using Completion = std::function<void(AssistantSuggestion)>;
    using OllamaModelsCompletion = std::function<void(std::vector<juce::String>, juce::String diagnostic)>;

    void requestAsync(AssistantGatewayConfig config, AssistantSessionContext context,
                      std::vector<AssistantChatTurn> conversationHistory,
                      juce::String musicalIntent, Completion completion) const;
    void listOllamaModelsAsync(juce::String generateEndpoint, OllamaModelsCompletion completion) const;

    static juce::String defaultEndpoint(AssistantProvider provider);
    static juce::String defaultModel(AssistantProvider provider);

private:
    static AssistantSuggestion request(const AssistantGatewayConfig& config,
                                       const AssistantSessionContext& context,
                                       const std::vector<AssistantChatTurn>& conversationHistory,
                                       const juce::String& musicalIntent);
    static std::vector<juce::String> listOllamaModels(const juce::String& generateEndpoint,
                                                       juce::String& diagnostic);
};

} // namespace fishpond
