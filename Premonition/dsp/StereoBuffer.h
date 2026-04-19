#pragma once

#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

struct StereoBuffer
{
  std::vector<float> L;
  std::vector<float> R;

  std::size_t frames() const noexcept { return L.size(); }
};

}} // namespace premonition::dsp
