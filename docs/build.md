# Fishpond build specification

## Scope

Fishpond currently provides the P0 verification suite, the P1.1 macOS
desktop/audio shell, and the early P1.2 embedded-Python evaluation path. The
plugin host and audible instrument channel are later P1 work.

## Approved pins

| Component | Pin | Policy |
|---|---|---|
| Language | C++17 | Required for every Fishpond target. Compiler extensions are disabled. |
| CMake | 3.25 or newer | Presets version 6; macOS uses Unix Makefiles, Windows uses Visual Studio 2022 x64, and Linux uses Ninja. |
| JUCE | `8.0.13` / `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` | Stable release tag and verified immutable commit, used under AGPLv3. |
| CPython | `3.12.13` | Embedded-runtime baseline. Distribution packaging must use a verified CPython artifact appropriate to the platform; P0.2 does not consume the local Python installation. |
| macOS compiler | Apple Clang 16 or newer | Build universal-binary policy is deferred to P4. |
| Windows compiler | MSVC 19.38 or newer (VS 2022 17.8+) | Use the x64 native tools environment. |
| Linux compiler | GCC 13 or newer, or Clang 16 or newer | Use a supported distribution image in CI. |

The JUCE tag is intentionally version-pinned rather than branch-pinned. The
P0.2 environment could not resolve GitHub to record the annotated-tag object;
P0.6 must fail evidence publication until the tag resolves to a recorded commit
and the acquired archive/checksum is retained. Do not substitute `master` or
`develop`.

## Licensing

Fishpond is intended for distribution under **AGPL-3.0-or-later**. JUCE is used
under its AGPLv3 option. This permits the JUCE host and fixture work in Phase 0
without a commercial JUCE licence; every distributed Fishpond build and its
corresponding source must satisfy the applicable AGPL obligations. The project
does not incorporate Sardine source or packages.

## Bootstrap

Install CMake 3.25+, the generator selected by the platform preset, and a listed compiler. The P0.2 smoke target has no JUCE or Python dependency:

```sh
cmake --preset dev-macos
cmake --build --preset dev-macos
ctest --preset p0-build-contract-macos
```

Use the matching `dev-windows`/`p0-build-contract-windows` or
`dev-linux`/`p0-build-contract-linux` presets on those hosts.

The build-contract preset runs only `BUILD-CONTRACT` tests. The nine
`SARDINE-COMPAT;PENDING-RUNTIME` tests are deliberately disabled contracts;
they make the required IDs visible to CTest but are not passing evidence. P1.2
implements and turns this corpus green; P0 evidence preserves their pending status.

## CI coverage

`.github/workflows/p0-build-contract.yml` runs the same configure/build/test
contract on macOS, Windows, and Linux. It deliberately runs only the
`BUILD-CONTRACT` label while the Sardine-compatible runtime does not exist.
P0.6 publishes the traceability manifest. The workflow must retain it as an
artifact and fail if any required P0 ID is missing, disabled, or fails.

Run `cmake --build --preset dev-macos --target traceability-report` to create
`build/dev-macos/traceability.json`. It exits non-zero when a required P0 test
is missing or disabled. The P1 runtime corpus remains separately pending.

The Phase 0 evidence package records JUCE VST3/AU smoke and platform-runner
status with the GitHub Actions run and artifact name that produced each result.
The initial build-contract evidence is run
[`29654991101`](https://github.com/whatevergeek/fishpond/actions/runs/29654991101):
macOS, Windows, Linux, and the macOS JUCE VST3/AU smoke job passed. The
cross-platform JUCE lifecycle evidence is run
[`29666295778`](https://github.com/whatevergeek/fishpond/actions/runs/29666295778):
the Windows and Linux lifecycle jobs also passed. Required external checks are
only marked passing after their own run and artifact are recorded.

## Adding the first JUCE target

An implementation target must call `fishpond_require_juce()` before linking
JUCE modules. Supply either:

```sh
cmake --preset dev-macos -DFISHPOND_JUCE_SOURCE_DIR=/absolute/path/to/verified/JUCE
```

or explicitly opt into fetching the recorded tag:

```sh
cmake --preset dev-macos -DFISHPOND_FETCH_JUCE=ON
```

The verified checkout must be the recorded `8.0.13` tag object. Never add a
Sardine package dependency: Fishpond implements its own compatible runtime as
recorded in the SDD repository's `sardine-compatibility-decision.md`.

## Build and run the macOS desktop app

From the Fishpond source repository, configure the app preset, build its
target, then open the resulting bundle:

The development build requires the approved CPython 3.12.13 headers and
framework. On an Apple Silicon Homebrew installation:

```sh
brew install python@3.12
```

This is a developer-machine build dependency only. Fishpond's release bundle
will carry its approved Python runtime; musicians will not be asked to install
Python separately.

```sh
cmake --preset app-macos -DFISHPOND_FETCH_JUCE=ON
cmake --build --preset app-macos --target FishpondApp
open build/app-macos/app/FishpondApp_artefacts/Debug/Fishpond.app
```

The first command fetches the pinned JUCE `8.0.13` source and therefore needs
network access. If you already have a verified JUCE checkout (including the
fixture checkout created by `juce-fixture-macos`), configure with that source
instead:

```sh
cmake --preset app-macos \
  -DPython3_ROOT_DIR=/opt/homebrew/opt/python@3.12 \
  -DFISHPOND_JUCE_SOURCE_DIR="$PWD/build/juce-fixture-macos/_deps/juce-src"
cmake --build --preset app-macos --target FishpondApp
open build/app-macos/app/FishpondApp_artefacts/Debug/Fishpond.app
```

The app evaluates editor requests on a dedicated embedded-Python thread. Press
**Shift+Return** to evaluate the current line or the selected lines; the
submitted line range flashes orange once. The Live Coding editor recognises
Fishpond/Sardine-style helpers including `n()`, chord groups such as
`{C3 E3 G3}`, rests (`.`), named players, and `silence()`.

Use the native **File** menu (or standard macOS shortcuts) to manage Live
Coding text files:

- **New** (`Cmd+N`) clears the editor after a discard confirmation when it has
  unsaved changes.
- **Open…** (`Cmd+O`) loads a `.fp` file after the same confirmation.
- **Save** (`Cmd+S`) saves the current `.fp`, or opens Save As for a new file.
- **Save As…** (`Cmd+Shift+S`) creates a `.fp` file; Fishpond appends the
  extension when necessary.

For now, `.fp` stores only the Live Coding text. It does not store loaded
plugins, instrument aliases, audio-device selection, BPM, or active players.

A ready instrument channel is required before notes can be heard. Use the
**Instruments** tab to load a VST3 into one of the four slots, then route with
`target="instrument_01"` through `target="instrument_04"`.

VST3 preparation runs away from the audio callback through JUCE's asynchronous
VST3 creation path. You may replace an instrument while audio is running: the
previous instrument continues playing during loading, then Fishpond adopts the
fully prepared replacement at the next audio-block boundary. The slot's load
and plugin-UI buttons are unavailable until that handoff completes. A plug-in
may briefly keep the UI busy while it initialises, but it must never run on the
audio callback.

On macOS, build the real VST3 fixture with `cmake --preset juce-fixture-macos`
then `cmake --build --preset juce-fixture-macos --target FishpondFixtureInstrument_VST3`.
