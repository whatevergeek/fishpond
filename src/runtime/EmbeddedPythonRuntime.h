#pragma once

#include <string>

namespace fishpond {
struct PythonEvaluationResult {
    bool accepted {};
    std::string diagnostic;
};

class EmbeddedPythonRuntime {
public:
    EmbeddedPythonRuntime();
    bool ready() const { return isReady; }
    const std::string& lastDiagnostic() const { return diagnostic; }
    PythonEvaluationResult evaluate(const std::string& source);

private:
    bool isReady {};
    std::string diagnostic;
    void* globals {};
};
}
