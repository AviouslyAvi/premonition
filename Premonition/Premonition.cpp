#include "Premonition.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "Parameters.h"
#include "dsp/AudioLoader.h"
#include "dsp/WavWriter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

// Compact IR slot — small dashed drop zone that reveals itself when the
// Convolution algorithm is active. Shows filename + duration when loaded.
class IRSlotControl : public IControl
{
public:
  using LoadFunc = std::function<void(const char*)>;
  using NameGetter = std::function<const std::string&()>;
  using HasGetter = std::function<bool()>;
  using DurGetter = std::function<float()>;

  IRSlotControl(const IRECT& bounds, LoadFunc onLoad, NameGetter getName,
                HasGetter hasIR, DurGetter getDur)
    : IControl(bounds), mOnLoad(std::move(onLoad)),
      mGetName(std::move(getName)), mHasIR(std::move(hasIR)),
      mGetDur(std::move(getDur))
  {
    SetTooltip("Drop an impulse response (WAV / AIFF) — or click to browse");
  }

  void Draw(IGraphics& g) override
  {
    g.DrawDottedRect(mHover ? kMustard : kTerracotta, mRECT, nullptr, 1.2f, 6.f);
    const IRECT pad = mRECT.GetPadded(-10.f, -4.f, -10.f, -4.f);
    const IRECT labelR = pad.GetFromLeft(22.f);
    const IRECT textR  = pad.GetReducedFromLeft(26.f);

    g.DrawText(IText(10.f, kTerracotta, kSans, EAlign::Near, EVAlign::Middle),
               "IR", labelR);

    const bool loaded = mHasIR && mHasIR();
    const std::string& nm = mGetName ? mGetName() : kEmpty;
    char buf[160];
    if (loaded)
    {
      const float dur = mGetDur ? mGetDur() : 0.f;
      std::snprintf(buf, sizeof(buf), "%s  ·  %.2fs",
                    nm.empty() ? "(impulse)" : nm.c_str(), dur);
    }
    else
    {
      std::snprintf(buf, sizeof(buf), "Drop impulse response");
    }
    g.DrawText(IText(11.f, loaded ? kEspresso : kInkFaint, kSans,
                     EAlign::Near, EVAlign::Middle),
               buf, textR);
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

private:
  LoadFunc mOnLoad;
  NameGetter mGetName;
  HasGetter mHasIR;
  DurGetter mGetDur;
  bool mHover = false;
  static inline const std::string kEmpty{};
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

    // Dim bars outside the crop window with a charcoal-tinted scrim.
    const IColor kScrim(170, 22, 20, 19);
    if (lx > inset.L)
      g.FillRect(kScrim, IRECT(inset.L, inset.T, lx, inset.B));
    if (rx < inset.R)
      g.FillRect(kScrim, IRECT(rx, inset.T, inset.R, inset.B));

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

// Preview toggle — reflects plugin preview state; label/fill swap live.
class PreviewButtonControl : public IControl
{
public:
  using Toggle = std::function<void()>;
  using IsPlaying = std::function<bool()>;
  using IsReady = std::function<bool()>;

  PreviewButtonControl(const IRECT& bounds, Toggle toggle,
                       IsPlaying isPlaying, IsReady isReady)
    : IControl(bounds)
    , mToggle(std::move(toggle))
    , mIsPlaying(std::move(isPlaying))
    , mIsReady(std::move(isReady)) {}

  void Draw(IGraphics& g) override
  {
    const bool playing = mIsPlaying && mIsPlaying();
    const bool ready = mIsReady && mIsReady();
    IColor fill = playing ? kOxblood : kTerracotta;
    if (!ready) { fill.A = 110; }
    else if (mPressed) { fill.R *= 0.8f; fill.G *= 0.8f; fill.B *= 0.8f; }
    else if (mHover) {
      fill.R = std::min(255, static_cast<int>(fill.R * 1.12f));
      fill.G = std::min(255, static_cast<int>(fill.G * 1.12f));
      fill.B = std::min(255, static_cast<int>(fill.B * 1.12f));
    }
    g.FillRoundRect(fill, mRECT, 10.f);
    g.DrawRoundRect(kEspresso, mRECT, 10.f, nullptr, 1.5f);
    g.DrawText(IText(13.f, kCotton, kSans, EAlign::Center, EVAlign::Middle),
               playing ? "STOP" : "PREVIEW", mRECT);

    // Keep redrawing so auto-stop at end-of-buffer reflects visually.
    SetDirty(false);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  { if (mIsReady && mIsReady()) { mPressed = true; SetDirty(false); } }
  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    const bool fire = mPressed && mRECT.Contains(x, y);
    mPressed = false; SetDirty(false);
    if (fire && mToggle) mToggle();
  }
  void OnMouseOver(float, float, const IMouseMod&) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; mPressed = false; SetDirty(false); }

private:
  Toggle mToggle;
  IsPlaying mIsPlaying;
  IsReady mIsReady;
  bool mHover = false, mPressed = false;
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

// A/B slot toggle — two small pills. Active = sage, inactive = cotton2. A
// thin oxblood dot on the inactive pill flags that the other slot holds a
// render (so user can tell "switch to compare" apart from "empty slot").
class SlotToggleControl : public IControl
{
public:
  using GetActive = std::function<int()>;
  using SetActive = std::function<void(int)>;
  using SlotHasRender = std::function<bool(int)>;

  SlotToggleControl(const IRECT& bounds, GetActive getActive, SetActive setActive,
                    SlotHasRender hasRender)
    : IControl(bounds)
    , mGetActive(std::move(getActive))
    , mSetActive(std::move(setActive))
    , mHasRender(std::move(hasRender))
  {
    SetTooltip("A/B render slots — switch to compare without re-rendering");
  }

  void Draw(IGraphics& g) override
  {
    const int active = mGetActive ? mGetActive() : 0;
    const float gap = 4.f;
    const float pw = (mRECT.W() - gap) * 0.5f;
    mPillA = IRECT(mRECT.L,           mRECT.T, mRECT.L + pw,       mRECT.B);
    mPillB = IRECT(mRECT.R - pw,      mRECT.T, mRECT.R,            mRECT.B);
    DrawPill(g, mPillA, "A", active == 0, mHasRender && mHasRender(0));
    DrawPill(g, mPillB, "B", active == 1, mHasRender && mHasRender(1));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!mSetActive) return;
    if (mPillA.Contains(x, y)) mSetActive(0);
    else if (mPillB.Contains(x, y)) mSetActive(1);
    SetDirty(false);
  }

  void OnMouseOver(float, float, const IMouseMod&) override { SetDirty(false); }

private:
  void DrawPill(IGraphics& g, const IRECT& r, const char* label,
                bool active, bool hasRender)
  {
    const IColor fill = active ? kSage : kCotton2;
    g.FillRoundRect(fill, r, 8.f);
    g.DrawRoundRect(kEspresso, r, 8.f, nullptr, 1.2f);
    const IColor textColor = active ? kCotton : kEspresso;
    g.DrawText(IText(12.f, textColor, kSans, EAlign::Center, EVAlign::Middle),
               label, r);
    if (!active && hasRender)
      g.FillCircle(kOxblood, r.R - 6.f, r.T + 6.f, 2.5f);
  }

  GetActive mGetActive;
  SetActive mSetActive;
  SlotHasRender mHasRender;
  IRECT mPillA, mPillB;
};

// Drag-out result: sage pill showing filename + meta, with arrow glyph.
// Mouse-down while a render exists writes a 32-bit float WAV to a temp file
// and hands it to the host via InitiateExternalFileDragDrop.
class DragoutControl : public IControl
{
public:
  DragoutControl(const IRECT& bounds, Premonition* plug)
    : IControl(bounds), mPlug(plug)
  {
    SetTooltip("Drag into your DAW — 32-bit float WAV");
  }

