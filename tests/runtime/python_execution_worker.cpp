#include "runtime/PythonExecutionWorker.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    fishpond::PythonExecutionWorker worker;
    if (! worker.submit("tempo = 120") || ! worker.submit("assert tempo == 120")) {
        std::cerr << "worker rejected an available execution";
        return 1;
    }

    int completed = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    fishpond::PythonExecutionCompletion completion;
    while (std::chrono::steady_clock::now() < deadline && completed != 2) {
        if (worker.tryTakeCompletion(completion)) {
            if (! completion.result.accepted) {
                std::cerr << completion.result.diagnostic;
                return 1;
            }
            ++completed;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (completed != 2) {
        std::cerr << "worker did not complete both evaluations";
        return 1;
    }
    return 0;
}
