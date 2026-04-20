// render_ir.cpp — Offline impulse-response renderer for Premonition.
// Standalone C++17. No external deps. Produces four stereo 32-bit float WAVs
// at 44.1 kHz in Premonition/resources/ir/:
//   hall.wav, plate.wav, spring.wav, room.wav
//
// Build (via tools/CMakeLists.txt):
//   cmake --build <build-dir> --target render_ir
// Run from project root so the output directory resolves correctly.
//
// DSP references:
//   - Dattorro, "Effect Design Part 1: Reverberator and Other Filters" (JAES, 1997)
//   - Jot & Chaigne, "Digital Delay Networks for Designing Artificial Reverberators" (1991)
//   - Schroeder, "Natural Sounding Artificial Reverberation" (1962)
//   - Van Duyne & Smith, "Dispersive allpass for spring reverb" (ICMC 1994)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kSampleRate = 44100.0f;

// ---------- Primitives ------------------------------------------------------

class DelayLine
{
public:
  void setSize(std::size_t n) { buf_.assign(n == 0 ? 1 : n, 0.0f); cur_ = 0; }
  float tickAndRead(float x, std::size_t d) noexcept
  {
    const auto n = buf_.size();
    const std::size_t r = (cur_ + n - (d % n)) % n;
    const float y = buf_[r];
    buf_[cur_] = x;
    cur_ = (cur_ + 1) % n;
    return y;
  }
  float peek(std::size_t d) const noexcept
  {
    const auto n = buf_.size();
    return buf_[(cur_ + n - (d % n)) % n];
  }
  // Arbitrary read/write for Dattorro-style tap structures.
  float read(std::size_t d) const noexcept { return peek(d); }
  void write(float x) noexcept { buf_[cur_] = x; cur_ = (cur_ + 1) % buf_.size(); }
  std::size_t size() const noexcept { return buf_.size(); }
private:
  std::vector<float> buf_;
  std::size_t cur_ = 0;
};

class LPFComb
{
public:
  void prepare(int d, float fb, float damp)
  {
    line_.setSize(std::max(1, d));
    fb_ = fb; damp_ = std::clamp(damp, 0.0f, 0.999f); lpf_ = 0.0f;
  }
  float process(float x) noexcept
  {
    const float delayed = line_.peek(line_.size() - 1);
    lpf_ = delayed * (1.0f - damp_) + lpf_ * damp_;
    line_.tickAndRead(x + lpf_ * fb_, line_.size() - 1);
    return delayed;
  }
private:
  DelayLine line_; float fb_ = 0.0f, damp_ = 0.0f, lpf_ = 0.0f;
};

class SchroederAllpass
{
public:
  void prepare(int d, float g) { line_.setSize(std::max(1, d)); g_ = g; }
  float process(float x) noexcept
  {
    const std::size_t D = line_.size() - 1;
    const float delayed = line_.peek(D);
    const float store = x + delayed * g_;
    line_.tickAndRead(store, D);
    return -g_ * store + delayed;
  }
private:
  DelayLine line_; float g_ = 0.5f;
};

// ---------- WAV writer (float32 stereo) ------------------------------------

void writeWavFloat32(const std::string& path,
                     const std::vector<float>& interleaved,
                     int channels, int sampleRate)
{
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return; }
  const uint32_t numSamples = static_cast<uint32_t>(interleaved.size());
  const uint32_t byteRate = sampleRate * channels * 4u;
  const uint16_t blockAlign = static_cast<uint16_t>(channels * 4);
  const uint32_t dataBytes = numSamples * 4u;
  const uint32_t chunkSize = 36 + dataBytes;

  auto wu32 = [&](uint32_t v){ std::fwrite(&v, 4, 1, f); };
  auto wu16 = [&](uint16_t v){ std::fwrite(&v, 2, 1, f); };

  std::fwrite("RIFF", 1, 4, f); wu32(chunkSize); std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); wu32(16);
  wu16(3);                                   // format = IEEE float
  wu16(static_cast<uint16_t>(channels));
  wu32(static_cast<uint32_t>(sampleRate));
  wu32(byteRate); wu16(blockAlign); wu16(32); // bits/sample
  std::fwrite("data", 1, 4, f); wu32(dataBytes);
  std::fwrite(interleaved.data(), 4, numSamples, f);
  std::fclose(f);
}

