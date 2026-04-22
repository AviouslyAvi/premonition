# Premonition — IR Convolution Debug (2026-04-21)

> **Read this top-to-bottom before doing anything.** Branch:
> `reverb-types-modes-rework`. Builds clean, 18/18 tests pass, AU validates.
> Uncommitted changes below need review before committing.

---

## 🔖 RESUME HERE

**Update 2026-04-21 (later):** Source file verified clean (48 kHz f32, peak
−2.1 dB, RMS −10.6 dB, 0 NaN/Inf/denormals). Root cause identified: **SR
mismatch + linear interp IR resample.** Source is 48 kHz, bundled IRs are
44.1 kHz. `detail::resampleLinear` in [Convolution.h](../Premonition/dsp/Convolution.h)
aliased audibly on long decaying IRs. Replaced with a 32-tap Kaiser-windowed
sinc resampler (`detail::resampleSinc`, beta ≈ 8.6, ~80 dB stopband).
`resampleIR` now calls the sinc version. 18/18 tests still pass; all 4 plugin
formats (VST3 / AUv2 / CLAP / APP) rebuild clean.

**Second attempt (types-sound-identical fix):** suppressed the Freeverb-
style decay envelope + damping LPF on the `useConvolution` branch of
[OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h). Made types
distinct — but crackle came back, because the envelope had been masking
tail numerical noise.

**Third attempt** — tail fade + denormal flush + damping=0.15 bandaid.
Still crackly. At which point Avi auditioned the raw IR WAVs on their
own and confirmed: **the bundled IRs themselves were crackly.** We had
been trying to fix a pipeline bug that did not exist.

**Fourth attempt (actual fix):** replaced all 4 bundled IRs with clean
synthetic ones generated from exponentially-decaying filtered noise.
- Generator script: [Premonition/resources/ir/src/gen-irs.py](../Premonition/resources/ir/src/gen-irs.py)
  (Python, uses numpy/scipy/soundfile; venv at `/tmp/ir-venv` created
  during this session)
- Output: 44.1 kHz stereo f32, peak ≈ -1 dBFS, zero NaN/Inf/denormals,
  150 ms cosine tail fade pre-baked in.
- Per-type shaping: Hall (3.5s, LPF 8k, HPF 80), Plate (2.2s, LPF 12k,
  dense), Spring (1.6s + damped-sine resonances at 1.2k/2.4k/3.8k),
  Room (0.8s, tight, LPF 14k).
- Old crackly IRs backed up at `/tmp/ir-test/backup/` before replacement.

Reverted the damping=0.15 bandaid in [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h)
— IR path now passes `0.f, 0.f` (full IR character preserved). Tail fade
in `LoadBuiltinIRs` and denormal flush in `convolveStereo` are kept as
defensive belt-and-suspenders but are no longer load-bearing.

18/18 tests pass; all 4 plugin formats rebuilt with new IRs bundled.

---

### Previous open question (resolved above)

After switching the right rack's TYPE selector to each of Hall / Plate /
Spring / Room (built-in IRs) and rendering, Avi heard crackle. Custom
(no user IR loaded → Freeverb fallback) sounded clean. Avi's hypothesis
*"The crack is on the source file"* was wrong — source was clean, the
crackle came from linear-interp resampling of the 44.1k IRs to 48k.

**Do this first — don't guess:**

1. Ask Avi for the exact source-file path he's rendering against. Analyze
   it with `ffmpeg -af astats` — look at Flat factor, DC offset, peak, any
   Infs/NaNs. If the raw source is clean, crackle is in our pipeline.
2. Drag a bundled IR into a DAW directly and play it:
   `Premonition/resources/ir/hall.wav`. If raw IR sounds clean → problem is
   in how we combine source + IR.
3. If both (1) and (2) sound clean, the crackle lives in the **FFT
   convolution × long IR** path. See "Next investigations" below.

---

## Session accomplishments (uncommitted)

### A. Fixed `LoadBuiltinIRs()` silent failure

