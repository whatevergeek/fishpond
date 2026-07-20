# P2 four-instrument real-VST3 smoke

Run this on macOS with supported instrument `.vst3` bundles that are not the
Fishpond fixture. This is a controlled four-slot load path, not catalogue scan
coverage.

1. Launch Fishpond, open **Instruments**, and click **Load Inst 01 VST3...** while audio is
   stopped. Select the instrument bundle and confirm the status reads
   `Instrument 01 VST3 ready: …`.
2. Start audio at 48 kHz stereo. In Live Coding, execute each of these lines
   separately:

   ```python
   Pa >> n("{C2 E2 G2}", target="instrument_01", p=0.5, dur=0.25, velocity=100)
   Pb >> n("C4", target="instrument_01", p=1, dur=0.5, velocity=80)
   ```

   Confirm both patterns are audible concurrently for two minutes, with no
   interruption, stuck note, or application error.
3. Replace only `Pa`:

   ```python
   Pa >> n("E2 G2", target="instrument_01", p=0.5, dur=0.25, velocity=100)
   ```

   Confirm `Pa` changes while `Pb` continues.
4. Execute this invalid target and confirm its diagnostic is shown while both
   existing patterns continue:

   ```python
   Pc >> n("C5", target="missing", p=1)
   ```
5. Execute `panic()` and confirm all audio stops with no sustained/stuck note.

6. Stop audio. Load a supported VST3 into each of **Load Inst 01 VST3...**
   through **Load Inst 04 VST3...**, then start audio and evaluate:

   ```python
   Pa >> n("C2", target="instrument_01", p=1, dur=0.25)
   Pb >> n("E2", target="instrument_02", p=1, dur=0.25)
   Pc >> n("G2", target="instrument_03", p=1, dur=0.25)
   Pd >> n("C3", target="instrument_04", p=1, dur=0.25)
   ```

   Confirm all four targets are accepted and audible through their loaded
   instruments. Stop one player (for example, `Pc.stop()`) and confirm the
   other three remain audible. Then execute `silence()` and confirm all four
   routes stop without a sustained note or application error.

   To stop several players together, select the separate stop lines and use
   **Shift+Return**. A control-only selection is applied line by line:

   ```python
   Pa.stop()
   Pb.stop()
   Pc.stop()
   Pd.stop()
   ```

Record the selected plugin name and pass/fail result in the GenFlex audit.
