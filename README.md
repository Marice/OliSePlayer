# OliSe Player

Tracker music radio for the PS5, in FastTracker II style.

![logo](assets/logo.png)

OliSe Player grabs random MOD, XM, S3M and IT tunes from
[The Mod Archive](https://modarchive.org) and plays them one after the
other on your PS5. Pick a genre (chiptune, trance, jazz, whatever you feel
like), pick a format, or just let it run. While a tune plays you see the
pattern data scroll by, like in the old tracker days, with a starfield, a
chrome logo, a scroller, copper bars and VU meters around it.

You need a jailbroken PS5 with an ELF loader (etaHEN, elfldr or websrv).
Internet is needed for The Mod Archive. Without it the player plays the
files from its own `music/` folder.

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
| L2 + R2 | Quit |

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

Local files go in `music/` next to `eboot.elf`. On the PS5 that is
`/data/homebrew/OliSePlayer/music/`.

## Installing on the PS5

Download the zip from the releases page and copy the `OliSePlayer` folder
to `/data/homebrew/` on your console (FTP works fine). It then shows up in
the [websrv](https://github.com/ps5-payload-dev/websrv) homebrew menu.

## Building it yourself

You need the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk)
with the [SDL2 port](https://github.com/ps5-payload-dev/SDL) installed, and
cmake for libxmp.

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make deps        # builds libxmp-lite into deps/
make             # eboot.elf
make homebrew    # dist/OliSePlayer/ and a zip
```

`make native` builds a desktop version (needs libsdl2-dev) that plays
files from `music/`. The Mod Archive part only works on the PS5.

`make assets` regenerates the logo and the icon with the Python scripts in
`tools/` (needs Pillow). `tools/netxm_test.c` tests the page parsing
without a network.

## Thanks

- The Mod Archive and everyone who uploaded music there
- [libxmp](https://github.com/libxmp/libxmp) for playing the modules
- [font8x8](https://github.com/dhepper/font8x8) by Daniel Hepper
- [ps5-payload-dev](https://github.com/ps5-payload-dev) for the toolchain and SDL2
- Triton, for FastTracker II
