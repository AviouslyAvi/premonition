# Premonition — GUI Design Brief

**Status:** Design spec for v1 IGraphics implementation.
**Visual source of truth:** [mockup.html](mockup.html). When this doc and the mockup disagree, the mockup wins.
**Product spec:** [PREMONITION_BRIEF.md](PREMONITION_BRIEF.md)

---

## 1. What Premonition looks like

Warm, editorial, AVIOUS-branded. Not dark studio. Not skeuomorphic. Two-panel split: the left is a workflow surface (drop → queue → waveform → render → drag out), the right is a minimal effect rack. Terracotta and oxblood accents, cotton background, espresso ink, mustard knob pointers.

Think: editorial magazine meets boutique plugin, not FabFilter or Serum.

---

## 2. Framework constraints

- **iPlug2 IGraphics** (C++). NanoVG renderer: Metal (macOS), GL2 (Windows).
- **Default size:** 960×600 (matches mockup max-width). Corner resizable, scales proportionally.
- **Fonts:** Roboto-Regular (bundled default). Custom fonts (Space Grotesk, DM Mono, Fraunces) are the aspirational target — add as bundled resources when we wire real fonts. For v1 implementation, Roboto with careful tracking approximates the feel.
- **No custom bitmap assets required for v1.** All shapes drawable with NanoVG primitives.

---

## 3. Parameters → UI controls

Params defined in [Parameters.h](Premonition/Parameters.h). This table maps them to UI.

| Param | Control | Location | Notes |
|---|---|---|---|
| `kStart`, `kEnd` | Waveform drag handles | Inside midnight waveform panel | Two vertical handles on the loaded sample waveform. Seconds display on hover. |
| `kStretch` | Knob | Right rack, advanced row | Shown as "Stretch" with ratio value (e.g. `1.25×`). |
| `kSize` | Knob | Right rack, knob grid (top-left) | Mockup value: 48% |
| `kDecay` | Knob | Right rack, knob grid (top-right) | Mockup value: 2.4s |
| `kMix` | Knob | Right rack, knob grid (bottom-right) | Mockup value: 100% |
| `kLength` | Knob | Right rack, knob grid (bottom-left), labeled **Tail** | Mockup says "Tail · 4 beats" — tempo-synced bars from frozen spec, displayed as beats for friendliness. |
| `kAlgorithm` | Preset row (dropdown) | Bottom of right rack | Mockup: "Cinematic" — our values: Hall / Plate / Spring / Room |
| `kForward` | Mode tab | Top-right header, second tab ("Forward") | Replaces mockup's "Insert" tab — when on, pipeline skips reverse step. |
| `kNormalize` | Slide toggle | Below knob grid, left of dropdown | Visible in the reference screenshot. |
| `kMonoStereo` | Slide toggle | Below knob grid, right of dropdown | Mono-sum output when on. |

### Non-parameter UI elements

| Element | Purpose |
|---|---|
| **Drop zone** | Dashed terracotta border, "Drop audio here · WAV · AIFF" — accepts file drops, opens file dialog on click. |
| **Queue list** | Batch-render list + recent-renders. Dot colors: sage (rendered), terracotta (ready), ink-faint (pending). |
| **Waveform stage** | Midnight-blue panel. Shows the loaded source pre-render; after render, morphs to show rendered riser. Start/End handles live here. |
| **Render button** | Oxblood primary. Triggers offline pipeline. 3px espresso drop shadow, press-depresses on click (mockup shows `translate(1.5px, 1.5px)` on hover). |
| **Preview button** | Ghost link style, mono caps. Plays the most recent render through the host output. |
| **Drag-out bar** | Sage green pill with filename + duration + `↗` icon. Drag-to-DAW target. |
| **Footer** | Mono caps: sample rate · BPM · version. |

---

## 4. Layout (960×600 default)

```
┌────────────────────────────────────────────────────────────┐
│  [orb] Premonition              DROP · FORWARD · INSERT*   │  ← header, 70px
├─────────────────────────────────────┬──────────────────────┤
│                                     │                      │
│  [ Drop audio here · WAV · AIFF ]   │  Reverse Reverb      │
│                                     │  ─── MODULE · AVIOUS │
│  QUEUE                              │                      │
│  · snare_hit_14.wav        0.82s    │   ┌────┐   ┌────┐    │
│  · kick_808_low.wav        1.40s    │   │Size│   │Dcay│    │
│  · vocal_chop_01.wav       2.10s    │   └────┘   └────┘    │
│                                     │                      │
│  ┌─────────────────────────────┐    │   ┌────┐   ┌────┐    │  ← body grid:
│  │  [ midnight waveform ]      │    │   │Tail│   │Mix │    │    left 1fr,
│  │   [Start]           [End]   │    │   └────┘   └────┘    │    right 300px
│  └─────────────────────────────┘    │                      │
│                                     │  [Stretch ═══]       │
│  [  RENDER  →  ]    Preview         │  [Normalize] [Mono]  │
│                                     │                      │
│  RESULT · DRAG INTO YOUR DAW        │  ┌──────────────┐    │
│  [ filename  3.22s · stereo  ↗ ]    │  │ PRESET  Hall▾│    │
│                                     │  └──────────────┘    │
├─────────────────────────────────────┴──────────────────────┤
│  48 kHz · 120 bpm                               v0.1.0     │  ← footer, 40px
└────────────────────────────────────────────────────────────┘
```