  void Draw(IGraphics& g) override
  {
    const bool ready = mPlug && mPlug->HasRendered();
    IColor fill = kSage;
    if (!ready) fill.A = 120;
    g.FillRoundRect(fill, mRECT, 10.f);
    g.DrawRoundRect(kEspresso, mRECT, 10.f, nullptr, 1.5f);

    const IRECT pad = mRECT.GetPadded(-16.f, -10.f, -16.f, -10.f);
    const IRECT arrow = pad.GetFromRight(28.f);
    const IRECT text  = pad.GetReducedFromRight(34.f);
    const IRECT top = text.GetFromTop(text.H() * 0.5f);
    const IRECT bot = text.GetFromBottom(text.H() * 0.5f);

    char nameBuf[128];
    char metaBuf[64];
    if (ready)
    {
      const int slot = mPlug->ActiveSlot();
      const auto& src = mPlug->SourceDisplayName();
      std::snprintf(nameBuf, sizeof(nameBuf), "Premonition %c%s%s",
                    slot == 0 ? 'A' : 'B',
                    src.empty() ? "" : " — ",
                    src.c_str());
      const float sr = static_cast<float>(mPlug->GetSampleRate());
      const double dur = sr > 0.f
        ? static_cast<double>(mPlug->Rendered().frames()) / sr : 0.0;
      std::snprintf(metaBuf, sizeof(metaBuf),
                    "%.2fs  ·  32-bit float  ·  drag to DAW", dur);
    }
    else
    {
      std::snprintf(nameBuf, sizeof(nameBuf), "Render to populate this row");
      std::snprintf(metaBuf, sizeof(metaBuf), "No render yet");
    }

    g.DrawText(IText(13.f, kCotton, kSans, EAlign::Near, EVAlign::Bottom),
               nameBuf, top);
    g.DrawText(IText(10.f, IColor(190, 241, 236, 225), kSans, EAlign::Near, EVAlign::Top),
               metaBuf, bot);
    DrawArrowUpRight(g, arrow, kCotton);

    SetDirty(false);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    if (!mPlug || !mPlug->HasRendered()) return;
    const std::string path = mPlug->ExportRenderedToTempWav();
    if (path.empty()) return;
    if (auto* ui = GetUI())
      ui->InitiateExternalFileDragDrop(path.c_str(), mRECT);
  }

