# Changelog

All notable changes to this project are documented here.

## [2.0.0] - 2026-07-18

This release breaks the public API deliberately. Every name now follows one consistent
convention, and the index-based layout removes the structure copy problem that plagued
the original public-domain source and every previous opal release, where an initialized
chip could never be copied or moved.

### Added

- Public constants for the two state fields that previously spoke in magic numbers:
  `OPAL_ENVELOPE_STAGE_*` (OFF, ATTACK, DECAY, SUSTAIN, RELEASE) for
  `OpalOperator::envelopeStage` and `OPAL_CHANNEL_TYPE_*` (2OP, 4OP, 4OP2, DRUM) for
  `OpalChannel::chanType`. Visualizers no longer hardcode the values.

### Changed

- The whole library follows one naming convention: functions are `opal` plus PascalCase
  (`opalWriteReg`), types are `Opal` plus PascalCase with the struct tag matching the
  typedef (`OpalChannel`, no `_t` suffix), struct members are camelCase (`egOut`,
  `opSlot`), and macros stay `OPAL_` SCREAMING_CASE. The old mixed style kept the
  public-domain source readable against upstream. That concern is retired, and the
  internal helpers in `src/opal.c` follow the same convention now.
- Three functions are renamed for accuracy as well as style: `Opal_Port` is now
  `opalWriteReg` (the two-port address/data dance was never emulated, a register is
  written directly), `Opal_PortBuffered` is now `opalWriteRegBuffered`, and `Opal_Read`
  is now `opalReadStatus` (the status register is the only readable thing). The other
  five change style only: `opalInit`, `opalSetSampleRate`, `opalFlushWriteBuf`,
  `opalPan`, and `opalSample`. Signatures and behavior are unchanged.
- The `Opal` struct is position independent. Every internal cross reference is an index
  instead of a pointer: `OpalChannel::Op[]` became `opSlot[]` (operator indices),
  `ChannelPair` became `pairIndex`, `OutPtr[]` became `outSource[]` (operator indices
  whose outputs sum into the channel), and `OpalOperator::Mod` became `modSource`
  (an operator index, `OPAL_MOD_OWN_FB`, or `OPAL_OP_NONE`). Copying, assigning, or
  memcpying an instance yields a fully working chip, so a plain copy is a save state.
  Rendered output is bit-identical to 1.0.2.
- This is a breaking layout change for consumers that read `Opal` internals, such as
  visualizers. The new index fields make the active FM routing readable as data, which
  pointers never were.
- The boolean flags (`noteSel`, `tremoloEnable`, and friends) are C11 `bool` instead of
  the `opal_bool` int typedef.
- The constants moved from an enum to defines: `OpalOPL3SampleRate` is now
  `OPAL_OPL3_SAMPLE_RATE`, `OpalNumChannels` is `OPAL_CHANNEL_COUNT`, and
  `OpalNumOperators` is `OPAL_OPERATOR_COUNT`. The header guard is
  `OPAL_OPAL_H_INCLUDED`.

### Removed

- The `Master` and `Chan` back pointers on operators, the `Master` and pointer-based
  pair and routing fields on channels, and the `ZeroMod` field on the chip. The no-copy
  rule and every warning about it are gone with them.
- `opal_bool`, `OPAL_TRUE`, and `OPAL_FALSE`.

### Documentation

- README and CONTRIBUTING describe the single naming convention. The section explaining
  why the core kept its upstream names is gone, since the reason went with the names.
- BINDINGS.md lists binding versions as the bound opal release plus a packaging
  revision (`2.0.0-1` is the first opal-zig release binding opal `2.0.0`).

## [1.0.2] - 2026-07-18

### Added

- The web player accepts a `?play=1` query parameter and starts the first track on load, or
  on the first tap where the browser demands a gesture. The README's live demo links use it.
