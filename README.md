# Fishpond

Fishpond is a live-coding-first desktop DAW. Musicians evaluate Python using a
Fishpond-owned, Sardine-compatible runtime to schedule notes directly into
hosted VST3 instruments (and Audio Units on macOS).

## Build status

P0.2 provides the reproducible CMake/CTest skeleton only. It does not yet
contain JUCE, an embedded Python runtime, a plugin host, or musical behaviour.
The P0.1 Sardine compatibility corpus is registered as disabled contract tests
until the runtime work packages implement it; disabled tests are not passing
evidence for a phase gate.

See [the build guide](docs/build.md) for dependency pins, bootstrap choices,
and the configure/build/test commands.
