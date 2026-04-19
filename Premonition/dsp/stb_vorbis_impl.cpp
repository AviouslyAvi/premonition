// Single translation unit for stb_vorbis implementation — kept isolated so
// its internal macros (L, R, etc.) don't leak into our audio loader.
// Compiled as C++ since that's the target-wide standard. stb_vorbis is
// tested by upstream to build cleanly as C++.

extern "C" {
#include "stb_vorbis.c"
}
