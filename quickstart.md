# Fishpond Quickstart — 7-Minute Demo

**The story:** *Write a line of Python, press Shift+Return, and hear it, with no restart and no stopped music.* Fishpond is a native live-coding workspace that hosts your own VST3 instruments and is driven from Python.

**Audience takeaway:** you can perform music with code, change it while it plays, and ask an AI assistant for ideas without leaving the editor.

---

## Before the demo (do this 15 minutes early)

- [ ] `Fishpond.app` is in Applications and opens. It is unsigned, so use **right-click > Open** the first time.
- [ ] A VST3 synth is installed (for example [Surge XT](https://surge-synthesizer.github.io/) or [Vital](https://vital.audio/)).
- [ ] Load synths into **Instrument 01** and **Instrument 02** (and **03** if you will do the assistant scene) on the **Instruments** tab. Pick a pluck or piano-like sound for 01 for the melody and a soft pad for 02 for the chords.
- [ ] Start the audio engine and confirm the output device. Play a test line so you know the volume is right.
- [ ] Master volume is around `-12 dB`.
- [ ] Paste the code from the [Cheat sheet](#cheat-sheet-paste-ready) into the **Live Coding** tab, one scene at a time.
- [ ] **Assistant (optional):** with Ollama, run `ollama serve` and pick a model. With OpenAI, set `OPENAI_API_KEY`. Do a test Send.
- [ ] Turn off notifications and enter full screen (**View > Enter Full Screen**).
- [ ] Save the demo file as `demo.fp` so you can recover quickly.

> [!TIP]
> Keep `panic()` on its own line at the bottom of the editor. If anything goes wrong on stage, put the cursor on it and press Shift+Return.

---

## Run of show

| Time | Scene | What you show | What you say |
|---|---|---|---|
| 0:00 | **1. Hook** | The workspace: editor, console, instrument slots | "This is Fishpond. It's Python on one side and real synth plugins on the other." |
| 0:45 | **2. First sound** | One line of code, then Shift+Return | "One line. The line flashes to show what ran." |
| 2:00 | **3. Layer it** | A second player on another instrument | "Each player is independent and lives in a persistent session." |
| 3:30 | **4. Change it live** | Edit the pattern and re-evaluate | "I never stop the music. The new pattern replaces the old one on the beat." |
| 5:00 | **5. Safety net** | An unknown target, `silence()` and `panic()` | "Mistakes produce a message, not a crash, and the music keeps going." |
| 6:00 | **6. AI assistant** | Ask for a variation, insert it, evaluate it | "I review it before it runs. I stay in control." |
| 7:00 | **7. Close** | Where it is and what's next | See [Closing](#closing). |

---

## Scene 1 — Hook (0:45)

Show the main window. Point out four things:

1. **Live Coding** tab: the editor, with the Console pane below it.
2. **Instruments** tab: four VST3 slots. Instrument 01 and 02 are loaded.
3. The **master volume fader**.
4. **Show Assistant**: the optional AI panel (save it for Scene 6).

*Line:* "No SuperCollider setup. Bring your own synth plugins and write Python."

## Scene 2 — First sound (1:15)

Set the tempo and start a bass line:

Everyone knows this one: Beethoven's *Ode to Joy* (public domain).

```python
clock.bpm = 100
Pa >> n("E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 E4 D4 D4 .", target="instrument_01", p=1, dur=0.9)
```

Select both lines and press **Shift+Return**. Ask the audience to name the tune.

Explain the pattern:
- `Pa` is a named **player**.
- `"E4 E4 F4 G4 ..."` is a note sequence, and `.` is a rest.
- `p=1` is the pattern period in beats, so one note per beat.
- `dur=0.9` is the note length, slightly detached.
- `target="instrument_01"` picks the synth slot.

## Scene 3 — Layer it (1:30)

Add a one-chord-per-bar accompaniment on the other instrument (C, G, C, G):

```python
Pb >> n("{C3 E3 G3} {G2 B2 D3} {C3 E3 G3} {G2 B2 D3}", target="instrument_02", p=4, dur=3.5)
```

Press Shift+Return. Two independent players now run on two instruments, and the braces play each chord together.

## Scene 4 — Change it live (1:30)

Never stop the music. Swap the melody for *Twinkle, Twinkle, Little Star* (public domain, Mozart's variations on a French folk tune) and re-harmonise it:

```python
clock.bpm = 110
Pa >> n("C4 C4 G4 G4 A4 A4 G4 . F4 F4 E4 E4 D4 D4 C4 .", target="instrument_01", p=1, dur=0.9)
Pb >> n("{C3 E3 G3} {F3 A3 C4} {F3 A3 C4} {G2 B2 D3}", target="instrument_02", p=4, dur=3.5)
```

Point out:
- Assigning to `Pa` again **replaces** the pattern. It doesn't stack a duplicate.
- Changes land on the quantization boundary, so they stay in time.
- Pull the **master volume** fader or run `master.volume = -18`.

Show the Console: `print("hello fishpond")`. Output appears in the Console. **Cmd+K** clears it.

## Scene 5 — Safety net (1:00)

Make a deliberate mistake:

```python
Pc >> n("C4 E4 G4", target="nope", p=1)
```

Fishpond reports an unknown target as a diagnostic, and **Pa and Pb keep playing**.

Then stop things:

```python
silence(Pb)   # stop one player
silence()     # stop all players
panic()       # stop all players and clear pending instrument events
```

*Line:* "On stage, that's the difference between a bad moment and a ruined set."

## Scene 6 — AI assistant (1:00)

1. Click **Show Assistant** in the Live Coding tab.
2. In **Chat**, ask: *"Add a simple counter-melody on a third instrument that fits over my Twinkle Twinkle melody."* (Load a third synth into Instrument 03 beforehand.)
3. Show the highlighted code in the reply. Point out that Fishpond sends your editor buffer, tempo, and ready instruments as context.
4. Click **Insert**, read the code, then press **Shift+Return**.

*Line:* "It suggests and I decide. Nothing runs until I press Shift+Return."

> [!WARNING]
> The assistant is under active development. Code review is limited, and end-to-end provider and playback validation is still in progress. Request context is sent to your configured provider. If the provider is slow or offline, skip this scene and say it is optional.

## Closing

*Where it stands:* a macOS preview with an embedded Python 3.12 runtime, four routed VST3 instrument slots, named players, pattern replacement, `silence()` and `panic()`, `.fp` file save and load, and an optional assistant (Ollama or OpenAI).

*What's next:* a searchable plugin catalogue, multi-channel mixer controls, effects and parameter automation, and project save and restore.

*Call to action:* "Try it from GitHub Releases. All you need is a VST3 synth."

---

## Cheat sheet (paste-ready)

```python
# --- Scene 2: first sound (Ode to Joy) ---
clock.bpm = 100
Pa >> n("E4 E4 F4 G4 G4 F4 E4 D4 C4 C4 D4 E4 E4 D4 D4 .", target="instrument_01", p=1, dur=0.9)

# --- Scene 3: layer (chords: C G C G) ---
Pb >> n("{C3 E3 G3} {G2 B2 D3} {C3 E3 G3} {G2 B2 D3}", target="instrument_02", p=4, dur=3.5)

# --- Scene 4: change live (Twinkle Twinkle) ---
clock.bpm = 110
Pa >> n("C4 C4 G4 G4 A4 A4 G4 . F4 F4 E4 E4 D4 D4 C4 .", target="instrument_01", p=1, dur=0.9)
Pb >> n("{C3 E3 G3} {F3 A3 C4} {F3 A3 C4} {G2 B2 D3}", target="instrument_02", p=4, dur=3.5)
master.volume = -18
print("hello fishpond")

# --- Scene 5: safety ---
Pc >> n("C4 E4 G4", target="nope", p=1)
silence(Pb)
silence()

# --- Emergency ---
panic()
```

| Action | Shortcut |
|---|---|
| Evaluate line or selection | **Shift+Return** |
| Send to assistant | **Cmd+Return** |
| Clear Console | **Cmd+K** |
| Full screen | **View > Enter Full Screen** |

## Fallbacks if something goes wrong

| Symptom | Fix |
|---|---|
| No sound | Check that the plugin shows as ready, the audio engine is started, and the target name matches the slot (`instrument_01`). |
| Stuck or hanging notes | Run `panic()`. |
| Unknown-target message you didn't intend | Check the spelling of `target=`, and that the slot is loaded. |
| Assistant is slow or errors | Skip it. Failed prompts are restored in the composer, so you can retry. |
| macOS refuses to open the app | **Right-click > Open**, then confirm. The preview is not notarized. |

## Known limitations (be upfront)

- macOS only. Windows, Linux, and Audio Units are not validated.
- You must install your own VST3 instrument. Fishpond ships no sounds.
- `.fp` files save code only, not plugin selection or state. Re-load your instruments after a restart.
- The plugin loader is controlled for now. There is no searchable catalogue yet.
- Chat history is not saved, and previous turns aren't sent to the model.

