# Premonition — Reverb Types + Modes Rework (2026-04-19)

> **⚠️ AVI: `/clear` BETWEEN EVERY STEP.** Each numbered step is a self-contained
> session. Load this doc fresh, execute one step, commit, then `/clear` before
> starting the next. Do not batch steps — context from one doesn't help the
> next, and carrying it wastes tokens + invites drift.

**For a fresh Claude session.** Read this top-to-bottom, confirm scope with
Avi, execute the current step, commit, stop.

---

## Why we're doing this

Two user-facing problems with the current build:

1. **Hall / Plate / Spring / Room are labels only** — they all run the same
   Freeverb network today ([OfflinePipeline.h:60-62](../Premonition/dsp/OfflinePipeline.h#L60-L62)). Users
   expect distinct character per type.
2. **The output sounds "manipulated / stretchy"** — hypothesis: the fit-to-bar
   resampling at [OfflinePipeline.h:207-214](../Premonition/dsp/OfflinePipeline.h#L207-L214) is the
   culprit, not the `kStretch` ratio knob (which is skipped at 1.0 already).
   Hypothesis must be confirmed by ear before redesigning around it — see
   Pre-flight A.

Goal: ship genuinely distinct reverb types (IR-backed), plus a Mode enum
(Natural / Stretch / Forward) that gives users a resampling-free path.

---

## Decisions (agreed with Avi)

### Reverb Types
- 5 slots total: **Hall / Plate / Spring / Room / Custom** (Custom = existing
  user-drop IR slot, rename from "Convolution").
- Default selected slot = **Hall**.
- **Hall / Room**: sourced from free stereo IRs. Strictly **CC0 / public
  domain** only — no CC-BY, no CC-BY-SA, no attribution requirements.
- **Plate / Spring**: synthesized in-house and baked to stereo WAV. 100%
  Avi-owned, zero licensing risk.
- **Fallback plan (if CC0 Hall/Room pickings are thin)**: synthesize those
  too, in-house. Avi explicitly approved "everything synthesized in-house"
  as the fallback.
- **Tone targets** (approved):
  - Hall: 3–5 s RT60, bright, wide concert-hall character
  - Plate: 2–3 s, dense, bright (~EMT 140 flavor, Dattorro topology)
  - Spring: 1.5–2.5 s, boingy, metallic (allpass chain + dispersion)
  - Room: 0.5–1.2 s, warm, small live room
- **Storage**: embedded in the plugin binary as resources (iPlug2 binary
  resource system), not shipped as loose files. ~300–500 KB per IR stereo
  WAV × 4 = ~1–2 MB binary size impact. Acceptable.
- **Size / Decay / Damping knobs** remain active for all types. For IR-based
  types they post-process the convolved output (envelope + LPF on tail) so
  the knobs still feel meaningful.

### Modes (new `kMode` param — 3-way enum, radio group UI)
- **Natural** (default): reverb → reverse → hard-trim-from-front to Tail +
  100 ms linear fade-out. **Zero resampling anywhere.**
- **Stretch**: reverb → reverse → fit-to-bar resampling (current behavior).
  Pre-reverb Stretch ratio knob is active here only.
- **Forward**: reverb only (no reverse) → hard-trim + 100 ms fade.
  Zero resampling.
- Matrix:
  | Mode     | Pre-stretch | Reverse | Fit-to-bar | Trim + fade |
  |----------|-------------|---------|------------|-------------|
  | Natural  | no          | yes     | no         | yes         |
  | Stretch  | yes (knob)  | yes     | yes        | no          |
  | Forward  | no          | no      | no         | yes         |

### Output-assembly behavior
- **Tail overrun (output shorter than Tail)**: pad with **leading silence**
  so the riser peak lands on the bar downbeat. Musical behavior — risers
  build up to a drop.
- **Tail overrun (output longer than Tail)**: **hard-trim from the front**,
  keep the last N samples. The reverse-reverb peak is always at the end;
  preserve it, drop the quiet beginning.
- **Fade-out**: 100 ms linear at the tail.
- **Crossfade-into-source** (current behavior at
  [OfflinePipeline.h:197-204](../Premonition/dsp/OfflinePipeline.h#L197-L204)):
  - Natural / Forward default = **off** (riser is a standalone render; user
    places it before their source in the DAW).
  - Stretch = **keep on** (don't break what works).
  - Ship Natural/Forward with a hidden flag to enable (b) so Avi can A/B
    during verification. Decision deferred to verification.

### Stereo handling
- **Input → stereo-widened output**, always, without Haas (mono-compat risk).
- Widening is **reverb-side decorrelation**: bump the L/R Freeverb stereo
  spread from 23 samples → ~150–400 samples, and seed the comb filter
  lengths with different primes per channel (not just an offset).
- For IR types: ship **stereo IRs**. Mono IR would be duplicated L=R and
  the algorithmic decorrelation on pre/post stages provides width.
- **Mono source auto-detection**: at load time, if L == R exactly, treat as
  mono and the widening above applies. No user toggle.
- `kMonoStereo` param (renamed "Mono Out") remains — user can collapse the
  final stereo output to mono if they explicitly want that.

### Parameter renames / restructure
| Current (internal → UI) | New (internal → UI) | Notes |
|-------------------------|---------------------|-------|
| `kLength` → "Length"    | `kLength` → **"Tail"** | UI label only; keep internal name |
| `kAlgorithm` → "Algorithm" | `kReverbType` → **"Type"** | Enum now includes Custom |
| `kForward` (bool)       | **deleted** → folded into `kMode` (enum) | Breaking |
| `kStretch` (0.25–4.0)   | `kStretch` → **grayed out in Natural/Forward** | Still continuous in Stretch mode |
| `kMonoStereo`           | `kMonoStereo` → **"Mono Out"** | UI label only |
| `kAlgoConvolution`      | **`kTypeCustom`** → "Custom" | Rename |

### Presets
- **Break all existing presets.** Avi is pre-release. No migration.
- Presets are **knob state only** — decoupled from Reverb Type selection.
  Saving a preset captures Size / Decay / Mix / Tail / Mode / etc., not
  which Type is loaded. (Design rationale: a user's "ambient" knob
  settings should apply across Hall and Plate.)

---

## Non-goals / explicitly deferred

- **Phase 2 — real DSP algorithms**: Dattorro plate, genuine allpass-chain
  spring with dispersion, Jot FDN hall. If Phase 1 IR-backed types sound
  sufficient, Phase 2 may never happen. Survey will be done by an agent
  during Phase 1 so the decision is informed, but no implementation.
- **User-tunable IR pack folder on disk**: baked-in only for now. If
  demand emerges, add post-release.
- **Per-type Size/Decay behavior tuning** beyond the generic envelope+LPF
  post-processing: Phase 2 polish.

---

## Agent-delegation plan

These keep the main-context session lean. Each agent call should return
under 500 tokens of summary. **Spawn in parallel where independent.**

### Agent 1 — CC0 IR curator (general-purpose)
Prompt skeleton: *"Find stereo CC0 / true-public-domain impulse responses
for a commercial closed-source audio plugin. Must be redistributable with
zero attribution or share-alike requirements. Target categories: (1) Hall
— 3–5 s RT60, bright, wide concert hall. (2) Room — 0.5–1.2 s, warm small
live room. Return up to 3 candidates per category with: URL, RT60, sample
rate, stereo/mono, exact license text, and a one-line tone description.
**Reject anything CC-BY, CC-BY-SA, or commercial-pack unless
explicitly licensed for embedded redistribution.** If fewer than 2 strong
CC0 candidates exist per category, say so — we have a synthesis fallback."*

### Agent 2 — Phase 2 DSP topology research (general-purpose)
Prompt skeleton: *"Survey three reverb topologies for possible Phase 2
implementation: (1) Dattorro plate reverb, (2) Schroeder-Moorer / allpass
chain spring reverb with dispersion, (3) Jot / Jean-Marc Jot FDN hall
reverb. For each return: minimal code sketch (C++, ~30-50 lines), CPU cost
relative to current Freeverb, 2-3 open-source reference implementations
with licenses, and a one-paragraph difficulty assessment for integrating
into the existing [Reverb.h](../Premonition/dsp/Reverb.h) structure.
Purpose: inform a future-vs-now decision, not implement now."*

### Agent 3 — Plate + Spring IR synthesis tool (general-purpose)
Prompt skeleton: *"Write a standalone C++ offline renderer that produces
stereo WAV impulse responses for: (1) a Dattorro-topology plate reverb
(~2–3 s RT60, dense, bright, EMT-140 flavor), (2) an allpass-chain
spring reverb with dispersion (~1.5–2.5 s, boingy, metallic). Use the
existing [Reverb.h](../Premonition/dsp/Reverb.h) allpass/comb primitives where
possible. Drive each with a single-sample impulse, render for RT60 + 20%
headroom, peak-normalize to -1 dBFS, write 44.1 kHz 32-bit float stereo
WAV. Deliver: `tools/render_ir.cpp` + a CMake target + the two rendered
WAVs in `resources/ir/`. Must compile standalone — no plugin host
dependencies."*

### Agent 4 — Handoff doc refresher (general-purpose, end of project)
After verification completes, agent scans commit history + generates the
next handoff (`HANDOFF-next.md`) summarizing what shipped, what deferred,
and what's ready for Phase 2.

### Not delegated (Avi + main-context Claude only)
- Any edit to [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h),
  [Parameters.h](../Premonition/Parameters.h), or the GUI.
- Per-type knob-behavior tuning (needs ears).
- The crossfade-into-source A/B verification (needs Avi's ears).

---

## Pre-flight audits (before any redesign)

### Pre-flight A — confirm the "stretchy" culprit
Render three versions of the current build with identical settings:
1. Stretch ratio = 1.0, Length = default (fit-to-bar active)
2. Stretch ratio = 1.0, Length bypass patched in (fit-to-bar skipped)
3. Stretch ratio = 0.5, Length = default (pre-stretch active, fit-to-bar active)

Expected: (1) and (3) sound "stretchy", (2) sounds natural. Confirms
fit-to-bar is the real culprit. If (1) and (2) sound identical, the
culprit is elsewhere and this whole plan needs revisiting.

### Pre-flight B — audit the `monoOutput` flag
Grep [Premonition.cpp](../Premonition/Premonition.cpp) for every write to
`cfg.monoOutput` / `kMonoStereo`. Confirm it is only set from the user-facing
toggle and not stuck-on from some pre-release default. Check GUI default
value for `kMonoStereo`. Fix if wrong.

Both pre-flights are ~30-minute audits. Do before Step 1.

---

## Phase 1 — implementation steps

**Commit after each step. `/clear` before the next.**

### Step 0 — Pre-flights
Execute Pre-flight A + B above. Commit findings to
`handoff/findings-preflight.md`. If Pre-flight A falsifies the fit-to-bar
hypothesis, **stop** — re-plan with Avi.

### Step 1 — Parameter restructure (breaking)
1. Rename `kAlgoConvolution` → `kTypeCustom`. Rename `EAlgorithm` →
   `EReverbType`, `kAlgorithm` → `kReverbType`.
2. Delete `kForward` bool. Add `kMode` enum (`kModeNatural`, `kModeStretch`,
   `kModeForward`, default `kModeNatural`).
3. Update [Parameters.h](../Premonition/Parameters.h), [Premonition.cpp](../Premonition/Premonition.cpp)
   param registration, and [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h)
   `PipelineConfig` to match.
4. Delete existing preset file (if any ships with the build). Presets are
   breaking-reset.
5. GUI: replace the Forward toggle with a 3-way radio for Mode. Hide UI
   element for Stretch knob in Natural/Forward modes (gray-out per Q3).
6. Build VST3 + AU, load in Logic, confirm params enumerate correctly.

### Step 2 — Implement Mode pipeline branching
Refactor `renderRiser()` in [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h) to switch on `cfg.mode`:
- Natural: `crop → reverb → reverse → hardTrimWithFade(tailBars)`.
  Pad with leading silence if output shorter than target.
- Stretch: keep current path verbatim (`crop → stretch → reverb → reverse
  → fit-to-bar → normalize → mono?`).
- Forward: `crop → reverb → hardTrimWithFade(tailBars)`.

Write `hardTrimWithFade(samples, targetLen, fadeMs=100)` as a helper —
leading-silence pad if short, trim-from-front if long, linear fade-out at
end either way.

For Natural/Forward, skip the existing crossfade-into-source **by default**;
gate it behind an internal `cfg.crossfadeIntoSource` flag (default false in
Natural/Forward, true in Stretch) so verification A/B is possible without
another commit.

Commit. Render one riser per mode with Type=Hall (still Freeverb at this
point), listen, confirm Natural is artifact-free.

### Step 3 — Spawn Agent 1 (CC0 IR research) + Agent 2 (Phase 2 survey) in parallel
Don't block on them. While they run, proceed to Step 4.

### Step 4 — Stereo widening in the algorithmic reverb
In [Reverb.h](../Premonition/dsp/Reverb.h):
- Bump `kStereoSpread44k` from 23 → configurable via a new
  `PipelineConfig::stereoSpread` field (default ~200).
- Add a second comb-length table with different primes for the R channel
  (e.g. shift each length by a prime offset like 37, 43, 47, ... at 44.1k).
- Add mono-source detection in [AudioLoader.mm](../Premonition/dsp/AudioLoader.mm):
  if L == R exactly, flag `isMono` on the loaded `StereoBuffer`.
- When `isMono == true`, use the wider spread; else use the default narrow
  spread (avoid over-widening already-stereo sources).

Render a test mono sample + stereo sample through the same settings,
confirm: mono source → wide stereo tail, stereo source → natural width,
neither collapses to narrow-mono when summed.

### Step 5 — Plate + Spring synthesis (Agent 3)
Spawn Agent 3. It produces `tools/render_ir.cpp`, CMake target, and the
two WAV files. Avi listens to the WAVs standalone, approves or asks for
tone tweaks. Commit the WAVs to `resources/ir/`.

### Step 6 — Bake IRs into the binary
1. Once Agent 1 returns with Hall/Room WAVs (or fallback synthesis WAVs
   from an Agent 3 follow-up), place all four in `resources/ir/`:
   `hall.wav`, `plate.wav`, `spring.wav`, `room.wav`.
2. Wire them into the iPlug2 binary resource system. Reference iPlug2 doc
   for the embed pattern (search for `MAKE_DEFAULT_PRESET` / binary resource
   examples in existing iPlug2 samples).
3. In [Premonition.cpp](../Premonition/Premonition.cpp) on plugin init, load each embedded WAV
   into a cached `StereoBuffer`, indexed by `EReverbType`.
4. When `kReverbType` changes, swap the cached IR into
   `PipelineConfig::ir` before next render. `kTypeCustom` still uses the
   user-dropped IR slot as today.
5. For IR-backed types, route through existing
   [Convolution.h](../Premonition/dsp/Convolution.h) path. For algorithmic fallback (shouldn't
   hit now but leave code in place) Freeverb still works.

### Step 7 — Post-convolution Size/Decay/Damping shaping
Apply the user's Size / Decay / Damping knobs to the convolved output so
they feel meaningful for IR types:
- **Size**: no-op on IR type directly; document as "no effect for IR types,
  use the type switch." OR: small pre-filter IR trim (use front N% of the
  IR only for small Size). Avi to decide by ear.
- **Decay**: apply an exponential envelope to the convolved tail (scales
  effective RT60 down from the IR's native RT60).
- **Damping**: one-pole LPF on the convolved output, cutoff driven by
  damping value.

Keep it simple — the goal is the knobs *feel* responsive, not recreate
the algorithmic reverb's behavior exactly.

### Step 8 — Verification A/B
1. Render a riser per (Type × Mode) combination. 5 × 3 = 15 renders.
   Listen to each, confirm distinct character per Type, zero artifacts in
   Natural mode.
2. Toggle the hidden `crossfadeIntoSource` flag for Natural. Render both
   versions. Avi picks which becomes the default.
3. Confirm mono source → stereo output fills the space without phase
   cancellation on mono sum.
4. Confirm Stretch mode still sounds like it did pre-rework (no
   regression).
5. Run existing tests in [tests/](../tests/). Fix any regressions.

### Step 9 — Release
1. Rebuild VST3 + AU via the Xcode macOS project.
2. Run `./scripts/release-adhoc.sh <new-version>`.
3. Final DMG → dist/.

### Step 10 — Retrospective (per user memory)
See [agent_specialist_audit](../../.claude/projects/-Users-aviouslyavi-Projects-Plugins-premonition/memory/agent_specialist_audit.md).
Audit every Agent invocation across Phase 1, cluster patterns, propose
2–5 reusable custom agent definitions for future plugin projects.

---

## Phase 2 (deferred — do not implement in this pass)

Only pursue if Phase 1 IR-backed types feel insufficient after release.
Agent 2's research report lives in `handoff/phase2-dsp-survey.md` and
should be read first. Likely scope:
- Dattorro plate replaces the synthesized plate IR with a live-DSP version
  — parameters become responsive in real time instead of baked.
- Genuine spring via Van Duyne allpass chain with dispersion.
- FDN hall (Jot-style 8- or 16-channel feedback delay network) replaces
  the Freeverb fallback and the Hall IR.
Expected cost: 2–4 weeks of DSP work + test + tuning. Do not start
without Avi's explicit go-ahead.

---

## Open items for verification

| Item | Decided at |
|------|-----------|
| Crossfade-into-source default for Natural/Forward | Step 8 verification (A/B) |
| Size knob behavior for IR types (no-op vs. IR-trim) | Step 7 listening test |
| Fallback to full synthesis if CC0 pickings thin | After Agent 1 returns |

---

## File map (what this plan touches)

- [Parameters.h](../Premonition/Parameters.h) — param restructure, mode enum, rename
- [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h) — mode branching, hardTrimWithFade helper
- [Reverb.h](../Premonition/dsp/Reverb.h) — stereo spread, mono-source widening
- [Convolution.h](../Premonition/dsp/Convolution.h) — no changes expected (reused by all IR types)
- [AudioLoader.mm](../Premonition/dsp/AudioLoader.mm) — mono detection flag
- [Premonition.cpp](../Premonition/Premonition.cpp) — param registration, embedded-IR loading, GUI
- [resources/ir/](../Premonition/resources) — new: bundled IR WAVs
- [tools/render_ir.cpp](../tools) — new: standalone IR synthesis renderer
- [Presets.h](../Premonition/Presets.h) — factory presets rewritten against new param layout
