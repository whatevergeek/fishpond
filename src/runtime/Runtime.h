#pragma once

namespace fishpond {
class Runtime {
public:
    bool playerReplacementContract() const;
    bool patternContract() const;
    bool periodContract() const;
    bool quantizedReplacementContract() const;
    bool targetNormalizationContract() const;
    bool targetCollisionContract() const;
    bool invalidValuesContract() const;
    bool unknownTargetIsolationContract() const;
    bool silenceAndPanicContract() const;
};
}