  void OnMouseOver(float, float, const IMouseMod&) override
  {
    if (auto* ui = GetUI())
      ui->SetMouseCursor(mPlug && mPlug->HasRendered()
                           ? ECursor::HAND : ECursor::ARROW);
  }

  void OnMouseOut() override
  { if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW); }

private:
  Premonition* mPlug = nullptr;
};

// Footer info: "<SR> kHz  ·  <bpm> bpm". Tracks host tempo live; falls back
// to the manual BPM param when the host reports 0. When idle, the bpm segment
// is click-to-edit; when host tempo is live, the manual label is de-emphasized.
class FooterInfoControl : public IControl
{
public:
  FooterInfoControl(const IRECT& bounds, Premonition* plug)
    : IControl(bounds, kManualBPM), mPlug(plug)
  {
    SetTooltip("Click to set manual BPM (used when host transport is stopped)");
  }

  void Draw(IGraphics& g) override
  {
    const double hostBpm = mPlug ? mPlug->GetTempo() : 0.0;
    const bool hostLive = hostBpm > 0.0;
    const double bpm = hostLive ? hostBpm
                                : GetDelegate()->GetParam(kManualBPM)->Value();
    const float sr = mPlug ? static_cast<float>(mPlug->GetSampleRate()) : 0.f;

    char srBuf[32];
    std::snprintf(srBuf, sizeof(srBuf), "%.1f kHz", sr / 1000.f);
    char bpmBuf[32];
    std::snprintf(bpmBuf, sizeof(bpmBuf), "%.1f bpm%s",
                  bpm, hostLive ? " (host)" : "");

    const IRECT srRect  = IRECT(mRECT.L,           mRECT.T, mRECT.L + 72.f, mRECT.B);
    const IRECT dotRect = IRECT(srRect.R,          mRECT.T, srRect.R + 14.f, mRECT.B);
    mBpmRect            = IRECT(dotRect.R,         mRECT.T, dotRect.R + 140.f, mRECT.B);

    const IColor bpmColor = hostLive ? kInkFaint : kEspresso;
    IText srText (10.f, kInkFaint, kSans, EAlign::Near,   EVAlign::Middle);
    IText dotText(10.f, kInkFaint, kSans, EAlign::Center, EVAlign::Middle);
    IText bpmText(10.f, bpmColor,  kSans, EAlign::Near,   EVAlign::Middle);

    g.DrawText(srText,  srBuf,  srRect);
    g.DrawText(dotText, "·",    dotRect);
    g.DrawText(bpmText, bpmBuf, mBpmRect);

    // Redraw continuously so host tempo changes reflect live.
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!mPlug || mPlug->GetTempo() > 0.0) return;
    if (mBpmRect.Contains(x, y))
      PromptUserInput(mBpmRect);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const bool editable = mPlug && mPlug->GetTempo() <= 0.0
                       && mBpmRect.Contains(x, y);
    if (auto* ui = GetUI())
      ui->SetMouseCursor(editable ? ECursor::IBEAM : ECursor::ARROW);
  }

  void OnMouseOut() override
  { if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW); }

