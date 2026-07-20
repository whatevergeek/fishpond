# Fishpond

Fishpond is a live-coding-first desktop DAW. Musicians evaluate Python using a
Fishpond-owned, Sardine-compatible runtime to schedule notes directly into
hosted VST3 instruments (and Audio Units on macOS).

## Current status

Fishpond has a macOS JUCE application with an embedded Python live-coding
runtime, four independently routed VST3 instrument slots, quantised player
scheduling, chord groups, rests, syntax-coloured editor feedback, and `.fp`
files for saving and loading Live Coding text. Instrument state, transport
state, and plugin selections are deliberately not yet part of `.fp` files.

See [the build guide](docs/build.md) for dependency pins, bootstrap choices,
and the configure/build/test commands.
