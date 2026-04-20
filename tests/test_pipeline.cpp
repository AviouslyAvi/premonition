#include <catch2/catch_test_macros.hpp>

#include "dsp/OfflinePipeline.h"

#include <vector>

using namespace premonition::dsp;

static StereoBuffer makeImpulseStereo(std::size_t nFrames)
{
  StereoBuffer s;
  s.L.assign(nFrames, 0.0f);
  s.R.assign(nFrames, 0.0f);
  s.L[0] = 1.0f;
  s.R[0] = 1.0f;
  return s;
}

TEST_CASE("Pipeline: empty source returns empty output", "[pipeline]")
{
  StereoBuffer empty;
  PipelineConfig cfg;
  const auto out = renderRiser(empty, 44100.0f, cfg);
  REQUIRE(out.L.empty());
  REQUIRE(out.R.empty());
}

TEST_CASE("Pipeline: impulse source produces bar-length output", "[pipeline]")
{
  const auto src = makeImpulseStereo(44100); // 1 s impulse
  PipelineConfig cfg;
  cfg.bpm        = 120.0;
  cfg.lengthBars = 1.0; // 2 s @ 120 BPM

  const auto out = renderRiser(src, 44100.0f, cfg);

  // 1 bar @ 120 BPM = 2 s = 88200 samples @ 44.1 kHz
  REQUIRE(out.L.size() == 88200);
  REQUIRE(out.R.size() == 88200);
}

TEST_CASE("crossfadeTailWithHead: equal-power blend of tail and head",
          "[pipeline][tail-fade]")
{
  // DC 0.3 tail + DC 0.3 head — equal-power sum peaks at 0.3·sqrt(2) ≈ 0.4243
  // at t = 0.5. Endpoints cos(0)=sin(π/2)=1 preserve 0.3 on each side.
  const std::size_t N     = 1000;
  const std::size_t fadeN = 200;
  std::vector<float> tail(N, 0.3f);
  const std::vector<float> head(fadeN, 0.3f);

  detail::crossfadeTailWithHead(tail.data(), tail.size(), head.data(), fadeN);

  // Before the fade region: untouched.
  REQUIRE(tail[N - fadeN - 1] == 0.3f);

  // Endpoints preserved at 0.3 (one weight is 1, other is 0; DC both sides).
  REQUIRE(std::fabs(tail[N - fadeN] - 0.3f) < 1.0e-5f);
  REQUIRE(std::fabs(tail[N - 1]     - 0.3f) < 1.0e-5f);

  // Middle of fade: must peak above 0.3 (i.e. a real blend happened) and
  // below 0.3·sqrt(2) + ε.
  float peak = 0.0f;
  for (std::size_t i = N - fadeN; i < N; ++i) peak = std::max(peak, tail[i]);
  REQUIRE(peak > 0.40f);
  REQUIRE(peak < 0.43f);
}

TEST_CASE("crossfadeTailWithHead: head=0 fades tail out to zero",
          "[pipeline][tail-fade]")
{
  const std::size_t N     = 200;
  const std::size_t fadeN = 50;
  std::vector<float> tail(N, 1.0f);
  const std::vector<float> head(fadeN, 0.0f);

  detail::crossfadeTailWithHead(tail.data(), tail.size(), head.data(), fadeN);

  // Head is silent → last sample drops to ~0 (cos(π/2) = 0).
  REQUIRE(std::fabs(tail[N - 1]) < 1.0e-5f);
  // Start of fade still ~1 (cos(0) = 1).
  REQUIRE(std::fabs(tail[N - fadeN] - 1.0f) < 1.0e-5f);
  // Pre-fade region untouched.
  REQUIRE(tail[N - fadeN - 1] == 1.0f);
}

TEST_CASE("crossfadeTailWithHead: guards against oversized fade",
          "[pipeline][tail-fade]")
{
  // fadeN larger than tail size → no-op.
  std::vector<float> tail{1.0f, 2.0f, 3.0f};
  const std::vector<float> head{9.0f, 9.0f, 9.0f, 9.0f, 9.0f};
  detail::crossfadeTailWithHead(tail.data(), tail.size(), head.data(), 5);
  REQUIRE(tail[0] == 1.0f);
  REQUIRE(tail[1] == 2.0f);
  REQUIRE(tail[2] == 3.0f);
}

TEST_CASE("Pipeline: Forward mode skips reverse step", "[pipeline]")
{
  const auto src = makeImpulseStereo(44100);
  PipelineConfig cfgRev; cfgRev.mode = premonition::kModeNatural;
  PipelineConfig cfgFwd; cfgFwd.mode = premonition::kModeForward;

  const auto rev = renderRiser(src, 44100.0f, cfgRev);
  const auto fwd = renderRiser(src, 44100.0f, cfgFwd);

  REQUIRE(rev.L.size() == fwd.L.size());
  // Reversed vs forward renders should differ meaningfully somewhere.
  bool anyDiff = false;
  for (std::size_t i = 0; i < rev.L.size(); ++i)
  {
    if (std::fabs(rev.L[i] - fwd.L[i]) > 1.0e-4f) { anyDiff = true; break; }
  }
  REQUIRE(anyDiff);
}