private:
  Premonition* mPlug = nullptr;
  IRECT mBpmRect;
};

// Status line — polls plug state each draw. Priority (top beats bottom):
//   Rendering → Playing → Rendered → Ready(filename+dur) → No sample
class StatusLineControl : public IControl
{
public:
  StatusLineControl(const IRECT& bounds, Premonition* plug)
    : IControl(bounds), mPlug(plug) {}

  void Draw(IGraphics& g) override
  {
    char buf[160];
    const char* msg = "Ready  ·  click Render or Preview";
    if (!mPlug) { /* fall through */ }
    else if (mPlug->IsRendering())
      msg = "Rendering…";
    else if (mPlug->IsPreviewing())
      msg = "Playing…";
    else if (mPlug->HasRendered())
      msg = "Rendered  ·  drag the pill into your DAW";
    else if (mPlug->Source().frames() > 0)
    {
      const auto& name = mPlug->SourceDisplayName();
      std::snprintf(buf, sizeof(buf), "Ready  ·  %s  (%.2fs)",
                    name.empty() ? "(unnamed)" : name.c_str(),
                    mPlug->SourceDurationSec());
      msg = buf;
    }
    else
      msg = "No sample loaded";

    g.DrawText(IText(11.f, kInkSoft, kSans, EAlign::Near, EVAlign::Middle),
               msg, mRECT);
    SetDirty(false);
  }

private:
  Premonition* mPlug = nullptr;
};

// Preset picker: single strip with hit regions for ◀ / NAME / ▶ / SAVE.
// Clicking NAME opens a popup of all presets. Clicking SAVE opens a text-
// entry prefilled with the next auto-name; committing writes a new preset.
class PresetPickerControl : public IControl
{
public:
  PresetPickerControl(const IRECT& bounds, Premonition* plug)
  : IControl(bounds), mPlug(plug)
  {
    SetTooltip("Click name to browse presets, ◀ ▶ to cycle, SAVE to store current settings");
  }

  void Draw(IGraphics& g) override
  {
    const IColor& accent = kTerracotta;
    const IRECT prev = LeftArrow();
    const IRECT next = RightArrow();
    const IRECT save = SaveHit();
    const IRECT name = NameHit();

    g.DrawText(IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Bottom),
               "PRESET", IRECT(mRECT.L + 14.f, mRECT.T, mRECT.L + 60.f, mRECT.MH()));

