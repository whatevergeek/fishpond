#include "AudioShell.h"

#include <utility>

namespace fishpond {

bool AudioShell::selectDevice(std::string identifier, int outputChannels)
{
    if (identifier.empty() || outputChannels != 2)
        return false;

    stop();
    device = std::move(identifier);
    masterChannels = outputChannels;
    currentState = AudioShellState::ready;
    return true;
}

bool AudioShell::start()
{
    if (currentState != AudioShellState::ready)
        return false;

    currentState = AudioShellState::running;
    return true;
}

void AudioShell::stop() noexcept
{
    if (currentState == AudioShellState::running)
        currentState = AudioShellState::ready;
}

} // namespace fishpond
