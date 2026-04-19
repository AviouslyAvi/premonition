#include "Premonition.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "Parameters.h"
#include "dsp/AudioLoader.h"

using namespace premonition;

namespace {

// Space-separated extension list iPlug2 feeds to the platform file dialog
// and that we also use to filter drag-drop payloads. Keep the leading dots
// off (iPlug2 docs show "wav mp3 …"). ExtAudioFile handles all except ogg,
// which stb_vorbis picks up inside the loader.
constexpr const char* kSupportedExts = "wav aif aiff mp3 m4a ogg";

std::string basename_(const char* path)
{
  std::string p(path ? path : "");
  auto slash = p.find_last_of("/\\");
  return (slash == std::string::npos) ? p : p.substr(slash + 1);
}

// Drop zone: click to browse, drop a supported audio file to load. Displays
// placeholder text until a file loads, then shows the filename + hint.
class DropZoneControl : public IControl
{
public:
  using LoadFunc = std::function<void(const char*)>;

  DropZoneControl(const IRECT& bounds,
                  const IColor& bg, const IColor& border,
                  const IText& text, const IText& textLoaded,
                  LoadFunc onLoad)
  : IControl(bounds)
  , mBG(bg), mBorder(border)
  , mText(text), mTextLoaded(textLoaded)
  , mOnLoad(std::move(onLoad))
  {
    SetTooltip("Click to browse, or drop a WAV / AIFF / MP3 / M4A / OGG");
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mBG, mRECT);
    g.DrawRect(mBorder, mRECT);

    if (mFilename.empty())
    {
      IRECT top = mRECT.GetFromTop(mRECT.H() * 0.5f);
      IRECT bot = mRECT.GetFromBottom(mRECT.H() * 0.5f);
      g.DrawText(mTextLoaded, "Drop audio here", top);
      g.DrawText(mText, "WAV · AIFF · MP3 · M4A · OGG", bot);
    }
    else
    {
      IRECT top = mRECT.GetFromTop(mRECT.H() * 0.5f);
      IRECT bot = mRECT.GetFromBottom(mRECT.H() * 0.5f);
      g.DrawText(mTextLoaded, mFilename.c_str(), top);
      g.DrawText(mText, "click to replace  ·  drop to swap", bot);
    }
  }

  void OnMouseDown(float /*x*/, float /*y*/, const IMouseMod& /*mod*/) override
  {
    WDL_String fileName, dir;
    GetUI()->PromptForFile(
      fileName, dir, EFileAction::Open, kSupportedExts,
      [this](const WDL_String& fn, const WDL_String& /*dir*/) {
        if (fn.GetLength() > 0 && mOnLoad) mOnLoad(fn.Get());
      });
  }

  void OnDrop(const char* path) override
  {
    if (mOnLoad && path) mOnLoad(path);
  }

  void OnDropMultiple(const std::vector<const char*>& paths) override
  {
    if (mOnLoad && !paths.empty()) mOnLoad(paths[0]);
  }

  void SetCurrentFilename(const char* path)
  {
    mFilename = basename_(path);
    SetDirty(false);
  }

  void SetOnLoad(LoadFunc fn) { mOnLoad = std::move(fn); }

private:
  IColor mBG, mBorder;
  IText mText, mTextLoaded;
  LoadFunc mOnLoad;
  std::string mFilename;
};

// Session-lifetime recent-files list. Up to kMaxRecents rows; newest on top.
// Click a row to re-load that file. Duplicates are promoted, not appended.
class RecentFilesControl : public IControl
{
public:
  using LoadFunc = std::function<void(const char*)>;
  static constexpr std::size_t kMaxRecents = 5;

  RecentFilesControl(const IRECT& bounds,
                     const IColor& rowBG, const IColor& rowBGHover,
                     const IColor& border,
                     const IText& rowText, const IText& emptyText,
                     LoadFunc onLoad)
  : IControl(bounds)
  , mRowBG(rowBG), mRowBGHover(rowBGHover)
  , mBorder(border)
  , mRowText(rowText), mEmptyText(emptyText)
  , mOnLoad(std::move(onLoad))
  {
    SetTooltip("Recently loaded files");
  }

