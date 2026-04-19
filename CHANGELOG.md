# Changelog

All notable changes to Premonition.

## [Unreleased]

### Added
- Initial project scaffolding (iPlug2 template, VST3/AU/CLAP/AAX formats)
- Clean-room DSP: Schroeder/Moorer algorithmic reverb, buffer reverse, time-stretch wrapper (signalsmith-stretch), tempo-synced length fitter
- Offline pipeline orchestrator (crop → stretch → reverb → reverse → fit-to-bar → normalize)
- Parameter set per frozen v1 spec: Start, End, Stretch, Size, Decay, Mix, Algorithm, Length, Forward, Normalize, Mono/Stereo
- Placeholder GUI (knob row — full drop-bin / waveform / queue UI deferred)
