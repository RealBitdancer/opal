# Changelog

All notable changes to this project are documented here.

## [1.0.0] - 2026-07-07

### Added

- C11 static library with an eight-function public API (`Opal_Init`, `Opal_SetSampleRate`,
  `Opal_Port`, `Opal_PortBuffered`, `Opal_FlushWriteBuf`, `Opal_Pan`, `Opal_Sample`, `Opal_Read`)
- Full OPL2 and OPL3 stereo synthesis with two and four operator FM, eight waveforms, tremolo
  and vibrato LFOs, rhythm mode, timers, and the status register
- Linear resampling from the native 49716 Hz to any output rate
- Example `player` that streams DRO, HSC, and IMF or WLF files to the audio device via miniaudio
- Seven public-domain HSC demo tracks under `music/` from HSCDEMO3.EXE (1992) by Hannes Seifert,
  limited to the tracks the demo itself marks `independent` or `public domain`
- WebAssembly player (`examples/web`) that compiles the core and format decoders with Emscripten
  behind a tracker-styled browser front end, built by the `web` CMake preset
- Audio unlock on iOS so the ring switch no longer mutes Web Audio, done by starting the device
  inside the tap and playing a silent looping media element
- CMake install rules and a `find_package(opal)` package config
- CMake presets for Windows (MSVC, Clang, and MinGW in 32 and 64 bit), Linux (GCC), and macOS
  (Clang), plus library-only cross presets for Android (NDK), iOS, and WebAssembly
- GitHub Actions workflows that build every preset on Windows, Linux, macOS, Android, iOS, and
  the web on each push to main, deploy the web player to GitHub Pages, and drive the
  per-platform status badges in the README

### Notes

- Stereo output matches established YMF262 reference emulators at chip rate
- DRO playback handles version 1 only, meaning `DBRAWOPL` with a version word of zero
- The CSW register bit is decoded but left inert, as on real YMF262 hardware
