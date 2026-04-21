// Sanity check for convolveStereo — convolve a unit impulse with a known IR
// and verify the output matches the IR (up to peak-normalization).

#include "dsp/Convolution.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

TEST_CASE("convolveStereo: impulse in = IR out (wet)", "[convolution]")
{
  using namespace premonition::dsp;

  // 16-sample IR with a distinctive shape on each channel.
  std::vector<float> irL = {0.f, 0.8f, -0.4f, 0.2f, -0.1f, 0.05f, 0.f, 0.f,
                            0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  std::vector<float> irR = {0.f, 0.f, 0.7f, -0.35f, 0.17f, -0.08f, 0.f, 0.f,
                            0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  // Unit impulse input: L=[1,0,0,...], R=[1,0,0,...] (need at least 2 samples
  // to hit the FFT path with a meaningful outLen).
  std::vector<float> inL(8, 0.f), inR(8, 0.f);
  inL[0] = 1.f;
  inR[0] = 1.f;

  std::vector<float> outL, outR;
  convolveStereo(inL.data(), inR.data(), irL.data(), irR.data(),
                 inL.size(), irL.size(), /*mix*/ 1.f, outL, outR);

  // Output length = inN + irN - 1 = 23
  REQUIRE(outL.size() == inL.size() + irL.size() - 1);
  REQUIRE(outR.size() == outL.size());

  // Find the shared peak-normalization gain: the convolution should land at
  // 0.89 / max(|irL ∪ irR|). Max |irL| = 0.8; max |irR| = 0.7 → shared peak 0.8
  // → gain 0.89 / 0.8 = 1.1125. Each output sample should equal gain * ir[i].
  const float expectedGain = 0.89f / 0.8f;

  for (std::size_t i = 0; i < irL.size(); ++i)
  {
    INFO("i=" << i);
    REQUIRE(outL[i] == Catch::Approx(expectedGain * irL[i]).margin(1e-3f));
    REQUIRE(outR[i] == Catch::Approx(expectedGain * irR[i]).margin(1e-3f));
  }

  // Remainder of output (past IR length) should be near zero.
  for (std::size_t i = irL.size(); i < outL.size(); ++i)
  {
    INFO("tail i=" << i);
    REQUIRE(std::fabs(outL[i]) < 1e-3f);
    REQUIRE(std::fabs(outR[i]) < 1e-3f);
  }
}

TEST_CASE("convolveStereo: no NaN/Inf with long IR", "[convolution]")
{
  using namespace premonition::dsp;

  // Simulate a ~4.8s IR at 44100 = ~212k samples, as in hall.wav.
  const std::size_t irN = 211680;
  std::vector<float> irL(irN, 0.f), irR(irN, 0.f);
  // Fill with a decaying noise-ish pattern.
  for (std::size_t i = 0; i < irN; ++i)
  {
    float g = std::exp(-static_cast<float>(i) / 44100.f);
    irL[i] = 0.3f * g * std::sin(i * 0.1f);
    irR[i] = 0.3f * g * std::cos(i * 0.11f);
  }

  // 2-second source at 44100 = 88200 samples.
  const std::size_t inN = 88200;
  std::vector<float> inL(inN, 0.f), inR(inN, 0.f);
  for (std::size_t i = 0; i < inN; ++i)
  {
    inL[i] = 0.5f * std::sin(i * 0.05f);
    inR[i] = 0.5f * std::sin(i * 0.051f);
  }

  std::vector<float> outL, outR;
  convolveStereo(inL.data(), inR.data(), irL.data(), irR.data(),
                 inN, irN, 1.f, outL, outR);

  REQUIRE(outL.size() == inN + irN - 1);

  // Verify no NaN/Inf and no clipping past ±1.
  float peakL = 0.f, peakR = 0.f;
  for (float v : outL) { REQUIRE(std::isfinite(v)); peakL = std::max(peakL, std::fabs(v)); }
  for (float v : outR) { REQUIRE(std::isfinite(v)); peakR = std::max(peakR, std::fabs(v)); }

  // After peak-normalization to 0.89, peaks should be <= 0.89 (plus tiny FP slack).
  REQUIRE(peakL <= 0.9f);
  REQUIRE(peakR <= 0.9f);
}
