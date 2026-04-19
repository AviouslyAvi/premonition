#include "Premonition.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "Parameters.h"
#include "dsp/AudioLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace premonition;

#if IPLUG_EDITOR
namespace {

// ---- AVIOUS palette (from mockup.html) ------------------------------------
const IColor kCotton    (255, 241, 236, 225);
const IColor kCotton2   (255, 232, 225, 210);
const IColor kEspresso  (255,  36,  25,  22);
const IColor kTerracotta(255, 162,  70,  23);
const IColor kOxblood   (255,  78,   0,   0);
const IColor kSage      (255, 121, 112,  40);
const IColor kMustard   (255, 211, 146,   3);
const IColor kMidnight  (255,  26,  36,  61);
const IColor kCharcoal  (255,  46,  40,  36);
const IColor kInkSoft   (255,  90,  74,  62);
const IColor kInkFaint  (255, 138, 122, 108);

const char* kSans = "Roboto-Regular";

constexpr const char* kSupportedExts = "wav aif aiff mp3 m4a ogg";

std::string basename_(const char* path)
{
  std::string p(path ? path : "");
  auto slash = p.find_last_of("/\\");
  return (slash == std::string::npos) ? p : p.substr(slash + 1);
}

// Upper-right arrow — Roboto lacks the "↗" glyph, so we stroke it ourselves.
void DrawArrowUpRight(IGraphics& g, const IRECT& bounds, const IColor& color)
{
  const float size = std::min(bounds.W(), bounds.H()) * 0.60f;
  const float cx = bounds.MW(), cy = bounds.MH();
  const float half = size * 0.5f;
  const float x0 = cx - half, y0 = cy + half;
  const float x1 = cx + half, y1 = cy - half;
  g.DrawLine(color, x0, y0, x1, y1, nullptr, 2.2f);
  const float head = size * 0.40f;
  g.DrawLine(color, x1, y1, x1 - head, y1, nullptr, 2.2f);
  g.DrawLine(color, x1, y1, x1, y1 + head, nullptr, 2.2f);
}

// Brand orb: concentric circles fake a radial gradient.
class BrandOrbControl : public IControl
{
public:
  BrandOrbControl(const IRECT& bounds) : IControl(bounds) {}
  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float r  = std::min(mRECT.W(), mRECT.H()) * 0.5f;
    g.FillCircle(kOxblood, cx, cy, r);
    g.FillCircle(kTerracotta, cx, cy, r * 0.78f);
    g.FillCircle(kMustard, cx - r * 0.28f, cy - r * 0.28f, r * 0.42f);
  }
};

// Thin rect divider.
class DividerControl : public IControl
{
public:
  DividerControl(const IRECT& bounds, IColor color = kEspresso)
    : IControl(bounds), mColor(color) {}
  void Draw(IGraphics& g) override { g.FillRect(mColor, mRECT); }
private:
  IColor mColor;
};

// Dropzone: dashed terracotta border, click = file picker, drop = load.
class DropzoneControl : public IControl
{
public:
  using LoadFunc = std::function<void(const char*)>;

  DropzoneControl(const IRECT& bounds, LoadFunc onLoad)
    : IControl(bounds), mOnLoad(std::move(onLoad))
  {
    SetTooltip("Click to browse, or drop a WAV / AIFF / MP3 / M4A / OGG");
  }

  void Draw(IGraphics& g) override
  {
    if (mHover)
      g.FillRoundRect(IColor(24, 162, 70, 23), mRECT, 10.f);
    g.DrawDottedRect(kTerracotta, mRECT, nullptr, 1.5f, 8.f);

    const IRECT top = mRECT.GetFromTop(mRECT.H() * 0.55f);
    const IRECT bot = mRECT.GetFromBottom(mRECT.H() * 0.45f);
    g.DrawText(IText(20.f, kEspresso, kSans, EAlign::Center, EVAlign::Bottom),
               "Drop audio here", top);
    g.DrawText(IText(10.f, kInkFaint, kSans, EAlign::Center, EVAlign::Top),
               "WAV  ·  AIFF", bot);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    WDL_String fn, dir;
    GetUI()->PromptForFile(fn, dir, EFileAction::Open, kSupportedExts,
      [this](const WDL_String& f, const WDL_String&) {
        if (f.GetLength() > 0 && mOnLoad) mOnLoad(f.Get());
      });
  }

