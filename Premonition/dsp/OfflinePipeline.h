#pragma once

// Premonition offline pipeline — the heart of the plugin.
//
// Signal chain branches by cfg.mode:
//   Natural : crop -> reverb -> reverse -> hardTrimWithFade(Length)
//   Stretch : crop -> stretch -> reverb -> reverse -> fit-to-bar
//   Forward : crop -> reverb -> hardTrimWithFade(Length)
// Shared tail: normalize? -> mono-sum?
//
// All stages run off the audio thread. Memory is allocated freely.

#include "StereoBuffer.h"
#include "Convolution.h"
#include "FitToBar.h"
#include "Reverb.h"
#include "Reverse.h"
#include "TimeStretch.h"

#include "../Parameters.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

// Tail crossfade length — blends the reversed tail into the unprocessed
// cropped source so the riser resolves into the original sample instead of
// cutting hard at peak.
inline constexpr double kTailFadeMs = 15.0;

struct PipelineConfig
{
  // Source crop
  double startSeconds = 0.0;
  double endSeconds   = 0.0; // 0 or >= source duration → use full source

  // Pre-reverb
  double stretchRatio = 1.0;

  // Reverb
  float roomSize     = 0.5f;
  float rt60Seconds  = 2.0f;
  float damping      = 0.5f;
  float mix          = 1.0f;   // pre-reverse wet/dry

  // Output
  double lengthBars  = 2.0;    // tempo-synced target
  double bpm         = 120.0;
  int    beatsPerBar = 4;
  int    mode        = kModeNatural;  // Natural / Stretch / Forward (see EMode)
  bool   normalize   = true;
  bool   monoOutput  = false;

  // Reverb type selection. Hall/Plate/Spring/Room currently share the
  // Freeverb network (differentiation flagged as a separate task). Custom
  // replaces the algorithmic reverb with FFT-based IR convolution when `ir`
  // is non-null and non-empty — falls back to algorithmic reverb otherwise.
  int  reverbType         = kTypeHall;
  const StereoBuffer* ir  = nullptr;
  float irSampleRate      = 0.f;

  // Internal A/B flag for Natural/Forward. When true, the last N samples of
  // the reversed tail crossfade into the first N samples of the untouched
  // cropped source (same behavior Stretch mode ships with). Default off in
  // Natural/Forward so the riser is a clean standalone render; Stretch forces
  // it on below regardless of this flag.
  bool crossfadeIntoSource = false;
};

