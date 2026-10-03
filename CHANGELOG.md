# Changelog

All notable changes to this project are documented here. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
