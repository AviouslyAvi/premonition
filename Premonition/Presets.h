#pragma once

// JSON-backed preset store for Premonition.
//
// Schema (flat, knob-state only — start/end/source/bpm/reverbType excluded so
// presets are source-agnostic AND reverb-type-agnostic per v2 design):
//
//   {
//     "name": "Default",
//     "version": 2,
//     "params": {
//       "stretch": 1.0, "size": 0.6, "decay": 3.0, "mix": 1.0,
//       "length": 5, "mode": 0,
//       "normalize": true, "monoStereo": false
//     }
//   }
//
// Location: ~/Library/Application Support/Premonition/presets/*.json
// Files are human-editable and shareable by design.

#include "Parameters.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace premonition {

struct PresetValues
{
  double stretch = ranges::kStretchDefault;
  double size = 0.5;
  double decay = ranges::kDecayDefaultSec;
  double mix = 1.0;
  int length = kLen2;
  int mode = kModeNatural;
  bool normalize = true;
  bool monoStereo = false;
};

struct Preset
{
  std::string name;
  std::string path; // empty for in-memory/not yet saved
  PresetValues values;
};

class PresetManager
{
public:
  // Ensures preset dir exists, seeds 4 factory presets if empty, rescans.
  void Init()
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(Directory(), ec);
    if (fs::is_empty(Directory(), ec)) SeedFactory();
    Reload();
  }

  void Reload()
  {
    namespace fs = std::filesystem;
    mPresets.clear();
    std::error_code ec;
    if (!fs::exists(Directory(), ec)) return;
    std::vector<fs::path> files;
    for (auto& entry : fs::directory_iterator(Directory(), ec))
    {
      if (!entry.is_regular_file()) continue;
      if (entry.path().extension() != ".json") continue;
      files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (auto& p : files)
    {
      Preset pr;
      if (LoadFile(p.string(), pr))
        mPresets.push_back(std::move(pr));
    }
    if (mCurrent >= (int) mPresets.size()) mCurrent = mPresets.empty() ? -1 : 0;
    if (mCurrent < 0 && !mPresets.empty()) mCurrent = 0;
  }

  const std::vector<Preset>& List() const { return mPresets; }
  int Count() const { return (int) mPresets.size(); }
  int CurrentIndex() const { return mCurrent; }
  void SetCurrentIndex(int i)
  {
    if (mPresets.empty()) { mCurrent = -1; return; }
    mCurrent = (i % (int) mPresets.size() + (int) mPresets.size())
               % (int) mPresets.size();
  }
  const Preset* Current() const
  {
    return (mCurrent >= 0 && mCurrent < (int) mPresets.size())
      ? &mPresets[mCurrent] : nullptr;
  }

  // Save values under `name`. Overwrites if a preset with that name exists.
  // Returns true on success; writes to disk, reloads, selects the saved one.
  bool Save(const std::string& name, const PresetValues& values)
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(Directory(), ec);

    std::string safe = SanitizeFilename(name);
    if (safe.empty()) safe = "Untitled";
    fs::path file = fs::path(Directory()) / (safe + ".json");

    std::ofstream f(file);
    if (!f) return false;
    WriteJson(f, name, values);
    f.close();

    Reload();
    for (int i = 0; i < (int) mPresets.size(); ++i)
      if (mPresets[i].name == name) { mCurrent = i; break; }
    return true;
  }

  // Suggests "User 1", "User 2", ... skipping names already in use.
  std::string NextAutoName() const
  {
    for (int n = 1; n < 1000; ++n)
    {
      std::string candidate = "User " + std::to_string(n);
      bool taken = false;
      for (auto& p : mPresets) if (p.name == candidate) { taken = true; break; }
      if (!taken) return candidate;
    }
    return "User";
  }

  std::string Directory() const
  {
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/tmp";
    return base + "/Library/Application Support/Premonition/presets";
  }

