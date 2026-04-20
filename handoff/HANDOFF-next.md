# Premonition — Next-Session Handoff (2026-04-18)

**For a fresh Claude session. Read this first, then ask Avi which item to start.**

## Items 1, 2, 3 — DONE this session
- **#1** Render + Preview flat `ActionButtonControl` — shipped.
- **#2** BPM-sync reverb tail padding — shipped. `kReverbTailBeats=8`, clamped [2s,15s]. `setBPM` rebuilds from Reverb stage.
- **#3** Decay expressed in beats — shipped. Discrete steps `1/4…16` (same labels as Tail). `applyReverbStereo(..., rt60Seconds, damping, mix)` — per-comb feedback solved via `g = 10^(-3*D / (fs*RT60))`. Damping tied to step index (`1/4` = brightest, `16` = darkest). `kReverbMinRT60Seconds=0.05` floor so extreme BPM+short-beat combos don't click.

## Still pending (in order): #4 Normalize, #5 Stacked waveforms.
Open question still unanswered for #4: normalize target `-0.3 dBFS` vs `0 dBFS`.

## Most recent increment (item #1 — DONE)
- Render + Preview buttons now use `ActionButtonControl` (existing flat-style class, lines 306-349 of Premonition.cpp) instead of `IVButtonControl`. Oxblood fill for Render, terracotta for Preview. Flat rounded fill, 1.5 px espresso border, centered bold cotton label, hover/pressed states.
- Built clean as VST3 and deployed to `~/Library/Audio/Plug-Ins/VST3/Premonition.vst3` + mirrored to `/Library/Audio/Plug-Ins/VST3`.
- **Open questions still unanswered by Avi (needed for items 3 and 4):**
  1. Decay knob: continuous 0.5–16 beats float, or discrete steps 1/4…16?
  2. Normalize target: -0.3 dBFS or 0 dBFS?

## What was just finished this session
Three follow-ups to Avi's audio feedback on the reverse-reverb. All built clean as VST3, deployed to `~/Library/Audio/Plug-Ins/VST3/Premonition.vst3`.

1. **Size, Decay, Mix decoupled.** Previously all three knobs drove a single `lush` param (most were stubs). Now:
   - `Size` → `roomFactor` ∈ [0.5, 1.5] (scales comb/allpass delay times)
   - `Decay` → `feedback` ∈ [0.70, 0.90] + `damping` ∈ [0.2, 0.5]
   - `Mix` → final wet/dry blend
   - `applyReverbStereo(..., size, decay, mix)` — new signature. Tests updated.
2. **Mode toggle (Stretch / Trim)** added behind a new `kParamMode` enum.
   - **Stretch** (default): original behavior — time-stretch reversed reverb to fit Tail beats.
   - **Trim:** natural-rate playback, cut to `beats × samplesPerBeat`, 15 ms end fade (`kTrimFadeMs`). `renderTrimmed()` in `RvrseProcessor.h` keeps the last `targetLen` samples so the climax lands on the beat boundary; front-pads with silence if source is shorter.
   - UI: `IVSlideSwitchControl` between the knob grid and the preset row.
3. **Waveform trim handles.** `WaveformControl` now shows the **loaded source sample** (not the rendered tail). Two mustard handles with grip squares; drag to select the portion that gets reverb'd.
   - Normalized 0–1 stored in `Premonition::mTrimStart / mTrimEnd` (atomics).
   - Re-render fires on mouse-up only (not during drag).
   - Loading a new sample resets trim to `[0, 1]`.
   - Pipeline slice happens in `runPipeline` stage 0, *before* resample.

## Pending plan — Avi wants all 5, in this order
Quoted asks verbatim so future-you doesn't re-interpret:
> "I would like to see the rendered waveform as well."
> "Can you add a normalize button for the rendered waveform?"
> "The length of the tail should be depending on the reverb tail affected by the bpm of the current project it is sitting in." (clarified: both options A and B below)
> "The preview button also is messed up like the render, they should be in the same style as the drag to daw button"

### 1. Render + Preview buttons — flat DragoutControl style ✅ DONE

### 2. BPM-sync the internal reverb tail padding (option A)
**Problem:** `kReverbTailSeconds = 5.0` is fixed. At slow BPMs the tail gets truncated relative to the musical time; at fast BPMs we waste compute on silence.
**Fix:** replace with `kReverbTailBeats` (suggest `8.0`), compute `tailFrames = processingRate * 60.0 / bpm * kReverbTailBeats` in `runPipeline`. Clamp to `[2s, 15s]` so extreme BPMs don't explode memory / starve decay.
**Watch out:** `setBPM()` currently only rebuilds the Stretch stage. Once padding is BPM-linked, BPM changes must rebuild from **Reverb** stage. Same concern applies when `kReverseModeTrim` is active — BPM change resizes the output. Add a flag or just make `setBPM` trigger Reverb rebuild always; the pipeline already caches so hit is minimal on stretch-only cases.

### 3. Decay expressed in beats (option B) — BIGGER change
**Currently:** Decay knob is 0–100% → linear map to feedback + damping.
**New:** Decay = **decay time in beats**. Solve feedback from RT60:
  - `RT60_seconds = decayBeats * 60.0 / bpm`
  - For each comb with delay D samples, feedback g: `g = 10 ^ (-3 * D / (fs * RT60))`
  - Damping: split off into its own param, or leave tied to normalized position.