namespace detail {

inline std::size_t clampCrop(double startSec, double endSec,
                             std::size_t nFrames, float sampleRate,
                             std::size_t& outStart, std::size_t& outCount)
{
  outStart = static_cast<std::size_t>(
    std::max<double>(0.0, startSec * sampleRate));
  outStart = std::min(outStart, nFrames);

  const bool endValid = endSec > startSec && endSec > 0.0;
  std::size_t endIdx = endValid
    ? static_cast<std::size_t>(std::llround(endSec * sampleRate))
    : nFrames;
  endIdx = std::min(endIdx, nFrames);

  outCount = (endIdx > outStart) ? (endIdx - outStart) : 0;
  return outCount;
}

inline void peakNormalize(std::vector<float>& L, std::vector<float>& R)
{
  float peak = 0.0f;
  for (float v : L) peak = std::max(peak, std::fabs(v));
  for (float v : R) peak = std::max(peak, std::fabs(v));
  if (peak < 1.0e-9f) return;
  const float g = 0.96605f / peak; // -0.3 dBFS target
  for (float& v : L) v *= g;
  for (float& v : R) v *= g;
}

// Equal-power crossfade: blends the last `fadeN` samples of `tail` against the
// first `fadeN` samples of `head`, in place. Caller guarantees fadeN ≤ both
// sizes. Curve: out[i] = tail[end-N+i]·cos(πt/2) + head[i]·sin(πt/2).
inline void crossfadeTailWithHead(float* tail, std::size_t tailN,
                                  const float* head, std::size_t fadeN)
{
  if (fadeN < 2 || tailN < fadeN) return;
  const std::size_t tailStart = tailN - fadeN;
  constexpr double kHalfPi = 1.5707963267948966;
  const double denom = static_cast<double>(fadeN - 1);
  for (std::size_t i = 0; i < fadeN; ++i)
  {
    const double t = static_cast<double>(i) / denom;
    const float aTail = static_cast<float>(std::cos(kHalfPi * t));
    const float aHead = static_cast<float>(std::sin(kHalfPi * t));
    tail[tailStart + i] = tail[tailStart + i] * aTail + head[i] * aHead;
  }
}

// Conform the rendered tail to `targetLen` samples with a linear fade-out.
// - If shorter than target: pad with leading silence so the peak (end of buf)
//   lands on the bar downbeat.
// - If longer than target: hard-trim from the FRONT, keep the last N samples.
//   The reverse-reverb peak is always at the end; preserve it, drop the quiet
//   head.
// - Always apply a `fadeMs` linear fade at the very end.
// Operates on L/R in lockstep. If targetLen == 0, leaves buffers untouched.
inline void hardTrimWithFade(std::vector<float>& L, std::vector<float>& R,
                             std::size_t targetLen, float sampleRate,
                             double fadeMs = 100.0)
{
  if (targetLen == 0) return;
  const std::size_t n = std::min(L.size(), R.size());

  if (n < targetLen)
  {
    const std::size_t pad = targetLen - n;
    L.insert(L.begin(), pad, 0.f);
    R.insert(R.begin(), pad, 0.f);
    L.resize(targetLen);
    R.resize(targetLen);
  }
  else if (n > targetLen)
  {
    const std::size_t drop = n - targetLen;
    L.erase(L.begin(), L.begin() + drop);
    R.erase(R.begin(), R.begin() + drop);
    L.resize(targetLen);
    R.resize(targetLen);
  }
  else
  {
    L.resize(targetLen);
    R.resize(targetLen);
  }

  const std::size_t fadeN = std::min<std::size_t>(
    static_cast<std::size_t>(fadeMs * 1.0e-3 * sampleRate), targetLen);
  if (fadeN < 2) return;
  const std::size_t start = targetLen - fadeN;
  const double denom = static_cast<double>(fadeN - 1);
  for (std::size_t i = 0; i < fadeN; ++i)
  {
    const float g = static_cast<float>(1.0 - static_cast<double>(i) / denom);
    L[start + i] *= g;
    R[start + i] *= g;
  }
}

inline void monoSum(std::vector<float>& L, std::vector<float>& R)
{
  const std::size_t n = std::min(L.size(), R.size());
  for (std::size_t i = 0; i < n; ++i)
  {
    const float m = 0.5f * (L[i] + R[i]);
    L[i] = m;
    R[i] = m;
  }
}

} // namespace detail

