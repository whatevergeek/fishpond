#include "runtime/Runtime.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    const std::string test = argc == 2 ? argv[1] : "";
    fishpond::Runtime runtime;

    if (test == "UT_SardineCompat_PlayerAssignmentReplaces")
        return runtime.playerReplacementContract() ? 0 : 1;
    if (test == "UT_SardineCompat_PatternNotesAndLiterals")
        return runtime.patternContract() ? 0 : 1;
    if (test == "UT_SardineCompat_ChordGroups")
        return runtime.chordPatternContract() ? 0 : 1;
    if (test == "UT_SardineCompat_PeriodPattern")
        return runtime.periodContract() ? 0 : 1;
    if (test == "UT_SardineCompat_QuantizedReplacement")
        return runtime.quantizedReplacementContract() ? 0 : 1;
    if (test == "UT_SardineCompat_TargetNormalization")
        return runtime.targetNormalizationContract() ? 0 : 1;
    if (test == "UT_SardineCompat_TargetCollision")
        return runtime.targetCollisionContract() ? 0 : 1;
    if (test == "UT_SardineCompat_InvalidValues")
        return runtime.invalidValuesContract() ? 0 : 1;
    if (test == "IT_SardineCompat_UnknownTargetIsolation")
        return runtime.unknownTargetIsolationContract() ? 0 : 1;
    if (test == "IT_SardineCompat_SilenceAndPanic")
        return runtime.silenceAndPanicContract() ? 0 : 1;

    std::cerr << "unknown test: " << test << '\n';
    return 2;
}