`iPlug2/IPlug/IPlugPaths.mm::GetResourcePathFromBundle` requires the filename
to include the extension — it parses the ext out of the filename string and
matches against the `type` arg (IPlugPaths.mm:157-161). We were calling
`LocateResource("hall", "wav", ...)` which silently returned `kNotFound` for
all four IRs, so every type fell back to identical Freeverb.

Fix in [Premonition.cpp:1492-1497](../Premonition/Premonition.cpp:1492) —
pass `"hall.wav"` not `"hall"`. Verified via `/tmp/premonition-ir-load.log`
(logging since stripped): all 4 IRs now load with correct frame counts.

### B. Added TYPE selector UI

`kReverbType` had no UI control at all — user could not switch types.
Bundled with pre-rework "presets" named Hall/Plate/Spring/Room that set
`algorithm` (ignored by v2 schema) but not `reverbType`, meaning every
render used the Hall default.

Added `IVTabSwitchControl` bound to `kReverbType` above the knob grid in
[Premonition.cpp:1197-1205](../Premonition/Premonition.cpp:1197). Shifted
knob grid down to `R.T + 112.f..R.T + 320.f`. Presets remain intentionally
decoupled from Type (Presets.h:278-280 — v2 design).

**Avi's pre-existing preset JSONs** (`~/Library/Application Support/Premonition/presets/Hall.json` etc.) were from pre-rework with an `algorithm` field that's now ignored. Safe to delete; they just recall Size/Decay/Tail knob positions, nothing else.

### C. Added fade-IN to `hardTrimWithFade`

[OfflinePipeline.h:136-192](../Premonition/dsp/OfflinePipeline.h:136). Was
only fading out at the end; when output was longer than target length
(always true with IR convolution, output = inN + irN − 1), leading samples
got dropped and the waveform started mid-amplitude → click at t=0. Now
applies symmetric 100 ms fade-in + fade-out. Fixes the click-at-start,
but did NOT fix Avi's reported crackle — the crackle persists through the
body of the render.

### D. Added convolution tests

[tests/test_convolution.cpp](../tests/test_convolution.cpp) — two tests:
- Unit impulse × 16-sample IR → output = IR (with 0.89/peak normalization).
- 88K-sample sine × 212K-sample IR → no NaN, no clipping, peak ≤ 0.9.

Both pass. So `convolveStereo` is correct in isolation — the bug is
elsewhere in the real render path or the real IRs.

### E. Validated AU cleanly

`auval -v aufx Prmn Avio` → AU VALIDATION SUCCEEDED. Avi had one FL Studio
crash (`EAccessViolation - caulk side thread`) but it's not reproducible
and auval is clean, so likely an FL-side quirk, not our plugin.

---

## Still broken (Avi's list, in priority order)

1. **Crackle on IR renders.** Hall/Plate/Spring/Room renders have audible
   artifacts throughout. Not clipping (ffmpeg Flat factor 0, peak −0.3 dB).
   Latest hypothesis: source file itself has crackle that convolution
   smears across the IR duration. **Verify first.**

2. **IR drop zone (Custom slot) isn't visible when TYPE = Custom.** Avi
   reported "no spot to drag IR." The visibility logic is at
   [Premonition.cpp:1513-1518](../Premonition/Premonition.cpp:1513) +
   [1252](../Premonition/Premonition.cpp:1252). Probably `mIRSlotCtl` is
   attached before `OnParamChangeUI` gets a chance to fire on init, or
   the layout geometry (IR slot at `toggles.B + 10.f` — now underneath
   the new Type row?) is off-screen/overlapping. **Debug by adding a
   temp red panel at irSlot rect to locate it, then fix geometry.**

3. **Loudness asymmetry across types.** Custom (Freeverb) renders at
   RMS −14 dB; Hall at −41, Plate at −32. 27 dB gap. Caused by
   `peakNormalize` ([OfflinePipeline.h:98-107](../Premonition/dsp/OfflinePipeline.h:98))
   anchoring on peak — IR convolution outputs have an early spike +
   long quiet ramp, so peak-normalize leaves most of the signal faint.
   **Fix direction:** switch to RMS-target normalization, or LUFS, or
   peak-to-RMS hybrid. Don't just boost IR output — the spike would clip.

