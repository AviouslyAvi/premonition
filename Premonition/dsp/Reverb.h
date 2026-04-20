#pragma once

// Algorithmic reverb — Freeverb-style (Jezar at Dreampoint, public-domain
// algorithm, 2000). Eight lowpass-feedback comb filters in parallel feed four
// series allpass filters. Implementation written from:
//   - Jezar's Freeverb description (public domain)
//   - M. R. Schroeder, "Natural Sounding Artificial Reverberation" (1962)
//   - J. A. Moorer, "About This Reverberation Business" (1979)
//   - J. O. Smith, "Physical Audio Signal Processing" (CCRMA, online)
// The delay lengths and gain conventions follow Freeverb at 44.1 kHz and are
// scaled linearly for other sample rates.

#include "DelayLine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace premonition { namespace dsp {

// Base Freeverb comb delay lengths in samples @ 44.1 kHz.
// These are the canonical Freeverb values (Jezar, public domain).
inline constexpr std::array<int, 8> kCombLengths44k = {
  1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617
};

// R-channel comb lengths: each L length shifted by a distinct prime offset so
// L/R share no common comb-length ratios and decorrelate naturally.
// Offsets (at 44.1 kHz): 37, 43, 47, 53, 59, 61, 67, 71
inline constexpr std::array<int, 8> kCombLengths44kR = {
  1153, 1231, 1324, 1409, 1481, 1552, 1624, 1688
};

// Narrow stereo spread applied as a uniform allpass + comb offset (samples @
// 44.1 kHz). Used for stereo sources where the R comb table already provides
// decorrelation. Original Freeverb value.
inline constexpr int kStereoSpread44k = 23;

// Base allpass delay lengths in samples @ 44.1 kHz.
inline constexpr std::array<int, 4> kAllpassLengths44k = {
  556, 441, 341, 225
};

// Lowpass-feedback comb filter: comb with a one-pole LPF in the feedback
// path. Produces a warmer tail than a bare comb.
class LPFComb
{
public:
  void prepare(int delaySamples, float feedback, float damping)
  {
    line_.setSize(static_cast<std::size_t>(std::max(1, delaySamples)));
    feedback_ = feedback;
    damping_ = std::clamp(damping, 0.0f, 0.999f);
    lpfState_ = 0.0f;
  }

  void reset()
  {
    line_.reset();
    lpfState_ = 0.0f;
  }

  float process(float x) noexcept
  {
    const float delayed = line_.peek(line_.size() - 1);
    // One-pole lowpass on feedback tap.
    lpfState_ = delayed * (1.0f - damping_) + lpfState_ * damping_;
    const float toWrite = x + lpfState_ * feedback_;
    line_.tickAndRead(toWrite, line_.size() - 1);
    return delayed;
  }

private:
  DelayLine line_;
  float feedback_ = 0.0f;
  float damping_ = 0.0f;
  float lpfState_ = 0.0f;
};

// Schroeder allpass: y[n] = -g*x[n] + x[n-D] + g*y[n-D]
// Direct-form II implementation: same coefficient both paths, single delay
// line, scales gracefully in series.
class SchroederAllpass
{
public:
  void prepare(int delaySamples, float gain)
  {
    line_.setSize(static_cast<std::size_t>(std::max(1, delaySamples)));
    g_ = gain;
  }

  void reset() { line_.reset(); }

  float process(float x) noexcept
  {
    const std::size_t D = line_.size() - 1;
    const float delayed = line_.peek(D);
    const float toStore = x + delayed * g_;
    line_.tickAndRead(toStore, D);
    return -g_ * toStore + delayed;
  }

private:
  DelayLine line_;
  float g_ = 0.5f;
};

// One reverb channel: 8 combs in parallel summed, then 4 allpasses in series.
class ReverbChannel
{
public:
  struct Params
  {
    float sampleRate = 44100.0f;
    float roomSize = 0.5f;         // 0..1
    float damping  = 0.5f;         // 0..1
    float rt60Seconds = 2.0f;      // target decay to -60 dB
    int spreadSamples = 0;         // uniform offset added to every delay
    std::array<int, 8> combLengths = kCombLengths44k; // per-channel comb table
  };

