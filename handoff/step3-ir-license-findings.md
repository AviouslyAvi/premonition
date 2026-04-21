# Step 3 — CC0 IR license findings (2026-04-21)

Executed per [HANDOFF-2026-04-19-reverb-types-and-modes.md](HANDOFF-2026-04-19-reverb-types-and-modes.md) Agent 1 prompt.

## Verdict: **0 usable CC0 IRs exist for Hall or Room** — synthesize all four types in-house.

This is the pre-approved fallback per the handoff's Decisions section ("*Fallback plan (if CC0 Hall/Room pickings are thin): synthesize those too, in-house. Avi explicitly approved 'everything synthesized in-house' as the fallback.*").

## Sources evaluated

| Source | License | Verdict |
|---|---|---|
| **EchoThief** (Christopher Warren) | CC-BY 4.0 — "*made available through the Creative Commons Attribution License*" | **REJECT** — attribution required |
| **Voxengo free IR pack** | "*free to use in your productions*" — no PD dedication, silent on redistribution/embedding | **REJECT** — ambiguous for closed-source embedded redistribution |
| **Fokke van Saane** | "*free to use*" — no formal license grant | **REJECT** — no redistribution clause, not a PD dedication |
| **openair-impulseresponses.co.uk** | CC-BY-SA | **REJECT** (handoff excludes share-alike) |
| **MIT IR Survey / Damian Murphy** | CC-BY-SA or academic-only | **REJECT** |
| **NASA / US-gov audio (PD per 17 USC §105)** | Public domain | **REJECT** — wrong content (spoken word/telemetry, no stereo room IRs) |
| **Freesound CC0 tag** | Self-declared CC0, no vetting | **REJECT** — risky provenance for a shipping commercial plugin |
| **archive.org PD collections** | Varies | No curated IR library meeting spec |

## Why not commission or license?

Commissioning a work-for-hire IR capture with full copyright assignment is the only clean path to a "captured hall" — but this contradicts the "everything synthesized in-house" fallback Avi pre-approved and adds cost/time with no user-visible benefit for a reverse-reverb riser generator (where parametric control matters more than authentic room capture).

## Action → re-scope Agent 3

Original Agent 3 scope: synthesize **Plate + Spring**. New scope: synthesize **Plate + Spring + Hall + Room**. All four IRs rendered in-house via `tools/render_ir.cpp`. This keeps licensing clean (100% Avi-owned) and the binary impact unchanged (~1–2 MB for four stereo WAVs).

Proposed synthesis targets:
- **Hall** — long FDN or velvet-noise decay, ~3–5 s RT60, bright tilt, wide concert-hall spread. (Phase 2's FDN work would replace this with live DSP; in Phase 1 we bake the IR.)
- **Plate** — Dattorro-topology, ~2–3 s, dense, bright, EMT-140 flavor.
- **Spring** — allpass chain + dispersion, ~1.5–2.5 s, boingy, metallic.
- **Room** — short FDN with early-reflection pattern, 0.5–1.2 s, warm small live room.

## Ready for Step 5

Step 4 (stereo widening) proceeds unchanged — doesn't depend on IR content. Step 5 (Agent 3 synthesis) should be spawned with the expanded 4-type scope.
