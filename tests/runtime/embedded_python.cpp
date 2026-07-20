#include "runtime/EmbeddedPythonRuntime.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    fishpond::EmbeddedPythonRuntime runtime;
    if (! runtime.ready()) {
        std::cerr << runtime.lastDiagnostic() << '\n';
        return 1;
    }

    const std::string test = argc == 2 ? argv[1] : "";
    if (test == "persistent")
        return runtime.evaluate("tempo = 120").accepted && runtime.evaluate("assert tempo == 120").accepted ? 0 : 1;
    if (test == "error-isolation")
        return runtime.evaluate("player = 'valid'").accepted
            && ! runtime.evaluate("raise RuntimeError('broken evaluation')").accepted
            && runtime.evaluate("assert player == 'valid'").accepted ? 0 : 1;
    if (test == "player-replacement")
        return runtime.evaluate("Pa >> n('C2', target='bass')").accepted
            && runtime.evaluate("Pa >> n('D2', target='bass')").accepted
            && runtime.evaluate("assert _fishpond_players['Pa'].notes == 'D2'").accepted ? 0 : 1;
    if (test == "silence-panic")
        return runtime.evaluate("Pa >> n('C2', target='bass')").accepted
            && runtime.evaluate("Pb * n('D2', target='bass')").accepted
            && runtime.evaluate("silence(); assert not _fishpond_players").accepted
            && runtime.evaluate("Pa >> n('C3', target='bass'); panic(); assert not _fishpond_players").accepted ? 0 : 1;
    if (test == "single-player-stop")
        return runtime.evaluate("Pa >> n('C2', target='bass'); Pb >> n('D2', target='bass')").accepted
            && runtime.evaluate("silence(Pa); assert 'Pa' not in _fishpond_players and 'Pb' in _fishpond_players").accepted
            && runtime.evaluate("Pa >> n('E2', target='bass'); Pa.stop(); assert 'Pa' not in _fishpond_players and 'Pb' in _fishpond_players").accepted ? 0 : 1;
    if (test == "multiple-player-stop")
        return runtime.evaluate("Pa >> n('C2', target='bass')\nPb >> n('D2', target='bass')\nPc >> n('E2', target='bass')\nPd >> n('F2', target='bass')").accepted
            && runtime.evaluate("Pa.stop()\nPb.stop()\nPc.stop()\nPd.stop()\nassert not _fishpond_players").accepted ? 0 : 1;
    if (test == "target-required") {
        const auto result = runtime.evaluate("Pa >> n('C2')");
        return ! result.accepted && result.diagnostic.find("FP_TARGET_REQUIRED") != std::string::npos ? 0 : 1;
    }
    if (test == "clock-state") {
        const auto invalidTempo = runtime.evaluate("clock.bpm = 0");
        const auto tempoChange = runtime.evaluate("clock.bpm = 126; clock.advance(4.5)");
        return tempoChange.accepted && tempoChange.changedTempoBpm && *tempoChange.changedTempoBpm == 126.0
            && runtime.evaluate("assert clock.tempo == 126 and clock.bar == 1 and clock.phase == 0.5").accepted
            && runtime.evaluate("clock.tempo *= 2").changedTempoBpm == std::optional<double>(252.0)
            && runtime.evaluate("clock.pause(); clock.advance(2); assert clock.beat == 4.5").accepted
            && runtime.evaluate("clock.start(); clock.advance(1.5); clock.stop(); assert not clock.running and clock.beat == 0").accepted
            && ! invalidTempo.accepted && invalidTempo.diagnostic.find("FP_TEMPO_INVALID") != std::string::npos ? 0 : 1;
    }
    if (test == "syntax-location") {
        const auto result = runtime.evaluate("tempo =\n");
        return ! result.accepted && result.diagnostic.find("line 1") != std::string::npos ? 0 : 1;
    }

    return 2;
}