  void Draw(IGraphics& g) override
  {
    if (mPaths.empty())
    {
      g.DrawText(mEmptyText, "No recent files", mRECT);
      return;
    }

    const float rowH = _rowHeight();
    for (std::size_t i = 0; i < mPaths.size(); ++i)
    {
      IRECT row = _rowRect(i, rowH);
      const bool hovered = (static_cast<int>(i) == mHoverRow);
      g.FillRect(hovered ? mRowBGHover : mRowBG, row, &mRowBlend);
      g.DrawRect(mBorder, row, &mRowBlend);

      IRECT inner = row.GetPadded(-10.f, 0, -10.f, 0);
      g.DrawText(mRowText, basename_(mPaths[i].c_str()).c_str(), inner);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& /*mod*/) override
  {
    const int row = _hitRow(x, y);
    if (row < 0 || !mOnLoad) return;
    // Snapshot the path before we reorder — mOnLoad may call AddRecent
    // which mutates mPaths.
    const std::string path = mPaths[static_cast<std::size_t>(row)];
    mOnLoad(path.c_str());
  }

  void OnMouseOver(float x, float y, const IMouseMod& /*mod*/) override
  {
    const int row = _hitRow(x, y);
    if (row != mHoverRow) { mHoverRow = row; SetDirty(false); }
  }

  void OnMouseOut() override
  {
    if (mHoverRow != -1) { mHoverRow = -1; SetDirty(false); }
  }

  void AddRecent(const char* path)
  {
    if (!path) return;
    std::string p(path);
    auto it = std::find(mPaths.begin(), mPaths.end(), p);
    if (it != mPaths.end()) mPaths.erase(it);
    mPaths.insert(mPaths.begin(), std::move(p));
    if (mPaths.size() > kMaxRecents) mPaths.resize(kMaxRecents);
    SetDirty(false);
  }

  void SetOnLoad(LoadFunc fn) { mOnLoad = std::move(fn); }

private:
  float _rowHeight() const
  {
    const std::size_t n = std::max<std::size_t>(mPaths.size(), 1);
    const float gap = 2.f;
    return (mRECT.H() - gap * (n - 1)) / static_cast<float>(n);
  }

  IRECT _rowRect(std::size_t i, float rowH) const
  {
    const float gap = 2.f;
    const float y0 = mRECT.T + static_cast<float>(i) * (rowH + gap);
    return IRECT(mRECT.L, y0, mRECT.R, y0 + rowH);
  }

  int _hitRow(float x, float y) const
  {
    if (mPaths.empty()) return -1;
    const float rowH = _rowHeight();
    for (std::size_t i = 0; i < mPaths.size(); ++i)
      if (_rowRect(i, rowH).Contains(x, y)) return static_cast<int>(i);
    return -1;
  }

  IColor mRowBG, mRowBGHover, mBorder;
  IText mRowText, mEmptyText;
  IBlend mRowBlend{EBlend::Default, 1.f};
  LoadFunc mOnLoad;
  std::vector<std::string> mPaths;
  int mHoverRow = -1;
};

} // anonymous namespace

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
    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    // ---- Palette (from mockup.html) --------------------------------------
    const IColor kOxblood    = IColor::FromColorCode(0x4E0000);
    const IColor kTerracotta = IColor::FromColorCode(0xA24617);
    const IColor kSage       = IColor::FromColorCode(0x797028);
    const IColor kMustard    = IColor::FromColorCode(0xD39203);
    const IColor kEspresso   = IColor::FromColorCode(0x241916);
    const IColor kCotton     = IColor::FromColorCode(0xF1ECE1);
    const IColor kCotton2    = IColor::FromColorCode(0xE8E1D2);
    const IColor kInkSoft    = IColor::FromColorCode(0x5A4A3E);
    const IColor kInkFaint   = IColor::FromColorCode(0x8A7A6C);

    pGraphics->AttachPanelBackground(kCotton);

    // ---- Shared knob style -----------------------------------------------
    const IVStyle kKnobStyle = DEFAULT_STYLE
      .WithColors(IVColorSpec{
          COLOR_TRANSPARENT, // kBG
          kEspresso,         // kFG (dial body)
          kOxblood,          // kPR (pressed)
          kEspresso,         // kFR (frame)
          kMustard,          // kHL (highlight / pointer)
          kEspresso,         // kSH (shadow)
          kTerracotta,       // kX1
          kSage,             // kX2
          kCotton2           // kX3
      })
      .WithRoundness(0.3f)
      .WithDrawShadows(true)
      .WithLabelText(IText(10.f, kInkSoft, "Roboto-Regular",
                           EAlign::Center, EVAlign::Middle))
      .WithValueText(IText(11.f, kEspresso, "Roboto-Regular",
                           EAlign::Center, EVAlign::Middle));

    const IVStyle kPrimaryBtnStyle = kKnobStyle
      .WithColors(IVColorSpec{
          kOxblood, kOxblood, kOxblood.WithOpacity(0.85f),
          kEspresso, kCotton, kEspresso, kTerracotta, kSage, kCotton2
      })
      .WithLabelText(IText(13.f, kCotton, "Roboto-Regular",
                           EAlign::Center, EVAlign::Middle));

    const IVStyle kPresetStyle = kKnobStyle
      .WithColors(IVColorSpec{
          kCotton, kEspresso, kTerracotta, kEspresso, kEspresso,
          kEspresso, kTerracotta, kSage, kCotton2
      });

    // ---- Layout -----------------------------------------------------------
    const IRECT bounds = pGraphics->GetBounds();
    const IRECT frame  = bounds.GetPadded(-12.f);

    // Outer frame border
    pGraphics->AttachControl(new IPanelControl(frame, kCotton, false));

    const IRECT header  = frame.GetFromTop(70);
    const IRECT footer  = frame.GetFromBottom(40);
    const IRECT body    = frame.GetReducedFromTop(70).GetReducedFromBottom(40);

    // Horizontal separators
    pGraphics->AttachControl(new IPanelControl(
      IRECT(header.L, header.B - 1.5f, header.R, header.B), kEspresso));
    pGraphics->AttachControl(new IPanelControl(
      IRECT(footer.L, footer.T, footer.R, footer.T + 1.5f), kEspresso));

    // Header: brand + mode tabs
    pGraphics->AttachControl(new ITextControl(
      header.GetPadded(-28.f, 0, -28.f, 0).GetFromLeft(260),
      "Premonition",
      IText(26.f, kEspresso, "Roboto-Regular", EAlign::Near, EVAlign::Middle)));

    pGraphics->AttachControl(new ITextControl(
      header.GetPadded(-28.f, 0, -28.f, 0).GetFromRight(260),
      "DROP   ·   FORWARD",
      IText(11.f, kInkFaint, "Roboto-Regular", EAlign::Far, EVAlign::Middle)));

    // Footer: sample rate · tempo · version
    pGraphics->AttachControl(new ITextControl(
      footer.GetPadded(-28.f, 0, -28.f, 0),
      "48 kHz  ·  120 bpm",
      IText(10.f, kInkFaint, "Roboto-Regular", EAlign::Near, EVAlign::Middle)));

    pGraphics->AttachControl(new ITextControl(
      footer.GetPadded(-28.f, 0, -28.f, 0),
      "v" PLUG_VERSION_STR,
      IText(10.f, kInkFaint, "Roboto-Regular", EAlign::Far, EVAlign::Middle)));

    // Split body: left (workflow) | right (rack, 300px)
    const float kRackWidth = 300.f;
    const IRECT left  = body.GetReducedFromRight(kRackWidth);
    const IRECT right = body.GetFromRight(kRackWidth);

    // Vertical divider
    pGraphics->AttachControl(new IPanelControl(
      IRECT(right.L, right.T, right.L + 1.5f, right.B), kEspresso));

    // Right panel background (cotton-2)
    pGraphics->AttachControl(new IPanelControl(
      IRECT(right.L + 1.5f, right.T, right.R, right.B), kCotton2));

    // ---- Left panel: drop zone placeholder + render row -------------------
    const IRECT leftInner = left.GetPadded(-28.f, -32.f, -28.f, -32.f);

    // Drop zone + recent-files list. Drop zone sits on top; recents below.
    const IRECT dropZone = leftInner.GetFromTop(110);
    const IRECT recents  = leftInner.GetReducedFromTop(118).GetFromTop(100);

    auto* dropZoneCtl = new DropZoneControl(
      dropZone,
      kCotton, kEspresso,
      IText(11.f, kInkFaint, "Roboto-Regular", EAlign::Center, EVAlign::Middle),
      IText(18.f, kEspresso, "Roboto-Regular", EAlign::Center, EVAlign::Middle),
      /* wired below once recents control exists */ nullptr);
    pGraphics->AttachControl(dropZoneCtl);

    auto* recentsCtl = new RecentFilesControl(
      recents,
      kCotton2,
      IColor::FromColorCode(0xDCD2C0), // slightly darker hover
      kEspresso,
      IText(11.f, kEspresso, "Roboto-Regular", EAlign::Near, EVAlign::Middle),
      IText(11.f, kInkFaint, "Roboto-Regular", EAlign::Center, EVAlign::Middle),
      nullptr);
    pGraphics->AttachControl(recentsCtl);

    // Shared load lambda: loads into the plugin, updates the drop zone label,
    // and pushes the path onto the recents list. Both controls call this.
    auto loadAndRecord = [this, dropZoneCtl, recentsCtl](const char* path) {
      if (!LoadSourceFile(path)) return;
      dropZoneCtl->SetCurrentFilename(path);
      recentsCtl->AddRecent(path);
    };
    dropZoneCtl->SetOnLoad(loadAndRecord);
    recentsCtl->SetOnLoad(loadAndRecord);

    // Waveform placeholder (v1c)
    const IRECT wave = leftInner.GetReducedFromTop(170).GetFromTop(180);
    pGraphics->AttachControl(new IPanelControl(wave,
      IColor::FromColorCode(0x1A243D)));
    pGraphics->AttachControl(new ITextControl(wave,
      "— waveform —",
      IText(11.f, kInkFaint, "Roboto-Regular", EAlign::Center, EVAlign::Middle)));

    // Render + Preview row
    const IRECT actionRow = leftInner.GetFromBottom(56);
    const IRECT renderBtn  = actionRow.GetFromLeft(180).GetPadded(0, -8, 0, -8);
    const IRECT previewBtn = actionRow.GetReducedFromLeft(200).GetFromLeft(120);

    pGraphics->AttachControl(new IVButtonControl(
      renderBtn,
      SplashClickActionFunc,
      "RENDER  →",
      kPrimaryBtnStyle,
      true, false, EVShape::Rectangle));

    pGraphics->AttachControl(new ITextControl(previewBtn,
      "Preview",
      IText(13.f, kInkSoft, "Roboto-Regular", EAlign::Near, EVAlign::Middle)));

    // ---- Right panel: rack ------------------------------------------------
    const IRECT rackInner = right.GetPadded(-28.f, -32.f, -28.f, -32.f);

    // Rack head
    const IRECT rackHead = rackInner.GetFromTop(56);
    pGraphics->AttachControl(new ITextControl(rackHead.GetFromTop(28),
      "Reverse Reverb",
      IText(20.f, kEspresso, "Roboto-Regular", EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new ITextControl(rackHead.GetFromBottom(16),
      "MODULE  ·  AVIOUS",
      IText(10.f, kInkFaint, "Roboto-Regular", EAlign::Near, EVAlign::Middle)));

    // 2x2 knob grid: Size, Decay / Tail (Length), Mix
    const IRECT knobArea = rackInner.GetReducedFromTop(72).GetFromTop(220);
    const int kGridParams[4]    = { kSize,  kDecay, kLength, kMix };
    const char* kGridLabels[4]  = { "Size", "Decay", "Tail", "Mix" };

    for (int i = 0; i < 4; ++i)
    {
      const int row = i / 2;
      const int col = i % 2;
      const IRECT cell = knobArea.GetGridCell(row, col, 2, 2).GetPadded(-10.f);
      pGraphics->AttachControl(new IVKnobControl(
        cell, kGridParams[i], kGridLabels[i], kKnobStyle));
    }

    // Preset dropdown at bottom of rack
    const IRECT presetRow = rackInner.GetFromBottom(44);
    pGraphics->AttachControl(new ICaptionControl(
      presetRow,
      kAlgorithm,
      IText(13.f, kEspresso, "Roboto-Regular", EAlign::Center, EVAlign::Middle),
      kCotton,
      false));
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

bool Premonition::LoadSourceFile(const char* path)
{
  dsp::StereoBuffer buf;
  float rate = 0.f;
  if (!dsp::loadAudioFile(path, buf, rate)) return false;
  mSource = std::move(buf);
  mSourceSampleRate = rate;
  return true;
}
#endif