// Render the full pipeline. `src` is the loaded source (stereo, deinterleaved).
// Returns the rendered riser.
inline StereoBuffer renderRiser(const StereoBuffer& src, float sampleRate,
                                const PipelineConfig& cfg)
{
  StereoBuffer out;
  if (src.frames() == 0) return out;

  // 1. Crop to [Start, End]
  std::size_t cropStart = 0, cropLen = 0;
  detail::clampCrop(cfg.startSeconds, cfg.endSeconds, src.frames(), sampleRate,
                    cropStart, cropLen);
  if (cropLen == 0) return out;

  std::vector<float> L(src.L.begin() + cropStart,
                       src.L.begin() + cropStart + cropLen);
  std::vector<float> R(src.R.begin() + cropStart,
                       src.R.begin() + cropStart + cropLen);

  // Keep an untouched copy of the cropped source for the post-reverse
  // crossfade (step 4.5 below).
  const std::vector<float> srcL = L;
  const std::vector<float> srcR = R;

  // 2. Pre-reverb stretch — Stretch mode only. Natural/Forward skip this so
  //    they stay resampling-free.
  if (cfg.mode == kModeStretch
      && std::fabs(cfg.stretchRatio - 1.0) > 1.0e-6)
  {
    std::vector<float> sL, sR;
    stretchStereoByRatio(L.data(), R.data(), L.size(), sampleRate,
                         cfg.stretchRatio, sL, sR);
    L = std::move(sL);
    R = std::move(sR);
  }

  // 3. Reverb (pre-reverse — this is what makes it a reverse-REVERB, not a
  //    reverse-SAMPLE). Mix blends wet into dry here; when Forward is on we
  //    want a pure wet render is a judgment call — we leave mix user-controlled.
  const bool useConvolution = (cfg.reverbType == kTypeCustom)
    && cfg.ir && !cfg.ir->L.empty() && !cfg.ir->R.empty();
  if (useConvolution)
  {
    StereoBuffer ir = (cfg.irSampleRate > 0.f && cfg.irSampleRate != sampleRate)
      ? resampleIR(*cfg.ir, cfg.irSampleRate, sampleRate)
      : *cfg.ir;
    std::vector<float> wL, wR;
    convolveStereo(L.data(), R.data(), ir.L.data(), ir.R.data(),
                   L.size(), ir.L.size(), cfg.mix, wL, wR);
    L = std::move(wL);
    R = std::move(wR);
  }
  else
  {
    std::vector<float> wL(L.size()), wR(R.size());
    renderReverbStereo(L.data(), R.data(), wL.data(), wR.data(), L.size(),
                       sampleRate, cfg.roomSize, cfg.rt60Seconds, cfg.damping,
                       cfg.mix);
    L = std::move(wL);
    R = std::move(wR);
  }

  // 4. Reverse — Natural and Stretch; Forward keeps the tail forward.
  const bool doReverse = (cfg.mode != kModeForward);
  if (doReverse)
    reverseStereoInPlace(L.data(), R.data(), L.size());

  // 4.5 Crossfade the reversed tail into the untouched cropped source so the
  //     riser resolves into the original sample instead of cutting at peak.
  //     Always on in Stretch (ships with it); gated by cfg.crossfadeIntoSource
  //     in Natural; never in Forward (no reverse-peak to soften).
  const bool doCrossfade = doReverse
    && (cfg.mode == kModeStretch || cfg.crossfadeIntoSource);
  if (doCrossfade)
  {
    const std::size_t fadeN = std::min<std::size_t>(
      static_cast<std::size_t>(kTailFadeMs * 1.0e-3 * sampleRate),
      std::min(L.size(), srcL.size()));
    detail::crossfadeTailWithHead(L.data(), L.size(), srcL.data(), fadeN);
    detail::crossfadeTailWithHead(R.data(), R.size(), srcR.data(), fadeN);
  }

  // 5. Output-length stage. Stretch uses fit-to-bar resampling (the legacy
  //    path). Natural/Forward use hard-trim + 100 ms fade and stay
  //    resampling-free — pad with leading silence when short so the peak
  //    lands on the bar downbeat.
  if (cfg.mode == kModeStretch)
  {
    std::vector<float> fL, fR;
    fitToBarStereo(L.data(), R.data(), L.size(), sampleRate,
                   cfg.lengthBars, cfg.bpm, fL, fR, cfg.beatsPerBar);
    L = std::move(fL);
    R = std::move(fR);
  }
  else
  {
    const double secsPerBeat = (cfg.bpm > 0.0) ? 60.0 / cfg.bpm : 0.5;
    const double targetSec = cfg.lengthBars * cfg.beatsPerBar * secsPerBeat;
    const long long rounded = std::llround(targetSec * sampleRate);
    const std::size_t targetLen = (rounded > 0)
      ? static_cast<std::size_t>(rounded) : 0;
    detail::hardTrimWithFade(L, R, targetLen, sampleRate);
  }

  // 6. Normalize
  if (cfg.normalize)
    detail::peakNormalize(L, R);

  // 7. Mono sum (post-normalize so channel balance is preserved)
  if (cfg.monoOutput)
    detail::monoSum(L, R);

  out.L = std::move(L);
  out.R = std::move(R);
  return out;
}

}} // namespace premonition::dsp