*`INSERT` tab is v4 scope — hidden or disabled in v1. Mode tabs in v1 = `DROP` (default) and `FORWARD` (toggles `kForward`).

### Padding and grid rules

- Outer plugin border: 1.5px espresso, 16px rounded, 6×6 espresso drop shadow.
- Inner padding: 28px left/right on both panels, 32px top/bottom.
- Header and footer separators: 1.5px espresso horizontal lines.
- Left/right panel divider: 1.5px espresso vertical line.
- Right panel background: cotton-2 (slightly darker than main cotton).
- Knob grid: 2×2, 54×54 dials, 20px vertical gap / 16px horizontal.
- Button shadow: 3×3 espresso offset, collapses to 1.5×1.5 on press.

---

## 5. Color palette (authoritative — from mockup.html)

| Token | Hex | Role |
|---|---|---|
| `--oxblood` | `#4E0000` | Primary action button (Render), brand depth |
| `--terracotta` | `#A24617` | Drop zone border, active tab underline, hover accent |
| `--sage` | `#797028` | Drag-out bar, "rendered" status dot |
| `--midnight` | `#1A243D` | Waveform background panel |
| `--mustard` | `#D39203` | Knob pointers, highlight accents |
| `--espresso` | `#241916` | Borders, primary text, shadows |
| `--cotton` | `#F1ECE1` | Main background, button text on dark |
| `--cotton-2` | `#E8E1D2` | Right panel (rack) background |
| `--ink-soft` | `#5a4a3e` | Secondary text, mono labels |
| `--ink-faint` | `#8a7a6c` | Tertiary text, meta, inactive tab labels |

### Waveform colors (for the midnight stage)

| Region | Color |
|---|---|
| Background | `#1A243D` (midnight) |
| Quiet intro (first bars) | `rgba(241,236,225,0.25)` cotton at 25% |
| Swell body (gradient) | Terracotta `#A24617` → Mustard `#D39203` as amplitude rises |
| Peak crest | Oxblood `#4E0000` |
| Tail after peak | Terracotta at fading alpha |
| Start/End handles | Mustard vertical line, cotton dot at top |

---

## 6. Typography

| Element | Font | Size | Weight | Color |
|---|---|---|---|---|
| Brand name "Premonition" | Fraunces (display) | 26px | 600 | espresso |
| Zone headline ("Drop audio here") | Fraunces | 20px | 500 | espresso |
| Rack head ("Reverse Reverb") | Fraunces | 20px | 600 | espresso |
| UI body | Space Grotesk (sans) | 13–14px | 400–500 | espresso |
| Mono labels (`QUEUE`, `RESULT · DRAG...`) | DM Mono | 10–11px | 400, `letter-spacing: 0.22em`, UPPERCASE | ink-faint |
| Mono values ("0.82s", "v0.1.0") | DM Mono | 10–11px | 400 | ink-soft / ink-faint |
| Mode tabs | DM Mono | 11px | 400, `letter-spacing: 0.28em`, UPPERCASE | ink-faint (active: espresso, underline: terracotta) |
| Button text ("RENDER →") | Space Grotesk | 13px | 600, `letter-spacing: 0.2em`, UPPERCASE | cotton |

**iPlug2 fallback:** Until Space Grotesk / DM Mono / Fraunces are bundled, use Roboto-Regular for all text with manual tracking. Visual difference is acceptable for v1.

---

## 7. Control styling

### Knob (`IVKnobControl`)

- 54px diameter
- Dial body: espresso (`#241916`) solid fill
- Pointer: 2.5px wide × 12px tall mustard (`#D39203`) line, rounded caps, positioned 7px from top center
- Arc indicator around the dial: mustard sweep on the right third of the circumference (from bottom-left around to top), cotton-2 for the unswept portion — see the reference screenshot
- Label below dial: DM Mono 10px, ink-soft, uppercase, 0.2em tracking
- Value below label: DM Mono 11px, espresso
- Range: -135° (min) to +135° (max), top-center = default for symmetric params

### Buttons

