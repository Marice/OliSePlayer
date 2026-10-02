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
| **R3** | Next random track from The Mod Archive |
| **L3** | Previous track (history) |
| **Cross** | Pause / play |
| **Circle** | Local file list (`music/`) |
| **L1** / **R1** | Previous / next local file |
| **Triangle** | Instrument list <-> oscilloscope and VU meters |
| **Square** | Effects: full / calm / off |
| **R2** | CRT scanlines |
| **D-pad left / right** | Scroll channels in the pattern view |
| **D-pad up / down** | Volume (hold **L2**: seek by pattern) |
| **Options** | Show the now-playing card |
| **L2 + R2** (together) | Quit back to the launcher |

Keyboard for the desktop build: `n`/`p` next/previous, Space pause, `o`
file list, `,`/`.` local prev/next, `s` scopes, `f` effects, `c` CRT,
arrows channels/volume, Enter now-playing, `h` help, Esc quit.

## Music

The radio asks The Mod Archive for a random module of a random format
(MOD, XM, S3M or IT), downloads it over plain HTTP and plays it once. When
it ends, the next random track starts. Tracks you have heard stay in a
history of 20 so L3 can go back without downloading again. If the network
is down the player retries with a growing delay and keeps playing local
files in the meantime.

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
