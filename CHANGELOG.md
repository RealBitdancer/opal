# Changelog

All notable changes to this project are documented here.

## [1.0.1] - 2026-07-08

### Fixed

- Two operator channels ignored the connection bit (C0 bit 0) and always played serial FM.
  Additive instruments are additive again.
- Enabling four operator mode via register 0x104, or writing C0 on the primary channel of a
  pair, never installed the operator routing. Only a C0 write on the secondary channel did.
- Four operator connection modes 2 and 3 used wrong operator topologies. All four modes now
  match the YMF262 algorithms.
- Disabling four operator mode left both channels of the pair wired as halves of the old
  four operator channel.
- Note select (register 08 bit 6) picked the wrong F-Number bit for the key scale number,
  which skewed KSR and KSL rates. The value is also latched on frequency writes now, so
  changing NTS no longer rescales notes already sounding.
- An A0 write to a four operator primary recomputed the secondary key scale number from the
  secondary octave instead of inheriting the primary value.
- OPL3 waveforms 4 to 7 were folded back to 0 to 3 the moment OPL3 mode switched off. The
  hardware keeps playing the stored waveform, so opal does too.

### Changed

- The status register now uses the hardware layout. Bit 7 is IRQ, bit 6 timer 1 overflow,
  bit 5 timer 2 overflow, and flags persist until a reset write to register 4 instead of
  clearing on read.
- The `Opal_Pan` comment described a channel number multiplied by 256. The function wants a
  plain channel index 0 to 17 and the comment now says so.
- The README no longer lists OPL2 waveform select as a feature. The enable bit is stored
  and ignored, since the YMF262 ignores it too.

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
