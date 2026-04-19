#pragma once

// Premonition parameter IDs and ranges. Mirrors PREMONITION_BRIEF.md v1 spec.

namespace premonition {

enum EParams
{
  // Source crop
  kStart = 0,     // seconds into source
  kEnd,           // seconds into source

  // Pre-reverb processing
  kStretch,       // ratio 0.25 .. 4.0 (1.0 = no stretch)

  // Reverb
  kSize,          // 0..1 (room size)
  kDecay,         // 0..1 mapped to 0.1..20 s RT60
  kMix,           // 0..1 wet/dry pre-reverse
  kAlgorithm,     // enum: 0=Hall, 1=Plate, 2=Spring, 3=Room

  // Output shaping
  kLength,        // bars (tempo-synced), 0.25 .. 16
  kForward,       // bool: on = skip reverse step
  kNormalize,     // bool
  kMonoStereo,    // bool: on = mono-sum output

  // Tempo fallback — used when host transport reports BPM == 0 (standalone /
  // stopped). Hidden from UI unless host tempo is idle.
  kManualBPM,     // 40..300 bpm

  kNumParams
};

enum ELength
{
  kLen1_16 = 0,
  kLen1_8,
  kLen1_4,
  kLen1_2,
  kLen1,
  kLen2,
  kLen4,
  kLen8,
  kLen16,
  kNumLengths
};

inline constexpr double kLengthBarsTable[kNumLengths] = {
  0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0
};

enum EAlgorithm
{
  kAlgoHall = 0,
  kAlgoPlate,
  kAlgoSpring,
  kAlgoRoom,
  kAlgoConvolution,
  kNumAlgorithms
};

// Parameter ranges — kept here so DSP and UI read from a single source.
namespace ranges {

inline constexpr double kStretchMin = 0.25;
inline constexpr double kStretchMax = 4.0;
inline constexpr double kStretchDefault = 1.0;

inline constexpr double kDecayMinSec = 0.1;
inline constexpr double kDecayMaxSec = 20.0;
inline constexpr double kDecayDefaultSec = 2.0;

inline constexpr double kLengthMinBars = 0.25;
inline constexpr double kLengthMaxBars = 16.0;
inline constexpr double kLengthDefaultBars = 2.0;

// Engine constraints
inline constexpr double kMaxInputSeconds = 120.0; // 2 min hard cap (spec)

} // namespace ranges

} // namespace premonition
