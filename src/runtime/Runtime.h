#pragma once

#include <cstddef>
#include <string>
#include <optional>
#include <vector>

namespace fishpond {
struct EvaluationResult {
    bool accepted {};
    std::string diagnostic;
};

class Runtime {
public:
    EvaluationResult evaluateEditorText(const std::string& source, bool bassReady = false) const;
    std::optional<int> firstNoteFromEditorText(const std::string& source) const;
    std::vector<int> notesFromEditorText(const std::string& source) const;
    std::optional<std::size_t> playerIndexFromEditorText(const std::string& source) const;
    std::optional<double> periodBeatsFromEditorText(const std::string& source) const;
    std::optional<double> durationBeatsFromEditorText(const std::string& source) const;
    std::optional<int> velocityFromEditorText(const std::string& source) const;
    bool playerReplacementContract() const;
    bool patternContract() const;
    bool periodContract() const;
    bool quantizedReplacementContract() const;
    bool targetNormalizationContract() const;
    bool targetCollisionContract() const;
    bool invalidValuesContract() const;
    bool unknownTargetIsolationContract() const;
    bool silenceAndPanicContract() const;
};
}
