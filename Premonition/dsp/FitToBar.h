#pragma once

// Tempo-synced length fitter. Converts a riser buffer to exactly N samples
// where N = bars * (60 / bpm) * 4 * sampleRate.
//
// Per PREMONITION_BRIEF.md: Length wins over Stretch. Stretch shapes tonal
// character; FitToBar shapes timing. Always run FitToBar as the last pass
// before normalize so the output lands precisely on the bar grid.

#include "TimeStretch.h"

#include <cstddef>
#include <cmath>
#include <vector>

namespace premonition { namespace dsp {

// Samples per bar for a given BPM, sample rate, and time signature.
// Assumes 4/4 unless numerator is overridden.
inline std::size_t samplesPerBar(double bpm, float sampleRate,
                                 int beatsPerBar = 4)
{
  if (bpm <= 0.0) return 0;
  const double secondsPerBeat = 60.0 / bpm;
  const double secondsPerBar  = secondsPerBeat * static_cast<double>(beatsPerBar);
  return static_cast<std::size_t>(std::llround(secondsPerBar * sampleRate));
}

// Target sample count for `bars` bars.
inline std::size_t targetSamplesForBars(double bars, double bpm,
                                        float sampleRate, int beatsPerBar = 4)
{
  const std::size_t perBar = samplesPerBar(bpm, sampleRate, beatsPerBar);
  return static_cast<std::size_t>(std::llround(bars * static_cast<double>(perBar)));
}

// Fit mono buffer to exact length via signalsmith-stretch.
inline std::vector<float> fitToBarMono(const float* src, std::size_t n,
                                       float sampleRate, double bars,
                                       double bpm, int beatsPerBar = 4)
{
  const std::size_t target = targetSamplesForBars(bars, bpm, sampleRate, beatsPerBar);
  return stretchToLength(src, n, sampleRate, target);
}

// Fit stereo pair to exact length.
inline void fitToBarStereo(const float* inL, const float* inR, std::size_t n,
                           float sampleRate, double bars, double bpm,
                           std::vector<float>& outL, std::vector<float>& outR,
                           int beatsPerBar = 4)
{
  const std::size_t target = targetSamplesForBars(bars, bpm, sampleRate, beatsPerBar);
  stretchStereoToLength(inL, inR, n, sampleRate, target, outL, outR);
}

}} // namespace premonition::dsp