    g.DrawText(IText(13.f, accent, kSans, EAlign::Center, EVAlign::Middle),
               "<", prev);
    g.DrawText(IText(13.f, accent, kSans, EAlign::Center, EVAlign::Middle),
               ">", next);

    const char* label = "—";
    auto* cur = mPlug ? mPlug->Presets().Current() : nullptr;
    std::string nm = cur ? cur->name : std::string("—");
    g.DrawText(IText(13.f, kEspresso, kSans, EAlign::Center, EVAlign::Middle),
               nm.c_str(), name);

    g.DrawText(IText(10.f, kInkFaint, kSans, EAlign::Center, EVAlign::Middle),
               "SAVE", save);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!mPlug) return;
    auto& mgr = mPlug->Presets();
    if (LeftArrow().Contains(x, y))
    {
      if (mgr.Count() > 0)
      {
        mgr.SetCurrentIndex(mgr.CurrentIndex() - 1);
        if (auto* p = mgr.Current()) mPlug->ApplyPresetValues(p->values);
        SetDirty(false);
      }
    }
    else if (RightArrow().Contains(x, y))
    {
      if (mgr.Count() > 0)
      {
        mgr.SetCurrentIndex(mgr.CurrentIndex() + 1);
        if (auto* p = mgr.Current()) mPlug->ApplyPresetValues(p->values);
        SetDirty(false);
      }
    }
    else if (SaveHit().Contains(x, y))
    {
      std::string suggested = mgr.NextAutoName();
      // valIdx must be kNoValIdx (-1) — passing a sentinel like 9001 makes
      // iPlug2 call GetParamIdx() which indexes out of bounds and crashes
      // the host on text-entry commit.
      GetUI()->CreateTextEntry(*this, IText(12.f, kEspresso, kSans),
                               NameHit(), suggested.c_str(), kNoValIdx);
    }
    else if (NameHit().Contains(x, y))
    {
      // Menu must outlive this call: CreatePopupMenu is async on macOS and
      // captures the IPopupMenu by reference into a dispatch_async block.
      mPresetMenu.Clear();
      const auto& list = mgr.List();
      for (int i = 0; i < (int) list.size(); ++i)
      {
        auto* item = mPresetMenu.AddItem(list[i].name.c_str(), i);
        if (item && i == mgr.CurrentIndex()) item->SetChecked(true);
      }
      if (list.empty()) mPresetMenu.AddItem("(no presets)", 0, IPopupMenu::Item::kDisabled);
      GetUI()->CreatePopupMenu(*this, mPresetMenu, NameHit());
    }
  }

  void OnPopupMenuSelection(IPopupMenu* pMenu, int valIdx) override
  {
    if (!pMenu || !mPlug) return;
    int chosen = pMenu->GetChosenItemIdx();
    if (chosen < 0) return;
    auto& mgr = mPlug->Presets();
    if (chosen >= mgr.Count()) return;
    mgr.SetCurrentIndex(chosen);
    if (auto* p = mgr.Current()) mPlug->ApplyPresetValues(p->values);
    SetDirty(false);
  }

  void OnTextEntryCompletion(const char* str, int /*valIdx*/) override
  {
    if (!mPlug) return;
    std::string name = str ? str : "";
    while (!name.empty() && std::isspace((unsigned char) name.back())) name.pop_back();
    if (name.empty()) return;
    mPlug->Presets().Save(name, mPlug->CurrentPresetValues());
    SetDirty(false);
  }

