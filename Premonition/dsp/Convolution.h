#pragma once

// Offline FFT convolution. One-shot full-buffer transform — fine because the
// whole pipeline runs on the UI thread during a render. Uses WDL complex FFT
// with WDL_fft_complexmul (which operates directly on permuted-order output,
// so we never need to un-permute between forward and inverse).
//
// IR resampling: IRs load at their own sample rate; linear resample to the
// render rate before convolving. Linear interp is audibly fine for IRs.

#include "StereoBuffer.h"

#include "fft.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

namespace detail {

inline std::vector<float> resampleLinear(const std::vector<float>& in,
                                         float srIn, float srOut)
{
  if (in.empty() || srIn <= 0.f || srOut <= 0.f || srIn == srOut)
    return in;
  const double ratio = static_cast<double>(srOut) / srIn;
  const std::size_t outN =
    std::max<std::size_t>(1, static_cast<std::size_t>(in.size() * ratio));
  std::vector<float> out(outN, 0.f);
  for (std::size_t i = 0; i < outN; ++i)
  {
    const double srcPos = i / ratio;
    const std::size_t i0 = static_cast<std::size_t>(srcPos);
    const std::size_t i1 = std::min(i0 + 1, in.size() - 1);
    const float f = static_cast<float>(srcPos - i0);
    out[i] = in[i0] * (1.f - f) + in[i1] * f;
  }
  return out;
}

inline int nextPow2(std::size_t n)
{
  int p = 1;
  while (static_cast<std::size_t>(p) < n) p <<= 1;
  return p;
}

// Full-buffer FFT convolution of two real signals. Output length = aN+bN-1.
// Algorithm: load a/b into complex arrays (imag=0), forward FFT both,
// complex-multiply, inverse FFT, take real part. Pre-scale input A by
// 1/fftLen to keep the forward+inverse round-trip unity.
inline std::vector<float> convolveReal(const float* a, std::size_t aN,
                                       const float* b, std::size_t bN)
{
  if (aN == 0 || bN == 0) return {};
  const std::size_t outLen = aN + bN - 1;
  const int fftLen = nextPow2(outLen);

  WDL_fft_init();

  std::vector<WDL_FFT_COMPLEX> A(fftLen);
  std::vector<WDL_FFT_COMPLEX> B(fftLen);
  for (int i = 0; i < fftLen; ++i) { A[i].re = A[i].im = 0.f; B[i].re = B[i].im = 0.f; }
  const float invN = 1.f / static_cast<float>(fftLen);
  for (std::size_t i = 0; i < aN; ++i) A[i].re = a[i] * invN;
  for (std::size_t i = 0; i < bN; ++i) B[i].re = b[i];

  WDL_fft(A.data(), fftLen, 0);
  WDL_fft(B.data(), fftLen, 0);
  WDL_fft_complexmul(A.data(), B.data(), fftLen);
  WDL_fft(A.data(), fftLen, 1);

  std::vector<float> out(outLen);
  for (std::size_t i = 0; i < outLen; ++i) out[i] = A[i].re;
  return out;
}

} // namespace detail

// Stereo convolution. Returns full convolution length (inN+irN-1) in the
// output buffers; the pipeline's fit-to-bar stage reshapes afterwards.
// Dry path is zero-padded to output length. `mix` blends dry/wet in [0,1].
inline void convolveStereo(const float* inL, const float* inR,
                           const float* irL, const float* irR,
                           std::size_t inN, std::size_t irN,
                           float mix,
                           std::vector<float>& outL, std::vector<float>& outR)
{
  if (inN == 0 || irN == 0)
  {
    outL.assign(inL, inL + inN);
    outR.assign(inR, inR + inN);
    return;
  }

  std::vector<float> wetL = detail::convolveReal(inL, inN, irL, irN);
  std::vector<float> wetR = detail::convolveReal(inR, inN, irR, irN);

  // Peak-normalize the wet tail. IR convolution can easily overshoot unity;
  // we normalize to -1 dBFS so the final normalize stage has headroom to
  // manage.
  float peak = 1.0e-9f;
  for (float v : wetL) peak = std::max(peak, std::fabs(v));
  for (float v : wetR) peak = std::max(peak, std::fabs(v));
  const float g = 0.89f / peak;
  for (auto& v : wetL) v *= g;
  for (auto& v : wetR) v *= g;

  const float wet = std::clamp(mix, 0.f, 1.f);
  const float dry = 1.f - wet;

  const std::size_t outLen = wetL.size();
  outL.assign(outLen, 0.f);
  outR.assign(outLen, 0.f);
  for (std::size_t i = 0; i < outLen; ++i)
  {
    const float dL = (i < inN) ? inL[i] : 0.f;
    const float dR = (i < inN) ? inR[i] : 0.f;
    outL[i] = dL * dry + wetL[i] * wet;
    outR[i] = dR * dry + wetR[i] * wet;
  }
}

// Resample a stereo IR buffer to a target rate (linear interpolation).
inline StereoBuffer resampleIR(const StereoBuffer& ir, float irRate,
                               float targetRate)
{
  StereoBuffer out;
  out.L = detail::resampleLinear(ir.L, irRate, targetRate);
  out.R = detail::resampleLinear(ir.R, irRate, targetRate);
  const std::size_t n = std::min(out.L.size(), out.R.size());
  out.L.resize(n);
  out.R.resize(n);
  return out;
}

}} // namespace premonition::dsp
