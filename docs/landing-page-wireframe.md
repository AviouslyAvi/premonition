# Premonition — Landing Page Wireframe

**Goal:** convert producers and investors in under 15 seconds of scroll. Dark, cinematic, confident. No stock-plugin clichés.

**Companion file:** [`landing-page-wireframe.html`](./landing-page-wireframe.html) — static visual mockup you can open in a browser.

---

## Section Map

| # | Section | Purpose | Primary CTA |
|---|---------|---------|-------------|
| 1 | Hero | Hook + tagline + demo audio | "Hear it" (scroll / play) |
| 2 | The Idea | Reframe reverb as foresight | — |
| 3 | Demo | Before/after A/B audio player | Play toggle |
| 4 | What It Does | 4 feature pillars | — |
| 5 | Why It Matters | Source → Becomes table | — |
| 6 | Built For | Use-case grid | — |
| 7 | Identity | Brand poetry block | — |
| 8 | Tech Strip | VST3 · AU · CLAP · Mac/Win | — |
| 9 | Final CTA | Download / Early Access | "Get Premonition" |
| 10 | Footer | Links, socials, license | — |

---

## 1. Hero

```
┌──────────────────────────────────────────────────────────┐
│  PREMONITION                                   [ menu ]   │
│                                                            │
│                                                            │
│           HEAR WHAT HAPPENS                                │
│           BEFORE IT HAPPENS.                               │
│                                                            │
│           A forward-pulling reverse-reverb engine.         │
│                                                            │
│           [ ▶ Hear it ]   [ Early Access → ]              │
│                                                            │
│                                                            │
│                   ~~~ animated waveform ~~~                │
│                   (reverse swell → impact)                 │
└──────────────────────────────────────────────────────────┘
```

- Full-viewport hero, near-black (#07080B) with a single accent swell.
- Headline in a tight display serif or a confident geometric sans (NOT Inter).
- Animated SVG/Canvas waveform that visually "pulls inward" toward a downbeat marker — loops every ~4s.
- Two CTAs: primary (accent-filled) and ghost.

---

## 2. The Idea

```
┌──────────────────────────────────────────────────────────┐
│                                                            │
│   What if sound didn't trail behind —                      │
│   but led the way?                                         │
│                                                            │
│   Traditional reverb reflects the past.                    │
│   Premonition reaches into the future.                     │
│                                                            │
└──────────────────────────────────────────────────────────┘
```

- Quiet, centered, high leading. Pull-quote treatment.
- Subtle horizontal rule made of a reverse-swell gradient.

---

## 3. Demo (A/B)

```
┌──────────────────────────────────────────────────────────┐
│   HEAR THE DIFFERENCE                                      │
│                                                            │
│   ┌────────────────┐   ┌────────────────┐                 │
│   │  DRY           │   │  WITH PREMONI- │                 │
│   │  ▶ ──────────  │   │  ▶ ──────────  │                 │
│   │  vocal / drop  │   │  vocal / drop  │                 │
│   └────────────────┘   └────────────────┘                 │
│                                                            │
│   [ Vocal ]  [ Drop ]  [ Transition ]  [ Sound Design ]   │
└──────────────────────────────────────────────────────────┘
```

- Side-by-side audio cards with synced scrubbers.
- Tab row swaps the source material.
- Waveforms render live; the "with Premonition" card shows the pre-impact swell visibly leading the hit.

---

## 4. What It Does

Four-up pillar grid:

```
┌────────────┬────────────┬────────────┬────────────┐
│ PRE-IMPACT │ TENSION    │ FORWARD    │ TRANSITION │
│ SWELLS     │ SHAPING    │ MOTION     │ INTELLIGENCE│
│            │            │            │            │
│ Generated  │ Sample-    │ Static     │ Drops,     │
│ automati-  │ accurate   │ sources    │ vocals,    │
│ cally, in  │ lead-in    │ gain       │ cinematic  │
│ real time. │ control.   │ momentum.  │ builds.    │
└────────────┴────────────┴────────────┴────────────┘
```

- Each cell gets a tiny abstract glyph (not icons-from-a-pack).
- Hover state: glyph animates into a reverse-swell gesture.

---

## 5. Why It Matters — Transformation Table

```
┌──────────────────────────────────────────────────────────┐
│   SOURCE            →   BECOMES                            │
│   ─────────────────────────────────────────               │
│   Vocals            →   Emotional lead-ins                 │
│   Drops             →   Gravitational events               │
│   Transitions       →   Narrative devices                  │
│   Sound design      →   Foreshadowing                      │
└──────────────────────────────────────────────────────────┘
```

- Monospaced or tabular-figures font. Feels like a spec sheet on purpose.

---

## 6. Built For

Horizontal chip row, scrollable on mobile:

`EDM build-ups` · `R&B vocal beds` · `Ambient textures` · `Film scoring` · `Trailer sound design` · `Game audio`

---

## 7. Identity

```
┌──────────────────────────────────────────────────────────┐
│                                                            │
│   Between anticipation and impact.                         │
│   Between intuition and certainty.                         │
│   Between present and future.                              │
│                                                            │
│   Not nostalgia. Foresight.                                │
│                                                            │
└──────────────────────────────────────────────────────────┘
```

- Oversized type, low contrast on background. Negative space does the work.

---

## 8. Tech Strip

One slim band, centered:

`VST3  ·  AU  ·  CLAP  ·  macOS (Universal)  ·  Windows`

Light, uppercase, letter-spaced. No logos unless licensed.

---

## 9. Final CTA

```
┌──────────────────────────────────────────────────────────┐
│                                                            │
│        THE REVERB THAT ARRIVES                             │
│        BEFORE THE SOUND.                                   │
│                                                            │
│        [  Get Early Access  →  ]                           │
│                                                            │
│        $14.99 · VST3 / AU / CLAP                           │
└──────────────────────────────────────────────────────────┘
```

- Mirrors hero typographically to bookend the page.
- Email capture inline, no modal.

---

## 10. Footer

```
Premonition    |    Docs    GitHub    Changelog    License    |    © 2026
```

---

## Visual Direction

- **Palette:** near-black `#07080B` background · off-white `#ECECEC` text · single accent in the violet-to-cyan range (e.g. `#8B5CF6` → `#22D3EE` gradient for the swell animation only).
- **Type:** display = a confident geometric (e.g. *Neue Haas Grotesk Display*, *Söhne*, *GT America Mono* for labels). **Avoid Inter.**
- **Motion:** one hero swell, one demo waveform, one on-scroll fade per section. Nothing else moves.
- **Imagery:** zero stock photos. All visuals are generated waveforms or typography.
- **Dark theme only.** No light-mode toggle — commits to the studio aesthetic.

## Page-Level Notes

- Single page, scroll-driven. No nav beyond a sticky logo + "Get" button in the top-right after hero leaves viewport.
- All audio lazy-loads and is pre-decoded so A/B playback is gapless.
- Copy is final (from the polished brief) — don't substitute placeholder lorem.
- Accessibility: every audio demo needs a transcript/description; animation respects `prefers-reduced-motion`.