**Open UX question (ASK AVI):**
  - (a) **Continuous knob** 0.5–16.0 beats (float)
  - (b) **Discrete steps** 1/4, 1/2, 1, 2, 4, 8, 16 (mirrors Tail)
**Rebuild:** on BPM change → rebuild Reverb stage (each comb's feedback depends on fs and RT60_seconds which depends on BPM).
**Tradeoff to surface:** Decay becomes time-musical but loses its "raw wetness" feel. If Avi wants both, add a second `Character` param for damping only.

### 4. Normalize toggle
**Param:** `kParamNormalize` (enum On/Off, default Off).
**Pipeline:** after final fade, in both `runPipeline` and `rebuildStretchSync`:
```
if (normalize) {
  float peak = 0.f;
  for (auto& v : riser->mLeft)  peak = std::max(peak, std::abs(v));
  for (auto& v : riser->mRight) peak = std::max(peak, std::abs(v));
  if (peak > 1e-6f) {
    const float target = std::pow(10.f, kNormalizeTargetDBFS / 20.f); // -0.3 dBFS
    const float gain = target / peak;
    for (auto& v : riser->mLeft)  v *= gain;
    for (auto& v : riser->mRight) v *= gain;
  }
}
```
**Open UX question (ASK AVI):** target `-0.3 dBFS` (safe — default) vs `0 dBFS` (maxed, risks inter-sample peaks).
**UI:** toggle next to Mode switch. Or small button in the rendered-waveform strip (after item 5).

### 5. Stacked waveforms — layout work, do last
- Grow the waveform area from 110 px to ~150 px tall.
- Split: top ~55 % source + trim handles (current), bottom ~45 % rendered tail bars with beat-aligned gradient (terracotta → oxblood approaching peak, fade after).
- 1 px divider between strips, rounded `kMidnight` background spans both.
- Push Render/Preview row down ~40 px; shrink dragout row if tight.
- Rendered-tail bars reuse the pre-refactor draw code from `WaveformControl` — just rescope to the bottom strip.

**Why last:** items 1–4 don't change layout. Doing layout changes before them means redoing bar math twice.

## Open questions to ask Avi before starting
1. **#3 Decay:** continuous (0.5–16 beats float) or discrete steps?
2. **#4 Normalize target:** -0.3 dBFS or 0 dBFS?
3. **Execution order:** he agreed to "all 5"; proposed order is 1 → 2 → 3 → 4 → 5. Confirm or resequence.

## Key files (post-session state)
- `RVRSE/Reverb.h` — split params done. `applyReverbStereo(..., size, decay, mix)`.
- `RVRSE/RvrseProcessor.h` — `setSize/Decay/Mix/Mode/Trim`. `runPipeline` branches on mode; `rebuildStretchSync` mirrors it. `renderTrimmed()` helper for trim path. `mMode`, `mTrimStart/End` fields.
- `RVRSE/Constants.h` — `kSizeDefault`, `kDecayDefault`, `kMixDefault`, `EReverseMode`, `kTrimFadeMs`.
- `RVRSE/Premonition.h` — `kParamMode` added. `GetLoadedSample() / GetTrim() / SetTrim()` exposed. `mTrimStart/End` atomics.
- `RVRSE/Premonition.cpp` — `WaveformControl` rewritten for source + handles. Mode switch added to layout. `LoadSampleFromFile` resets trim.
- `tests/test_reverb.cpp` — updated to new 3-param signature. Tail-energy-vs-decay test holds size+mix constant.

## Build command (use this, not the old one)
```bash
cmake --build /Users/aviouslyavi/Claude/Projects/premonition/build-tests --target Premonition-vst3 -j
```
Configure (if needed):
```bash
cmake -S /Users/aviouslyavi/Claude/Projects/premonition \
      -B /Users/aviouslyavi/Claude/Projects/premonition/build-tests \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
```
(`build/` is stale from a prior run and throws a CMakeCache mismatch. `build-tests` is fresh and works.)
**Note:** `Premonition-app` target fails because Xcode isn't installed (only Command Line Tools; `ibtool` missing). VST3 and AU build fine.
**Tests:** `rvrse_tests` target has pre-existing breakage (`stb_vorbis.h` / `Stutter.h` not found) in files *other* than `test_reverb.cpp`. Not from this session. `test_reverb.cpp.o` compiles individually.

## Things to NOT touch unless asked
- `SerializeState/UnserializeState` — Avi said he hasn't saved projects yet. But adding params (`kParamMode`, the upcoming `kParamNormalize`) increases `kNumParams`, so existing DAW project files would misalign. If Avi reports a load crash after adding new params, bump the state version and branch on it.
- The existing tail-fade constants (`kRiserTailFadeBeats`, `kRiserOverlapBeatsBase/Max`) — these tune the *stretch* path's blend into the hit. Trim mode has its own `kTrimFadeMs`. Don't cross them.

## Resumption prompt
Paste this into a new Claude session:
> Read `/Users/aviouslyavi/Claude/Projects/premonition/HANDOFF-next.md`. Then ask me which of the 5 pending items to start with, and the 3 open questions at the bottom.
