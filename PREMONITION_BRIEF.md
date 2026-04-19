# Premonition — v1 Product Brief

**Status:** Frozen spec. Implementation pending.
**Owner:** Avi Ben-Abram / Lumora IT
**Price point:** $12.99

---

## One-liner

Offline reverse-reverb riser generator. Drop a sample in, drag a tempo-synced riser out.

---

## Signal chain (v1)

```
[source sample] → [crop: Start/End] → [Stretch] → [Reverb] → [Reverse]
                                                                 ↓
                                                       [Fit to Length bars]
                                                                 ↓
                                                          [Normalize?]
                                                                 ↓
                                                       [Rendered riser]
```

All processing is offline. No realtime audio path. No DAW latency.

---

## Parameters

| Param | Behavior |
|---|---|
| **Start** | Crop in-point in source (seconds, draggable waveform handle) |
| **End** | Crop out-point in source (seconds, draggable waveform handle) |
| **Stretch** | Time-stretch ratio applied to cropped source *before* reverb. Changes tonal character of the reverse-reverb. |
| **Size** | Reverb room size |
| **Decay** | Reverb RT60 |
| **Mix** | Reverb wet/dry (applied pre-reverse) |
| **Length** | Output riser duration in **bars** (tempo-synced to host BPM). Riser ends exactly at the downbeat. **Length wins** over Stretch — final fit-to-bar pass is applied after all other processing. |
| **Forward** | Toggle. When on, bypasses the reverse step → outputs a normal wet-reverb render. Useful for baking reverb into a sample. |
| **Normalize** | Peak-normalize output (on/off) |
| **Mono/Stereo** | Output channel mode (stereo default; mono-sums when engaged) |
| **Algorithm** | Reverb preset: Hall / Plate / Spring / Room |

---

## UI surfaces

- Drop-audio-here bin
- Original sample waveform (pre-render) with Start/End drag handles
- Rendered tail waveform (post-render)
- Preset picker (reverb algorithm)
- Render button
- Preview button
- Drag-to-render handle (drag WAV out to DAW)
- Queue panel (batch-render list + recent-renders history)

---

## Constraints

- **Max input duration:** 2 minutes
- **Input channels:** mono or stereo accepted
- **Output channels:** always stereo (mono-summed if Mono toggle on)
- **Tempo:** pulled from host
- **Output:** riser only. Does not include the dry hit. User places the hit on the next bar in their DAW.

---

## Out of scope for v1

| Milestone | Feature |
|---|---|
| v2 | Snapback-style pre-swell: bake `[reverse-reverb swell] + [original hit]` into one WAV so user can drag out a combined clip |
| v3 | VST3 reverb plugin hosting — user swaps in 3rd-party reverbs (filtered to reverb category) in place of the internal algorithms |
| v4 | Realtime insert mode (requires host-lookahead story) |

---

## Dependencies

| Dep | License | Role |
|---|---|---|
| iPlug2 | zlib-like | Plugin framework |
| signalsmith-stretch | MIT | Time-stretching |
| Catch2 (tests) | BSL-1.0 | Unit tests, not shipped |

All commercial-friendly. No GPL/JUCE obligations. Ship-ready for paid distribution with a bundled `THIRD-PARTY-NOTICES.txt`.

---

## Relationship to prior code

Earlier scaffolding was forked from [SamuFL/rverse](https://github.com/SamuFL/rverse) (MIT). The DSP, project files, and naming will be rewritten from scratch (clean-room, textbook-reference-based) so Premonition ships as fully independent code. The product concept (reverse-reverb riser generator) was developed independently — this spec is the reference going forward, not the upstream brief.
