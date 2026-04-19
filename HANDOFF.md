# Premonition — Handoff

**Last updated:** 2026-04-18
**Purpose:** Resume work in a fresh Claude context without losing state.

---

## 1. Project Identity

- **Name:** Premonition
- **Vendor:** AVIOUS (Avi Ben-Abram)
- **Price:** $12.99 (one-time)
- **Bundle ID:** `com.avious.Premonition`
- **Plug Unique ID:** `'Prmn'` / **Mfr ID:** `'Avio'`
- **Formats:** VST3, AU, CLAP, AAX (targets; only VST3/AU tested locally)
- **Concept:** Offline reverse-reverb riser generator. Drag source in → tempo-synced riser out. Signal chain: crop → stretch → reverb → reverse → fit-to-bar → normalize.

---

## 2. Current State (Phases 1 + 2 complete)

- rverse-derived code, docs, and scripts purged
- Full backup at `/Users/aviouslyavi/Projects/Plugins/premonition-backup-2026-04-18/` (332M mirror)
- Project re-scaffolded via iPlug2 `duplicate.py`
- Clean-room DSP written from textbook refs (Schroeder '62, Moorer '79, Freeverb PD, Smith/CCRMA)
- Plugin wired: 11 params, offline render entry point, placeholder GUI with knob row
- Fresh `CMakeLists.txt` (CMake 3.25+, C++17, `compile_commands.json` on)
- Catch2 v3 tests stubbed for reverb, reverse, fit-to-bar, pipeline
- Fresh `README.md`, `THIRD-PARTY-NOTICES.txt`, `CHANGELOG.md`
- GUI design brief written, anchored to `mockup.html`

**Not yet done:** CMake config never actually run. Build not verified. Real GUI not built.

---

## 3. Key Pointer Files

| File | Role |
|---|---|
| `PREMONITION_BRIEF.md` | Frozen v1 product spec (signal chain, params, constraints) |
| `GUI_DESIGN_BRIEF.md` | GUI target + 5-phase implementation plan |
| `mockup.html` | Authoritative visual source of truth (warm AVIOUS palette) |
| `README.md` | Public-facing product pitch + dep table |
| `THIRD-PARTY-NOTICES.txt` | iPlug2 + signalsmith-stretch attribution for installer |
| `CHANGELOG.md` | Release log |

---

## 4. Source Tree

```
premonition/
├── CMakeLists.txt                 # top-level, CMake 3.25+, C++17
├── .clangd                        # silences LSP pre-config noise
├── Premonition/
│   ├── config.h                   # iPlug2 plugin identity
│   ├── Parameters.h               # 11-param enum + ranges::
│   ├── Premonition.h/.cpp         # plugin class, RenderRiserFromSource()
│   └── dsp/                       # ← clean-room, do NOT reference rverse
│       ├── DelayLine.h
│       ├── Reverb.h               # Freeverb-style (8 LPF-combs + 4 APs)
│       ├── Reverse.h
│       ├── TimeStretch.h          # wraps signalsmith-stretch
│       ├── FitToBar.h             # tempo math
│       └── OfflinePipeline.h      # renderRiser() orchestrator
├── tests/
│   ├── CMakeLists.txt
│   ├── test_reverb.cpp
│   ├── test_reverse.cpp
│   ├── test_fit_to_bar.cpp
│   └── test_pipeline.cpp
└── iPlug2/                        # vendored framework
```

---

## 5. Dependencies

| Lib | License | How | Notes |
|---|---|---|---|
| iPlug2 | zlib-like | vendored | Attribution required in installer |
| signalsmith-stretch | MIT | FetchContent main | Time-stretcher |
| Catch2 v3.5.3 | BSL-1.0 | FetchContent, tests-only | Not shipped |

No rverse code or docs remain. Premonition is product-concept-adjacent to rverse but code-independent.

---

## 6. Recommended Next Actions (in order)

1. **Verify build configures:**
   ```bash
   cd /Users/aviouslyavi/Projects/Plugins/premonition
   cmake -B build -G Xcode
   ```
   Confirms FetchContent pulls signalsmith-stretch + Catch2 and the scaffold compiles.

2. **Run tests:**
   ```bash
   cmake -B build -DPREMONITION_BUILD_TESTS=ON
   cmake --build build --target premonition_tests
   ctest --test-dir build
   ```

3. **GUI v1a** (per `GUI_DESIGN_BRIEF.md`):
   - Skeleton layout matching `mockup.html` left/right panels
   - 4 main knobs: Size, Decay, Length (tail), Mix — `IVKnobControl`
   - Render button (oxblood `#4E0000` w/ espresso shadow)
   - Preview link
   - Preset dropdown (Hall / Plate / Spring / Room)
   - Use Roboto-Regular as v1 fallback; defer custom font bundling to polish phase

4. **GUI v1b–v1d:** drop zone/queue → waveform + Start/End handles → drag-out bar + advanced row.

5. **Polish:** custom fonts (Space Grotesk, DM Mono, Fraunces), animations.

---

## 7. Parameters (wired; Premonition.cpp)

```
kStart, kEnd           (0..1 crop region)
kStretch               (0.25..4.0, default 1.0)
kSize, kDecay, kMix    (reverb)
kAlgorithm             (Hall / Plate / Spring / Room — all share Freeverb settings for now)
kLength                (0.25..16 bars, default 2.0) — WINS over Stretch when both set
kForward               (bool — skips reverse step)
kNormalize             (bool)
kMonoStereo            (bool)
```

---

## 8. Deferred / Future

- Per-algorithm reverb tuning (all four currently identical)
- Host time signature integration (hardcoded 4/4)
- iPlug2 drag-drop WAV import/export
- Custom font loading
- **v2:** Snapback pre-swell bake (riser + dry hit in one WAV)
- **v3:** VST3 reverb plugin hosting
- **v4:** Realtime insert mode

---

## 9. Legal

- Premonition ships fully independent of SamuFL/rverse (clean-room rewrite).
- `THIRD-PARTY-NOTICES.txt` covers only iPlug2 + signalsmith-stretch.
- © 2026 AVIOUS. Proprietary product; source closed.
