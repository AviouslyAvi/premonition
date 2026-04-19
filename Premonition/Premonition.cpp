#include "Premonition.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "Parameters.h"

using namespace premonition;

Premonition::Premonition(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // ---- Parameter declarations (frozen spec) ------------------------------

  GetParam(kStart)->InitDouble("Start", 0.0, 0.0,
                               ranges::kMaxInputSeconds, 0.01, "s");

  GetParam(kEnd)->InitDouble("End", 0.0, 0.0,
                             ranges::kMaxInputSeconds, 0.01, "s");

  GetParam(kStretch)->InitDouble("Stretch",
                                 ranges::kStretchDefault,
                                 ranges::kStretchMin,
                                 ranges::kStretchMax,
                                 0.01, "x");

  GetParam(kSize)->InitDouble("Size", 0.5, 0.0, 1.0, 0.001);

  GetParam(kDecay)->InitDouble("Decay",
                               ranges::kDecayDefaultSec,
                               ranges::kDecayMinSec,
                               ranges::kDecayMaxSec,
                               0.01, "s");

  GetParam(kMix)->InitDouble("Mix", 1.0, 0.0, 1.0, 0.001);

  GetParam(kAlgorithm)->InitEnum("Algorithm", kAlgoHall,
                                 { "Hall", "Plate", "Spring", "Room" });

  GetParam(kLength)->InitDouble("Length",
                                ranges::kLengthDefaultBars,
                                ranges::kLengthMinBars,
                                ranges::kLengthMaxBars,
                                0.25, "bars");

  GetParam(kForward)->InitBool("Forward", false);
  GetParam(kNormalize)->InitBool("Normalize", true);
  GetParam(kMonoStereo)->InitBool("Mono", false);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_DARK_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IRECT bounds       = pGraphics->GetBounds();
    const IRECT inner        = bounds.GetPadded(-12.f);
    const IRECT titleBounds  = inner.GetFromTop(40);

    pGraphics->AttachControl(new ITextControl(titleBounds,
      "PREMONITION — reverse-reverb riser", IText(22, COLOR_WHITE)));

    // Placeholder knob row. Full UI (drop bin, waveforms, queue, drag-out)
    // is scoped as a separate GUI pass — see PREMONITION_BRIEF.md.
    const IRECT knobRow = inner.GetFromTop(200).GetTranslated(0, 60);
    const int knobs[] = { kStart, kEnd, kStretch, kSize, kDecay, kMix, kLength };
    const int n = sizeof(knobs) / sizeof(int);
    for (int i = 0; i < n; ++i)
    {
      const IRECT cell = knobRow.GetGridCell(0, i, 1, n).GetPadded(-6);
      pGraphics->AttachControl(new IVKnobControl(cell, knobs[i]));
    }
  };
#endif
}

#if IPLUG_DSP
void Premonition::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // Premonition is an offline renderer. The realtime audio path is a
  // pass-through; rendering happens on demand via RenderRiserFromSource()
  // and the UI drag-to-render handle.
  const int nChans = NOutChansConnected();
  for (int s = 0; s < nFrames; ++s)
    for (int c = 0; c < nChans; ++c)
      outputs[c][s] = inputs[c][s];
}
#endif

#if IPLUG_EDITOR
premonition::dsp::StereoBuffer Premonition::RenderRiserFromSource(
  const premonition::dsp::StereoBuffer& source,
  float sourceSampleRate)
{
  dsp::PipelineConfig cfg;
  cfg.startSeconds = GetParam(kStart)->Value();
  cfg.endSeconds   = GetParam(kEnd)->Value();
  cfg.stretchRatio = GetParam(kStretch)->Value();
  cfg.roomSize     = static_cast<float>(GetParam(kSize)->Value());
  cfg.rt60Seconds  = static_cast<float>(GetParam(kDecay)->Value());
  cfg.damping      = 0.5f; // algorithm preset — wire per-algo later
  cfg.mix          = static_cast<float>(GetParam(kMix)->Value());
  cfg.lengthBars   = GetParam(kLength)->Value();
  cfg.bpm          = GetTempo() > 0.0 ? GetTempo() : 120.0;
  cfg.beatsPerBar  = 4; // host time-sig integration TODO
  cfg.forward      = GetParam(kForward)->Bool();
  cfg.normalize    = GetParam(kNormalize)->Bool();
  cfg.monoOutput   = GetParam(kMonoStereo)->Bool();

  mRendered = dsp::renderRiser(source, sourceSampleRate, cfg);
  return mRendered;
}
#endif
