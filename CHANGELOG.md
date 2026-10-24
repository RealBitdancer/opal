# Changelog

All notable changes to this project are documented here.

## [3.0.0] - 2026-10-24

The emulation core and the HSC decoder are rewritten from scratch. The struct layout
changes. The eight API functions keep their names and signatures.

### Changed

- The core in `src/opal.c` is new. Only the output resampler still follows the
  public-domain Opal.
- `OpalOperator`, `OpalChannel`, and `Opal` have a new internal layout. The visualizer
  fields keep their names and meaning: `envelopeStage`, `egOut`, `key`, `modSource`, and
  `out` in each operator, `opSlot`, `pairIndex`, `outSource`, `chanType`, `leftPan`, and
  `rightPan` in each channel, and the `op` and `chan` arrays. `OPAL_VERSION_MAJOR` is 3.
- The HSC decoder's instrument upload, volume handling, level effects, and start-up
  sequence are rewritten. Their register writes match the previous decoder.

### Fixed

- Clearing NEW silenced channels 9-17 and split four-operator pairs. Both now stay as set,
  as on the chip.
- The compatibility-mode limits on waveforms and speakers applied at output time, so
  changing NEW changed what was already set. They now apply when E0h-F5h and C0h-C8h are
  written.
- Enabling a four-operator pair left the secondary channel on its old frequency, and its
  own frequency writes were lost. The pair now takes the primary's frequency at once, and
  the secondary's values return when the pair is split.
- A masked timer still set its status flag. It now sets nothing, and setting a mask bit
  clears a flag already set.
- DOSBox writes DRO registers 00h-04h and 104h without the escape and opens captures with a
  register dump. The loader read those bytes as delays and switches, so captures started
  with a long pause or lost their 105h write and played as OPL2. It now recognizes both.
- OPL3 DRO captures set NEW before playback when their opening block enables it.
- DRO delays carry their rounding remainder, so songs of short delays no longer run fast.
- A DRO length field near 4 GB could make the loader read past the end of the buffer.
- Dual OPL2 DRO captures put the second chip's writes on top of the first. They now play in
  OPL3 mode with both chips on both speakers.
- `opalInit` and `opalSetSampleRate` overflowed on rates above INT32_MAX - 49716. Those now
  clamp.
- `opalSetSampleRate` discarded the write queue, so a queued key-off could be lost. It now
  changes only the rate.
- `opalPan` values above 127 drove the left channel to full scale. They now clamp to 127.
- The installed static library held only link-time optimization bytecode. It is now built
  without it.
- HSC reported its loop point one row early.
- More than 1024 zero-delay events in a row shifted the rest of the song in the example and
  web players.
- Web demo: the end of a song was cut off, background tabs stopped after one track, a
  failed download kept the old track playing, and Stop left part of the old track queued.
  `webSongEnd` is gone, and `webLoad` rejects null pointers.
- Web demo: with `?play=1`, the first tap now runs the iOS silent-switch workaround.
- `player --rate` with no value now says the value is missing.

### Build

- `.clang-format` sets `InsertBraces`.

### Documentation

- README describes compatibility mode as it behaves now, and the one known difference
  from Nuked-OPL3. It no longer calls the core a public-domain port, or
  `opalWriteRegBuffered` chip-accurate. The install example asks for 3.0.
- README, THIRD_PARTY_LICENSES.md, BINDINGS.md, and CONTRIBUTING.md state where the code
  comes from. Language bindings are at `3.0.0-1`.
- The install steps build and install the Release configuration.

## [2.0.3] - 2026-08-22

The synthesis core is the same as 2.0.2. This release is the example player and the web
demo. `OPAL_VERSION_PATCH` is 3 so the tag, the CMake package, and the header agree.

### Fixed

- HSC replay in `format_hsc.c` followed AdPlug's ChscPlayer. It now follows HSC-Tracker
  HSCOBJ 1.4 / HSCPLAY. Unmatched effects including `0x3n`, `0x5n`, `0x6n`, and `0xDn`
  set speed to `n + 1`. OPL register 08 is written `0x40` (NOTESEL), not `0x80` (CSW).
  Notes pack fnum, block, and key-on into one 16-bit word on A0/B0. `0x1n`/`0x2n` slide
  that word's low byte by `n + 1` with no carry into the block. `0xAn`/`0xBn`/`0xCn`
  replace the level byte and drop the instrument KSL bits. An instrument-set with slide
  0 leaves the key off. Setting the same patch again is a no-op, so the channel is not
  keyed off. Out-of-range pattern indexes play empty rows. The 51st order slot is played.
  The KSL xor applies to the carrier always, and to the modulator only in additive mode.
  Instrument slide is the high nibble of byte 11.
