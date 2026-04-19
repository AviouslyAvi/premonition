#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Parameters.h"
#include "dsp/OfflinePipeline.h"

#include <atomic>
#include <cstdint>
#include <mutex>

const int kNumPresets = 1;

using namespace iplug;
using namespace igraphics;

class Premonition final : public Plugin
{
public:
  Premonition(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif

#if IPLUG_EDITOR
  // GUI hook for triggering an offline render from the user's loaded source.
  // Returns the rendered riser; UI owns display and drag-out.
  premonition::dsp::StereoBuffer RenderRiserFromSource(
    const premonition::dsp::StereoBuffer& source,
    float sourceSampleRate);

  // Loads a file (WAV for now) from disk into mSource. Returns true on
  // success. The UI drop zone calls this on click-to-browse and on drag-drop.
  bool LoadSourceFile(const char* path);

  const premonition::dsp::StereoBuffer& Source() const { return mSource; }
  const premonition::dsp::StereoBuffer& Rendered() const { return mRendered; }
  float SourceSampleRate() const { return mSourceSampleRate; }

  // Preview transport (one-shot). Returns new playing state.
  bool TogglePreview();
  bool IsPreviewing() const { return mPreviewPlaying.load(); }
  bool HasRendered() const { return mRendered.frames() > 0; }
#endif

private:
  // Loaded source audio (populated by drag-drop in the UI layer).
  premonition::dsp::StereoBuffer mSource;
  float mSourceSampleRate = 44100.0f;

  // Most recently rendered output (for preview / drag-to-render).
  premonition::dsp::StereoBuffer mRendered;

  // Preview transport state. mRenderedMutex serializes audio-thread reads
  // against UI-thread writes (RenderRiserFromSource swaps mRendered).
  std::atomic<bool> mPreviewPlaying{false};
  std::atomic<int64_t> mPreviewPos{0};
  std::mutex mRenderedMutex;
};
