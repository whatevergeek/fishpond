#include "AudioShell.h"

#include <iostream>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    const std::string test = argc > 1 ? argv[1] : "";
    fishpond::AudioShell shell;

    if (test == "IT_AudioDeviceState") {
        return require(!shell.selectDevice("", 2), "empty device accepted")
            && require(!shell.selectDevice("mono", 1), "non-stereo device accepted")
            && require(shell.selectDevice("test-device", 2), "stereo device rejected")
            && require(shell.state() == fishpond::AudioShellState::ready, "device not ready")
            && require(shell.deviceIdentifier() == "test-device", "device identity lost")
            && require(shell.masterOutputChannels() == 2, "master is not stereo") ? 0 : 1;
    }

    if (test == "IT_AudioEngineLifecycle") {
        return require(!shell.start(), "shell starts without a device")
            && require(shell.selectDevice("test-device", 2), "device selection failed")
            && require(shell.start(), "shell did not start")
            && require(shell.state() == fishpond::AudioShellState::running, "shell not running")
            && (shell.stop(), require(shell.state() == fishpond::AudioShellState::ready, "shell did not stop")) ? 0 : 1;
    }

    std::cerr << "unknown test\n";
    return 2;
}
