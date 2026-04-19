#include <catch2/catch_test_macros.hpp>

#include "dsp/Reverse.h"

using namespace premonition::dsp;

TEST_CASE("reverseInPlace: empty and single-sample no-ops", "[reverse]")
{
  reverseInPlace(nullptr, 0);
  float single = 0.42f;
  reverseInPlace(&single, 1);
  REQUIRE(single == 0.42f);
}

TEST_CASE("reverseInPlace: flips ordering", "[reverse]")
{
  float a[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
  reverseInPlace(a, 5);
  REQUIRE(a[0] == 5.0f);
  REQUIRE(a[1] == 4.0f);
  REQUIRE(a[2] == 3.0f);
  REQUIRE(a[3] == 2.0f);
  REQUIRE(a[4] == 1.0f);
}

TEST_CASE("reverseInto: writes reversed source to destination", "[reverse]")
{
  const float src[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  float dst[4] = {};
  reverseInto(src, dst, 4);
  REQUIRE(dst[0] == 4.0f);
  REQUIRE(dst[1] == 3.0f);
  REQUIRE(dst[2] == 2.0f);
  REQUIRE(dst[3] == 1.0f);
}