// ---------- Helpers ---------------------------------------------------------

// Peak-normalize stereo channels jointly to the given dBFS.
void normalizePeak(std::vector<float>& L, std::vector<float>& R, float targetDb)
{
  float peak = 0.0f;
  for (float v : L) peak = std::max(peak, std::fabs(v));
  for (float v : R) peak = std::max(peak, std::fabs(v));
  if (peak <= 1e-12f) return;
  const float target = std::pow(10.0f, targetDb / 20.0f);
  const float g = target / peak;
  for (auto& v : L) v *= g;
  for (auto& v : R) v *= g;
}

std::vector<float> interleaveLR(const std::vector<float>& L, const std::vector<float>& R)
{
  std::vector<float> out(L.size() * 2);
  for (std::size_t i = 0; i < L.size(); ++i) { out[2*i] = L[i]; out[2*i+1] = R[i]; }
  return out;
}

// Hadamard 4x4 mixing: y = H * x * 0.5 (energy-preserving).
inline void hadamard4(std::array<float,4>& v)
{
  const float a = v[0] + v[1], b = v[2] + v[3];
  const float c = v[0] - v[1], d = v[2] - v[3];
  const float s = 0.5f;
  v[0] = s * (a + b);
  v[1] = s * (c + d);
  v[2] = s * (a - b);
  v[3] = s * (c - d);
}

// ---------- HALL: 4-channel FDN --------------------------------------------

// Two FDNs (one per stereo side) with prime-offset delays for decorrelation.
std::pair<std::vector<float>, std::vector<float>>
renderHall(float rt60, float damping)
{
  const std::size_t N = static_cast<std::size_t>((rt60 * 1.2f) * kSampleRate);
  std::vector<float> L(N, 0.0f), R(N, 0.0f);

  const std::array<int, 4> dL = { 1753, 2371, 3119, 3863 };
  const std::array<int, 4> dR = { 1831, 2459, 3209, 3967 }; // prime-shifted

  auto runFDN = [&](const std::array<int,4>& dly, std::vector<float>& out)
  {
    std::array<DelayLine, 4> lines;
    std::array<float, 4> lpf = {0,0,0,0};
    for (int i = 0; i < 4; ++i) lines[i].setSize(dly[i] + 8);

    // Per-line feedback gain for -60 dB at rt60.
    std::array<float, 4> g;
    for (int i = 0; i < 4; ++i)
      g[i] = std::pow(10.0f, -3.0f * dly[i] / (kSampleRate * rt60));

    for (std::size_t n = 0; n < N; ++n)
    {
      std::array<float, 4> s;
      for (int i = 0; i < 4; ++i) s[i] = lines[i].peek(dly[i] - 1);
      // Hall: subtle damping (warm but still bright).
      for (int i = 0; i < 4; ++i) {
        lpf[i] = s[i] * (1.0f - damping) + lpf[i] * damping;
        s[i] = lpf[i] * g[i];
      }
      std::array<float, 4> m = s;
      hadamard4(m);
      const float in = (n == 0) ? 1.0f : 0.0f;
      for (int i = 0; i < 4; ++i)
        lines[i].tickAndRead(in * 0.5f + m[i], dly[i] - 1);
      out[n] = 0.5f * (s[0] + s[1] + s[2] + s[3]);
    }
  };
  runFDN(dL, L);
  runFDN(dR, R);
  return {L, R};
}