  void OnMouseOver(float, float, const IMouseMod&) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; SetDirty(false); }
  void OnDrop(const char* path) override { if (mOnLoad && path) mOnLoad(path); }
  void OnDropMultiple(const std::vector<const char*>& paths) override
  { if (mOnLoad && !paths.empty()) mOnLoad(paths[0]); }

  void SetOnLoad(LoadFunc fn) { mOnLoad = std::move(fn); }

private:
  LoadFunc mOnLoad;
  bool mHover = false;
};

// Waveform: bar viz against a midnight ground. Renders styled empty state
// when no source is loaded (matches the mockup screenshot).
class WaveformControl : public IControl
{
public:
  using BufferGetter = std::function<const dsp::StereoBuffer*()>;
  using SRGetter = std::function<float()>;

  WaveformControl(const IRECT& bounds, BufferGetter getSource,
                  BufferGetter getRendered, SRGetter getSourceSR)
    : IControl(bounds)
    , mGetSource(std::move(getSource))
    , mGetRendered(std::move(getRendered))
    , mGetSourceSR(std::move(getSourceSR)) {}

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(kCharcoal, mRECT, 10.f);

    // Split: top ~60% source, bottom ~40% rendered.
    const float splitY = mRECT.T + mRECT.H() * kSplitFrac;
    const IRECT topRect = IRECT(mRECT.L, mRECT.T, mRECT.R, splitY);
    const IRECT botRect = IRECT(mRECT.L, splitY, mRECT.R, mRECT.B);

    DrawSourceStrip(g, topRect);
    DrawRenderedStrip(g, botRect);

    // 1px divider between strips.
    g.FillRect(IColor(80, 241, 236, 225),
               IRECT(mRECT.L + 10.f, splitY - 0.5f, mRECT.R - 10.f, splitY + 0.5f));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    mDragging = HitTestGrip(x, y);
    if (mDragging != Grip::None)
    {
      const int idx = (mDragging == Grip::Left) ? kStart : kEnd;
      GetDelegate()->BeginInformHostOfParamChangeFromUI(idx);
      UpdateCropFromX(x);
    }
  }

  void OnMouseDrag(float x, float, float, float, const IMouseMod&) override
  {
    if (mDragging == Grip::None) return;
    UpdateCropFromX(x);
  }

  void OnMouseUp(float, float, const IMouseMod&) override
  {
    if (mDragging != Grip::None)
    {
      const int idx = (mDragging == Grip::Left) ? kStart : kEnd;
      GetDelegate()->EndInformHostOfParamChangeFromUI(idx);
    }
    mDragging = Grip::None;
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const Grip g = HitTestGrip(x, y);
    if (g != mHoverGrip)
    {
      mHoverGrip = g;
      SetTooltip(g == Grip::Left  ? "Drag to set Start"
              : g == Grip::Right ? "Drag to set End"
              : "");
      if (auto* ui = GetUI())
        ui->SetMouseCursor(g == Grip::None ? ECursor::ARROW : ECursor::SIZEWE);
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mHoverGrip != Grip::None)
    {
      mHoverGrip = Grip::None;
      if (auto* ui = GetUI())
        ui->SetMouseCursor(ECursor::ARROW);
      SetDirty(false);
    }
  }

