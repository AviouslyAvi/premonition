#include "AudioLoader.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreFoundation/CoreFoundation.h>

// stb_vorbis declarations (implementation lives in stb_vorbis_impl.c).
extern "C" int stb_vorbis_decode_filename(
  const char* filename, int* channels, int* sample_rate, short** output);

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace premonition { namespace dsp {

namespace {

std::string toLowerExt(const char* path)
{
  std::string s(path);
  auto dot = s.find_last_of('.');
  if (dot == std::string::npos) return "";
  std::string ext = s.substr(dot);
  for (auto& c : ext) c = static_cast<char>(std::tolower(c));
  return ext;
}

// Reads the entire file via ExtAudioFile. Handles WAV/AIFF/MP3/M4A/AAC/CAF.
bool loadViaExtAudioFile(const char* path, StereoBuffer& out, float& outRate)
{
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
    kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(path),
    static_cast<CFIndex>(std::strlen(path)),
    false);
  if (!url) return false;

  ExtAudioFileRef af = nullptr;
  OSStatus err = ExtAudioFileOpenURL(url, &af);
  CFRelease(url);
  if (err != noErr || !af) return false;

  AudioStreamBasicDescription srcFormat{};
  UInt32 size = sizeof(srcFormat);
  err = ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileDataFormat,
                                &size, &srcFormat);
  if (err != noErr) { ExtAudioFileDispose(af); return false; }

  UInt32 srcChannels = srcFormat.mChannelsPerFrame;
  if (srcChannels == 0) { ExtAudioFileDispose(af); return false; }
  UInt32 outChannels = (srcChannels >= 2) ? 2 : 1;

  AudioStreamBasicDescription clientFormat{};
  clientFormat.mSampleRate       = srcFormat.mSampleRate;
  clientFormat.mFormatID         = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags      = kAudioFormatFlagIsFloat
                                 | kAudioFormatFlagIsPacked
                                 | kAudioFormatFlagIsNonInterleaved;
  clientFormat.mChannelsPerFrame = outChannels;
  clientFormat.mBytesPerPacket   = sizeof(float);
  clientFormat.mBytesPerFrame    = sizeof(float);
  clientFormat.mFramesPerPacket  = 1;
  clientFormat.mBitsPerChannel   = 32;

  err = ExtAudioFileSetProperty(af, kExtAudioFileProperty_ClientDataFormat,
                                sizeof(clientFormat), &clientFormat);
  if (err != noErr) { ExtAudioFileDispose(af); return false; }

  SInt64 totalFrames = 0;
  size = sizeof(totalFrames);
  err = ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames,
                                &size, &totalFrames);
  if (err != noErr || totalFrames <= 0)
  { ExtAudioFileDispose(af); return false; }

  const std::size_t frames = static_cast<std::size_t>(totalFrames);
  out.L.assign(frames, 0.f);
  out.R.assign(frames, 0.f);

  // Variable-length AudioBufferList: reserve space for up to 2 buffers.
  struct { UInt32 n; AudioBuffer buffers[2]; } list{};
  list.n = outChannels;
  list.buffers[0].mNumberChannels = 1;
  list.buffers[0].mDataByteSize   = static_cast<UInt32>(frames * sizeof(float));
  list.buffers[0].mData           = out.L.data();
  if (outChannels == 2)
  {
    list.buffers[1].mNumberChannels = 1;
    list.buffers[1].mDataByteSize   = static_cast<UInt32>(frames * sizeof(float));
    list.buffers[1].mData           = out.R.data();
  }

  UInt32 framesToRead = static_cast<UInt32>(frames);
  err = ExtAudioFileRead(af, &framesToRead,
                         reinterpret_cast<AudioBufferList*>(&list));
  ExtAudioFileDispose(af);
  if (err != noErr) { out.L.clear(); out.R.clear(); return false; }

  // Mono → duplicate L into R.
  if (outChannels == 1)
  {
    out.R = out.L;
    out.isMono = true;
  }

  // ExtAudioFile may report fewer frames than advertised (common for VBR).
  if (framesToRead < frames)
  {
    out.L.resize(framesToRead);
    out.R.resize(framesToRead);
  }

  // Stereo file whose channels are bit-for-bit identical (e.g. mono-to-stereo
  // upmix). Treat it as mono so widening is applied.
  if (outChannels == 2 && !out.isMono)
    out.isMono = (out.L == out.R);

  outRate = static_cast<float>(clientFormat.mSampleRate);
  return true;
}

bool loadOgg(const char* path, StereoBuffer& out, float& outRate)
{
  int channels = 0;
  int sampleRate = 0;
  short* interleaved = nullptr;
  int sampleCount = stb_vorbis_decode_filename(
    path, &channels, &sampleRate, &interleaved);
  if (sampleCount <= 0 || channels <= 0 || !interleaved)
  {
    if (interleaved) std::free(interleaved);
    return false;
  }

  const std::size_t frames = static_cast<std::size_t>(sampleCount);
  out.L.resize(frames);
  out.R.resize(frames);

  constexpr float kInvShortMax = 1.f / 32768.f;
  if (channels == 1)
  {
    for (std::size_t i = 0; i < frames; ++i)
      out.L[i] = out.R[i] = interleaved[i] * kInvShortMax;
    out.isMono = true;
  }
  else
  {
    for (std::size_t i = 0; i < frames; ++i)
    {
      out.L[i] = interleaved[i * channels + 0] * kInvShortMax;
      out.R[i] = interleaved[i * channels + 1] * kInvShortMax;
    }
    out.isMono = (out.L == out.R);
  }

  std::free(interleaved);
  outRate = static_cast<float>(sampleRate);
  return true;
}

} // namespace

bool loadAudioFile(const char* path, StereoBuffer& out, float& outRate)
{
  if (!path) return false;
  out.L.clear();
  out.R.clear();

  const std::string ext = toLowerExt(path);
  if (ext == ".ogg") return loadOgg(path, out, outRate);
  // ExtAudioFile handles wav/aiff/aif/mp3/m4a/aac/caf.
  return loadViaExtAudioFile(path, out, outRate);
}

}} // namespace premonition::dsp
