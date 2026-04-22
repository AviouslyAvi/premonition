#pragma once

// Offline FFT convolution. One-shot full-buffer transform — fine because the
// whole pipeline runs on the UI thread during a render. Uses WDL complex FFT
// with WDL_fft_complexmul (which operates directly on permuted-order output,
// so we never need to un-permute between forward and inverse).
//
// IR resampling: IRs load at their own sample rate; Kaiser-windowed sinc
// resample to the render rate before convolving. Linear interp aliased
// audibly on long decaying IRs (e.g., 44.1k → 48k) — hence the sinc kernel.

#include "StereoBuffer.h"

#include "WDL/fft.h"

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

// Kaiser-windowed sinc resampler. 32-tap kernel, beta ≈ 8.6 (~80 dB stopband).
// Offline one-shot: O(inN * outN / ratio * taps) — plenty fast at render time,
// and preserves HF content that linear interp would alias. Zero-extends at
// boundaries; fine for IRs whose head is impulsive and tail is already faded.
inline std::vector<float> resampleSinc(const std::vector<float>& in,
                                       float srIn, float srOut)
{
  if (in.empty() || srIn <= 0.f || srOut <= 0.f || srIn == srOut)
    return in;
  const double ratio = static_cast<double>(srOut) / srIn;
  const std::size_t inN = in.size();
  const std::size_t outN = std::max<std::size_t>(
    1, static_cast<std::size_t>(std::llround(static_cast<double>(inN) * ratio)));

  constexpr int halfTaps = 16;   // 32-tap kernel
  constexpr double beta = 8.6;

  auto bessel_i0 = [](double x) {
    double sum = 1.0, term = 1.0;
    const double hx2 = 0.25 * x * x;
    for (int k = 1; k < 30; ++k) {
      term *= hx2 / (static_cast<double>(k) * k);
      sum += term;
      if (term < 1e-12 * sum) break;
    }
    return sum;
  };
  const double invI0Beta = 1.0 / bessel_i0(beta);
  // Downsampling bandlimits to output Nyquist; upsampling keeps cutoff at 1.
  const double cutoff = std::min(1.0, ratio);

  std::vector<float> out(outN, 0.f);
  for (std::size_t i = 0; i < outN; ++i)
  {
    const double srcPos = static_cast<double>(i) / ratio;
    const long long center = static_cast<long long>(std::floor(srcPos));
    double acc = 0.0;
    for (int k = -halfTaps + 1; k <= halfTaps; ++k)
    {
      const long long idx = center + k;
      if (idx < 0 || idx >= static_cast<long long>(inN)) continue;
      const double t = srcPos - static_cast<double>(idx);   // input-samples
      const double x = t * cutoff;
      double sinc;
      if (std::fabs(x) < 1e-12) sinc = 1.0;
      else { const double px = M_PI * x; sinc = std::sin(px) / px; }
      const double wPos = t / static_cast<double>(halfTaps);
      const double inside = 1.0 - wPos * wPos;
      const double w = inside > 0.0
        ? bessel_i0(beta * std::sqrt(inside)) * invI0Beta : 0.0;
      acc += static_cast<double>(in[static_cast<std::size_t>(idx)])
           * sinc * w * cutoff;
    }
    out[i] = static_cast<float>(acc);
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
// decayRT60Sec <= 0 disables the envelope. Otherwise an exponential decay is
// applied to the wet signal such that the envelope reaches -60 dB at
// decayRT60Sec; layered on top of the IR's native decay it scales the
// effective RT60 *down* (the envelope can only shorten the tail, never
// extend it beyond what the IR already contains).
// dampingNorm is the Freeverb convention: 0 = fully bright, 1 = fully dark.
// Internally mapped to a one-pole feedback coefficient.
inline void convolveStereo(const float* inL, const float* inR,
                           const float* irL, const float* irR,
                           std::size_t inN, std::size_t irN,
                           float mix,
                           std::vector<float>& outL, std::vector<float>& outR,
                           float sampleRate = 44100.f,
                           float decayRT60Sec = 0.f,
                           float dampingNorm = 0.f)
{
  if (inN == 0 || irN == 0)
  {
    outL.assign(inL, inL + inN);
    outR.assign(inR, inR + inN);
    return;
  }

  std::vector<float> wetL = detail::convolveReal(inL, inN, irL, irN);
  std::vector<float> wetR = detail::convolveReal(inR, inN, irR, irN);

  // Flush denormals in the FFT output. WDL FFT + long decaying IRs can
  // produce subnormal floats in the tail; downstream multiplications
  // then re-normalize them into audible numerical noise (crackle).
  constexpr float kDenormalThreshold = 1.0e-25f;
  for (float& v : wetL) if (std::fabs(v) < kDenormalThreshold) v = 0.f;
  for (float& v : wetR) if (std::fabs(v) < kDenormalThreshold) v = 0.f;

  // Step 7a — exponential decay envelope: g[n] = 10^(-3 * n / (fs * RT60))
  if (decayRT60Sec > 0.f)
  {
    const float k = -3.0f / (sampleRate * decayRT60Sec);
    const std::size_t N = wetL.size();
    for (std::size_t i = 0; i < N; ++i)
    {
      const float g = std::pow(10.0f, k * static_cast<float>(i));
      wetL[i] *= g;
      wetR[i] *= g;
    }
  }

  // Step 7b — damping: one-pole LPF on the wet output. Coefficient `a` comes
  // from dampingNorm ∈ [0,1], capped below 1 so the filter never stalls.
  if (dampingNorm > 0.f)
  {
    const float a = std::clamp(dampingNorm, 0.f, 1.f) * 0.95f;
    const float oneMinusA = 1.f - a;
    float yL = 0.f, yR = 0.f;
    for (std::size_t i = 0; i < wetL.size(); ++i)
    {
      yL = oneMinusA * wetL[i] + a * yL;
      yR = oneMinusA * wetR[i] + a * yR;
      wetL[i] = yL;
      wetR[i] = yR;
    }
  }

  // Peak-normalize the wet tail (post-envelope, post-LPF). IR convolution can
  // easily overshoot unity; we normalize to -1 dBFS so the final normalize
  // stage has headroom to manage.
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

// Resample a stereo IR buffer to a target rate (Kaiser-windowed sinc).
inline StereoBuffer resampleIR(const StereoBuffer& ir, float irRate,
                               float targetRate)
{
  StereoBuffer out;
  out.L = detail::resampleSinc(ir.L, irRate, targetRate);
  out.R = detail::resampleSinc(ir.R, irRate, targetRate);
  const std::size_t n = std::min(out.L.size(), out.R.size());
  out.L.resize(n);
  out.R.resize(n);
  return out;
}

}} // namespace premonition::dsp
