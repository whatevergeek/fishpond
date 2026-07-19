#pragma once

#include <string>

namespace fishpond {

enum class AudioShellState { noDevice, ready, running };

class AudioShell {
public:
    bool selectDevice(std::string identifier, int outputChannels);
    bool start();
    void stop() noexcept;

    AudioShellState state() const noexcept { return currentState; }
    const std::string& deviceIdentifier() const noexcept { return device; }
    int masterOutputChannels() const noexcept { return masterChannels; }

private:
    AudioShellState currentState = AudioShellState::noDevice;
    std::string device;
    int masterChannels = 0;
};

} // namespace fishpond
