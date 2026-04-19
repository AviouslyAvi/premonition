#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Parameters.h"
#include "Presets.h"
#include "dsp/OfflinePipeline.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

const int kNumPresets = 1;

using namespace iplug;
using namespace igraphics;

class Premonition final : public Plugin
{
public:
  Premonition(const InstanceInfo& info);
  ~Premonition();

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif

#if IPLUG_EDITOR
  // GUI hook for triggering an offline render from the user's loaded source.
  // Returns the rendered riser; UI owns display and drag-out.
  premonition::dsp::StereoBuffer RenderRiserFromSource(
    const premonition::dsp::StereoBuffer& source,
    float sourceSampleRate);

  void OnParamChangeUI(int paramIdx, EParamSource source) override;

  // Loads a file (WAV for now) from disk into mSource. Returns true on
  // success. The UI drop zone calls this on click-to-browse and on drag-drop.
  bool LoadSourceFile(const char* path);

  // Loads an impulse response for the Convolution algorithm. Stored raw; the
  // render path resamples to the current render rate on demand.
  bool LoadIRFile(const char* path);
  const premonition::dsp::StereoBuffer& IR() const { return mIR; }
  float IRSampleRate() const { return mIRSampleRate; }
  const std::string& IRDisplayName() const { return mIRDisplayName; }
  bool HasIR() const { return !mIR.L.empty(); }

  // Called by the layout lambda to register the IR slot control so we can
  // toggle its visibility when the algorithm enum changes.
  void RegisterIRSlot(IControl* c);

  const premonition::dsp::StereoBuffer& Source() const { return mSource; }
  const premonition::dsp::StereoBuffer& Rendered() const { return ActiveRendered(); }
  float SourceSampleRate() const { return mSourceSampleRate; }
  const std::string& SourceDisplayName() const { return mSourceDisplayName; }
  float SourceDurationSec() const
  {
    return (mSourceSampleRate > 0.f && !mSource.L.empty())
             ? static_cast<float>(mSource.L.size()) / mSourceSampleRate : 0.f;
  }
  bool IsRendering() const { return mRendering.load(std::memory_order_acquire); }

  // Writes the active rendered buffer as 32-bit float WAV to a temp path.
  // Returns the path (empty on failure). Paths are tracked and removed at
  // plugin destruction.
  std::string ExportRenderedToTempWav();

  // Preview transport (one-shot). Returns new playing state.
  bool TogglePreview();
  bool IsPreviewing() const { return mPreviewPlaying.load(); }
  bool HasRendered() const { return ActiveRendered().frames() > 0; }

  // A/B slots. Render writes to the active slot; Rendered() / preview /
  // dragout all read from it. Switching slots stops any in-flight preview.
  enum Slot { kSlotA = 0, kSlotB = 1 };
  int ActiveSlot() const { return mActiveSlot.load(std::memory_order_acquire); }
  void SetActiveSlot(int slot);
  bool SlotHasRender(int slot) const
  { return (slot == kSlotA ? mRenderedA : mRenderedB).frames() > 0; }

  // Preset store (JSON files in ~/Library/Application Support/Premonition/presets).
  premonition::PresetManager& Presets() { return mPresetStore; }
  // Captures the 9 module params (excludes start/end/BPM/source).
  premonition::PresetValues CurrentPresetValues() const;
  // Pushes values into params, notifies host + UI.
  void ApplyPresetValues(const premonition::PresetValues& v);
#endif

private:
  // Loaded source audio (populated by drag-drop in the UI layer).
  premonition::dsp::StereoBuffer mSource;
  float mSourceSampleRate = 44100.0f;

  // Two render slots for A/B comparison. Render always targets mActiveSlot;
  // toggling the slot re-points preview / dragout / waveform without touching
  // the other slot's buffer. Avi's workflow: render into A, switch, tweak,
  // render into B, toggle to compare.
  premonition::dsp::StereoBuffer mRenderedA;
  premonition::dsp::StereoBuffer mRenderedB;
  std::atomic<int> mActiveSlot{kSlotA};

  premonition::dsp::StereoBuffer& ActiveRendered()
  { return mActiveSlot.load(std::memory_order_acquire) == kSlotA ? mRenderedA : mRenderedB; }
  const premonition::dsp::StereoBuffer& ActiveRendered() const
  { return mActiveSlot.load(std::memory_order_acquire) == kSlotA ? mRenderedA : mRenderedB; }

  // Preview transport state. mRenderedMutex serializes audio-thread reads
  // against UI-thread writes (RenderRiserFromSource swaps mRendered).
  std::atomic<bool> mPreviewPlaying{false};
  std::atomic<int64_t> mPreviewPos{0};
  std::mutex mRenderedMutex;

  std::string mSourceDisplayName;
  std::atomic<bool> mRendering{false};

  // Impulse response for Convolution algorithm.
  premonition::dsp::StereoBuffer mIR;
  float mIRSampleRate = 0.f;
  std::string mIRDisplayName;
  IControl* mIRSlotCtl = nullptr;

  // Drag-out temp files (32f WAVs). Kept until plugin instance destruction.
  std::vector<std::string> mTempFiles;

  premonition::PresetManager mPresetStore;
};
