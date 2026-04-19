#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Reverb.h"

#include <cmath>
#include <vector>

using namespace premonition::dsp;

TEST_CASE("Reverb: silent input produces silent output", "[reverb]")
{
  constexpr std::size_t N = 4096;
  std::vector<float> in(N, 0.0f);
  std::vector<float> out(N, 0.0f);

  renderReverbMono(in.data(), out.data(), N, 44100.0f,
                   /*roomSize*/ 0.5f, /*rt60*/ 2.0f,
                   /*damping*/ 0.5f,  /*mix*/ 1.0f);

  for (float v : out)
    REQUIRE(std::fabs(v) < 1.0e-6f);
}

TEST_CASE("Reverb: impulse produces decaying tail", "[reverb]")
{
  constexpr std::size_t N = 44100; // 1 s @ 44.1k
  std::vector<float> in(N, 0.0f);
  std::vector<float> out(N, 0.0f);
  in[0] = 1.0f;

  renderReverbMono(in.data(), out.data(), N, 44100.0f,
                   0.5f, 2.0f, 0.5f, 1.0f);

  // Energy should exist in the tail.
  float earlyEnergy = 0.0f;
  float lateEnergy  = 0.0f;
  for (std::size_t i = 0;      i < N / 4; ++i) earlyEnergy += out[i] * out[i];
  for (std::size_t i = N * 3/4; i < N;    ++i) lateEnergy  += out[i] * out[i];

  REQUIRE(earlyEnergy > 0.0f);
  REQUIRE(lateEnergy  > 0.0f);
  // Early > late (energy decays).
  REQUIRE(earlyEnergy > lateEnergy);
}

TEST_CASE("Reverb: dry mix passes input unchanged", "[reverb]")
{
  constexpr std::size_t N = 256;
  std::vector<float> in(N, 0.0f);
  std::vector<float> out(N, 0.0f);
  for (std::size_t i = 0; i < N; ++i)
    in[i] = std::sin(static_cast<float>(i) * 0.01f);

  renderReverbMono(in.data(), out.data(), N, 44100.0f,
                   0.5f, 2.0f, 0.5f, /*mix*/ 0.0f);

  for (std::size_t i = 0; i < N; ++i)
    REQUIRE_THAT(out[i], Catch::Matchers::WithinAbs(in[i], 1.0e-6f));
}
