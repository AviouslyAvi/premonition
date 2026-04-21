#pragma once

#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

struct StereoBuffer
{
  std::vector<float> L;
  std::vector<float> R;
  // True if the source was single-channel (duplicated to L+R) or if L == R
  // exactly. Set by the audio loader; consumed by the reverb to decide
  // whether to apply wide-stereo decorrelation to a mono input.
  bool isMono = false;

  std::size_t frames() const noexcept { return L.size(); }
};

}} // namespace premonition::dsp
