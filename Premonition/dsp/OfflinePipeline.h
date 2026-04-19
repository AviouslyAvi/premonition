#pragma once

// Premonition offline pipeline — the heart of the plugin.
//
// Signal chain per PREMONITION_BRIEF.md:
//   [source] -> crop(Start, End) -> stretch -> reverb -> reverse
//                                                         |
//                                             fit-to-bar (Length wins)
//                                                         |
//                                                    normalize?
//                                                         |
//                                                  rendered riser
//
// All stages run off the audio thread. Memory is allocated freely.

#include "FitToBar.h"
#include "Reverb.h"
#include "Reverse.h"
#include "TimeStretch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

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
  bool   forward     = false;  // true = skip the reverse step
  bool   normalize   = true;
  bool   monoOutput  = false;
};

struct StereoBuffer
{
  std::vector<float> L;
  std::vector<float> R;

  std::size_t frames() const noexcept { return L.size(); }
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

  // 2. Stretch (tonal shaping)
  if (std::fabs(cfg.stretchRatio - 1.0) > 1.0e-6)
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
  {
    std::vector<float> wL(L.size()), wR(R.size());
    renderReverbStereo(L.data(), R.data(), wL.data(), wR.data(), L.size(),
                       sampleRate, cfg.roomSize, cfg.rt60Seconds, cfg.damping,
                       cfg.mix);
    L = std::move(wL);
    R = std::move(wR);
  }

  // 4. Reverse (unless Forward mode)
  if (!cfg.forward)
    reverseStereoInPlace(L.data(), R.data(), L.size());

  // 5. Fit-to-bar (Length wins over Stretch)
  {
    std::vector<float> fL, fR;
    fitToBarStereo(L.data(), R.data(), L.size(), sampleRate,
                   cfg.lengthBars, cfg.bpm, fL, fR, cfg.beatsPerBar);
    L = std::move(fL);
    R = std::move(fR);
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