- DRO register writes and IMF delay 0 return zero frames from `step`. The example player
  and the web callback then spun in `while (wait == 0)` on the audio thread. Each callback
  now caps consecutive zero-wait steps and yields, so a register dump cannot hang playback.

### Changed

- An HSC order jump or end marker is a loop point, not the end of playback. The example
  CLI player keeps the same file going, so tracks such as Rocky Margarine run until you
  stop them. The web demo treats that edge as the end of the current file and starts the
  next track, wrapping the list, until Stop. DRO and IMF still end when the stream runs
  out.

### Documentation

- README points at sehnsucht as the full-featured terminal player that uses the opal
  emulator, not the example C formats. The web demo plays the bundled tracks in order.
  Language bindings stay at `2.0.2-1`, because the C API did not change.

## [2.0.2] - 2026-08-13

### Added

- `OPAL_VERSION_MAJOR`, `OPAL_VERSION_MINOR`, `OPAL_VERSION_PATCH`, and the packed
  `OPAL_VERSION` (`major * 10000 + minor * 100 + patch`) in `opal.h`, so a consumer can
  test the release at compile time without going through CMake.

### Fixed

- The DRO v1 loader required a 24-byte file even for the 21-byte early DOSBox header.
  Length versus file size now picks 21 or 24 bytes, with the old padding test as a
  fallback. HSC effect `5x` no longer writes OPL AM or vibrato depth for nibbles 6 and 7.
- Disabling 4-op mode no longer clears the primary channel's `pairIndex`. That field is
  the physical partner (or `OPAL_CH_NONE` for channels that cannot pair). `chanType` says
  whether 4-op is active. `opalInit` now sets `egOut` to 0x1FF so a snapshot before the
  first sample is silent, matching `envelopeLevel`. The example player and web loader use
  the native OPL rate if the audio device reports a zero sample rate.
- NEW (register 105h bit 0) was stored and then ignored. Bank 1 writes still took
  effect while it was clear, channels 9-17 still mixed, and C0 stereo bits written in
  OPL2 mode were forced to both speakers and discarded. The YMF262 ignores A1 except
  for 105h itself. The extra channels stay silent. CHA and CHB are stored as written
  and apply only after NEW is set. Waveforms 4-7 stay stored and play as 0-3 while NEW
  is clear. Four-operator pairs wire as two-operator until NEW comes back. C0 resets to
  zero, so enabling OPL3 without writing C0 is silent.
- A masked timer overflow left FT1 and FT2 clear. Code that polls the flags and ignores
  IRQ never saw the tick. Overflow now always sets the timer flag. IRQ (bit 7) is set
  only when that timer is unmasked. Writing register 4 with ST1 or ST2 already set also
  reloaded the counter. The load is the 0 to 1 edge, so a mask-only write no longer
  restarts a running timer.

### Changed

- `opalPan` names its channel argument `channel`. It is still a channel index 0 to 17,
  or 0 to 8 with bit 8 set for the second bank.

### Documentation

- README treats NEW as a live mode bit. Bank 1, waveforms 4-7, CHA/CHB, and
  four-operator pairing apply only while it is set.
- The README `find_package` example asks for 2.0, which is what `SameMajorVersion`
  actually accepts from this release. IMF type 0 versus type 1 is listed with the other
  player limits. `OPAL_ENVELOPE_STAGE_OFF` is documented as unused by the core. Bindings
  listed in BINDINGS.md are opal-zig and opal-rust at `2.0.2-1`. Security support is 2.x
  and current main. GitHub Actions use `actions/checkout@v7`. The Android matrix includes
  x86. First-party targets compile with warnings as errors.

## [2.0.1] - 2026-07-25

### Fixed

- The output resampler in `opalSample` multiplied the 16.16 interpolation fraction by the
  step between two consecutive chip samples in `int`. Both factors reach 65535, so the
  product overflows a 32-bit signed integer whenever a loud track swings more than half
  of full scale in one chip sample, which OPL3 material with both banks and rhythm mode
  does routinely. The result is undefined behavior: a garbage sample under a plain
  compiler, and an immediate abort under UBSan or any toolchain that traps on signed
  overflow. The multiply is now 64 bit, in the new `opalInterpolate` helper. Output for
  every non-overflowing sample is unchanged, so this is bit-identical to 2.0.0 wherever
  2.0.0 was defined at all.

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