- BINDINGS.md lists third-party language bindings, starting with
  [opal-zig](https://github.com/RealBitdancer/opal-zig).

### Fixed

- The example player's DRO decoder assumed the 24-byte v1 header. The earliest DOSBox builds
  wrote a single-byte hardware type (21-byte header), so those files lost their first commands
  or desynced. Nonzero bytes in the padding now identify them (AdPlug's heuristic).
- The web player's nine channel meters no longer run a cosmetic CSS animation. They track the
  live OPL envelopes (`EgOut` on each channel's modulator and carrier) via a new `webLevels`
  export, so bars rise and fall with the music being played.

### Documentation

- `opal.h` documents the channel-to-operator slot layout (modulator `Op[slot]`, carrier
  `Op[slot + 3]`, with the `ch_slot` table inlined) so state snapshot consumers such as
  visualizers stop guessing a sequential pair layout that is wrong for every channel but 0.
- `opal.h` documents the operator fields most useful to visualizers: `EnvelopeStage` values,
  `EgOut` as total attenuation in 0.1875 dB steps, and `Key` as a key-on source bitmask.
- `opal.h` warns that `Opal_Init` wires internal cross pointers, so an instance must be
  initialized in place and never copied or relocated afterwards.
- README and CONTRIBUTING cover DRO early headers, `Opal` pointer stability, buffered ports,
  CI on main only, and core versus player naming.

## [1.0.1] - 2026-07-08

### Fixed

- Two-operator channels ignored the connection bit (C0 bit 0) and always played serial FM.
  Additive instruments are additive again.
- Enabling four-operator mode via register 0x104, or writing C0 on the primary channel of a
  pair, never installed the operator routing. Only a C0 write on the secondary channel did.
- Four-operator connection modes 2 and 3 used wrong operator topologies. All four modes now
  match the YMF262 algorithms.
- Disabling four-operator mode left both channels of the pair wired as halves of the old
  four-operator channel.
- Note select (register 08 bit 6) picked the wrong F-Number bit for the key scale number,
  which skewed KSR and KSL rates. The value is also latched on frequency writes now, so
  changing NTS no longer rescales notes already sounding.
- An A0 write to a four-operator primary recomputed the secondary key scale number from the
  secondary octave instead of inheriting the primary value.
- OPL3 waveforms 4 to 7 were folded back to 0 to 3 the moment OPL3 mode switched off. The
  hardware keeps playing the stored waveform, so opal does too.

### Changed

- The status register now uses the hardware layout. Bit 7 is IRQ, bit 6 timer 1 overflow,
  bit 5 timer 2 overflow, and flags persist until a reset write to register 4 instead of
  clearing on read.
- The `Opal_Pan` comment described a channel number multiplied by 256. The function accepts a
  plain channel index 0 to 17, or 0 to 8 with bit 8 set for the second bank, and the comment
  now says so.
- The README no longer lists OPL2 waveform select as a feature. The enable bit is stored
  and ignored, since the YMF262 ignores it too.

## [1.0.0] - 2026-07-07

### Added

- C11 static library with an eight-function public API (`Opal_Init`, `Opal_SetSampleRate`,
  `Opal_Port`, `Opal_PortBuffered`, `Opal_FlushWriteBuf`, `Opal_Pan`, `Opal_Sample`, `Opal_Read`)
- Full OPL2 and OPL3 stereo synthesis with two- and four-operator FM, eight waveforms, tremolo
  and vibrato LFOs, rhythm mode, timers, and the status register
- Linear resampling from the native 49716 Hz to any output rate
- Example `player` that streams DRO, HSC, and IMF or WLF files to the audio device via miniaudio
- Seven public-domain HSC demo tracks under `music/` from HSCDEMO3.EXE (1992) by Hannes Seifert,
  limited to the tracks the demo itself marks `independent` or `public domain`
- WebAssembly player (`examples/web`) that compiles the core and format decoders with Emscripten
  behind a browser front end styled after classic trackers, built by the `web` CMake preset
- Audio unlock on iOS so the ring switch no longer mutes Web Audio, done by starting the device
  inside the tap and playing a silent looping media element
- CMake install rules and a `find_package(opal)` package config
- CMake presets for Windows (MSVC, Clang, and MinGW in 32- and 64-bit), Linux (GCC), and macOS
  (Clang), plus library-only cross presets for Android (NDK), iOS, and WebAssembly
- GitHub Actions workflows that build every preset on Windows, Linux, macOS, Android, iOS, and
  the web on each push to main, deploy the web player to GitHub Pages, and drive the
  per-platform status badges in the README

### Notes

- Stereo output matches established YMF262 reference emulators at chip rate
- DRO playback handles version 1 only, meaning `DBRAWOPL` with a version word of zero
- The CSW register bit is decoded but left inert, as on real YMF262 hardware
