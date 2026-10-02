# OliSe Player (PlayStation 5)

A tracker-music radio for the PS5 with a FastTracker II heart. It streams
random MOD / XM / S3M / IT modules from [The Mod Archive](https://modarchive.org),
shows the pattern data live while it plays, and wraps it all in demoscene
eye candy: starfield, 3D chrome logo, twisting ribbon scroller, copper bars,
oscilloscope and VU meters.

![logo](assets/logo.png)

> **Requires a jailbroken PS5.** This is unsigned homebrew for a console
> with an ELF loader (elfldr / etaHEN / websrv). A network connection is
> needed for The Mod Archive; without it the player uses the local `music/`
> folder.

## Controls (DualSense)

Press **Touchpad** any time for this list on-screen.

| Button | Action |
|---|---|
| **R3** | Next track from the current station |
| **L3** | Previous track (history) |
| **Triangle** | Stations: random, featured, top rated, per format, 78 genres |
| **Cross** | Pause / play |
| **Circle** | Local file list (`music/`) |
| **L1** / **R1** | Previous / next local file (page up/down in lists) |
| **Options** | Instrument list <-> oscilloscope and VU meters |
| **Square** | Effects: full / calm / off |
| **R2** | CRT scanlines |
| **D-pad left / right** | Scroll channels in the pattern view |
| **D-pad up / down** | Volume (hold **L2**: seek by pattern) |
| **Touchpad** | Help |
| **L2 + R2** (together) | Quit back to the launcher |

Keyboard for the desktop build: `n`/`p` next/previous, `g` stations, Space
pause, `o` file list, `,`/`.` local prev/next, `s` scopes, `f` effects,
`c` CRT, arrows channels/volume, Enter now-playing, `h` help, Esc quit.

## Music and stations

Everything comes from [The Mod Archive](https://modarchive.org) over plain
HTTP, using the same pages a browser sees (the XML API needs a key):

| Station | How it picks |
|---|---|
| Random, any format | `request=view_random&format=<MOD/XM/S3M/IT>` with a random format |
| Random per format | the same, fixed format |
| Featured picks / Top rated | a random module from `request=view_chart&query=featured` or `topscore` |
| Genre (78 of them) | a random page of `request=search&search_type=genre&query=<id>`, then a random module on it |

Only MOD, XM, S3M and IT are played (libxmp-lite); other formats on a page
are skipped. A track plays once, then the next one from the same station
starts. The last 20 tracks stay in memory so L3 can go back without a new
download. Without network the player retries with a growing delay and
keeps playing local files.

`tools/netxm_test.c` runs the page parsers offline against saved HTML.

Local modules go in `music/` next to `eboot.elf`
(`/data/homebrew/OliSePlayer/music/` on the PS5).

## Building

Needs the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk)
with its [SDL2 port](https://github.com/ps5-payload-dev/SDL) installed, plus
`cmake` for the libxmp-lite build. Then:

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make deps           # builds libxmp-lite into deps/ (PS5 + native)
make                # builds eboot.elf
make homebrew       # assembles dist/OliSePlayer/ + zip for the launcher
```

A desktop test build (`make native`, needs `libsdl2-dev`) plays local files
from `music/`; the Mod Archive download is only compiled for the PS5.

`make assets` regenerates the logo header and the icon with
`tools/make_logo.py` and `tools/make_icon.py` (needs `python3-pil`).

## Installing on a jailbroken PS5

Copy `dist/OliSePlayer/` to `/data/homebrew/OliSePlayer/` on the PS5
(`eboot.elf` + `sce_sys/icon0.png`). It then appears in the
[websrv](https://github.com/ps5-payload-dev/websrv) homebrew menu.

For a quick dev loop, send the ELF to a console running elfldr:

```sh
make test PS5_HOST=<ps5-ip>
```

## Credits

- Music: [The Mod Archive](https://modarchive.org) and every artist on it
- Module playback: [libxmp-lite](https://github.com/libxmp/libxmp) (MIT)
- 8x8 bitmap font: [font8x8](https://github.com/dhepper/font8x8) by Daniel Hepper (public domain)
- PS5 toolchain and SDL2 port: [ps5-payload-dev](https://github.com/ps5-payload-dev)
- Look and feel inspired by FastTracker II (Triton, 1994-1997)