private:
  std::vector<Preset> mPresets;
  int mCurrent = -1;

  static std::string SanitizeFilename(const std::string& s)
  {
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
      if (c == '/' || c == '\\' || c == ':' || c == '\0') continue;
      out.push_back(c);
    }
    // trim whitespace
    while (!out.empty() && std::isspace((unsigned char) out.back())) out.pop_back();
    size_t start = 0;
    while (start < out.size() && std::isspace((unsigned char) out[start])) ++start;
    return out.substr(start);
  }

  static void WriteJson(std::ostream& f, const std::string& name,
                        const PresetValues& v)
  {
    f << "{\n";
    f << "  \"name\": \"" << EscapeString(name) << "\",\n";
    f << "  \"version\": 2,\n";
    f << "  \"params\": {\n";
    f << "    \"stretch\": "    << v.stretch    << ",\n";
    f << "    \"size\": "       << v.size       << ",\n";
    f << "    \"decay\": "      << v.decay      << ",\n";
    f << "    \"mix\": "        << v.mix        << ",\n";
    f << "    \"length\": "     << v.length     << ",\n";
    f << "    \"mode\": "       << v.mode       << ",\n";
    f << "    \"normalize\": "  << (v.normalize ? "true" : "false")  << ",\n";
    f << "    \"monoStereo\": " << (v.monoStereo ? "true" : "false") << "\n";
    f << "  }\n";
    f << "}\n";
  }

  static std::string EscapeString(const std::string& s)
  {
    std::string out; out.reserve(s.size());
    for (char c : s)
    {
      if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
      else if (c == '\n') out += "\\n";
      else out.push_back(c);
    }
    return out;
  }

  // Minimal hand-rolled JSON reader. Schema is flat and fixed; no need for
  // a full parser. Tolerates whitespace, returns false on malformed input.
  static bool LoadFile(const std::string& path, Preset& out)
  {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    std::string s = ss.str();

    out.path = path;
    out.name = ExtractString(s, "name");
    if (out.name.empty())
    {
      // Fall back to filename stem so a missing name field doesn't nuke display.
      out.name = std::filesystem::path(path).stem().string();
    }
    out.values.stretch    = ExtractNumber(s, "stretch",    out.values.stretch);
    out.values.size       = ExtractNumber(s, "size",       out.values.size);
    out.values.decay      = ExtractNumber(s, "decay",      out.values.decay);
    out.values.mix        = ExtractNumber(s, "mix",        out.values.mix);
    out.values.length     = (int) ExtractNumber(s, "length", out.values.length);
    out.values.mode       = (int) ExtractNumber(s, "mode",   out.values.mode);
    out.values.normalize  = ExtractBool(s, "normalize",  out.values.normalize);
    out.values.monoStereo = ExtractBool(s, "monoStereo", out.values.monoStereo);
    return true;
  }

  static size_t FindKey(const std::string& s, const std::string& key)
  {
    std::string needle = "\"" + key + "\"";
    return s.find(needle);
  }

  static std::string ExtractString(const std::string& s, const std::string& key)
  {
    size_t k = FindKey(s, key);
    if (k == std::string::npos) return "";
    size_t colon = s.find(':', k);
    if (colon == std::string::npos) return "";
    size_t q1 = s.find('"', colon);
    if (q1 == std::string::npos) return "";
    size_t q2 = q1 + 1;
    std::string out;
    while (q2 < s.size() && s[q2] != '"')
    {
      if (s[q2] == '\\' && q2 + 1 < s.size()) { out.push_back(s[q2 + 1]); q2 += 2; }
      else out.push_back(s[q2++]);
    }
    return out;
  }

  static double ExtractNumber(const std::string& s, const std::string& key,
                              double fallback)
  {
    size_t k = FindKey(s, key);
    if (k == std::string::npos) return fallback;
    size_t colon = s.find(':', k);
    if (colon == std::string::npos) return fallback;
    size_t i = colon + 1;
    while (i < s.size() && std::isspace((unsigned char) s[i])) ++i;
    if (i >= s.size()) return fallback;
    try { return std::stod(s.substr(i)); }
    catch (...) { return fallback; }
  }

  static bool ExtractBool(const std::string& s, const std::string& key,
                          bool fallback)
  {
    size_t k = FindKey(s, key);
    if (k == std::string::npos) return fallback;
    size_t colon = s.find(':', k);
    if (colon == std::string::npos) return fallback;
    size_t i = colon + 1;
    while (i < s.size() && std::isspace((unsigned char) s[i])) ++i;
    if (s.compare(i, 4, "true") == 0) return true;
    if (s.compare(i, 5, "false") == 0) return false;
    return fallback;
  }

  // Seeds knob-only factory presets. v2 schema decouples Reverb Type from
  // preset state (see file header) — a preset captures knob feel, not which
  // IR/algo is loaded.
  void SeedFactory()
  {
    Write("Default", PresetValues{ 1.0, 0.50, 2.0, 1.0, kLen2, kModeNatural, true, false });
    Write("Lush",    PresetValues{ 1.0, 0.70, 4.0, 1.0, kLen2, kModeNatural, true, false });
    Write("Short",   PresetValues{ 1.0, 0.30, 1.2, 1.0, kLen1, kModeNatural, true, false });
  }

  void Write(const std::string& name, const PresetValues& v)
  {
    namespace fs = std::filesystem;
    fs::path file = fs::path(Directory()) / (SanitizeFilename(name) + ".json");
    std::ofstream f(file);
    if (!f) return;
    WriteJson(f, name, v);
  }
};

} // namespace premonition
