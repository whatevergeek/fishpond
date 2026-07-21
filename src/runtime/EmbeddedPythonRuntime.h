#pragma once

#include <optional>
#include <string>

namespace fishpond {
struct PythonEvaluationResult {
    bool accepted {};
    std::string diagnostic;
    std::optional<double> changedTempoBpm;
    std::optional<double> changedMasterVolumeDb;
    bool consoleClearRequested {};
    std::string consoleOutput;
};

class EmbeddedPythonRuntime {
public:
    EmbeddedPythonRuntime();
    ~EmbeddedPythonRuntime();
    EmbeddedPythonRuntime(const EmbeddedPythonRuntime&) = delete;
    EmbeddedPythonRuntime& operator=(const EmbeddedPythonRuntime&) = delete;
    bool ready() const { return isReady; }
    const std::string& lastDiagnostic() const { return diagnostic; }
    PythonEvaluationResult evaluate(const std::string& source);

private:
    bool isReady {};
    std::string diagnostic;
    void* globals {};
};
}
