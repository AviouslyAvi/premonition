#pragma once

// Time-stretching wrapper around signalsmith-stretch (MIT).
// Two modes:
//   stretchByRatio  — output length = input length * ratio
//   stretchToLength — output length = exactly N samples (computes ratio internally)
//
// Stateless from the caller's perspective: each call owns its stretcher.
// Offline use only.

#include "signalsmith-stretch.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

// Stretch mono buffer by a ratio. Returns new vector of length round(n * ratio).
inline std::vector<float> stretchByRatio(const float* src, std::size_t n,
                                         float sampleRate, double ratio)
{
  if (n == 0 || ratio <= 0.0)
    return {};

  const std::size_t outLen = static_cast<std::size_t>(
    std::max<double>(1.0, static_cast<double>(n) * ratio));

  signalsmith::stretch::SignalsmithStretch<float> s;
  s.presetDefault(1, sampleRate);
  s.reset();

  std::vector<float> out(outLen, 0.0f);
  const float* inPtrs[1] = { src };
  float* outPtrs[1]      = { out.data() };

  s.process(inPtrs, static_cast<int>(n), outPtrs, static_cast<int>(outLen));
  return out;
}

// Stretch to exact target length (samples). Ratio is derived internally.
inline std::vector<float> stretchToLength(const float* src, std::size_t n,
                                          float sampleRate,
                                          std::size_t targetSamples)
{
  if (n == 0 || targetSamples == 0)
    return std::vector<float>(targetSamples, 0.0f);

  signalsmith::stretch::SignalsmithStretch<float> s;
  s.presetDefault(1, sampleRate);
  s.reset();

  std::vector<float> out(targetSamples, 0.0f);
  const float* inPtrs[1] = { src };
  float* outPtrs[1]      = { out.data() };

  s.process(inPtrs, static_cast<int>(n), outPtrs, static_cast<int>(targetSamples));
  return out;
}

// Stereo variants — de-interleaved L/R buffers.
inline void stretchStereoByRatio(const float* inL, const float* inR,
                                 std::size_t n, float sampleRate, double ratio,
                                 std::vector<float>& outL, std::vector<float>& outR)
{
  if (n == 0 || ratio <= 0.0)
  {
    outL.clear();
    outR.clear();
    return;
  }

  const std::size_t outLen = static_cast<std::size_t>(
    std::max<double>(1.0, static_cast<double>(n) * ratio));
  outL.assign(outLen, 0.0f);
  outR.assign(outLen, 0.0f);

  signalsmith::stretch::SignalsmithStretch<float> s;
  s.presetDefault(2, sampleRate);
  s.reset();

  const float* inPtrs[2] = { inL, inR };
  float* outPtrs[2]      = { outL.data(), outR.data() };

  s.process(inPtrs, static_cast<int>(n), outPtrs, static_cast<int>(outLen));
}

inline void stretchStereoToLength(const float* inL, const float* inR,
                                  std::size_t n, float sampleRate,
                                  std::size_t targetSamples,
                                  std::vector<float>& outL,
                                  std::vector<float>& outR)
{
  outL.assign(targetSamples, 0.0f);
  outR.assign(targetSamples, 0.0f);
  if (n == 0 || targetSamples == 0) return;

  signalsmith::stretch::SignalsmithStretch<float> s;
  s.presetDefault(2, sampleRate);
  s.reset();

  const float* inPtrs[2] = { inL, inR };
  float* outPtrs[2]      = { outL.data(), outR.data() };

  s.process(inPtrs, static_cast<int>(n), outPtrs, static_cast<int>(targetSamples));
}

}} // namespace premonition::dsp
