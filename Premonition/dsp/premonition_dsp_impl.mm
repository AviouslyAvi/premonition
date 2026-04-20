// Single translation unit that provides the Objective-C++ DSP implementations
// the Xcode project compiles as one entry in Sources. Must be .mm so AudioToolbox
// Objective-C APIs compile correctly.

// AudioLoader — multi-format audio file loader (AudioToolbox + stb_vorbis).
#include "AudioLoader.mm"

// stb_vorbis — OGG decoder (C, wrapped in extern "C" to avoid name mangling).
extern "C" {
#include "../vendor/stb/stb_vorbis.c"
}

// WDL FFT is compiled separately in wdl_fft_impl.c (C, not C++) because
// fft.c uses the register keyword which is invalid in C++17.
