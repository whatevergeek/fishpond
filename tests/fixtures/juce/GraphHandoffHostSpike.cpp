#include "FixtureInstrument.h"
#include "host/PreparedGraphHandoff.h"
#include "host/SingleChannelHost.h"

#include <iostream>
#include <memory>

namespace {
bool require(bool condition, const char* message)
{
    if (! condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main()
{
    fishpond::SingleChannelHost host({ 48'000.0, 64, 1 });
    fishpond::PreparedGraphHandoff<juce::AudioProcessor, 4> handoff;
    std::string diagnostic;

    const auto prepared = host.prepareInstrument(std::make_unique<FixtureInstrument>(), diagnostic);
    const auto queued = handoff.submit({ host.releasePrepared(), 1, 1 });
    fishpond::PreparedGraphCommand<juce::AudioProcessor> command;
    const auto taken = handoff.tryTake(command);
    juce::AudioProcessor* retired = nullptr;
    const auto adopted = host.applyRawPreparedAtBlockBoundaryNoDiagnostic(command.graph,
                                                                           command.configurationVersion,
                                                                           retired);
    const auto retiredNothing = handoff.retireFromAudio(retired);

    host.reconfigureForDevice({ 48'000.0, 64, 2 });
    const auto replacementPrepared = host.prepareInstrument(std::make_unique<FixtureInstrument>(), diagnostic);
    const auto replacementQueued = handoff.submit({ host.releasePrepared(), 1, 2 });
    const auto replacementTaken = handoff.tryTake(command);
    const auto staleRejected = ! host.applyRawPreparedAtBlockBoundaryNoDiagnostic(command.graph,
                                                                                    command.configurationVersion,
                                                                                    retired);
    const auto staleRetired = handoff.retireFromAudio(command.graph);
    auto workerOwner = handoff.reclaimOnWorker();

    return require(prepared && queued && taken && adopted && retiredNothing,
                   "prepared graph was not adopted at the audio boundary")
        && require(replacementPrepared && replacementQueued && replacementTaken && staleRejected && staleRetired,
                   "stale graph command was not safely rejected")
        && require(workerOwner != nullptr, "stale graph was not returned to the worker") ? 0 : 1;
}