private:
  static constexpr float kSplitFrac = 0.60f;
  static constexpr float kGripHitTol = 10.f;  // ±10px around handle line
  static constexpr double kMinGapSec = 0.050; // 50 ms min crop window

  enum class Grip { None, Left, Right };

  IRECT GetSourceInset() const
  {
    const float splitY = mRECT.T + mRECT.H() * kSplitFrac;
    return IRECT(mRECT.L, mRECT.T, mRECT.R, splitY)
             .GetPadded(-18.f, -8.f, -18.f, -6.f);
  }

  float SourceDurSec() const
  {
    const dsp::StereoBuffer* src = mGetSource ? mGetSource() : nullptr;
    const float sr = mGetSourceSR ? mGetSourceSR() : 0.f;
    if (!src || src->L.empty() || sr <= 0.f) return 0.f;
    return static_cast<float>(src->L.size()) / sr;
  }

  // Treat kEnd == 0 as "end of source" so a freshly loaded sample shows the
  // right grip at the true end before the user touches it.
  double EndSecEffective(double dur)
  {
    const double v = GetDelegate()->GetParam(kEnd)->Value();
    return (v <= 0.0) ? dur : v;
  }

  float SecToX(double sec, const IRECT& inset, double dur) const
  {
    if (dur <= 0.0) return inset.L;
    const double frac = std::clamp(sec / dur, 0.0, 1.0);
    return inset.L + static_cast<float>(frac) * inset.W();
  }

  Grip HitTestGrip(float x, float y)
  {
    const double dur = SourceDurSec();
    if (dur <= 0.0) return Grip::None;
    const IRECT inset = GetSourceInset();
    if (y < inset.T - 4.f || y > inset.B + 4.f) return Grip::None;
    const float lx = SecToX(GetDelegate()->GetParam(kStart)->Value(), inset, dur);
    const float rx = SecToX(EndSecEffective(dur), inset, dur);
    const float dL = std::abs(x - lx);
    const float dR = std::abs(x - rx);
    if (dL <= kGripHitTol && dL <= dR) return Grip::Left;
    if (dR <= kGripHitTol) return Grip::Right;
    return Grip::None;
  }

  void UpdateCropFromX(float x)
  {
    const double dur = SourceDurSec();
    if (dur <= 0.0) return;
    const IRECT inset = GetSourceInset();
    const double frac = std::clamp((x - inset.L) / inset.W(), 0.f, 1.f);
    double sec = frac * dur;

    const int idx = (mDragging == Grip::Left) ? kStart : kEnd;
    if (mDragging == Grip::Left)
    {
      const double endV = EndSecEffective(dur);
      sec = std::clamp(sec, 0.0, endV - kMinGapSec);
    }
    else
    {
      const double startV = GetDelegate()->GetParam(kStart)->Value();
      sec = std::clamp(sec, startV + kMinGapSec, dur);
    }

    const double norm = GetDelegate()->GetParam(idx)->ToNormalized(sec);
    GetDelegate()->SendParameterValueFromUI(idx, norm);
    SetDirty(false);
  }


  void DrawSourceStrip(IGraphics& g, const IRECT& strip)
  {
    const IRECT inset = strip.GetPadded(-18.f, -8.f, -18.f, -6.f);
    const dsp::StereoBuffer* src = mGetSource ? mGetSource() : nullptr;
    const int kBars = 60;
    const float slot = inset.W() / kBars;
    const float barW = slot * 0.60f;

    if (!src || src->L.empty())
    {
      for (int i = 0; i < kBars; ++i)
      {
        const float t = static_cast<float>(i) / kBars;
        float h = std::max(2.f, inset.H() * (0.85f * std::exp(-3.f * t)));
        const float x = inset.L + i * slot + (slot - barW) * 0.5f;
        const float yC = inset.MH();
        const IColor c(static_cast<int>(120 - 90 * t), 162, 70, 23);
        g.FillRoundRect(c, IRECT(x, yC - h * 0.5f, x + barW, yC + h * 0.5f), 2.5f);
      }
      return;
    }

    const int total = static_cast<int>(src->L.size());
    std::vector<float> peaks(kBars, 0.f);
    float globalPeak = 1e-9f;
    for (int i = 0; i < kBars; ++i)
    {
      const int s0 = static_cast<int>(static_cast<double>(i)     / kBars * total);
      const int s1 = static_cast<int>(static_cast<double>(i + 1) / kBars * total);
      float m = 0.f;
      for (int s = s0; s < s1 && s < total; ++s)
        m = std::max(m, std::abs(src->L[s]));
      peaks[i] = m;
      globalPeak = std::max(globalPeak, m);
    }

    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const IColor kLo = kTerracotta;
    const IColor kHi = kMustard;

    for (int i = 0; i < kBars; ++i)
    {
      const float t = std::clamp(peaks[i] / globalPeak, 0.f, 1.f);
      const float h = std::max(2.f, std::min(1.f, peaks[i] * 1.6f) * inset.H());
      const float x = inset.L + i * slot + (slot - barW) * 0.5f;
      const float yC = inset.MH();
      const IColor c(lerp(150, 255, t),
                     lerp(kLo.R, kHi.R, t),
                     lerp(kLo.G, kHi.G, t),
                     lerp(kLo.B, kHi.B, t));
      g.FillRoundRect(c, IRECT(x, yC - h * 0.5f, x + barW, yC + h * 0.5f), 2.5f);
    }

    const double dur = SourceDurSec();
    const float lx = (dur > 0.0)
      ? SecToX(GetDelegate()->GetParam(kStart)->Value(), inset, dur)
      : inset.L;
    const float rx = (dur > 0.0)
      ? SecToX(EndSecEffective(dur), inset, dur)
      : inset.R;

    const IColor kHandleBase(255, 200, 149, 64);
    const IColor kHandleHot (255, 255, 196, 110);
    auto handleColor = [&](Grip which) {
      const bool active = (mDragging == which) || (mHoverGrip == which);
      return active ? kHandleHot : kHandleBase;
    };
    const IColor cL = handleColor(Grip::Left);
    const IColor cR = handleColor(Grip::Right);

    g.DrawLine(cL, lx, inset.T, lx, inset.B, nullptr, 2.f);
    g.DrawLine(cR, rx, inset.T, rx, inset.B, nullptr, 2.f);
    const float grip = 6.f;
    g.FillRect(cL, IRECT(lx - grip * 0.5f, inset.T, lx + grip * 0.5f, inset.T + grip));
    g.FillRect(cL, IRECT(lx - grip * 0.5f, inset.B - grip, lx + grip * 0.5f, inset.B));
    g.FillRect(cR, IRECT(rx - grip * 0.5f, inset.T, rx + grip * 0.5f, inset.T + grip));
    g.FillRect(cR, IRECT(rx - grip * 0.5f, inset.B - grip, rx + grip * 0.5f, inset.B));
  }

  void DrawRenderedStrip(IGraphics& g, const IRECT& strip)
  {
    const IRECT inset = strip.GetPadded(-18.f, -6.f, -18.f, -8.f);
    const dsp::StereoBuffer* ren = mGetRendered ? mGetRendered() : nullptr;
    const int kBars = 60;
    const float slot = inset.W() / kBars;
    const float barW = slot * 0.60f;

    if (!ren || ren->L.empty())
    {
      g.DrawText(IText(10.f, IColor(140, 241, 236, 225), kSans,
                       EAlign::Center, EVAlign::Middle),
                 "no render yet", inset);
      return;
    }

    const int total = static_cast<int>(ren->L.size());
    std::vector<float> peaks(kBars, 0.f);
    float globalPeak = 1e-9f;
    for (int i = 0; i < kBars; ++i)
    {
      const int s0 = static_cast<int>(static_cast<double>(i)     / kBars * total);
      const int s1 = static_cast<int>(static_cast<double>(i + 1) / kBars * total);
      float m = 0.f;
      for (int s = s0; s < s1 && s < total; ++s)
        m = std::max(m, std::abs(ren->L[s]));
      peaks[i] = m;
      globalPeak = std::max(globalPeak, m);
    }

    auto lerp = [](int a, int b, float t) { return static_cast<int>(a + (b - a) * t); };
    const IColor kLo = kOxblood;
    const IColor kHi = kMustard;

    for (int i = 0; i < kBars; ++i)
    {
      const float tPos = static_cast<float>(i) / (kBars - 1); // left→right riser build
      const float amp  = std::clamp(peaks[i] / globalPeak, 0.f, 1.f);
      const float h = std::max(2.f, amp * inset.H());
      const float x = inset.L + i * slot + (slot - barW) * 0.5f;
      const float yC = inset.MH();
      const IColor c(lerp(180, 255, tPos),
                     lerp(kLo.R, kHi.R, tPos),
                     lerp(kLo.G, kHi.G, tPos),
                     lerp(kLo.B, kHi.B, tPos));
      g.FillRoundRect(c, IRECT(x, yC - h * 0.5f, x + barW, yC + h * 0.5f), 2.5f);
    }
  }

  BufferGetter mGetSource;
  BufferGetter mGetRendered;
  SRGetter mGetSourceSR;
  Grip mDragging = Grip::None;
  Grip mHoverGrip = Grip::None;
};

