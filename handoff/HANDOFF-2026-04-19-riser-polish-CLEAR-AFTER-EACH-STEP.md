# Premonition — Riser Polish Phase (2026-04-19)

> **⚠️ AVI: `/clear` BETWEEN EVERY STEP.** Each numbered step is a self-contained
> session. Load this doc fresh, execute one step, commit, then `/clear` before
> starting the next. Do not batch steps — context from one doesn't help the next,
> and carrying it wastes tokens + invites drift.

**For a fresh Claude session.** Read this top-to-bottom, confirm the current
step's scope with Avi, execute, commit, stop. Earlier handoffs are in this
folder for history.

---

## Status at handoff

- **Commit `1f964cb`** — GUI v2 AVIOUS palette shipped: brand orb, cotton bg,
  midnight waveform strip, sage dragout, oxblood/terracotta buttons, knob grid.
- Source load + render + basic waveform viz all working.
- Normalize target now `-0.3 dBFS` (was `-0.1 dBFS`).
- `Rendered()` getter added to `Premonition.h` so the waveform control can
  display the rendered output.

## Known gaps (what this phase addresses)

- Tail is continuous — Avi wants **discrete musical divisions** only.
- Waveform background is **blue** — Avi wants **gray** + **bar glow** (mustard halo).
- Start/End grip handles are **decorative only** — no drag, no param wiring.
- **Preview button is a no-op.** Needs play + **stop**.
- **Reverse tail cuts hard** — needs 15 ms crossfade into unprocessed source.
- No tempo sync fallback UI for standalone / stopped transport.
- No A/B compare for successive renders.
- Drag-out doesn't work — pill is static.
- No preset save/load UI (param enum exists but algorithms not differentiated).
- Algorithm enum doesn't include **Convolution** yet.

---

## Tools available this phase

- **`frontend-design` plugin** + **`screenshot` skill** — use for visual
  iteration on `mockup.html` (glow alpha/radius, gray shade) BEFORE porting
  numeric values into `Premonition.cpp`. Render mockup.html, screenshot,
  eyeball, tweak, repeat. Only touch the C++ once the mockup looks right.