  void prepare(const Params& p)
  {
    const float rateScale = p.sampleRate / 44100.0f;

    // Per-comb feedback gain derived from RT60:
    //   g = 10^(-3 D / (Fs * RT60))
    // Each comb thus reaches -60 dB together regardless of its own length.
    for (std::size_t i = 0; i < p.combLengths.size(); ++i)
    {
      const int D = static_cast<int>(p.combLengths[i] * rateScale) + p.spreadSamples;
      const float rt60 = std::max(p.rt60Seconds, 0.05f);
      float g = std::pow(10.0f,
                         -3.0f * static_cast<float>(D) / (p.sampleRate * rt60));
      // Room-size tweak: larger rooms push feedback a bit higher (still <1).
      g = std::clamp(g * (0.7f + 0.3f * p.roomSize), 0.0f, 0.98f);
      combs_[i].prepare(D, g, p.damping);
    }

    for (std::size_t i = 0; i < kAllpassLengths44k.size(); ++i)
    {
      const int D = static_cast<int>(kAllpassLengths44k[i] * rateScale) + p.spreadSamples;
      allpasses_[i].prepare(D, 0.5f);
    }
  }

  void reset()
  {
    for (auto& c : combs_) c.reset();
    for (auto& a : allpasses_) a.reset();
  }

  float process(float x) noexcept
  {
    float sum = 0.0f;
    for (auto& c : combs_) sum += c.process(x);
    sum *= (1.0f / static_cast<float>(combs_.size()));
    for (auto& a : allpasses_) sum = a.process(sum);
    return sum;
  }

private:
  std::array<LPFComb, 8>         combs_{};
  std::array<SchroederAllpass, 4> allpasses_{};
};

// Public entry: render `wet = in * mix + dry * (1 - mix)` into `out`.
// `in`, `out` may alias. Processes one channel.
inline void renderReverbMono(const float* in, float* out, std::size_t n,
                             float sampleRate, float roomSize,
                             float rt60Seconds, float damping, float mix,
                             int spreadSamples = 0)
{
  ReverbChannel r;
  r.prepare({sampleRate, roomSize, damping, rt60Seconds, spreadSamples});

  const float wet = std::clamp(mix, 0.0f, 1.0f);
  const float dry = 1.0f - wet;

  for (std::size_t i = 0; i < n; ++i)
  {
    const float x = in[i];
    const float y = r.process(x);
    out[i] = x * dry + y * wet;
  }
}

// Stereo version — two independent reverb channels. The R channel uses a
// prime-offset comb table (kCombLengths44kR) for structural decorrelation, plus
// a uniform `stereoSpread` offset on all delays. Use a wide spread (~200 at
// 44.1 kHz) for mono sources and kStereoSpread44k (23) for stereo sources.
inline void renderReverbStereo(const float* inL, const float* inR,
                               float* outL, float* outR, std::size_t n,
                               float sampleRate, float roomSize,
                               float rt60Seconds, float damping, float mix,
                               int stereoSpread = kStereoSpread44k)
{
  const float rateScale = sampleRate / 44100.0f;
  const int spread = static_cast<int>(std::round(stereoSpread * rateScale));

  ReverbChannel L, R;
  L.prepare({sampleRate, roomSize, damping, rt60Seconds, 0, kCombLengths44k});
  R.prepare({sampleRate, roomSize, damping, rt60Seconds, spread, kCombLengths44kR});

  const float wet = std::clamp(mix, 0.0f, 1.0f);
  const float dry = 1.0f - wet;

  for (std::size_t i = 0; i < n; ++i)
  {
    const float xL = inL[i];
    const float xR = inR[i];
    outL[i] = xL * dry + L.process(xL) * wet;
    outR[i] = xR * dry + R.process(xR) * wet;
  }
}

}} // namespace premonition::dsp