// Flat action button — rounded fill + espresso border + centered label.
class ActionButtonControl : public IControl
{
public:
  using Action = std::function<void()>;
  ActionButtonControl(const IRECT& bounds, const char* label, IColor fill, Action action)
    : IControl(bounds), mFill(fill), mAction(std::move(action))
  { mLabel.Set(label); }

  void Draw(IGraphics& g) override
  {
    IColor fill = mFill;
    if (mPressed) { fill.R *= 0.8f; fill.G *= 0.8f; fill.B *= 0.8f; }
    else if (mHover) {
      fill.R = std::min(255, static_cast<int>(fill.R * 1.12f));
      fill.G = std::min(255, static_cast<int>(fill.G * 1.12f));
      fill.B = std::min(255, static_cast<int>(fill.B * 1.12f));
    }
    g.FillRoundRect(fill, mRECT, 10.f);
    g.DrawRoundRect(kEspresso, mRECT, 10.f, nullptr, 1.5f);
    g.DrawText(IText(13.f, kCotton, kSans, EAlign::Center, EVAlign::Middle),
               mLabel.Get(), mRECT);
  }

  void OnMouseDown(float, float, const IMouseMod&) override { mPressed = true; SetDirty(false); }
  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    const bool fire = mPressed && mRECT.Contains(x, y);
    mPressed = false; SetDirty(false);
    if (fire && mAction) mAction();
  }
  void OnMouseOver(float, float, const IMouseMod&) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; mPressed = false; SetDirty(false); }

