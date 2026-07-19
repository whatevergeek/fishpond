# P1 MVP real-VST3 smoke

Run this on macOS with a supported instrument `.vst3` bundle that is not the
Fishpond fixture. This is a controlled single-plugin load, not catalogue scan
coverage.

1. Launch Fishpond, open **Mixer**, and click **Load VST3…** while audio is
   stopped. Select the instrument bundle and confirm the status reads
   `Bass VST3 ready: …`.
2. Start audio at 48 kHz stereo. In Live Coding, execute each of these lines
   separately:

   ```python
   Pa >> n("C2 C3", target="bass", p=0.5, dur=0.25, velocity=100)
   Pb >> n("C4", target="bass", p=1, dur=0.5, velocity=80)
   ```

   Confirm both patterns are audible concurrently for two minutes, with no
   interruption, stuck note, or application error.
3. Replace only `Pa`:

   ```python
   Pa >> n("E2 G2", target="bass", p=0.5, dur=0.25, velocity=100)
   ```

   Confirm `Pa` changes while `Pb` continues.
4. Execute this invalid target and confirm its diagnostic is shown while both
   existing patterns continue:

   ```python
   Pc >> n("C5", target="missing", p=1)
   ```
5. Execute `panic()` and confirm all audio stops with no sustained/stuck note.

Record the selected plugin name and pass/fail result in the GenFlex audit.
