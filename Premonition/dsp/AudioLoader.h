#pragma once

// Multi-format audio file loader. macOS-only for now.
//   WAV / AIFF / MP3 / M4A / AAC / CAF : ExtAudioFile (AudioToolbox)
//   OGG                                : stb_vorbis
// The loader dispatches by extension. Returns true on success and fills
// `out` with deinterleaved float samples (mono sources duplicated to L+R).

#include "OfflinePipeline.h"

namespace premonition { namespace dsp {

bool loadAudioFile(const char* path, StereoBuffer& out, float& outRate);

}} // namespace premonition::dsp
