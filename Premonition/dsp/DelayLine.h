#pragma once

#include <cstddef>
#include <vector>

namespace premonition { namespace dsp {

// Fixed-length circular delay line. Power-of-two sizing is NOT required —
// we use modulo-based indexing. Written for offline use: simple and clear
// beats cycle-shaving here.
class DelayLine
{
public:
  void setSize(std::size_t samples)
  {
    buffer_.assign(samples == 0 ? 1 : samples, 0.0f);
    cursor_ = 0;
  }

  void reset()
  {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    cursor_ = 0;
  }

  // Push one sample, return the sample that was there `delay` positions ago.
  float tickAndRead(float x, std::size_t delay) noexcept
  {
    const auto n = buffer_.size();
    const std::size_t readIdx = (cursor_ + n - (delay % n)) % n;
    const float y = buffer_[readIdx];
    buffer_[cursor_] = x;
    cursor_ = (cursor_ + 1) % n;
    return y;
  }

  // Read only (does not advance).
  float peek(std::size_t delay) const noexcept
  {
    const auto n = buffer_.size();
    const std::size_t readIdx = (cursor_ + n - (delay % n)) % n;
    return buffer_[readIdx];
  }

  std::size_t size() const noexcept { return buffer_.size(); }

private:
  std::vector<float> buffer_;
  std::size_t cursor_ = 0;
};

}} // namespace premonition::dsp
