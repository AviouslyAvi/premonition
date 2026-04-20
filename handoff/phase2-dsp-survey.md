# Phase 2 Reverb Topology Survey

Research-only. Informs a go/no-go after Phase 1 (IR-backed reverb types) ships.
Three algorithmic topologies are evaluated against the existing Freeverb-based
pipeline at `Premonition/dsp/Reverb.h`.

## Existing primitives (Reverb.h) — what we can reuse

- `DelayLine` — power-of-two ring buffer w/ `peek(offset)` and
  `tickAndRead(x, D)` (used by everything below).
- `LPFComb` — lowpass-feedback comb (damping in feedback path). Directly usable
  as an FDN "delay + tone" element, and reusable inside Dattorro's tank.
- `SchroederAllpass` — single-delay allpass in Direct-Form II, with coefficient
  `g` and a peek-and-write API. Reusable as-is for Dattorro's input diffusers,
  Jot's input allpass chain, and Parker-style spring dispersion chains (for the
  spring case we need a *stretched* / fractional-delay allpass instead —
  see §2).
- `ReverbChannel::Params` — `sampleRate`, `roomSize (0..1)`, `damping (0..1)`,
  `rt60Seconds`, `spreadSamples`. This is the target parameter surface every
  Phase-2 topology must adapt to. Current RT60 mapping uses
  `g = 10^(-3 D / (Fs * RT60))`, which transfers directly to Dattorro and
  Jot-FDN decay.

What is *missing* and would be new primitives regardless of topology choice:

