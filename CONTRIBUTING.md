# Contributing

Contributions are welcome. The project is small and intends to stay that way, so the best
contributions are the ones that make it more correct rather than merely larger.

## What fits

Accuracy fixes to the emulation core are the most valuable thing you can send. The core's
whole reason to exist is matching real YMF262 behavior, so a fix should come with evidence.
A register sequence, a comparison against hardware or an established emulator, or a failing
song are all good evidence. An opinion that it sounds better is not.

New player formats are welcome and deliberately cheap to add. Each format lives behind the
small interface in `examples/player/src/player_format.h`. Add one `format_*.c`, register it
in the player, and document any limitation the format forces on you.

Ports, build fixes, and documentation corrections are welcome too. If the CI matrix goes
green on Windows, Linux, macOS, Android, iOS, and the web preset, you have not broken the
world in any way we test for.

## Building and testing

The README covers building. Every preset in CMakePresets.json is expected to configure and
build cleanly. GitHub Actions builds them on every push to `main` (and when you run a
workflow by hand). Pull requests do not get that matrix until they land on main, so run the
preset closest to your platform before opening a pull request.

## Style

The code follows a few firm conventions. Allman braces. A brace on every control body, with
no one-line bodies. `.clang-format` enforces the formatting, so run it and the argument is
over. Comment only what the code cannot say itself. A comment that narrates the line below
it will be asked to leave.

Naming keeps the public-domain core readable against upstream. The public API, the `Opal`
type, its members, and the core's internal `Type_Method` helpers in `src/opal.c` keep those
names. New code outside that core (player formats, tools, examples) uses PascalCase types,
camelCase functions, and SCREAMING_CASE macros.

## Licensing

The project's own code is MIT and your contributions are accepted under the same terms. The
emulation core's public-domain provenance and the bundled third-party components are
documented in THIRD_PARTY_LICENSES.md. Do not add dependencies with copyleft obligations,
and do not add music without provenance. The `music/` folder is picky about that for good
reason, as music/PUBLIC_DOMAIN_PROOF.txt explains.
