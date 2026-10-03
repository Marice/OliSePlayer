# Changelog

All notable changes to this project are documented here. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Native PS5 title build (title ID PPSA01153): `make` produces a signed
  `eboot.bin` with `sce_sys` and `libc.prx`, ready for ShadowMountPlus and
  the homebrew.page catalog. Uses the ps5-native-app-boilerplate tooling.
- `make upload` copies the title folder to the console over FTP.
- `LICENSE` (GPL-3.0-or-later) and `THIRD_PARTY_NOTICES.md`.

### Changed
- The websrv/elfldr payload build moved to `Makefile.payload` (`make payload`).
- Local file list and track history no longer use the C++ standard library
  containers, so the native build links without libc++.

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