private:
  WDL_String mLabel;
  IColor mFill;
  Action mAction;
  bool mHover = false, mPressed = false;
};

// Drag-out result: sage pill showing filename + meta, with arrow glyph.
class DragoutControl : public IControl
{
public:
  DragoutControl(const IRECT& bounds) : IControl(bounds)
  {
    mName.Set("Render to populate this row");
    mMeta.Set("No render yet");
  }

  void SetInfo(const char* name, const char* meta)
  { mName.Set(name); mMeta.Set(meta); SetDirty(false); }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(kSage, mRECT, 10.f);
    g.DrawRoundRect(kEspresso, mRECT, 10.f, nullptr, 1.5f);
    const IRECT pad = mRECT.GetPadded(-16.f, -10.f, -16.f, -10.f);
    const IRECT arrow = pad.GetFromRight(28.f);
    const IRECT text  = pad.GetReducedFromRight(34.f);
    const IRECT top = text.GetFromTop(text.H() * 0.5f);
    const IRECT bot = text.GetFromBottom(text.H() * 0.5f);
    g.DrawText(IText(13.f, kCotton, kSans, EAlign::Near, EVAlign::Bottom),
               mName.Get(), top);
    g.DrawText(IText(10.f, IColor(190, 241, 236, 225), kSans, EAlign::Near, EVAlign::Top),
               mMeta.Get(), bot);
    DrawArrowUpRight(g, arrow, kCotton);
  }

