# Step 0 — Pre-flight findings (2026-04-20)

Executed before Step 1 of [HANDOFF-2026-04-19-reverb-types-and-modes.md](HANDOFF-2026-04-19-reverb-types-and-modes.md).

## Pre-flight A — "stretchy" culprit → **CONFIRMED: fit-to-bar resampling**

Ear-test by Avi, BYPASS=0 vs BYPASS=1 via a temporary `PREMONITION_BYPASS_FIT_TO_BAR` compile flag in [OfflinePipeline.h](../Premonition/dsp/OfflinePipeline.h).

- BYPASS=0, Tail=2 bars @ 120 BPM → stretchy character audible.
- BYPASS=1 (fit-to-bar skipped), same settings → no stretching artifact.

Verdict: the fit-to-bar resampling stage at the end of `renderRiser()` is the source of the perceived stretch, not the `kStretch` ratio knob (which is skipped at 1.0). Plan proceeds as written — the new `Natural` and `Forward` modes replace fit-to-bar with hard-trim + fade, and the resampling-free path is what ships by default.

Test limitation: Avi rendered three Tail-length variants on the BYPASS=0 build and one render on the BYPASS=1 build, rather than the full Stretch=0.5/1.0/bypass matrix. Since `kStretch` is not surfaced in the current UI (exists as param, no knob), the Stretch=0.5 leg wasn't needed — the BYPASS=0 vs BYPASS=1 comparison at Stretch=1.0 is the decisive isolation.

Debug flag has been removed; pipeline restored to original behavior.

## Pre-flight B — `monoOutput` default → **CLEAN**

Grep of every write site in [Premonition.cpp](../Premonition/Premonition.cpp):
- [Premonition.cpp:1008](../Premonition/Premonition.cpp#L1008): `GetParam(kMonoStereo)->InitBool("Mono", false);` — default off.
- [Premonition.cpp:1281, 1304](../Premonition/Premonition.cpp#L1281): preset serialization (round-trip read/write).
- [Premonition.cpp:1396](../Premonition/Premonition.cpp#L1396): read into `cfg.monoOutput` at render time.

No stuck-on default. No code change needed.

## Incidental observations

- CMake is the source of truth for builds. The Xcode project (`Premonition/projects/Premonition-macOS.xcodeproj`) is stale: missing fetched deps (signalsmith-stretch) from its include paths and its deployment target (10.13 from `iPlug2/common-mac.xcconfig`) is too low for the `std::filesystem` usage in [Presets.h](../Premonition/Presets.h). Not blocking — all pre-flight builds used `cmake --build .build --target Premonition-vst3 Premonition-au`. Worth cleaning up as a follow-up but out of scope for this rework.
- `kStretch` is a registered param (range 0.25–4.0) with no UI control surface in the current build. The planned Mode=Stretch path will need the knob surfaced when Step 1 lands, or Stretch mode is functionally inaccessible.

## Ready for Step 1

Hypothesis confirmed → Step 1 (parameter restructure) unblocked.