// ---------- PLATE: Dattorro 1997 -------------------------------------------
// Simplified Dattorro tank: input diffusion (4 APs), then two parallel loops
// cross-feeding through modulated APs + delays + damping. We omit modulation
// (offline IR = static system) but otherwise follow delay-length conventions.
std::pair<std::vector<float>, std::vector<float>>
renderPlate(float rt60, float damping)
{
  const std::size_t N = static_cast<std::size_t>((rt60 * 1.2f) * kSampleRate);
  std::vector<float> L(N, 0.0f), R(N, 0.0f);

  // Input diffusers (Dattorro's values, scaled minimally).
  SchroederAllpass ap1, ap2, ap3, ap4;
  ap1.prepare(142, 0.75f);
  ap2.prepare(107, 0.75f);
  ap3.prepare(379, 0.625f);
  ap4.prepare(277, 0.625f);

  // Tank: two loops. Each: AP -> delay -> lowpass -> *decay -> AP -> delay
  // We treat each loop's "output" as split into L and R taps at different
  // positions for stereo width.
  SchroederAllpass apA1, apA2, apB1, apB2;
  apA1.prepare(672, 0.7f);
  apA2.prepare(1800, 0.5f);
  apB1.prepare(908, 0.7f);
  apB2.prepare(2656, 0.5f);

  DelayLine delA1, delA2, delB1, delB2;
  delA1.setSize(4453 + 8);
  delA2.setSize(3720 + 8);
  delB1.setSize(4217 + 8);
  delB2.setSize(3163 + 8);

  // Derive loop decay from rt60: total delay around each loop ~ 10k samples.
  const float loopLen = 10000.0f;
  const float decay = std::pow(10.0f, -3.0f * loopLen / (kSampleRate * rt60));
  const float decayG = std::clamp(decay, 0.0f, 0.95f);

  float lpA = 0.0f, lpB = 0.0f;
  const float d = std::clamp(damping, 0.0f, 0.95f);

  // Cross-feed state.
  float xA = 0.0f, xB = 0.0f;

  for (std::size_t n = 0; n < N; ++n)
  {
    float in = (n == 0) ? 1.0f : 0.0f;
    // Input diffusion.
    float y = ap4.process(ap3.process(ap2.process(ap1.process(in))));

    // Tank A: in + cross feedback from B.
    float a = apA1.process(y + xB);
    float aDel = delA1.peek(delA1.size() - 1);
    delA1.tickAndRead(a, delA1.size() - 1);
    lpA = aDel * (1.0f - d) + lpA * d;
    float aNext = apA2.process(lpA * decayG);
    float aOut = delA2.peek(delA2.size() - 1);
    delA2.tickAndRead(aNext, delA2.size() - 1);
    xA = aOut * decayG;

    // Tank B.
    float b = apB1.process(y + xA);
    float bDel = delB1.peek(delB1.size() - 1);
    delB1.tickAndRead(b, delB1.size() - 1);
    lpB = bDel * (1.0f - d) + lpB * d;
    float bNext = apB2.process(lpB * decayG);
    float bOut = delB2.peek(delB2.size() - 1);
    delB2.tickAndRead(bNext, delB2.size() - 1);
    xB = bOut * decayG;

    // Stereo taps: Dattorro-style — read from multiple positions.
    const float tapL = delA1.peek(266) + delA2.peek(2974)
                     - delB1.peek(353) + delB2.peek(1913);
    const float tapR = delB1.peek(353) + delB2.peek(2974)
                     - delA1.peek(266) + delA2.peek(1913);
    L[n] = 0.5f * tapL;
    R[n] = 0.5f * tapR;
  }
  return {L, R};
}

// ---------- SPRING: dispersive allpass chain -------------------------------
// Approximation: cascaded Schroeder APs (short delays) simulate the spring's
// dispersive "chirp" — group delay varies with frequency. We add a long comb
// tail for the fundamental resonance and a handful of short combs for the
// metallic character. True Van Duyne allpass polynomials omitted for brevity;
// this sounds reasonably spring-like but is not a physical model.
std::pair<std::vector<float>, std::vector<float>>
renderSpring(float rt60)
{
  const std::size_t N = static_cast<std::size_t>((rt60 * 1.2f) * kSampleRate);
  std::vector<float> L(N, 0.0f), R(N, 0.0f);

  auto runChannel = [&](const std::array<int, 7>& apDelays,
                        const std::array<int, 3>& combDelays,
                        std::vector<float>& out)
  {
    std::array<SchroederAllpass, 7> aps;
    for (int i = 0; i < 7; ++i) aps[i].prepare(apDelays[i], 0.72f);

    std::array<LPFComb, 3> combs;
    for (int i = 0; i < 3; ++i) {
      const float g = std::pow(10.0f, -3.0f * combDelays[i] / (kSampleRate * rt60));
      combs[i].prepare(combDelays[i], std::min(g, 0.93f), 0.15f); // bright
    }

    for (std::size_t n = 0; n < N; ++n) {
      float x = (n == 0) ? 1.0f : 0.0f;
      // Dispersive chain: many short APs in series = frequency-dependent
      // group delay → spring chirp.
      for (auto& a : aps) x = a.process(x);
      // Parallel bright combs supply the metallic resonance.
      float comb = 0.0f;
      for (auto& c : combs) comb += c.process(x);
      out[n] = x * 0.5f + comb * 0.25f;
    }
  };

  // L & R use different prime-offset delay sets → decorrelation.
  const std::array<int, 7> apL = { 43, 67, 89, 131, 173, 211, 257 };
  const std::array<int, 3> cbL = { 523, 727, 991 };
  const std::array<int, 7> apR = { 47, 71, 97, 139, 181, 223, 269 };
  const std::array<int, 3> cbR = { 547, 751, 1019 };

  runChannel(apL, cbL, L);
  runChannel(apR, cbR, R);
  return {L, R};
}

