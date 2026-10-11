# Fishpond release notes

User-facing changes, compatibility notes, and known limitations for Fishpond.
Download published builds from [GitHub Releases](https://github.com/whatevergeek/fishpond/releases).

## Unreleased

The changes below are on the development branch. They are not a declaration
that a new packaged release is available.

### Added

- An optional, resizable AI assistant on the right side of **Live Coding**,
  with separate **Chat** and **Config** tabs.
- Ollama as the default provider, with installed-model discovery and refresh.
- OpenAI Responses API support with an API key supplied in Config or through
  `FISHPOND_OPENAI_API_KEY` / `OPENAI_API_KEY`.
- Independent, syntax-highlighted code cards for each assistant reply, with
  **Copy** and **Insert** actions. Inserting code does not evaluate it.
- Request-time context including the editor buffer, selection, cursor location,
  evaluator output, tempo, ready instrument aliases and plugin names, and
  scheduler patterns marked active, queued or paused.
- A context summary above the composer, with focus and playback details on hover.
- A new Fishpond app icon, also displayed in the README.

### Improved

- Chat layout with wrapped text, compact code cards, and a fixed composer.
- **Cmd+Return** to send, blank-message prevention, and one quiet activity
  indicator while a request is pending.
- Failed requests restore the submitted prompt when the composer is empty,
  preserving any newer draft. Errors include diagnostic details and recovery advice.
- Provider-response parsing and a bounded correction retry for response or
  code-review failures.
- Code review ignores Python comments, reports unmatched delimiter locations,
  and rejects repeated assignments to the same player in one suggestion.
- Assistant instructions favour changed patterns and discourage copying
  `silence()` or `panic()` from editor context into musical suggestions.
- README navigation, quick-start instructions, and assistant setup guidance.

### Fixed

- Earlier generated code disappearing when a later reply reused the same editor.
- Model refresh overwriting the latest chat reply.
- JSON fallback decoding discarding recoverable string fields.
- Punctuation in Python comments being mistaken for unbalanced source syntax.

### Validation and limitations

- The macOS application compiled successfully after the assistant implementation
  and reviewer fixes. End-to-end provider, UI, and playback validation is still
  pending; compilation alone does not establish release readiness.
- Chat history lasts for the current app session. Prior chat turns are not sent
  to the provider and are not saved across restarts.
- Code review is limited, not a full Python syntax or musical-correctness check.
- Active scheduler state does not prove audible output. Previously queued notes
  can finish after replacement or stop.
- Request context is sent to the configured provider. Review generated code
  before evaluating it with **Shift+Return**.

## Existing macOS preview

This is a feature baseline, not a versioned release entry. No release tags were
available in the local checkout when these notes were created; consult
[GitHub Releases](https://github.com/whatevergeek/fishpond/releases) for published
versions, dates, and downloadable artifacts.

- Native JUCE application with bundled CPython 3.12.
- Four independently routed VST3 instrument slots.
- Python live coding with named players, note sequences, rests, and chords.
- Pattern replacement, tempo and master-volume control, silence and panic.
- Syntax highlighting, evaluator output, and `.fp` source-file saving/loading.
- Asynchronous plugin loading and replacement.

### Compatibility

- The preview is distributed for macOS; Linux and Windows releases remain
  unvalidated, and Audio Units support is not validated.
- A compatible VST3 instrument is required. Fishpond does not bundle instruments
  or a built-in sound engine.
- `.fp` files save source text, not plugin selections, plugin state, or a complete
  session. Save your code before restarting or replacing the app.
- The macOS preview is unsigned and not notarized.

## Maintaining these notes

When packaging a release, move the relevant Unreleased changes under its actual
version and release date, link the published release, and record the validation
performed for that build. Keep remaining development work under Unreleased.