private:
  IRECT LeftArrow()  const { return IRECT(mRECT.L + 60.f, mRECT.T, mRECT.L + 78.f, mRECT.B); }
  IRECT RightArrow() const { return IRECT(mRECT.R - 60.f, mRECT.T, mRECT.R - 42.f, mRECT.B); }
  IRECT NameHit()    const { return IRECT(mRECT.L + 78.f, mRECT.T, mRECT.R - 60.f, mRECT.B); }
  IRECT SaveHit()    const { return IRECT(mRECT.R - 40.f, mRECT.T, mRECT.R - 6.f,  mRECT.B); }

  Premonition* mPlug = nullptr;
  IPopupMenu mPresetMenu;
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
                                 { "Hall", "Plate", "Spring", "Room", "Convolution" });

  GetParam(kLength)->InitEnum("Length", kLen2,
                              { "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" });

  GetParam(kForward)->InitBool("Forward", false);
  GetParam(kNormalize)->InitBool("Normalize", true);
  GetParam(kMonoStereo)->InitBool("Mono", false);
  GetParam(kManualBPM)->InitDouble("Manual BPM", 120.0, 40.0, 300.0, 0.1, "bpm");

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
    const IRECT dropzone = L.GetFromTop(80.f);
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
    const IRECT wav = IRECT(L.L, qRow.B + 22.f, L.R, qRow.B + 134.f);
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

    pGraphics->AttachControl(new PreviewButtonControl(previewBtn,
      [this]() { TogglePreview(); },
      [this]() { return IsPreviewing(); },
      [this]() { return HasRendered(); }));

    // --- Result + dragout ---
    const IRECT rLabel = IRECT(L.L, actions.B + 18.f, L.R, actions.B + 30.f);
    const IRECT slotRect = IRECT(rLabel.R - 72.f, rLabel.T - 3.f,
                                 rLabel.R,        rLabel.B + 5.f);
    pGraphics->AttachControl(new ITextControl(
      IRECT(rLabel.L, rLabel.T, slotRect.L - 8.f, rLabel.B),
      "RESULT  ·  DRAG INTO YOUR DAW",
      IText(10.f, kInkFaint, kSans, EAlign::Near, EVAlign::Middle)));
    pGraphics->AttachControl(new SlotToggleControl(slotRect,
      [this]() { return ActiveSlot(); },
      [this](int s) {
        SetActiveSlot(s);
        if (auto* ui = GetUI()) ui->SetAllControlsDirty();
      },
      [this](int s) { return SlotHasRender(s); }));

    const IRECT dragout = IRECT(L.L, rLabel.B + 8.f, L.R, rLabel.B + 64.f);
    pGraphics->AttachControl(new DragoutControl(dragout, this));

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

    // IR slot — only visible when the Convolution algorithm is selected.
    // Sits between the toggles and the preset row; initial visibility is
    // synced from the current kAlgorithm value after attachment.
    const IRECT irSlot = IRECT(R.L, toggles.B + 10.f, R.R, toggles.B + 44.f);
    auto* irCtl = new IRSlotControl(irSlot,
      [this](const char* path) {
        if (LoadIRFile(path))
          if (auto* ui = GetUI()) ui->SetAllControlsDirty();
      },
      [this]() -> const std::string& { return IRDisplayName(); },
      [this]() { return HasIR(); },
      [this]() {
        return (mIRSampleRate > 0.f && !mIR.L.empty())
                 ? static_cast<float>(mIR.L.size()) / mIRSampleRate : 0.f;
      });
    pGraphics->AttachControl(irCtl);
    RegisterIRSlot(irCtl);
    irCtl->Hide(GetParam(kAlgorithm)->Int() != kAlgoConvolution);

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
    pGraphics->AttachControl(new PresetPickerControl(preset, this));

    // ============== FOOTER ==============
    const IRECT finset = footer.GetPadded(-28.f, 0.f, -28.f, 0.f);
    pGraphics->AttachControl(new FooterInfoControl(finset, this));
    pGraphics->AttachControl(new ITextControl(
      finset, "v" PLUG_VERSION_STR,
      IText(10.f, kInkFaint, kSans, EAlign::Far, EVAlign::Middle)));
  };
#endif

  mPresetStore.Init();
}