- **Primary (Render):** oxblood fill, 1.5px espresso border, 10px rounded, 16px padding, 3×3 espresso drop shadow. Press animation: translate(1.5, 1.5), shadow to 1.5×1.5.
- **Link (Preview):** transparent, ink-soft text, hover → terracotta.
- **Mode tab:** transparent, ink-faint text, 1.5px transparent bottom border. Active: espresso text, terracotta bottom border.

### Drop zone

- 1.5px dashed terracotta border, 10px rounded, cotton background
- Hover: background `rgba(162,70,23,0.06)` (terracotta at 6%)
- Active drag-over: background `rgba(162,70,23,0.12)`, border solid

### Drag-out bar

- Sage fill (`#797028`), 1.5px espresso border, 10px rounded
- Cotton text, DM Mono meta at 75% alpha
- Fraunces 22px `↗` glyph on the right
- Cursor: grab when audio is rendered, disabled (opacity 0.4) when empty

### Slide toggle (Normalize, Mono/Stereo)

- 44×22 pill, espresso border, cotton-2 track when off, sage track when on
- 18px espresso circle handle, translates left/right on toggle

### Preset dropdown

- Cotton fill, 1.5px espresso border, 10px rounded
- Left: DM Mono 10px uppercase "PRESET" label + Space Grotesk 13px value
- Right: ink-faint caret `▾`
- Click: opens iPlug2 popup menu with Hall / Plate / Spring / Room

---

## 8. iPlug2 implementation notes

### Widget mapping

| Mockup element | iPlug2 control |
|---|---|
| Knobs | `IVKnobControl` with custom `IVStyle` |
| Primary button | `IVButtonControl` with shadow override (likely needs custom `IControl` subclass for the offset shadow) |
| Slide toggle | `IVSlideSwitchControl` |
| Mode tabs | `IVTabSwitchControl` |
| Preset dropdown | `ICaptionControl` backed by param enum, triggers `PromptForPopupMenu` |
| Drop zone | Custom `IControl` subclass (handles drag-drop via `OnDrop`) |
| Queue list | Custom `IControl` — draws rows, dots, text manually |
| Waveform stage | Custom `IControl` — `IVDisplayControl` is close but needs our color gradient and handle overlays |
| Drag-out bar | Custom `IControl` — uses iPlug2's `CreateTextEntry` / platform drag-out API |

### IVStyle presets

Create three reusable styles in the plugin constructor:

```cpp
// Warm primary — for knobs and buttons
const IVStyle kPremonitionKnobStyle = DEFAULT_STYLE
  .WithColors(IVColorSpec{{
    /* kBG  */ COLOR_TRANSPARENT,
    /* kFG  */ IColor::FromColorCode(0x241916), // espresso
    /* kPR  */ IColor::FromColorCode(0x4E0000), // oxblood
    /* kFR  */ IColor::FromColorCode(0x241916),
    /* kHL  */ IColor::FromColorCode(0xD39203), // mustard
    /* kSH  */ IColor::FromColorCode(0x241916),
    /* kX1  */ IColor::FromColorCode(0xA24617), // terracotta
    /* kX2  */ IColor::FromColorCode(0x797028), // sage
    /* kX3  */ IColor::FromColorCode(0xE8E1D2), // cotton-2
  }})
  .WithRoundness(0.3f)
  .WithDrawShadows(true);
```

Define an `IColor::FromColorCode(uint32_t hex)` helper if iPlug2 lacks it — simple extraction of RGB bytes.

### Phasing

1. **GUI v1a** — Layout skeleton: panels, headers, the 4 knobs wired, Render/Preview buttons, preset dropdown. No drop-drag, no waveform, no queue.
2. **GUI v1b** — Drop zone + file loading + queue list.
3. **GUI v1c** — Waveform display + Start/End handles.
4. **GUI v1d** — Drag-out, advanced row (Stretch, Normalize, Mono toggles), mode tabs.
5. **Polish** — Custom fonts, shadow-offset button, animations.

Don't try to nail everything at once. v1a produces something you can click and render with.

---

## 9. Design principles (from mockup)

- **One-glance clarity.** Drop → render → drag-out is the story. Nothing gets between those three points.
- **Warm, not sterile.** Cotton background and editorial serifs keep it from feeling like a dev tool.
- **Shadows are part of the language.** The 3×3 espresso offset on the button, the 6×6 plugin frame shadow — these are not incidental. They give the UI a printed, tactile feel.
- **Type does heavy lifting.** Fraunces (serif) for soul, DM Mono for data, Space Grotesk for UI. The combination is the brand.
- **Color does work.** Oxblood is gravity (action). Terracotta is invitation (drop, hover). Sage is completion (drag-out). Mustard is focus (knob pointer). Everything else is quiet.
