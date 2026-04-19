# Premonition — Handoff

**Last updated:** 2026-04-19 (post-v1b)
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

## 2. Current State (Phases 1 + 2 complete; Steps #1–#4 verified)

- rverse-derived code, docs, and scripts purged
- Full backup at `/Users/aviouslyavi/Projects/Plugins/premonition-backup-2026-04-18/` (332M mirror)
- Project re-scaffolded via iPlug2 `duplicate.py`
- Clean-room DSP written from textbook refs (Schroeder '62, Moorer '79, Freeverb PD, Smith/CCRMA)
- Plugin wired: 11 params, offline render entry point, placeholder GUI with knob row
- Fresh `CMakeLists.txt` (CMake 3.25+, C++17, `compile_commands.json` on)
- Catch2 v3 tests stubbed for reverb, reverse, fit-to-bar, pipeline
- Fresh `README.md`, `THIRD-PARTY-NOTICES.txt`, `CHANGELOG.md`
- GUI design brief written, anchored to `mockup.html`
- **✅ Step #1 (2026-04-18): CMake configures cleanly.** `-G Xcode` fails (Xcode.app not installed — CLT only). Use `-G "Unix Makefiles"`. Fixed `Premonition/CMakeLists.txt` to pass `signalsmith-stretch` via the `LINK` arg of `iplug_add_plugin` (the per-format targets are `Premonition-vst3`, `Premonition-au`, `Premonition-app` — there is no bare `Premonition` target). Include-dir propagation loop added for the same reason. Targets available: `Premonition-app`, `Premonition-vst3`, `Premonition-au`. VST2/CLAP/AAX SDKs absent → those formats disabled (expected).

- **✅ Step #2 (2026-04-19): Tests + plugin compile clean.** Fixed `TimeStretch.h` include path: signalsmith-stretch target exports `include/`, so the header must be included as `"signalsmith-stretch/signalsmith-stretch.h"` (not bare). All 13 Catch2 tests pass (reverb, reverse, fit-to-bar, pipeline). `Premonition-vst3` and `Premonition-au` both build and auto-deploy to `~/Library/Audio/Plug-Ins/{VST3,Components}/`. One benign `-Wvla-cxx-extension` warning inside iPlug2's AUv2 code (not ours).

- **✅ Step #3 (2026-04-19): GUI v1a skeleton.** Plugin resized to 960×600 (was 720×520). Layout split into header/body/footer with left (workflow) and right (rack, 300px) panels using the mockup palette (cotton bg, cotton-2 rack, espresso dividers). Right rack has 2×2 knob grid (Size, Decay, Tail=kLength, Mix) with warm `IVStyle`, plus `ICaptionControl` preset dropdown on `kAlgorithm`. Left panel shows drop-zone placeholder, midnight waveform placeholder, oxblood `IVButtonControl` Render + ghost Preview label. Gotcha: `IPanelControl` has no frame-color arg — the 4th ctor param is `AttachFunc`; for a tinted frame we'll need a custom `IControl` in polish. Both VST3 and AU build clean and auto-deploy.

- **✅ Step #4 (2026-04-19): GUI v1b — drop zone + recent files.** Live drop zone: click opens platform file dialog (`PromptForFile`), drag-drop uses `IControl::OnDrop`/`OnDropMultiple`. Shows placeholder ("Drop audio here · WAV · AIFF · MP3 · M4A · OGG") until a file loads, then switches to `<filename>` + "click to replace · drop to swap" hint. Below the drop zone, a 5-row `RecentFilesControl` lists session-lifetime recent files (newest on top, duplicates promoted); clicking a row reloads that file. Both controls route through one `loadAndRecord` lambda that calls `Premonition::LoadSourceFile`, updates the drop zone label, and pushes onto the recents list. Multi-format loader (`dsp/AudioLoader.{h,mm}`) handles WAV/AIFF/MP3/M4A via macOS `ExtAudioFile`; OGG via `stb_vorbis` (isolated in `dsp/stb_vorbis_impl.cpp` because its internal macros `L`/`R` leak). `-framework AudioToolbox` + `-framework CoreFoundation` linked on all macOS targets. Mono sources duplicated to L+R; ExtAudioFile handles sample-rate/format conversion (we read at source rate). `PROMPT_FOR_FILE_CALLBACK_SIG`: `(const WDL_String& fileName, const WDL_String& dir)` — `fileName.Get()` is the full path on macOS. `stb` FetchContent'd from nothings/stb master. VST3 + AU build clean; 13/13 Catch2 tests pass.

**Not yet done:** Waveform is still a flat midnight rect (v1c — next). Advanced row (Stretch/Normalize/Mono toggles), drag-out bar, mode tabs — v1d. Custom fonts (Space Grotesk / DM Mono / Fraunces) — polish phase. Plugin not yet loaded/sanity-checked in a host (build succeeded but drag-drop + file dialog untested at runtime).

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
| stb (stb_vorbis) | MIT / public domain | FetchContent master | OGG decoder |
| Catch2 v3.5.3 | BSL-1.0 | FetchContent, tests-only | Not shipped |
| AudioToolbox | Apple system | `-framework` on macOS | WAV/AIFF/MP3/M4A decode |

No rverse code or docs remain. Premonition is product-concept-adjacent to rverse but code-independent.

---

## 6. Recommended Next Actions (in order)

1. ~~**Verify build configures.**~~ ✅ DONE 2026-04-18. See Section 2 for notes. Use `cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug` (not `-G Xcode`).

2. ~~**Run tests + compile plugin.**~~ ✅ DONE 2026-04-19. See Section 2.

3. ~~**GUI v1a.**~~ ✅ DONE 2026-04-19. Panels, 4 knobs, Render button, preset dropdown wired. See Section 2.

4. ~~**GUI v1b.**~~ ✅ DONE 2026-04-19. Drop zone (click + drag-drop), multi-format loader, 5-row recent-files list. See Section 2.

5. **GUI v1c (NEXT):** waveform display — custom `IControl` rendering `mSource` samples over the midnight background, plus Start/End handles bound to `kStart`/`kEnd`. Needs: (a) a way for the control to see `mSource` updates after load — expose via `Premonition::Source()` (already public in header) + a dirty ping (e.g. `pGraphics->ForControlWithTag(...)` from `LoadSourceFile`). (b) Min/max-per-bin downsampling for fast draw. (c) Drag logic for the two handles (translate `mRECT` x to seconds via `frames / sampleRate`, write to `kStart`/`kEnd`).

6. **GUI v1d:** drag-out bar, advanced row (Stretch knob + Normalize/Mono `IVSlideSwitchControl` toggles), mode tabs (`IVTabSwitchControl` bound to `kForward`).

7. **Polish:** custom fonts (Space Grotesk, DM Mono, Fraunces), animations, dashed terracotta drop-zone border (needs custom `IControl` — `IPanelControl` can't take a frame color), offset espresso button shadow.

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