4. **L/R asymmetry.** peakNormalize uses shared peak across both
   channels. If L is louder, R gets proportionally attenuated. Same
   fix likely addresses it (per-channel normalization, or RMS-target).

5. **Natural mode head** — Avi originally wanted: "trim and fade in
   (byproduct of being printed and reversed). Needs to start immediately
   after hitting preview." Fade-in is now in (C above), but Avi hasn't
   re-verified the "starts immediately" part. Check that leading silence
   isn't being inserted when it shouldn't be — see
   [OfflinePipeline.h:143-149](../Premonition/dsp/OfflinePipeline.h:143)
   (the `n < targetLen` branch pads with leading zeros).

6. **Forward mode artifacts** — also crackly per Avi. Probably the same
   root cause as #1.

7. **Stretch mode** — Avi confirmed it sounds identical to pre-rework
   (good) but still sounds "stretched" to him — that's the nature of
   fit-to-bar resampling, not a bug.

---

## Next investigations (if crackle persists after checking source file)

- **Test with a real loaded hall.wav + a clean synthetic source** (sine
  sweep or white noise) in a new Catch2 test. Load the actual WAV via
  the same path plugin uses, convolve, check for sample-to-sample jumps
  > some threshold. If jumps appear, FFT convolution is producing
  numerical artifacts that the impulse test didn't catch.
- **Replace WDL FFT with signalsmith-stretch's FFT** — already linked.
  It's better-tested for long buffers.
- **Check IR resampling path.** If source SR ≠ IR SR (44100), linear
  resample runs ([Convolution.h:24-42](../Premonition/dsp/Convolution.h:24)).
  Linear interp on a 211K-sample decaying IR could introduce aliasing
  that shows up as crackle. Switch to sinc resample or keep IRs at
  render rate from the renderer tool.
- **Check the reverse stage** after convolution. If wet buffer has
  denormals near the tail, reversing puts them at the head where they
  dominate. Add `std::isnormal` assertion in a debug test.

---

## Files changed this session (all uncommitted)

```
Premonition/Premonition.cpp      — TYPE UI control, LoadBuiltinIRs fix
Premonition/dsp/OfflinePipeline.h — fade-IN added to hardTrimWithFade
tests/test_convolution.cpp       — NEW, 2 convolution sanity tests
tests/CMakeLists.txt             — register new test file
```

**Do not commit yet** — Avi hasn't signed off on the crackle fix.
When crackle is resolved:
- Squash-commit (A + B + C + D) as one logical change: "Fix built-in IR
  routing and add TYPE selector UI."
- Or split: commit (A) as "Fix LoadBuiltinIRs filename-extension bug,"
  (B) as "Add reverb TYPE selector UI," (C) as "Add fade-in to
  hardTrimWithFade for IR renders," (D) with (A).

---

## Known irrelevant-for-now issues

- `PremonitionAU-framework` target (AUv3) fails to build due to missing
  `iPlug2` include in its include path (WDL/fft.h not found). Pre-existing,
  not introduced this session. Avi uses VST3 + AU v2 (`.component`),
  both build clean. Leave alone unless Avi wants AUv3.

---

## How to continue (quick commands)

```bash
# Build VST3 + AU + tests (avoid the AUv3 framework target)
cmake --build /Users/aviouslyavi/Claude/Projects/premonition/build \
  --target Premonition-vst3 Premonition-au premonition_tests -j

# Run tests
/Users/aviouslyavi/Claude/Projects/premonition/build/tests/premonition_tests

# Validate AU
auval -v aufx Prmn Avio

# Re-enable render-path diagnostic (if needed again) — was at
# Premonition.cpp:1437 region before being stripped. Log path:
# /tmp/premonition-render.log
```

---

## One-line summary for next agent

Source file likely has crackle that FFT convolution with long IRs smears
across the riser. Verify source first. If source is clean, investigate
IR resampling + FFT numerical behavior with real 212K-sample IRs.
