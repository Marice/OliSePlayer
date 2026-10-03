# OliSe Player

Tracker music radio for the PS5, in FastTracker II style.

![logo](assets/logo.png)

OliSe Player grabs random MOD, XM, S3M and IT tunes from
[The Mod Archive](https://modarchive.org) and plays them one after the
other on your PS5. Pick a genre (chiptune, trance, jazz, whatever you feel
like), pick a format, or just let it run. While a tune plays you see the
pattern data scroll by, like in the old tracker days, with a starfield, a
chrome logo, a scroller, copper bars and VU meters around it.

You need a jailbroken PS5 with
[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus). Internet is
needed for The Mod Archive; without it the player plays the files from its
own `music/` folder.

## Installing

OliSe Player is a native PS5 app with title ID PPSA01153. The release zip
contains a `PPSA01153` folder with `eboot.bin` and `sce_sys`. Copy that
folder to `/data/homebrew/` on the console (FTP works fine) and
ShadowMountPlus adds it to your home screen like any other game. Local
modules go in `/data/homebrew/PPSA01153/music/`. Close the app with the PS
button like any other game.

## Controls

Press the touchpad for this list on screen.

| Button | What it does |
|---|---|
| R3 | Next track |
| L3 | Previous track |
| Triangle | Choose a station: random, featured, top rated, a format or a genre |
| Cross | Pause / play |
| Circle | Browse the local `music/` folder |
| L1 / R1 | Previous / next local file (page up / down in lists) |
| Options | Switch the right panel between instruments and scopes |
| Square | Effects: full, calm, off |
| R2 | CRT scanlines on / off |
| D-pad left / right | Scroll through the channels |
| D-pad up / down | Volume (hold L2 to jump through the song) |

On the desktop build: `n` / `p` next and previous, `g` stations, space
pause, `o` file list, `s` scopes, `f` effects, `c` CRT, arrows for channels
and volume, `h` help, Esc quits.

## Where the music comes from

All tunes come from The Mod Archive. The player uses the normal website
pages, the same ones you see in a browser, because the XML API needs a key.
A genre station picks a random page from that genre and a random module on
it. The featured and top rated stations pick from the current charts.

A track plays once and then the next one from the same station starts. The
last 20 tracks stay in memory, so L3 goes back without downloading again.
If the network drops, the player keeps trying with longer pauses and plays
local files in the meantime.

## Building it yourself

You need Linux or WSL with clang-18, lld-18, cmake, ninja (`pip install
ninja` is enough) and Python 3. The first build downloads the public PS5
Payload SDK and the PacBrew ports (about 350 MB) into `.deps/`.

```sh
make deps        # libxmp-lite, SDK and PacBrew
make             # dist/PPSA01153/ and dist/PPSA01153.zip
make ffpfsc      # also a compressed .ffpfsc image
make upload      # copy the folder to the PS5 over FTP (port 1337)
make native      # desktop version, plays files from music/
```

Releases are tagged with the `contentVersion` from `sce_sys/param.json`
(for example `01.000.001`); the same number is shown in the app.

The native title build uses the tooling from BlackBearReloaded's
[ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate):
it links with LLVM, converts the ELF to a PS5 module, signs it as a
development FSELF and adds a source-built `libc.prx`. See
`THIRD_PARTY_NOTICES.md` for all licenses.

`make assets` regenerates the logo and the icon with the Python scripts in
`tools/` (needs Pillow). `tools/netxm_test.c` tests the page parsing
without a network.

## Thanks

- The Mod Archive and everyone who uploaded music there
- [libxmp](https://github.com/libxmp/libxmp) for playing the modules
- [font8x8](https://github.com/dhepper/font8x8) by Daniel Hepper
- [ps5-payload-dev](https://github.com/ps5-payload-dev) for the toolchain and SDL2
- BlackBearReloaded for the native app tooling and the homebrew catalog
- Triton, for FastTracker II

## License

GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
