#pragma once

// Buffer reversal. Offline utility — reverses a buffer in place or into
// a separate destination. O(n/2) swaps for in-place, O(n) copy-and-reverse
// for out-of-place.

#include <algorithm>
#include <cstddef>

namespace premonition { namespace dsp {

inline void reverseInPlace(float* buffer, std::size_t n) noexcept
{
  if (n < 2) return;
  std::reverse(buffer, buffer + n);
}

inline void reverseInto(const float* src, float* dst, std::size_t n) noexcept
{
  for (std::size_t i = 0; i < n; ++i) dst[i] = src[n - 1 - i];
}

// Stereo: reverse L and R independently (preserves stereo image time-flipped).
inline void reverseStereoInPlace(float* L, float* R, std::size_t n) noexcept
{
  reverseInPlace(L, n);
  reverseInPlace(R, n);
}

}} // namespace premonition::dsp
