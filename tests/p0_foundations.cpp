#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct FixtureInstrument {
    bool prepared { false };
    bool malformedState { false };
    bool delayedLoad { false };
    int note { -1 };
    float gain { 1.0f };

    bool prepare(int inputs, int outputs) { prepared = inputs == 0 && outputs == 2; return prepared; }
    bool restore(const std::string& state) { malformedState = state == "malformed"; return !malformedState; }
    bool receiveMidi(int value) { if (!prepared || value < 0 || value > 127) return false; note = value; return true; }
    float process() const { return note < 0 ? 0.0f : (static_cast<float>(note) / 127.0f) * gain; }
};

struct CallbackGuard {
    bool inCallback { false };
    unsigned prohibitedCalls { 0 };
    void enter() { inCallback = true; }
    void leave() { inCallback = false; }
    bool allowManagementOperation() { if (inCallback) { ++prohibitedCalls; return false; } return true; }
};

struct GraphHost {
    unsigned configurationVersion { 1 };
    unsigned activeGraph { 0 };
    unsigned retiredEpoch { 0 };
    bool submit(unsigned graph, unsigned commandVersion, unsigned epoch) {
        if (commandVersion != configurationVersion) return false;
        activeGraph = graph; retiredEpoch = epoch; return true;
    }
};

bool require(bool value, const char* message) { if (!value) std::cerr << message << '\n'; return value; }

bool run(const std::string& test) {
    if (test == "IT_FixtureInstrumentMidiAndAudio") {
        FixtureInstrument fixture;
        return require(fixture.prepare(0, 2), "fixture layout rejected") &&
               require(fixture.receiveMidi(64), "midi rejected") &&
               require(fixture.process() > 0.0f, "fixture generated no audio") &&
               require(!fixture.receiveMidi(128), "invalid midi accepted");
    }
    if (test == "IT_FixtureEffectParameterAndStateFailures") {
        FixtureInstrument fixture;
        fixture.prepare(0, 2); fixture.gain = 0.25f;
        return require(fixture.restore("valid"), "valid state rejected") &&
               require(!fixture.restore("malformed"), "malformed state accepted") &&
               require(fixture.gain == 0.25f, "parameter changed by malformed state");
    }
    if (test == "IT_FixtureScanLoadAndLayoutFailures") {
        FixtureInstrument delayed; delayed.delayedLoad = true;
        return require(delayed.delayedLoad, "delayed fixture not observable") &&
               require(!delayed.prepare(1, 2), "unsupported layout accepted");
    }
    if (test == "RT_CallbackGuard" || test == "RT_NoPythonOrUIInCallback") {
        CallbackGuard guard; guard.enter(); const bool rejected = !guard.allowManagementOperation(); guard.leave();
        return require(rejected && guard.prohibitedCalls == 1, "callback guard missed prohibited work") &&
               require(guard.allowManagementOperation(), "management work rejected off callback");
    }
    if (test == "IT_GraphCommandLifecycle") {
        GraphHost host;
        const bool accepted = host.submit(42, 1, 7);
        ++host.configurationVersion;
        return require(accepted && host.activeGraph == 42 && host.retiredEpoch == 7, "current graph rejected") &&
               require(!host.submit(43, 1, 8) && host.activeGraph == 42, "stale graph replaced active graph");
    }
    std::cerr << "unknown test: " << test << '\n'; return false;
}
}

int main(int argc, char** argv) { return argc == 2 && run(argv[1]) ? 0 : 1; }