// ---------- ROOM: Schroeder with strong damping ----------------------------

std::pair<std::vector<float>, std::vector<float>>
renderRoom(float rt60, float damping)
{
  const std::size_t N = static_cast<std::size_t>((rt60 * 1.2f) * kSampleRate);
  std::vector<float> L(N, 0.0f), R(N, 0.0f);

  auto runChannel = [&](const std::array<int, 4>& cb,
                        const std::array<int, 2>& ap,
                        std::vector<float>& out)
  {
    std::array<LPFComb, 4> combs;
    for (int i = 0; i < 4; ++i) {
      const float g = std::pow(10.0f, -3.0f * cb[i] / (kSampleRate * rt60));
      combs[i].prepare(cb[i], std::min(g, 0.92f), damping);
    }
    std::array<SchroederAllpass, 2> aps;
    for (int i = 0; i < 2; ++i) aps[i].prepare(ap[i], 0.5f);

    for (std::size_t n = 0; n < N; ++n) {
      float x = (n == 0) ? 1.0f : 0.0f;
      float s = 0.0f;
      for (auto& c : combs) s += c.process(x);
      s *= 0.25f;
      for (auto& a : aps) s = a.process(s);
      out[n] = s;
    }
  };

  const std::array<int, 4> cbL = { 743, 877, 1049, 1213 };
  const std::array<int, 2> apL = { 337, 223 };
  const std::array<int, 4> cbR = { 769, 907, 1087, 1259 };
  const std::array<int, 2> apR = { 353, 239 };

  runChannel(cbL, apL, L);
  runChannel(cbR, apR, R);
  return {L, R};
}

// ---------- Driver ----------------------------------------------------------

void renderAndWrite(const std::string& outDir,
                    const std::string& name,
                    std::vector<float> L,
                    std::vector<float> R)
{
  normalizePeak(L, R, -1.0f);
  const std::string path = outDir + "/" + name;
  auto inter = interleaveLR(L, R);
  writeWavFloat32(path, inter, 2, static_cast<int>(kSampleRate));
  float peak = 0.0f;
  for (float v : inter) peak = std::max(peak, std::fabs(v));
  std::printf("  wrote %s  (%zu samples/ch, %.2f s, peak %.3f = %.2f dBFS)\n",
              path.c_str(), L.size(),
              L.size() / kSampleRate, peak,
              20.0f * std::log10(std::max(peak, 1e-9f)));
}

} // namespace

int main()
{
  const std::string outDir = "Premonition/resources/ir";
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);

  std::printf("Rendering IRs to %s/\n", outDir.c_str());

  {
    auto [L, R] = renderHall(4.0f, 0.18f);  // bright hall, 4 s
    renderAndWrite(outDir, "hall.wav", std::move(L), std::move(R));
  }
  {
    auto [L, R] = renderPlate(2.5f, 0.25f); // bright plate, 2.5 s
    renderAndWrite(outDir, "plate.wav", std::move(L), std::move(R));
  }
  {
    auto [L, R] = renderSpring(2.0f);       // boingy spring, 2 s
    renderAndWrite(outDir, "spring.wav", std::move(L), std::move(R));
  }
  {
    auto [L, R] = renderRoom(0.8f, 0.55f);  // warm small room
    renderAndWrite(outDir, "room.wav", std::move(L), std::move(R));
  }
  std::printf("Done.\n");
  return 0;
}
