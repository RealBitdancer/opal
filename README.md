# opal

[![windows](https://github.com/RealBitdancer/opal/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/RealBitdancer/opal/actions/workflows/windows.yml)
[![linux](https://github.com/RealBitdancer/opal/actions/workflows/linux.yml/badge.svg?branch=main)](https://github.com/RealBitdancer/opal/actions/workflows/linux.yml)
[![macos](https://github.com/RealBitdancer/opal/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/RealBitdancer/opal/actions/workflows/macos.yml)
[![android](https://github.com/RealBitdancer/opal/actions/workflows/android.yml/badge.svg?branch=main)](https://github.com/RealBitdancer/opal/actions/workflows/android.yml)
[![ios](https://github.com/RealBitdancer/opal/actions/workflows/ios.yml/badge.svg?branch=main)](https://github.com/RealBitdancer/opal/actions/workflows/ios.yml)

A feature complete OPL2 and OPL3 FM synthesis core in C11 with a small API and a
permissive license.

**Hear it first:** the [web player](https://realbitdancer.github.io/opal/) runs the core
as WebAssembly in your browser and plays the bundled demo tracks. No build required.

## About

opal emulates the Yamaha YMF262, better known as the OPL3, together with its OPL2
predecessor. These are the chips that powered the AdLib and Sound Blaster cards, and
through them most of the music in DOS games and trackers. The core was written by Shayde
of Reality for Reality Adlib Tracker 2 and released into the public domain. What you find
here is a C11 port derived from the OpenMPT copy and wrapped in a small C API.

Feature complete means the full register set that affects audio. The core implements two
and four operator FM, all eight waveforms, the tremolo and vibrato LFOs, percussion mode
with its noise generator, OPL2 waveform select, and the timers with their status
register. CSW mode is decoded and then left inert. The real YMF262 ignores it, so opal
ignores it with equal care.

## Accuracy

Stereo output matches established YMF262 emulators, including
[Nuked-OPL3](https://github.com/nukeykt/Nuked-OPL3), sample for sample at the chip's
native rate of 49716 Hz. Waveforms, envelopes, LFOs, rhythm mode, two and four operator
FM, feedback, stereo routing, and buffered register timing all follow hardware behavior.
The right channel arrives one sample after the left because the YMF262 pipeline delays
it. Reproducing the quirk is cheaper than explaining its absence.

## Building

You need CMake 3.23 or newer and a C11 compiler. Presets exist for Windows (MSVC, Clang,
and MinGW in 32 and 64 bit), Linux (GCC), and macOS (Clang). All presets except the
Visual Studio ones use Ninja Multi-Config and expect ninja on PATH. The full list lives
in CMakePresets.json.

    cmake --preset default
    cmake --build --preset debug

On Linux or macOS use the matching presets:

    cmake --preset linux            # or: macos
    cmake --build --preset linux-debug

The result is a static library. Link opal and include include/opal/opal.h. The example
player builds by default. Set -DOPAL_BUILD_EXAMPLES=OFF if you do not want it.

Three cross presets cover platforms that cannot run the console player. The `android`
preset needs the NDK and `ANDROID_NDK_HOME`, and `ios` needs a macOS host. Both build the
library alone. The `web` preset needs an active Emscripten SDK and builds the browser
player described below.

To install and consume opal from another CMake project:

    cmake --install build/msvc-x64 --prefix /path/to/prefix
    # In your CMakeLists.txt:
    # find_package(opal 1.0 REQUIRED)
    # target_link_libraries(myapp PRIVATE opal::opal)

## Usage

The public interface is eight functions, declared in `include/opal/opal.h`. Drive it the
way you would drive the real chip. Write registers, then read back stereo samples at
whatever rate you asked for.

```c
#include <opal/opal.h>

Opal chip;
Opal_Init(&chip, 48000);
Opal_Port(&chip, 0x20, 0x21);
/* set registers, key on, etc */

int16_t left, right;
Opal_Sample(&chip, &left, &right);
```

Internally the chip always runs at its native 49716 Hz and resamples to the rate passed
to `Opal_Init` or `Opal_SetSampleRate`. `Opal_Pan` adds per channel panning on top of the
chip's own left and right enables. `Opal_Read` returns the status register, which timer
polling code will want.

## Example player

The bundled `player` streams OPL music to the default audio device through miniaudio. It
reads DRO version 1, HSC, and IMF or WLF files.

```sh
player song.dro
player song.hsc
player song.imf
player --rate 700 song.wlf
```

A few limitations are worth knowing. DRO files must be version 1, meaning the header word
at offset 8 is zero. HSC has no signature, so the player goes by the `.hsc` extension.
IMF and WLF are also recognized by extension, and the replay rate defaults to 560 Hz for
`.imf` and 700 Hz for `.wlf`. That rate is a property of the game rather than of the
file, so the `--rate` flag exists for the exceptions.

The player core is format agnostic. Each format sits behind the small interface in
`player_format.h`, and support for a new one is a single `format_*.c` away.

### Demo music

The `music` folder holds seven public domain HSC tracks from HSCDEMO3.EXE, a demo
[Hannes Seifert](https://de.wikipedia.org/wiki/Hannes_Seifert) released in 1992. Only his
own tracks are included, the ones the demo itself
marks independent or public domain. The track list, the attribution, and the reason the
game soundtracks stayed behind are all in [music/README.md](music/README.md).

Point the player at any of them and quote the path, since the names contain spaces:

    player "music/01 Shoot 'em up.hsc"

### Web player

The `examples/web` directory wraps the same core and the same format decoders into a
WebAssembly build with a browser front end, styled after the trackers this music was
written in. The `web` preset builds it into player.js and player.wasm, and a GitHub
Actions workflow deploys it together with the demo tracks to GitHub Pages on every push
to main. The live copy is at
[realbitdancer.github.io/opal](https://realbitdancer.github.io/opal/).

## Coding style

The sources follow project conventions. Allman braces, a brace on every control body, and
no one line bodies. `.clang-format` enforces all of it.

Identifier naming is the one exception. The API, the `Opal` type, and its members keep
the names from the public domain source rather than being forced into camelCase. The
original API stays intact and diffs against upstream stay readable.

## License

The project's own code is MIT, which covers the C API, the resampler, the pan helper, the
example player, and the build system. See [LICENSE](LICENSE). The Opal emulation core is
public domain, courtesy of Shayde of Reality. miniaudio is public domain or MIT-0 at your
option. Nothing in the library depends on LGPL code.

Full attribution and the provenance of the ROM tables are in
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Release history is in
[CHANGELOG.md](CHANGELOG.md).
