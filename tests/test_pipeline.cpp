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

TEST_CASE("Pipeline: Forward mode skips reverse step", "[pipeline]")
{
  const auto src = makeImpulseStereo(44100);
  PipelineConfig cfgRev; cfgRev.forward = false;
  PipelineConfig cfgFwd; cfgFwd.forward = true;

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
