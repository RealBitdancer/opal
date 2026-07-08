# Third-Party Licenses

Project code is MIT. See LICENSE. That covers the first-party code: the C API, resampling, pan
helper, example player, build system, and the thin miniaudio implementation TU. The project
bundles a few third party components listed below. They carry their own licenses.

## Opal OPL3 emulator (`src/opal.c`, `include/opal/opal.h`)

- **Original core:** Shayde / Reality (Reality Adlib Tracker 2). Public domain per [PUBLIC_DOMAIN_PROOF.txt](PUBLIC_DOMAIN_PROOF.txt).
- **This port:** Copyright (c) 2026 Bitdancer. C11 from OpenMPT `soundlib/opal.h`. C API,
  resampler, pan, rhythm, timers, OPL2 waveform select, and accuracy work. MIT. See LICENSE.
- **Lookup tables** (`LogSinTable`, `ExpTable`): numeric values from decapped Yamaha OPL2/OPL3 chip
  ROM. They are hardware constants, not original expressive code. The same values appear in many
  emulators (MAME, Nuked-OPL3, ymfm, etc.) because they come from the silicon.
- **Accuracy:** stereo output matches the YMF262 reference behavior used by established emulators
  such as [Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3). No third-party emulator source is
  included in this repository or linked into the `opal` library.
- **Other references:** public YMF262 documentation and ymfm (behavior only, no code copied).

## miniaudio (`examples/common/miniaudio/include/miniaudio/miniaudio.h`)

Copyright David Reid, https://miniaud.io . Dual public domain or MIT-0. Bundled as is. See end of miniaudio.h for text.

## HSC decoder (`examples/player/src/format_hsc.c`)

Original MIT code. Listed for attribution only. HSC logic follows the format. Uses AdPlug ChscPlayer (LGPL) as reference only. No AdPlug code copied.

## Demo music (`music/*.hsc`)

Seven public-domain HSC tracks from HSCDEMO3.EXE (1992) by [Hannes Seifert](https://de.wikipedia.org/wiki/Hannes_Seifert), creator of the HSC
format and the HSC AdLib Composer. The demo's title screen declares it a "Public Domain Version",
but only these seven are Seifert's own, the ones the demo itself marks `independent` or
`public domain`. The demo's other tracks are game soundtracks copyrighted to their publishers
(`MAX Design`, `Neo Software`) and are deliberately not bundled. The per-track evidence and the
archive.org Public Domain Mark 1.0 record are in
[music/PUBLIC_DOMAIN_PROOF.txt](music/PUBLIC_DOMAIN_PROOF.txt). See also [music/README.md](music/README.md).
