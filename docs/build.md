# Fishpond build specification

## Scope

This is the P0.2 build skeleton. It verifies the C++17/CMake/CTest contract
without downloading JUCE or starting an embedded interpreter. The first JUCE
host target and CPython embedding arrive in later P0 packages.

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

On macOS, build the real VST3 fixture with `cmake --preset juce-fixture-macos`
then `cmake --build --preset juce-fixture-macos --target FishpondFixtureInstrument_VST3`.
