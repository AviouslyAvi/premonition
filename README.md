# Premonition

**Offline reverse-reverb riser generator. Drop a sample in, drag a tempo-synced riser out.**

Premonition takes any sample you feed it, applies a reverb, reverses the result, and fits the output to an exact bar length synced to your host tempo — so the riser lands perfectly on the downbeat where your hit plays.

Built as a VST3 / AU / CLAP / AAX plugin on iPlug2.

---

## Status

v1 in development. See [PREMONITION_BRIEF.md](PREMONITION_BRIEF.md) for the frozen product spec.

## Signal chain

```
[source] → crop → stretch → reverb → reverse → fit-to-bar → normalize → riser
```

All offline. No realtime DAW latency.

## Parameters

Start, End, Stretch, Size, Decay, Mix, Algorithm, Length, Forward, Normalize, Mono/Stereo.

## Building

```bash
cmake -B build -G Xcode       # macOS
cmake --build build
```

Requires CMake 3.25+ and a C++17 compiler.

## Dependencies

| | License |
|---|---|
| [iPlug2](https://github.com/iPlug2/iPlug2) | zlib-like |
| [signalsmith-stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) | MIT |
| [Catch2](https://github.com/catchorg/Catch2) (tests only) | BSL-1.0 |

All commercial-friendly. See [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).

## License

Premonition itself is proprietary. © 2026 AVIOUS. All rights reserved.
