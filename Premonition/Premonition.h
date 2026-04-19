#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Parameters.h"
#include "dsp/OfflinePipeline.h"

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
#endif

private:
  // Loaded source audio (populated by drag-drop in the UI layer).
  premonition::dsp::StereoBuffer mSource;
  float mSourceSampleRate = 44100.0f;

  // Most recently rendered output (for preview / drag-to-render).
  premonition::dsp::StereoBuffer mRendered;
};