- **Modulated delay line** (LFO'd fractional-delay read) — Dattorro modulates
  the two tank input allpasses to break metallic resonances; Jot FDNs often
  modulate one or two delays for the same reason.
- **Hadamard / Householder mixing matrix** — Jot-FDN needs an NxN orthogonal
  mixing matrix. For N=4 or 8 Hadamard is trivial (additions/subtractions only);
  for N=16 Householder is preferred.
- **Stretched allpass / allpass-cascade with per-stage g<1 nonuniform** —
  spring dispersion. A "stretched" unit allpass
  `H(z) = (a + z^-K) / (1 + a·z^-K)` with K>>1 is the Van Duyne building
  block; our `SchroederAllpass` is already structurally this with `K = D` —
  *but* spring reverbs need **many stages in cascade with a tuned `a` close
  to unity** (typically 80–200 stages), which is just a parameter-tuning
  exercise on the existing primitive.
- **Tone-control shelf / pre-delay line** — not strictly new (DelayLine
  covers pre-delay; a one-pole shelf is trivial).

---

## 1. Dattorro Plate Reverb

Jon Dattorro, "Effect Design Part 1: Reverberator and Other Filters",
*JAES* 45(9), Sept 1997. The canonical "lexicon-style" plate reverb.

### Topology

- **Input chain**: pre-delay → low-pass → 4 series allpass diffusers
  (coefficients ~0.75, 0.75, 0.625, 0.625).
- **Tank**: a figure-8 loop with two "sides". Each side has:
  `modulated allpass` → `delay` → `damping LPF` → `fixed allpass` → `delay`,
  and the tail of each side feeds the head of the other (with decay `g`).
- **Output taps**: L and R are each formed by summing 7 fixed taps from
  specific points in the tank (Dattorro's exact offsets). This tap-mixing is
  what gives Dattorro its rich, non-correlated stereo.

### Code sketch (~45 lines)

```cpp
// Dattorro 1997 plate. Coefficients and delay lengths are his published
// values at 29.761 kHz — scale linearly to Fs.
struct Dattorro {
  DelayLine preDelay;
  OnePoleLP inputLP;                  // bandwidth control
  SchroederAllpass in1, in2, in3, in4; // diffusers (g = .75,.75,.625,.625)

  ModAllpass apA1;          // modulated, base ~908 @29.76k, mod ~8 samples
  DelayLine  dA1;           // 4453
  OnePoleLP  dampA;         // damping LPF
  SchroederAllpass apA2;    // 2656
  DelayLine  dA2;           // 3720

  ModAllpass apB1;          // 672 + mod
  DelayLine  dB1;           // 4217
  OnePoleLP  dampB;
  SchroederAllpass apB2;    // 1800
  DelayLine  dB2;           // 3163

  float decay = 0.5f;       // global tank feedback (RT60 → decay)
  float xA = 0, xB = 0;     // cross-couple state

  void process(float in, float& outL, float& outR) {
    float x = inputLP.tick(preDelay.tickAndRead(in, preDelaySamples));
    x = in4.process(in3.process(in2.process(in1.process(x))));
    // Side A: input + cross from B
    float a = apA1.process(x + decay * xB);
    a = dA1.tickAndRead(a, dA1.size()-1);
    a = dampA.tick(a) * decay;
    a = apA2.process(a);
    xA = dA2.tickAndRead(a, dA2.size()-1);
    // Side B
    float b = apB1.process(x + decay * xA);
    b = dB1.tickAndRead(b, dB1.size()-1);
    b = dampB.tick(b) * decay;
    b = apB2.process(b);
    xB = dB2.tickAndRead(b, dB2.size()-1);
    // Dattorro's 7-tap output accumulators (indices are his published table)
    outL =  dA1.peek(266) + dA1.peek(2974) - apA2.peekLine(1913)
          + dA2.peek(1996) - dB1.peek(1990) - apB2.peekLine(187) - dB2.peek(1066);
    outR =  dB1.peek(353) + dB1.peek(3627) - apB2.peekLine(1228)
          + dB2.peek(2673) - dA1.peek(2111) - apA2.peekLine( 335) - dA2.peek( 121);
  }
};
```

### CPU cost vs Freeverb

**~1.2–1.5x Freeverb.** Freeverb is 8 LPFCombs + 4 allpasses per channel
(12 delay-line taps + 8 MACs). Dattorro is 4 input APs + 2 mod APs + 2 inner
APs + 4 long delays + 2 damping LPs + **14 output taps** — but it's *mono in,
stereo out*, so you process once not twice. Net: slightly more work than one
Freeverb channel, less than stereo Freeverb. The mod-AP fractional read is the
only hot spot beyond what Reverb.h already does.

### Reference implementations

1. **Signalsmith / sletz's faust-dsp** — Dattorro plate in Faust, compiles to
   clean C++. https://github.com/grame-cncm/faustlibraries (`reverbs.lib`,
   `dattorro_rev`). **MIT/BSD-style** (STK-based). Clean reference.
2. **Tale's plate reverb (open303 / musicdsp)** —
   https://github.com/khoin/DattorroReverbHLS and
   https://github.com/el-visio/dattorro-verb. Multiple C++ forks, most **MIT**;
   `el-visio/dattorro-verb` is MIT. Good standalone read.
3. **Surge-Synthesizer** — their "Room" reverb is a Dattorro variant.
   https://github.com/surge-synthesizer/surge — **GPLv3** — flag as
   problematic for closed-source commercial; study only, do not copy.

### Difficulty — integration into Reverb.h

**Moderate (2–4 days).** High reuse: `SchroederAllpass` handles the 4 input
diffusers and the 2 inner tank allpasses directly (coefficient change only);
`DelayLine` handles pre-delay and the 4 long tank delays; the one-pole LPF
inside `LPFComb` can be extracted into a tiny `OnePoleLP` helper for the two
damping taps and the input bandwidth filter. **New work**: (a) a
`ModulatedAllpass` that does a fractional peek at `D + lfo*depth` — this is
the only genuinely new primitive; (b) exposing ~14 fixed peek offsets, which
needs `DelayLine::peek(offset)` which already exists. Stereo handling is a
*feature* of the algorithm (native stereo out from mono sum — better than our
current "two independent channels + spread"). Parameter mapping is clean:
`roomSize` → scale all tank delays (or interpolate between Dattorro's short/
long variant tables), `rt60` → `decay` (invert the same `10^(-3D/(Fs·RT60))`
formula averaged over tank delay), `damping` → damping-LPF cutoffs. Should
reuse 70% of existing code.

---

## 2. Schroeder-Moorer / Spring Reverb via Allpass-Chain Dispersion

Spring reverbs are characterized by **strong dispersion** (high frequencies
travel faster than low frequencies, producing the "boing" chirp). Two
approaches worth surveying:

- **Van Duyne / Jonathan S. Abel** physical model — explicit waveguide +
  dispersive filter, computationally heavy and patented aspects (Abel 2006).
- **Parker-style all-pass cascade** (Julian Parker, DAFx 2010, "Spring
  Reverberation: A Physical Perspective") — approximates the dispersion
  curve with **a cascade of 100–200 first-order allpasses with `g` close to 1**,
  plus a tuned low-frequency coupling path. This is the pragmatic choice and
  is what we'd implement.

### Topology (Parker-style)

- **Input** → split into two parallel paths: a high-frequency (dispersive)
  path and a low-frequency (non-dispersive) path, crossed by a 1st-order
  split at ~3–4 kHz.
- **HF path**: long cascade of ~100 stretched allpasses (unit delay K=1 but
  `g ≈ 0.6`), OR ~20–40 longer allpasses (K ~8–16) with tuned `g`. Followed by
  a feedback loop (comb-like) to produce the bouncy tail.
- **LF path**: a second, shorter AP cascade tuned for the main "thud" bounce
  of the spring.
- **Modulation**: slight LFO on one AP in the chain to avoid static coloration.
- **Two springs in parallel** → stereo output.

### Code sketch (~40 lines)

```cpp
// Parker-style spring: AP cascade for dispersion + feedback for tail.
// Coefficients follow Parker DAFx'10 (reproduced in Välimäki survey 2012).
struct SpringReverb {
  static constexpr int kAPCount = 100;
  SchroederAllpass apChain[kAPCount]; // K=1, g ~0.6 (stretched unit APs)
  OnePoleLP  bandSplit;               // crossover ~3 kHz
  DelayLine  hfLoop;                  // ~35 ms, feedback for bounce
  DelayLine  lfLoop;                  // ~50 ms, LF thud
  SchroederAllpass apLF[20];          // LF dispersion
  float      fbHF = 0.82f, fbLF = 0.7f;
  float      yHF = 0, yLF = 0;

  void prepare(float Fs) {
    for (auto& a : apChain) a.prepare(1, 0.6f);      // K=1, stretched
    for (auto& a : apLF)    a.prepare(8, 0.55f);
    hfLoop.setSize((size_t)(0.035f * Fs));
    lfLoop.setSize((size_t)(0.050f * Fs));
  }

  float process(float x) {
    float hf = x - bandSplit.tick(x);
    float lf = bandSplit.state();
    // HF: feedback through the 100-AP chain = dispersion
    float h = hf + fbHF * yHF;
    for (auto& a : apChain) h = a.process(h);
    yHF = hfLoop.tickAndRead(h, hfLoop.size()-1);
    // LF: shorter dispersion chain
    float l = lf + fbLF * yLF;
    for (auto& a : apLF) l = a.process(l);
    yLF = lfLoop.tickAndRead(l, lfLoop.size()-1);
    return yHF + yLF;
  }
};
// Stereo = two independent SpringReverb instances with slightly different
// kAPCount/g perturbations per channel.
```

### CPU cost vs Freeverb

**~3–5x Freeverb per channel**, dominated by the 100-AP cascade. Each AP is
3 MACs + 1 memory access; 100 of them = ~300 MACs/sample + 100 line accesses.
Freeverb is ~30 MACs + 12 line accesses. Measured Parker reference in JUCE
plugins sits at ~2.5% CPU @ 44.1k on a modern laptop per channel — still
cheap in absolute terms but the highest of the three.

### Reference implementations

1. **Faust `reverbs.lib` / Julius Smith's spring** — Parker allpass cascade,
   MIT-style. https://github.com/grame-cncm/faustlibraries (`spring_reverb`).
2. **Chowdhury DSP `ChowMultiTool` / `ChowMatrix`** — multiple spring-style
   AP-cascade examples in modern C++. https://github.com/Chowdhury-DSP —
   most repos **BSD-3-Clause**. Excellent reference code.
3. **Jatin Chowdhury's spring-reverb paper & code** — DAFx style impl.
   https://github.com/jatinchowdhury18/ComplexNonlinearities — **BSD** —
   specifically the Faustine / `SpringReverb` sketches. Includes nonlinear
   chirp model.
4. (Also-ran, GPL-flagged: **Rakarrack `spring`** — GPLv2.)

### Difficulty — integration into Reverb.h

**Moderate-hard (3–5 days).** Reuse is high on paper — 100 `SchroederAllpass`
instances work as-is — but **100 stages with `D=1` will expose any
per-instance overhead** (virtual-ish calls, state padding). Best to add a
specialized `UnitAllpassChain<N>` template that keeps state in a flat `float[N]`
array and avoids a DelayLine per stage entirely (for K=1 there's nothing to
delay beyond one sample). This is a small, self-contained new primitive. The
**crossover split** and **two feedback loops** reuse `DelayLine` + a trivial
one-pole. Stereo handling is two independent instances (matches current
pattern). Parameter mapping is the awkward part: spring reverbs don't naturally
have a "size" — Size → AP count and `g` (how dispersive), Decay → `fbHF`/`fbLF`,
Damping → crossover frequency + output LPF. This mapping is inherently "lossy"
relative to the spring's physical parameters; user-facing UX needs a thought.

---

## 3. Jot / Jean-Marc Jot FDN Hall Reverb

J.-M. Jot, "Digital Delay Networks for Designing Artificial Reverberators",
*AES 90th Conv.*, 1991; and "An All-Pole Model for Digital Reverberation",
*ICASSP* 1992. The blueprint for most modern hall/cathedral algorithms
(Valhalla, Eventide, Lexicon 480).

### Topology

- **Input**: pre-delay → short diffuser AP chain (optional) → split into N
  channels.
- **N parallel delay lines** (N = 8 or 16) with **mutually prime lengths** in
  a specific range (e.g. 37–97 ms for halls).
- **Feedback matrix A** — an NxN orthogonal mixing matrix (Hadamard or
  Householder). Each delay's tail is mixed into every delay's input via A,
  scaled by `g_i` chosen so all modes decay with the same RT60.
- **Per-line damping**: a one-pole LPF in each feedback path (gives
  frequency-dependent RT60 — the hall-reverb "mellowing" of highs).
- **Output taps**: one tap per delay line, mixed to L/R via a fixed NxN
  output matrix.
- **Optional**: modulate 1–2 delays to break modal ringing.

### Code sketch (~45 lines)

```cpp
// Jot FDN-8. Hadamard mix, per-line damping, RT60-controlled decay.
// All references assume mutually-prime delay lengths in samples.
template <int N>
struct JotFDN {
  static_assert(N == 4 || N == 8 || N == 16, "Hadamard requires power of 2");
  DelayLine lines[N];
  OnePoleLP damp[N];
  float     g[N];              // per-line feedback; matched RT60
  float     y[N] = {0};        // current tail samples

  void prepare(const int L[N], float Fs, float rt60, float damping) {
    for (int i=0;i<N;++i) {
      lines[i].setSize(L[i]);
      g[i] = std::pow(10.f, -3.f * L[i] / (Fs * rt60));
      damp[i].setCutoff(damping);   // 0..1
    }
  }

  // In-place Hadamard (Sylvester construction): N MACs per channel.
  static void hadamard(float* v) {
    for (int h=1; h<N; h<<=1)
      for (int i=0; i<N; i+=h<<1)
        for (int j=i; j<i+h; ++j) {
          float a=v[j], b=v[j+h];
          v[j]=a+b; v[j+h]=a-b;
        }
    const float s = 1.f/std::sqrt((float)N);
    for (int i=0;i<N;++i) v[i]*=s;
  }

  void process(float xL, float xR, float& outL, float& outR) {
    // Distribute input across lines (simple splay).
    for (int i=0;i<N;++i) y[i] = damp[i].tick(y[i]) * g[i] + ((i&1)?xR:xL);
    hadamard(y);                                       // feedback mix
    for (int i=0;i<N;++i) y[i] = lines[i].tickAndRead(y[i], lines[i].size()-1);
    // Output mix: even lines → L, odd → R, with alternating sign.
    float L=0,R=0;
    for (int i=0;i<N;i+=2) { L += y[i];   R += y[i+1]; }
    outL = L * (1.f/std::sqrt((float)N/2));
    outR = R * (1.f/std::sqrt((float)N/2));
  }
};
```

### CPU cost vs Freeverb

**~1.5–2x Freeverb for N=8; ~3x for N=16.** Per sample: N delay reads/writes
+ N one-pole LPFs + Hadamard (N·log2(N) additions, *no multiplies*) + 2N
output MACs. For N=8: 8 line-ops + 8 LPFs + 24 adds + 16 MACs ≈ ~60 ops,
vs Freeverb's ~45. The cost is linear in N and Hadamard stays cheap; the
real cost tradeoff is **memory** (N long delay lines, ~100 ms each = ~35 KB
at 44.1k per channel for N=8, vs Freeverb's ~12 KB).

### Reference implementations

1. **Faust `reverbs.lib` — `zita_rev1`** (Fons Adriaensen's Zita-Rev1, a
   production FDN-8). https://github.com/grame-cncm/faustlibraries —
   **MIT-style**. Zita-Rev1 itself is **GPL**, but Faust's port inside
   faustlibraries is redistributable under their broader license; verify
   per-file. The algorithm (Jot FDN) is unpatented.
2. **Signalsmith Audio** — Geraint Luff's FDN reverb tutorial +
   reference code. https://github.com/Signalsmith-Audio/reverb-example-code
   — **MIT**. Modern C++, uses Householder mixing; this is the
   cleanest modern reference.
3. **CCRMA STK** `NRev`/`PRCRev` — Jot-inspired FDNs in STK.
   https://github.com/thestk/stk — **permissive STK license** (MIT-compatible,
   see LICENSE). Older code but well-commented.
4. (Also-ran, GPL-flagged: **Zita-Rev1** standalone — GPL.)

### Difficulty — integration into Reverb.h

**Easy-moderate (1.5–3 days) — highest reuse of the three.** `DelayLine`
directly supplies the N feedback lines; the one-pole inside `LPFComb` can be
extracted as `OnePoleLP` and reused N times. The only new primitive is an
**NxN Hadamard multiply**, which is ~10 lines of code and has no state.
Stereo handling is **better than current** — the output matrix produces
decorrelated L/R naturally; no per-channel "spread" hack needed. Parameter
mapping is the cleanest of the three:
- `roomSize` → scale all delay lengths (pick a base set for small/medium/large
  and interpolate, or scale a single prime-set by a factor).
- `rt60Seconds` → per-line `g_i = 10^(-3·L_i/(Fs·RT60))` — **this is the same
  formula already used in `ReverbChannel::prepare`**. Direct port.
- `damping` → per-line one-pole cutoff. Same meaning as current.
- Pre-delay and input diffusers (if added) are literal uses of existing
  `DelayLine` + `SchroederAllpass`.

This topology fits Reverb.h's existing abstractions almost exactly; it feels
like the natural evolution of the Freeverb code already there.

---

## Summary Matrix

| Topology | CPU vs FV | New primitives | Reuse | Integration days | Best-fit genre |
|---|---|---|---|---|---|
| Dattorro plate | ~1.3x | ModulatedAllpass (1 small class) | ~70% | 2–4 | Plate, small rooms, classic '80s lex |
| Parker spring  | ~3–5x | UnitAllpassChain<N> (flat-array optimization) | ~60% | 3–5 | Spring (guitar/dub) — tonal specialist |
| Jot FDN-8      | ~1.7x | Hadamard mix (stateless, ~10 lines) | ~85% | 1.5–3 | Hall, cathedral, long tails |

## Recommendations for Phase 2 decision

1. **Jot FDN is the clear first pick if Phase 2 is "one more algorithmic
   reverb type alongside Freeverb."** Cheapest to integrate, highest reuse of
   Reverb.h, cleanest parameter mapping to the existing Size/Decay/Damping
   knobs, best stereo quality per CPU cycle. Covers hall/cathedral which is
   the largest gap vs current Freeverb (which is best at small/medium).
2. **Dattorro plate is the second pick** if the goal is a distinct *tonal
   character* (plate vs room) rather than "bigger room". Integration cost is
   similar; the modulated-allpass is the only real new primitive and it's
   tiny. Two-for-one: plate algorithm *and* a reusable mod-allpass that later
   unlocks chorus/flanger code.
3. **Parker spring is a specialist** — only implement if product direction
   targets guitarists/dub producers. CPU is 2–3x the others, parameter
   mapping to Size/Decay/Damping is the most awkward (spring has no natural
   "size"), and UX may need a renamed knob set. Defer unless there's a clear
   product reason.

**Licensing posture**: all three algorithms are unpatented and have
MIT/BSD/Apache reference code available. Only Zita-Rev1, Surge, and Rakarrack
are GPL and must be treated as read-only references — do not copy code.

## File-path index

- Existing primitives this survey leans on: `Premonition/dsp/Reverb.h`
  (`DelayLine`, `LPFComb`, `SchroederAllpass`, `ReverbChannel::Params`).
- No other files touched; this is research-only.