private:
  WDL_String mName, mMeta;
};

} // namespace
#endif // IPLUG_EDITOR

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

  GetParam(kLength)->InitEnum("Length", kLen2,
                              { "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" });

  GetParam(kForward)->InitBool("Forward", false);
  GetParam(kNormalize)->InitBool("Normalize", true);
  GetParam(kMonoStereo)->InitBool("Mono", false);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    if (pGraphics->NControls()) return;

    // Fonts MUST load before any text draws — NanoVG asserts on missing font.
    if (!pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN)) return;
    pGraphics->AttachPanelBackground(kCotton);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);

    const IRECT b = pGraphics->GetBounds();

    // ============== LAYOUT SKELETON ==============
    const float kHeaderH = 72.f;
    const float kFooterH = 36.f;
    const float kRightW  = 300.f;

    const IRECT header = b.GetFromTop(kHeaderH);
    const IRECT body   = b.GetReducedFromTop(kHeaderH).GetReducedFromBottom(kFooterH);
    const IRECT footer = b.GetFromBottom(kFooterH);
    const IRECT leftPanel  = body.GetReducedFromRight(kRightW);
    const IRECT rightPanel = body.GetFromRight(kRightW);

    // Right panel bg + dividers.
    pGraphics->AttachControl(new IPanelControl(rightPanel, kCotton2));
    pGraphics->AttachControl(new DividerControl(
      IRECT(header.L, header.B - 1.5f, header.R, header.B)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(footer.L, footer.T, footer.R, footer.T + 1.5f)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(rightPanel.L, rightPanel.T, rightPanel.L + 1.5f, rightPanel.B)));

    // ============== HEADER: orb + "Pre monition" + mode tabs ==============
    const IRECT orbRect = IRECT(header.L + 28.f, header.MH() - 14.f,
                                header.L + 56.f, header.MH() + 14.f);
    pGraphics->AttachControl(new BrandOrbControl(orbRect));

    const IRECT nameRect = IRECT(orbRect.R + 14.f, header.T,
                                 header.L + 320.f, header.B);
    const float splitX = nameRect.L + 46.f;
    pGraphics->AttachControl(new ITextControl(
      IRECT(nameRect.L, nameRect.T, splitX, nameRect.B),
      "Pre", IText(26.f, kTerracotta, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new ITextControl(
      IRECT(splitX, nameRect.T, nameRect.R, nameRect.B),
      "monition", IText(26.f, kEspresso, kSans, EAlign::Near, EVAlign::Middle)));

    // Mode tabs (DROP active, INSERT stubbed).
    const IRECT tabs = IRECT(header.R - 220.f, header.T,
                             header.R - 28.f, header.B);
    const IRECT dropTab = tabs.GetFromLeft(90.f);
    const IRECT insTab  = tabs.GetFromRight(90.f);
    pGraphics->AttachControl(new ITextControl(
      dropTab, "DROP",
      IText(11.f, kEspresso, kSans, EAlign::Center, EVAlign::Middle)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(dropTab.L + 20.f, dropTab.B - 8.f, dropTab.R - 20.f, dropTab.B - 6.f),
      kTerracotta));
    pGraphics->AttachControl(new ITextControl(
      insTab, "INSERT",
      IText(11.f, kInkFaint, kSans, EAlign::Center, EVAlign::Middle)));

    // ============== LEFT PANEL ==============
    const float pad = 28.f;
    const IRECT L = leftPanel.GetPadded(-pad, -pad, -pad, -pad);

    // --- Dropzone ---
    const IRECT dropzone = L.GetFromTop(110.f);
    auto* dzCtl = new DropzoneControl(dropzone, nullptr);
    pGraphics->AttachControl(dzCtl);

    // --- Queue label + row ---
    const IRECT qLabel = IRECT(L.L, dropzone.B + 18.f, L.R, dropzone.B + 30.f);
    pGraphics->AttachControl(new ITextControl(
      qLabel, "QUEUE",
      IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));

    const IRECT qRow = IRECT(L.L, qLabel.B + 6.f, L.R, qLabel.B + 32.f);
    pGraphics->AttachControl(new IPanelControl(
      IRECT(qRow.L, qRow.MH() - 4.f, qRow.L + 8.f, qRow.MH() + 4.f),
      kInkFaint));
    auto* qName = new ITextControl(
      IRECT(qRow.L + 18.f, qRow.T, qRow.R - 80.f, qRow.B),
      "No sample loaded",
      IText(13.f, kEspresso, kSans, EAlign::Near, EVAlign::Middle));
    pGraphics->AttachControl(qName);
    auto* qMeta = new ITextControl(
      IRECT(qRow.R - 80.f, qRow.T, qRow.R, qRow.B), "-",
      IText(11.f, kInkFaint, kSans, EAlign::Far, EVAlign::Middle));
    pGraphics->AttachControl(qMeta);
    pGraphics->AttachControl(new DividerControl(
      IRECT(qRow.L, qRow.B + 2.f, qRow.R, qRow.B + 3.f),
      IColor(60, 36, 25, 22)));

    // --- Waveform ---
    const IRECT wav = IRECT(L.L, qRow.B + 22.f, L.R, qRow.B + 172.f);
    pGraphics->AttachControl(new WaveformControl(wav,
      [this]() { return &Source(); },
      [this]() { return &Rendered(); },
      [this]() { return SourceSampleRate(); }));

    // --- Render + Preview row ---
    const IRECT actions = IRECT(L.L, wav.B + 22.f, L.R, wav.B + 64.f);
    const IRECT renderBtn  = actions.GetFromLeft(actions.W() * 0.7f).GetReducedFromRight(8.f);
    const IRECT previewBtn = actions.GetFromRight(actions.W() * 0.3f - 4.f);

    pGraphics->AttachControl(new ActionButtonControl(renderBtn, "RENDER", kOxblood,
      [this]() {
        if (Source().frames() == 0) return;
        RenderRiserFromSource(Source(), SourceSampleRate());
        if (auto* ui = GetUI()) ui->SetAllControlsDirty();
      }));

    pGraphics->AttachControl(new ActionButtonControl(previewBtn, "PREVIEW", kTerracotta,
      []() { /* preview hook — wire to transport in a later phase */ }));

    // --- Result + dragout ---
    const IRECT rLabel = IRECT(L.L, actions.B + 18.f, L.R, actions.B + 30.f);
    pGraphics->AttachControl(new ITextControl(
      rLabel, "RESULT  ·  DRAG INTO YOUR DAW",
      IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));

    const IRECT dragout = IRECT(L.L, rLabel.B + 8.f, L.R, rLabel.B + 64.f);
    pGraphics->AttachControl(new DragoutControl(dragout));

    // --- Status line ---
    pGraphics->AttachControl(new ITextControl(
      IRECT(L.L, dragout.B + 8.f, L.R, dragout.B + 24.f),
      "Ready  ·  click Render or Preview",
      IText(11.f, kInkSoft, kSans, EAlign::Near, EVAlign::Middle)));

    dzCtl->SetOnLoad([this, qName, qMeta](const char* path) {
      if (!LoadSourceFile(path)) return;
      qName->SetStr(basename_(path).c_str());
      char buf[32] = "-";
      const float rate = SourceSampleRate();
      if (rate > 0.f && Source().frames() > 0)
        std::snprintf(buf, sizeof(buf), "%.2fs",
                      static_cast<double>(Source().frames()) / rate);
      qMeta->SetStr(buf);
      if (auto* ui = GetUI()) ui->SetAllControlsDirty();
    });

    // ============== RIGHT PANEL ==============
    const IRECT R = rightPanel.GetPadded(-pad, -pad, -pad, -pad);

    pGraphics->AttachControl(new ITextControl(
      R.GetFromTop(28.f), "Reverse Reverb",
      IText(20.f, kEspresso, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new ITextControl(
      IRECT(R.L, R.T + 32.f, R.R, R.T + 46.f),
      "MODULE  ·  AVIOUS",
      IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));

    // Knob style.
    IVColorSpec knobColors {
      kCotton2, kEspresso, kOxblood, kEspresso, kMustard,
      IColor(80, 0, 0, 0), kMustard, kCotton, kInkSoft
    };
    IText knobLabel(11.f, kInkSoft, kSans, EAlign::Center, EVAlign::Middle);
    IText knobValue(11.f, kEspresso, kSans, EAlign::Center, EVAlign::Middle);
    IVStyle knobStyle(true, true, knobColors, knobLabel, knobValue,
                      /*hideCursor*/ true, /*drawFrame*/ false,
                      /*drawShadows*/ false, /*emboss*/ false,
                      /*roundness*/ 0.f, /*frameThickness*/ 1.5f,
                      /*shadowOffset*/ 2.f, /*widgetFrac*/ 0.62f);

    // 2x2 knob grid: Size / Decay / Tail(Length) / Mix
    const IRECT knobArea = IRECT(R.L, R.T + 64.f, R.R, R.T + 280.f);
    const float cw = knobArea.W() / 2.f;
    const float ch = knobArea.H() / 2.f;
    auto cell = [&](int col, int row) {
      return IRECT(knobArea.L + col * cw, knobArea.T + row * ch,
                   knobArea.L + (col + 1) * cw, knobArea.T + (row + 1) * ch)
             .GetPadded(-6.f);
    };
    pGraphics->AttachControl(new IVKnobControl(cell(0, 0), kSize,   "SIZE",  knobStyle));
    pGraphics->AttachControl(new IVKnobControl(cell(1, 0), kDecay,  "DECAY", knobStyle));
    pGraphics->AttachControl(new IVKnobControl(cell(0, 1), kLength, "TAIL",  knobStyle));
    pGraphics->AttachControl(new IVKnobControl(cell(1, 1), kMix,    "MIX",   knobStyle));

    // Toggle row: Mode (kForward) + Normalize.
    const IRECT toggles = IRECT(R.L, knobArea.B + 8.f, R.R, knobArea.B + 40.f);
    const float halfW = toggles.W() * 0.5f;
    const IRECT modeRect = IRECT(toggles.L, toggles.T, toggles.L + halfW, toggles.B);
    const IRECT normRect = IRECT(toggles.L + halfW, toggles.T, toggles.R, toggles.B);

    pGraphics->AttachControl(new ITextControl(
      IRECT(modeRect.L, modeRect.T, modeRect.L + 54.f, modeRect.B),
      "MODE", IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new IVSlideSwitchControl(
      modeRect.GetReducedFromLeft(54.f), kForward, "", knobStyle,
      /*valueInButton*/ true, EDirection::Horizontal));

    pGraphics->AttachControl(new ITextControl(
      IRECT(normRect.L, normRect.T, normRect.L + 54.f, normRect.B),
      "NORM", IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new IVSlideSwitchControl(
      normRect.GetReducedFromLeft(54.f), kNormalize, "", knobStyle,
      /*valueInButton*/ true, EDirection::Horizontal));

    // Preset row.
    const IRECT preset = IRECT(R.L, R.B - 56.f, R.R, R.B - 16.f);
    pGraphics->AttachControl(new IPanelControl(preset, kCotton));
    pGraphics->AttachControl(new DividerControl(
      IRECT(preset.L, preset.T, preset.L + 1.5f, preset.B)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(preset.R - 1.5f, preset.T, preset.R, preset.B)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(preset.L, preset.T, preset.R, preset.T + 1.5f)));
    pGraphics->AttachControl(new DividerControl(
      IRECT(preset.L, preset.B - 1.5f, preset.R, preset.B)));
    pGraphics->AttachControl(new ITextControl(
      IRECT(preset.L + 14.f, preset.T, preset.R - 30.f, preset.MH()),
      "PRESET", IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Bottom)));
    pGraphics->AttachControl(new ICaptionControl(
      IRECT(preset.L + 14.f, preset.MH(), preset.R - 30.f, preset.B),
      kAlgorithm,
      IText(13.f, kEspresso, kSans, EAlign::Near, EVAlign::Top),
      DEFAULT_BGCOLOR, false));
    pGraphics->AttachControl(new ITextControl(
      IRECT(preset.R - 28.f, preset.T, preset.R - 14.f, preset.B),
      "v", IText(13.f, kInkFaint, kSans, EAlign::Far, EVAlign::Middle)));

    // ============== FOOTER ==============
    const IRECT finset = footer.GetPadded(-28.f, 0.f, -28.f, 0.f);
    pGraphics->AttachControl(new ITextControl(
      finset, "48 kHz  ·  120 bpm",
      IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new ITextControl(
      finset, "v" PLUG_VERSION_STR,
      IText(10.f, kInkFaint, kSans, EAlign::Far, EVAlign::Middle)));
  };
#endif
}

#if IPLUG_DSP
void Premonition::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // Premonition is an offline renderer. Realtime path is pass-through.
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
  cfg.damping      = 0.5f;
  cfg.mix          = static_cast<float>(GetParam(kMix)->Value());
  {
    int lenIdx = GetParam(kLength)->Int();
    if (lenIdx < 0) lenIdx = 0;
    if (lenIdx >= kNumLengths) lenIdx = kNumLengths - 1;
    cfg.lengthBars = kLengthBarsTable[lenIdx];
  }
  cfg.bpm          = GetTempo() > 0.0 ? GetTempo() : 120.0;
  cfg.beatsPerBar  = 4;
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
