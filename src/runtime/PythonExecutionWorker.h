#pragma once

#include "runtime/EmbeddedPythonRuntime.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace fishpond {
struct PythonExecutionCompletion {
    std::string source;
    PythonEvaluationResult result;
};

// Owns the CPython interpreter on one non-real-time thread. The audio callback
// never creates, calls, or waits for this worker.
class PythonExecutionWorker {
public:
    PythonExecutionWorker();
    ~PythonExecutionWorker();
    PythonExecutionWorker(const PythonExecutionWorker&) = delete;
    PythonExecutionWorker& operator=(const PythonExecutionWorker&) = delete;

    bool submit(std::string source);
    bool tryTakeCompletion(PythonExecutionCompletion& completion);

private:
    void run();

    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::string> pending;
    std::deque<PythonExecutionCompletion> completed;
    bool stopping {};
    std::thread worker;
};
}