#if IPLUG_EDITOR
premonition::PresetValues Premonition::CurrentPresetValues() const
{
  using namespace premonition;
  PresetValues v;
  v.stretch    = GetParam(kStretch)->Value();
  v.size       = GetParam(kSize)->Value();
  v.decay      = GetParam(kDecay)->Value();
  v.mix        = GetParam(kMix)->Value();
  v.algorithm  = GetParam(kAlgorithm)->Int();
  v.length     = GetParam(kLength)->Int();
  v.forward    = GetParam(kForward)->Bool();
  v.normalize  = GetParam(kNormalize)->Bool();
  v.monoStereo = GetParam(kMonoStereo)->Bool();
  return v;
}

void Premonition::ApplyPresetValues(const premonition::PresetValues& v)
{
  using namespace premonition;
  auto push = [this](int idx, double raw) {
    GetParam(idx)->Set(raw);
    const double norm = GetParam(idx)->GetNormalized();
    BeginInformHostOfParamChangeFromUI(idx);
    SendParameterValueFromUI(idx, norm);            // host + OnParamChangeUI
    SendParameterValueFromDelegate(idx, norm, true); // refreshes UI controls
    EndInformHostOfParamChangeFromUI(idx);
  };
  push(kStretch,    v.stretch);
  push(kSize,       v.size);
  push(kDecay,      v.decay);
  push(kMix,        v.mix);
  push(kAlgorithm,  v.algorithm);
  push(kLength,     v.length);
  push(kForward,    v.forward ? 1.0 : 0.0);
  push(kNormalize,  v.normalize ? 1.0 : 0.0);
  push(kMonoStereo, v.monoStereo ? 1.0 : 0.0);
}
#endif

#if IPLUG_DSP
void Premonition::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();

  if (mPreviewPlaying.load(std::memory_order_acquire))
  {
    std::unique_lock<std::mutex> lk(mRenderedMutex, std::try_to_lock);
    if (!lk.owns_lock())
    {
      for (int s = 0; s < nFrames; ++s)
        for (int c = 0; c < nChans; ++c) outputs[c][s] = 0;
      return;
    }

    const auto& active = ActiveRendered();
    const int64_t total = static_cast<int64_t>(active.frames());
    int64_t pos = mPreviewPos.load(std::memory_order_relaxed);
    if (total <= 0)
    {
      mPreviewPlaying.store(false, std::memory_order_release);
      for (int s = 0; s < nFrames; ++s)
        for (int c = 0; c < nChans; ++c) outputs[c][s] = 0;
      return;
    }

    const float* L = active.L.data();
    const float* R = active.R.empty() ? L : active.R.data();
    for (int s = 0; s < nFrames; ++s)
    {
      if (pos >= total)
      {
        for (int c = 0; c < nChans; ++c) outputs[c][s] = 0;
        continue;
      }
      const sample l = static_cast<sample>(L[pos]);
      const sample r = static_cast<sample>(R[pos]);
      for (int c = 0; c < nChans; ++c)
        outputs[c][s] = (c == 0) ? l : (c == 1 ? r : l);
      ++pos;
    }
    mPreviewPos.store(pos, std::memory_order_relaxed);
    if (pos >= total)
      mPreviewPlaying.store(false, std::memory_order_release);
    return;
  }

  // Offline renderer — realtime path is pass-through when not previewing.
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
  cfg.bpm          = GetTempo() > 0.0 ? GetTempo()
                                      : GetParam(kManualBPM)->Value();
  cfg.beatsPerBar  = 4;
  cfg.forward      = GetParam(kForward)->Bool();
  cfg.normalize    = GetParam(kNormalize)->Bool();
  cfg.monoOutput   = GetParam(kMonoStereo)->Bool();
  cfg.algorithm    = GetParam(kAlgorithm)->Int();
  if (cfg.algorithm == kAlgoConvolution && !mIR.L.empty())
  {
    cfg.ir           = &mIR;
    cfg.irSampleRate = mIRSampleRate;
  }

  mRendering.store(true, std::memory_order_release);
  dsp::StereoBuffer out = dsp::renderRiser(source, sourceSampleRate, cfg);
  {
    std::lock_guard<std::mutex> lk(mRenderedMutex);
    mPreviewPlaying.store(false, std::memory_order_release);
    mPreviewPos.store(0, std::memory_order_relaxed);
    ActiveRendered() = std::move(out);
  }
  mRendering.store(false, std::memory_order_release);
  return ActiveRendered();
}

