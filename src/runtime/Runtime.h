#pragma once

#include <string>
#include <optional>

namespace fishpond {
struct EvaluationResult {
    bool accepted {};
    std::string diagnostic;
};

class Runtime {
public:
    EvaluationResult evaluateEditorText(const std::string& source, bool bassReady = false) const;
    std::optional<int> firstNoteFromEditorText(const std::string& source) const;
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
