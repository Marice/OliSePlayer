# Changelog

All notable changes to this project are documented here. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Modland as a second source.** Triangle now asks which archive to listen to
  before it asks for a station. Modland is read through its playlists, which
  are plain text files of direct links: the favourites of around 850 modules,
  a chiptune selection, netlabel releases, a few musicdisks and a demoparty
  compo. One request gets the list and a second gets the tune, where the Mod
  Archive route needs a page to be parsed first.

### Changed
- Stepping through visualiser presets follows a shuffled order instead of the
  alphabet. Sorted by filename, stepping meant hearing out one author at a
  time; the order is reshuffled once it has been walked through. R2 + Triangle
  still jumps anywhere at once.

## [02.000.001] - 2026-10-07

### Changed
- `PPSA01153.zip` now carries the MilkDrop presets, so an install from a store
  has them straight away. The zip is built after the presets are in place;
  before this it was written first and they never made it in.
- The test modules are no longer shipped. `music/` holds a note and an empty
  `index.txt` for the user's own files.

## [02.000.000] - 2026-10-07

OliSe Player now renders through OpenGL and plays the original MilkDrop
presets.

### Added
- **MilkDrop visualiser.** The app runs projectM 4.1.8 (LGPL-2.1), so the
  552 presets of the last official MilkDrop release work as intended, scripts
  and shaders included. Square cycles the visualiser (off, behind the
  interface, full screen); Triangle picks the next preset, L2 + Triangle the
  previous one and R2 + Triangle a random one. The presets are not part of
  this repository: they are attached to the release as
  `OliSePlayer-presets.zip`, and `make presets` fetches them from the source.
  The archive carries projectM's statement on their licence.
- **Built-in visualiser** with 24 presets over seven warp modes, used when
  projectM is unavailable or finds no presets, so the app always has an
  effect to show.
- **OpenGL rendering** through the PS5 OpenGL SDK (`make gl`). The software
  renderer still draws the FastTracker II interface; it is uploaded as a
  texture and composited on the GPU. The plain `make` build keeps rendering
  straight to VideoOut.
- `index.txt` in `music/` and `presets/`, written at build time. A title
  sandbox may open a file but not list a folder, so the app falls back to
  this index when its own directory scan is refused.
- The app writes `olise.log` next to itself: in a native title stderr goes
  nowhere, and this is what made the sandbox problems diagnosable.

### Changed
- The title is registered as a game (`applicationCategoryType` 0) instead of
  a media app. Media titles get a stricter sandbox on this firmware.
- Local modules are found through the app folder, located by looking for the
  app's own `eboot.bin` the way ProsperoStore does, rather than by deriving a
  path from `argv[0]`, which a native title does not provide.
- Square only drives the visualiser now; the demo effects keep their level
  instead of cycling along with it.

### Fixed
- Local modules in `music/` were never found on the console.

### Known limitation
- Adding files without rebuilding means editing `index.txt` by hand: the
  sandbox refuses to list a folder, so the app cannot discover them itself.

## [01.000.002] - 2026-10-03

### Added
- The version number (`contentVersion`) is shown next to the logo.

### Removed
- The websrv/elfldr payload build and `OliSePlayer.zip`; the native title is
  the only supported way to run OliSe Player.
- The L2 + R2 quit combination; close the app with the PS button.

## [01.000.001] - 2026-10-03

### Fixed
- The title reserved about 337 MB of `/download0` storage it never used
  (`downloadDataSize` 256 from the template); it is now 0. Remove and
  re-add the app once to free the space of an earlier install.
- `make upload` now stops on FTP errors and prints the local and remote
  hash of `eboot.bin`.

## [01.000.000] - 2026-10-03

First native PS5 title release (homebrew.page catalog format). Tags now
follow the `contentVersion` in `sce_sys/param.json`.

### Added
- Native PS5 title build (title ID PPSA01153): `make` produces a signed
  `eboot.bin` with `sce_sys` and `libc.prx`, ready for ShadowMountPlus and
  the homebrew.page catalog. Uses the ps5-native-app-boilerplate tooling.
- `make upload` copies the title folder to the console over FTP.
- `LICENSE` (GPL-3.0-or-later) and `THIRD_PARTY_NOTICES.md`.
- A dedication to Olivier and Elise shows as a notification at start-up.
- Error notifications on the PS5 when start-up fails (display, audio, player).

### Changed
- The websrv/elfldr payload build moved to `Makefile.payload` (`make payload`).
- Local file list and track history no longer use the C++ standard library
  containers, so the native build links without libc++.
- The native title draws through libSceVideoOut (tiled 1080p buffers) instead
  of SDL's video driver, which does not run inside the title sandbox; SDL is
  still used for audio and the controller. The title also carries its own
  mmap-based heap because the sandbox libc heap is too small for 1080p
  framebuffers and multi-megabyte modules.

## [v0.1.0] - 2026-10-02

Sneak preview build.

### Added
- PS5 homebrew tracker player built with the ps5-payload-dev SDK, SDL2 and
  libxmp-lite (MOD, XM, S3M, IT).
- Mod Archive radio: R3 downloads a random module straight from
  modarchive.org, tracks play once and the next one starts automatically,
  L3 goes back through the history.
- Stations (Triangle): random any format, random per format, featured
  picks, top rated, and 78 genres from the Mod Archive genre list.
- Genre and artist of a track shown in the info panel when the site
  provides them.
- Local `music/` folder next to `eboot.elf` with an FT2-style file list.
- FastTracker II look: starfield, 3D chrome logo, live pattern view with
  the blue row bar, info panel, instrument list with VU meters, oscilloscope.
- Demoscene extras: twisting ribbon scroller, copper bars under the pattern,
  warp burst on track change, logo shine, CRT scanlines.
- Now-playing card for six seconds on every track change.
