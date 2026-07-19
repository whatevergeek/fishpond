#include "runtime/PythonExecutionWorker.h"

namespace fishpond {
PythonExecutionWorker::PythonExecutionWorker()
    : worker([this] { run(); })
{
}

PythonExecutionWorker::~PythonExecutionWorker()
{
    {
        const std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    wake.notify_one();
    worker.join();
}

bool PythonExecutionWorker::submit(std::string source)
{
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (stopping || pending.size() >= 8)
            return false;
        pending.push_back(std::move(source));
    }
    wake.notify_one();
    return true;
}

bool PythonExecutionWorker::tryTakeCompletion(PythonExecutionCompletion& completion)
{
    const std::lock_guard<std::mutex> lock(mutex);
    if (completed.empty())
        return false;
    completion = std::move(completed.front());
    completed.pop_front();
    return true;
}

void PythonExecutionWorker::run()
{
    EmbeddedPythonRuntime runtime;
    for (;;) {
        std::string source;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return stopping || ! pending.empty(); });
            if (stopping && pending.empty())
                return;
            source = std::move(pending.front());
            pending.pop_front();
        }

        auto result = runtime.evaluate(source);
        {
            const std::lock_guard<std::mutex> lock(mutex);
            completed.push_back({ std::move(source), std::move(result) });
        }
    }
}
}
