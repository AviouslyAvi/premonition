#pragma once

// 32-bit float WAV writer. Used for drag-out to DAW. Stereo interleaved;
// mono sources fall back to L-in-both-channels.

#include "OfflinePipeline.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace premonition { namespace dsp {

inline bool writeWav32f(const std::string& path,
                        const StereoBuffer& buf,
                        float sampleRate)
{
  const std::size_t frames = buf.frames();
  if (frames == 0 || sampleRate <= 0.f) return false;

  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;

  const uint16_t channels    = 2;
  const uint16_t bitsPerSamp = 32;
  const uint32_t sr          = static_cast<uint32_t>(sampleRate);
  const uint16_t blockAlign  = channels * (bitsPerSamp / 8);
  const uint32_t byteRate    = sr * blockAlign;
  const uint32_t dataSize    = static_cast<uint32_t>(frames) * blockAlign;
  const uint32_t riffSize    = 36 + dataSize;

  auto w32 = [f](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto w16 = [f](uint16_t v) { std::fwrite(&v, 2, 1, f); };

  std::fwrite("RIFF", 1, 4, f);  w32(riffSize);  std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f);  w32(16);
  w16(3);            // WAVE_FORMAT_IEEE_FLOAT
  w16(channels);
  w32(sr);
  w32(byteRate);
  w16(blockAlign);
  w16(bitsPerSamp);
  std::fwrite("data", 1, 4, f);  w32(dataSize);

  const bool hasR = !buf.R.empty();
  for (std::size_t i = 0; i < frames; ++i)
  {
    const float l = buf.L[i];
    const float r = hasR ? buf.R[i] : l;
    std::fwrite(&l, 4, 1, f);
    std::fwrite(&r, 4, 1, f);
  }

  std::fclose(f);
  return true;
}

}} // namespace premonition::dsp
