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
| Triangle | Choose a source and station, or the next preset while the visualiser runs |
| L2 + Triangle | Previous preset (R2 + Triangle picks a random one) |
| Cross | Pause / play |
| Circle | Browse the local `music/` folder |
| L2 + Circle | Keep the playing radio track in `music/` |
| L1 / R1 | Previous / next local file (page up / down in lists) |
| Options | Switch the right panel between instruments and scopes |
| L2 + Options | Automatic preset change every minute, on or off |
| Square | Visualiser: off, behind the interface, full screen |
| R2 | CRT scanlines on / off |
| D-pad left / right | Scroll through the channels |
| D-pad up / down | Volume (hold L2 to jump through the song) |

On the desktop build: `n` / `p` next and previous, `g` stations, space
pause, `o` file list, `s` scopes, `f` effects, `c` CRT, `b` next preset, `k` keep this track,
arrows for channels and volume, `h` help, Esc quits.

## The visualiser

Square turns on a MilkDrop visualiser. With the preset pack installed those
are the real presets from the last official MilkDrop release, run by
projectM: the same scripts and shaders, reacting to the module that is
playing. Without the pack the app falls back to its own effect, which works
the same way MilkDrop does (warp the previous frame, dim it, draw the
waveform over it) but with 24 built-in presets instead of 552.

The presets ship inside `PPSA01153.zip`, so an install has them straight
away. They are also attached to each release on their own as
`OliSePlayer-presets.zip`, for adding them to an install that has none.
`make presets` fetches them from the projectM project, and `make gl` ships
whatever ends up in `presets/`.

The pack carries projectM's own statement on their licence, which comes down
to this: almost no MilkDrop preset was released under a specific licence, and
after two decades of free circulation they are treated as public domain. A
preset author who objects can have theirs removed; see
`presets/PRESETS-LICENSE.md`.

## Where the music comes from

Triangle asks which archive to listen to first, then which station within it.
The two are kept apart because they are organised differently, and one merged
list of genres would not be honest about where a tune comes from.

**The Mod Archive** holds around 160.000 modules, sorted by genre. The player
reads the normal website pages, the same ones a browser shows, because the XML
API needs a key. A genre station picks a random page from that genre and a
random module on it; featured and top rated pick from the current charts.

**Modland** is the larger archive and is organised by format and artist. The
player uses its playlists, which are plain text files of direct links: the
favourites of some 850 modules, a chiptune selection, netlabel releases, and a
few musicdisks and demoparty compos. One request gets the list, a second gets
the tune, so it tends to be quicker than the Mod Archive route.

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
make presets     # download the MilkDrop preset pack (optional)
make             # dist/PPSA01153/ and dist/PPSA01153.zip
make gl          # the OpenGL build, needed for the visualiser
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

`make gl` needs two things the plain build does not:

- The [PS5 OpenGL SDK](https://github.com/blackbearreloaded/ps5-opengl),
  unpacked so that `deps/ps5-opengl/sdk/include/EGL` exists, or pointed at
  with `make gl PS5_OPENGL_PREFIX=/path/to/sdk`.
- projectM built for the PS5 in `deps/projectm/`, for the real MilkDrop
  presets. Without it the app uses its own visualiser instead, and `make gl`
  says nothing about it. `docs/decision-log.md` records how projectM was
  ported, including the three changes its build needed.

The OpenGL build is about 27 MB against 2.6 MB for the plain one: Mesa is
linked statically, and that is the price of running shaders at all.

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