- **Impeccable tools** = the `frontend-design` plugin + screenshot skill
  (Avi's shorthand — confirm if unsure).

---

## The steps (execute one per session, `/clear` after each)

### Step 1 — Tail enum snap ✅ simplest, start here

**File:** `Premonition/Parameters.h`, `Premonition/Premonition.cpp` (param init
at ~line 385), wherever `kLength` is read in the pipeline.

Change `kLength` from `InitDouble` to `InitEnum` with these 9 steps:
`{"1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16"}`
→ bar values `{0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16}`.

- Add a `kLengthBarsTable[9]` constant in `Parameters.h`.
- In `RenderRiserFromSource` read the int param, index the table, assign
  `cfg.lengthBars`.
- Knob label stays "TAIL". The IVKnobControl will render discrete clicks
  automatically for an enum.
- Default index = 3 (→ 0.5 bars) — or whatever Avi prefers; ask.

**Commit:** `Tail: snap to discrete musical divisions (1/16..16 bars)`.

---

### Step 2 — Waveform: gray bg + mustard glow

**Tool prep:** open `mockup.html`, use frontend-design + screenshot skill to
iterate glow look first. Target: bars feel **luminous**, brighter than
current, mustard halo against warm charcoal bg.

**File:** `Premonition/Premonition.cpp` — `WaveformControl::Draw` (~line 143).

- Replace `kMidnight` fill at line 145 with warm charcoal,
  e.g. `IColor(255, 40, 36, 34)` — tune in mockup first.
- Bar glow: before drawing each solid bar, draw 2–3 concentric "halos":
  same rect expanded 2px/4px/6px, mustard tint `(a, 211, 146, 3)` with
  `a ∈ {30, 18, 10}`. Use `FillRoundRect` with the existing radius.
- Apply glow to **both** source and rendered strips.
- Bump solid bar brightness too — Avi said current bars are dim.

**Commit:** `Waveform: charcoal bg + mustard bar glow`.

---

### Step 3 — Start/End draggable handles

**File:** `Premonition/Premonition.cpp` — `WaveformControl::DrawSourceStrip`
(handles drawn ~line 215-222, currently static).

- Split `WaveformControl` hit-testing into three zones: left handle grip,
  right handle grip, body.
- On `OnMouseDown` near a handle, capture. On `OnMouseDrag`, clamp x to
  `[inset.L, inset.R]`, convert pixel → seconds via
  `(x - inset.L) / inset.W() * sourceDurationSec`, call
  `GetDelegate()->SendParameterValueFromUI(kStart or kEnd, normalizedValue)`.
- Enforce `Start < End - minGapSec` (e.g. 0.01s).
- Redraw the inset bounds based on current `kStart` / `kEnd` so the grips
  visually track the crop region.
- Tooltip on hover: "Drag to set Start" / "Drag to set End".

**Commit:** `Waveform: draggable Start/End handles wired to params`.

---

### Step 4 — Preview play + stop

**Files:** `Premonition.h` (add preview state), `Premonition.cpp`
(`ProcessBlock` + preview button handler at ~line 512).

- Add to `Premonition` class:
  ```cpp
  std::atomic<bool> mPreviewPlaying{false};
  std::atomic<int64_t> mPreviewPos{0};
  ```
- In `ProcessBlock`, if `mPreviewPlaying`, read from `mRendered` at
  `mPreviewPos`, write to outputs (sum to channel count), advance pos,
  stop at end (`mPreviewPlaying = false`).
- Preview button becomes a **toggle**: click to start, click again to stop.
  Relabel dynamically: `"PREVIEW"` ↔ `"STOP"`. Fill swaps
  `kTerracotta` ↔ `kOxblood`.
- Guard: if `mRendered.frames() == 0`, button is inert.
- Thread safety: `mRendered` is only mutated on the UI thread during render;
  make render acquire a mutex that `ProcessBlock` tries to lock — if it can't,
  pass through silence that block.

**Commit:** `Preview: playback with stop, dynamic button state`.

---

### Step 5 — Reverse tail → 15 ms crossfade into unprocessed source

**File:** `Premonition/dsp/OfflinePipeline.h` — `renderRiser` between
reverse (step 4) and fit-to-bar (step 5).

- After reverse, before fit-to-bar, take the **last 15 ms** of the reversed
  buffer and crossfade it against the **first 15 ms of the cropped source**
  (the `L`/`R` from before stretch, or re-crop from `src`). Equal-power fade:
  `out[i] = reversed[i] * cos(πt/2) + source[i] * sin(πt/2)` with
  `t ∈ [0,1]`.
- Sample count: `kTailFadeSamples = (int)(0.015 * sampleRate)`.
- Guard: if cropped source is shorter than fade length, skip or shorten.
- Add `kTailFadeMs = 15.0` constant near `ranges::` so it's tweakable.
- Add a test in `tests/` that verifies the last N samples of the rendered
  output are a blend (non-zero, not equal to either input alone).

**Commit:** `Reverse tail: 15 ms equal-power fade into unprocessed source`.

---

### Step 6 — Tempo sync + manual BPM override

**Files:** `Premonition.h` (manual BPM field), `Premonition.cpp` (footer UI
~line 624, and `RenderRiserFromSource` tempo resolution ~line 660).

- Add `kManualBPM` param: `InitDouble("Manual BPM", 120, 40, 300, 0.1)`.
- In `RenderRiserFromSource`, resolve tempo: `GetTempo() > 0 ? GetTempo()
  : GetParam(kManualBPM)->Value()`.
- Footer: replace static `"48 kHz · 120 bpm"` with live text. Left slot
  shows `"<host SR> · <effective BPM>"`; when host tempo is 0, show the
  manual BPM and make it editable (`ICaptionControl` or small text entry
  on click).
- Visually de-emphasize the manual BPM label when host tempo is live.

**Commit:** `Tempo: fallback to manual BPM when host transport is idle`.

---

### Step 7 — A/B render slots

**Files:** `Premonition.h` (add `mRenderedB` buffer + active slot),
`Premonition.cpp` (UI near the dragout / above it).

- Add `mRenderedA`, `mRenderedB`, `mActiveSlot ∈ {A, B}` in `Premonition`.
- Render always writes to `mActiveSlot`.
- UI: two small pill buttons "A" / "B" above or next to the dragout, tinted
  sage (active) / cotton2 (inactive). Clicking switches `mActiveSlot` AND
  retargets waveform/preview/dragout to that slot.
- Use existing `Rendered()` getter but make it return the active slot.

**Commit:** `A/B render slots with toggle`.

---

### Step 8 — Drag-out + 32-bit WAV + dynamic status line

**Files:** `Premonition.cpp` (`DragoutControl` ~line 320, status line
~line 525), new helper for WAV export.

- **Drag-out:** on `OnMouseDown` in `DragoutControl`, if `mRendered.frames()
  > 0`, write a 32-bit float WAV to a temp file (e.g.
  `std::filesystem::temp_directory_path() / "Premonition-<timestamp>.wav"`).
  Call `IGraphics::CreateFileDragSource` or the iPlug2 equivalent — check
  `IGraphicsMac.mm` / `IGraphicsWin.cpp` for the exact API. If iPlug2
  doesn't expose it, fall back to platform-native drag source
  (NSPasteboard on mac).
- **Temp file policy:** keep until plugin instance destruction. Track paths
  in a `std::vector<std::string> mTempFiles` and delete in destructor.
  Overwrite-on-rerender NOT required (Avi chose "keep until close").
- **Sample rate:** use host (`GetSampleRate()`); no conversion needed if
  render was done at host rate. Confirm render path uses host SR.
- **Status line:** replace `"Ready · click Render or Preview"` with live
  state: `"No sample loaded"` → `"Ready — <filename> (<duration>s)"` →
  `"Rendering…"` (when render runs) → `"Rendered — drag to DAW"` →
  `"Playing…"` during preview. Single `ITextControl*` stored on the
  plugin, updated from the relevant callbacks.

**Commit:** `Dragout: 32-bit WAV export, dynamic status line`.

---

### Step 9 — Preset save/load (module params only)

**Files:** `Premonition.cpp`, possibly new `Presets.h`.

- Presets cover: `kStretch, kSize, kDecay, kMix, kAlgorithm, kLength,
  kForward, kNormalize, kMonoStereo`.
- Exclude: `kStart, kEnd, kManualBPM`, and the loaded source file.
- Replace the existing preset caption at the bottom of the right panel with
  a 3-button row: **◀ / NAME / ▶** to cycle, plus a small "SAVE" affordance
  that writes a new preset file.
- Use iPlug2's `IPluginBase::MakeDefaultPreset` / `MakePreset` machinery,
  or roll a simple JSON file in `~/Library/Application Support/Premonition/
  presets/`.
- Ship 4 factory presets keyed to the algorithm enum (Hall / Plate / Spring
  / Room) with tuned Size/Decay/Mix defaults. This also gives Step 10
  something to slot into.

**Commit:** `Presets: save/load module params, 4 factory presets`.

---

### Step 10 — Convolution algorithm (5th enum) + IR drop slot

**Files:** `Parameters.h` (add `kAlgoConvolution`), new
`Premonition/dsp/Convolution.h`, `OfflinePipeline.h` (branch on algo),
`Premonition.cpp` (reveal IR slot when selected).

- Add `"Convolution"` as 5th option in `kAlgorithm` enum.
- New `dsp/Convolution.h`: load IR WAV via existing `AudioLoader`, do
  partitioned FFT convolution (or time-domain if IR is short — <1s).
  Keep this offline-friendly; no realtime constraint.
- `renderRiser` branches: if `cfg.algorithm == kAlgoConvolution` and an
  IR is loaded, replace the `renderReverbStereo` call with
  `convolveStereo(...)`. Otherwise use the existing algorithmic reverb
  (Hall/Plate/Spring/Room — these still need internal differentiation,
  **flag as separate sub-task** if they're stubs).
- UI: when `kAlgorithm` == Convolution, show a small dashed drop zone
  below the knob grid labeled "IR" where an IR WAV can be dropped.
  Otherwise hide/collapse.
- Also differentiate Hall/Plate/Spring/Room in the reverb module params
  (different default damping / comb delay scaling) — if they're currently
  identical stubs, note it and either fix or leave a TODO.

**Commit:** `Convolution: 5th algorithm with IR drop slot`.

---

### Deferred (not this phase)

- **Auto-render on param tweak** — Avi wants to think on it.
- **Visual render progress spinner** — only meaningful once render is
  async (paired with auto-render). Revisit when auto-render lands.

---

## General rules for every step

- **Build clean before committing.** `cmake --build .build --target
  Premonition_VST3` and confirm no warnings introduced.
- **Deploy** to `~/Library/Audio/Plug-Ins/VST3/Premonition.vst3` and mirror
  to `/Library/Audio/Plug-Ins/VST3/` so Avi can test immediately.
- **Ask Avi before starting** each step — confirm scope, clarify any
  ambiguity. He may reorder.
- **Never batch steps.** Commit after each. `/clear` after each.
- **Write/update tests** where the DSP changes (Step 5 especially).
- **Don't invent params.** Every new param goes in `Parameters.h` with a
  documented range.

---

## Open questions to ask Avi at start of each session

- **Step 1:** default Tail index — 0.5 bars (index 3) or 2 bars (index 5)?
- **Step 2:** how bright is "a little brighter" — want to iterate in
  mockup.html together via screenshots, or trust my call?
- **Step 4:** should Preview loop, or one-shot only?
- **Step 7:** auto-switch to the just-rendered slot, or stay on the active
  slot so A/B compare is easy?
- **Step 9:** preset file format — iPlug2 native, or JSON?
- **Step 10:** if Hall/Plate/Spring/Room are currently stubs, differentiate
  them this step or flag a separate task?