void Premonition::SetActiveSlot(int slot)
{
  if (slot != kSlotA && slot != kSlotB) return;
  if (mActiveSlot.load(std::memory_order_acquire) == slot) return;
  // Stop preview before swapping — pos refers to the old slot's buffer.
  std::lock_guard<std::mutex> lk(mRenderedMutex);
  mPreviewPlaying.store(false, std::memory_order_release);
  mPreviewPos.store(0, std::memory_order_relaxed);
  mActiveSlot.store(slot, std::memory_order_release);
}

bool Premonition::TogglePreview()
{
  if (mPreviewPlaying.load(std::memory_order_acquire))
  {
    mPreviewPlaying.store(false, std::memory_order_release);
    return false;
  }
  if (ActiveRendered().frames() == 0) return false;
  mPreviewPos.store(0, std::memory_order_relaxed);
  mPreviewPlaying.store(true, std::memory_order_release);
  return true;
}

bool Premonition::LoadSourceFile(const char* path)
{
  dsp::StereoBuffer buf;
  float rate = 0.f;
  if (!dsp::loadAudioFile(path, buf, rate)) return false;
  mSource = std::move(buf);
  mSourceSampleRate = rate;
  mSourceDisplayName = basename_(path);
  return true;
}

bool Premonition::LoadIRFile(const char* path)
{
  dsp::StereoBuffer buf;
  float rate = 0.f;
  if (!dsp::loadAudioFile(path, buf, rate)) return false;
  mIR = std::move(buf);
  mIRSampleRate = rate;
  mIRDisplayName = basename_(path);
  return true;
}

void Premonition::RegisterIRSlot(IControl* c) { mIRSlotCtl = c; }

void Premonition::OnParamChangeUI(int paramIdx, EParamSource /*source*/)
{
  if (paramIdx == kAlgorithm && mIRSlotCtl)
  {
    const bool showIR = GetParam(kAlgorithm)->Int() == kAlgoConvolution;
    mIRSlotCtl->Hide(!showIR);
    mIRSlotCtl->SetDirty(false);
  }
}

std::string Premonition::ExportRenderedToTempWav()
{
  const dsp::StereoBuffer& ren = Rendered();
  if (ren.frames() == 0) return {};

  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  // Stem: source filename (sans extension), fall back to "render".
  std::string stem = mSourceDisplayName;
  if (auto dot = stem.find_last_of('.'); dot != std::string::npos)
    stem = stem.substr(0, dot);
  if (stem.empty()) stem = "render";

  char fname[160];
  std::snprintf(fname, sizeof(fname), "Premonition-%s-%c-%lld.wav",
                stem.c_str(), ActiveSlot() == kSlotA ? 'A' : 'B',
                static_cast<long long>(ms));

  std::filesystem::path path = std::filesystem::temp_directory_path() / fname;
  // Render buffer is produced at the source file's SR (see RenderRiserFromSource),
  // not the host SR — writing the host SR here would pitch/speed-shift the file.
  const float sr = mSourceSampleRate > 0.f ? mSourceSampleRate
                                           : static_cast<float>(GetSampleRate());
  if (!dsp::writeWav32f(path.string(), ren, sr > 0.f ? sr : 44100.f))
    return {};

  mTempFiles.push_back(path.string());
  return path.string();
}
#endif

Premonition::~Premonition()
{
  for (const auto& p : mTempFiles)
  {
    std::error_code ec;
    std::filesystem::remove(p, ec);
  }
}
