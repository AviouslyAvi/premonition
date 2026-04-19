#include <catch2/catch_test_macros.hpp>

#include "dsp/FitToBar.h"

using namespace premonition::dsp;

TEST_CASE("samplesPerBar: 120 BPM @ 44.1k = 88200", "[fit]")
{
  // 120 BPM → 0.5 s/beat → 2.0 s/bar → 88200 samples
  REQUIRE(samplesPerBar(120.0, 44100.0f) == 88200);
}

TEST_CASE("samplesPerBar: 60 BPM @ 48k = 192000", "[fit]")
{
  REQUIRE(samplesPerBar(60.0, 48000.0f) == 192000);
}

TEST_CASE("targetSamplesForBars: 2 bars @ 120 BPM @ 44.1k = 176400", "[fit]")
{
  REQUIRE(targetSamplesForBars(2.0, 120.0, 44100.0f) == 176400);
}

TEST_CASE("targetSamplesForBars: half-bar rounds correctly", "[fit]")
{
  REQUIRE(targetSamplesForBars(0.5, 120.0, 44100.0f) == 44100);
}
