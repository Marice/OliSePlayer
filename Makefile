# OliSe Player - Linux/WSL build entry points.
#
# Native PS5 title build (eboot.bin + sce_sys, for ShadowMountPlus and the
# homebrew.page catalog) based on the ps5-native-app-boilerplate tooling:
# Copyright (C) 2026 BlackBearReloaded, SPDX-License-Identifier: GPL-3.0-or-later.
# The websrv/elfldr payload build lives in Makefile.payload (make payload).

SHELL := /bin/bash
.DEFAULT_GOAL := app
# pip-installed ninja lives in ~/.local/bin
export PATH := $(HOME)/.local/bin:$(PATH)

-include .env

# --- OliSe Player defaults -------------------------------------------------
# libxmp-lite is built by tools/build-deps.sh into deps/ (same public SDK).
APP_DEFINITIONS ?= OLISE_NET OLISE_NATIVE
APP_INCLUDE_PATHS ?= deps/ps5/include/libxmp-lite
APP_STATIC_ARCHIVES ?= deps/ps5/lib/libxmp-lite.a
# SDL2 from PacBrew as plain archives: its pkg-config file drags in -lSce* flags
# that the boilerplate linker command cannot resolve by name.
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?= include
PACBREW_STATIC_ARCHIVES ?= lib/libSDL2.a lib/libiconv.a
APP_CATEGORY ?= media
APP_NAME ?= OliSe Player
TITLE_ID ?= PPSA01153

# --- boilerplate variables -------------------------------------------------
APP_RUNTIME_MODULES ?=
APP_WRAP_SYMBOLS ?=
APP_SOURCE_DIR ?=
APP_PARAM ?=
APP_SCE_SYS ?=
APP_ASSETS ?= assets
APP_ROOT_FILES ?=
PS5_HOST ?= 192.168.68.125
FTP_PORT ?= 2121
DEPLOY_FORMAT ?= folder
PS5_FTP_USER ?= anonymous
PS5_FTP_PASSWORD ?= codex
DEPLOY_DRY_RUN ?= 0
CONTENT_SUFFIX ?=
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 0
export BUILD_JOBS USE_CCACHE
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_RUNTIME_MODULES APP_WRAP_SYMBOLS
export APP_SOURCE_DIR APP_PARAM APP_SCE_SYS APP_ASSETS APP_ROOT_FILES
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export PS5_HOST FTP_PORT DEPLOY_FORMAT PS5_FTP_USER PS5_FTP_PASSWORD DEPLOY_DRY_RUN
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)
XMP_PS5 := deps/ps5/lib/libxmp-lite.a

.PHONY: all app build init doctor deps pacbrew pacbrew-list assets-check libc ffpkg ffpfsc packages deploy undeploy \
	payload native assets upload music-folder clean distclean help

all: app
build: app

init:
	@printf '%s\n' '==> [init] Configuring the application identity in sce_sys/param.json'
	@bash tools/init-project.sh sce_sys/param.json

doctor:
	@printf '%s\n' '==> [doctor] Checking the Linux/WSL host without changing it'
	@bash tools/doctor.sh

deps: $(XMP_PS5)
	@printf '%s\n' '==> [deps] Fetching declared native dependencies'
	@bash tools/setup-native-dependencies.sh
	@bash tools/setup-pacbrew-dependencies.sh --environment

$(XMP_PS5):
	@printf '%s\n' '==> [deps] Building libxmp-lite'
	@tools/build-deps.sh

pacbrew:
	@printf '%s\n' '==> [pacbrew] Fetching the pinned prebuilt ports sysroot'
	@bash tools/setup-pacbrew-dependencies.sh --all

pacbrew-list:
	@printf '%s\n' '==> [pacbrew] Listing available pkg-config modules'
	@bash tools/setup-pacbrew-dependencies.sh --list

assets-check:
	@printf '%s\n' '==> [assets] Validating icon, backgrounds, and selection audio'
	@bash tools/validate-assets.sh

libc:
	@printf '%s\n' '==> [libc] Rebuilding and verifying the clean-room runtime'
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME) $(XMP_PS5)
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder
	@$(MAKE) --no-print-directory music-folder

# Local modules live in <title>/music; ship the folder with a note and re-zip.
music-folder:
	@mkdir -p dist/$(TITLE_ID)/music
	@cp music/README.txt dist/$(TITLE_ID)/music/README.txt
	@cd dist && rm -f $(TITLE_ID).zip && python3 -m zipfile -c $(TITLE_ID).zip $(TITLE_ID)
	@printf '%s\n' '==> [music] Added music/README.txt to the app folder and zip'

ffpkg: $(RUNTIME) $(XMP_PS5)
	@printf '%s\n' '==> [ffpkg] Building the app folder and UFS2 image'
	@bash tools/build.sh Ffpkg

ffpfsc: $(RUNTIME) $(XMP_PS5)
	@printf '%s\n' '==> [ffpfsc] Building the app folder and compressed image'
	@bash tools/build.sh Ffpfsc

packages: $(RUNTIME) $(XMP_PS5)
	@printf '%s\n' '==> [packages] Building the app folder and both package formats'
	@bash tools/build.sh All

deploy:
	@printf '%s\n' '==> [deploy] Building and publishing the selected app output over FTP'
	@bash tools/deploy.sh

undeploy:
	@printf '%s\n' '==> [undeploy] Removing staged development files for this title over FTP'
	@bash tools/deploy.sh undeploy

# Plain curl upload of dist/<TITLE_ID>/ to /data/homebrew/<TITLE_ID>/ (etaHEN FTP on 1337).
upload: app
	@printf '%s\n' '==> [upload] Copying dist/$(TITLE_ID) to ftp://$(PS5_HOST):$(UPLOAD_PORT)/data/homebrew/$(TITLE_ID)/'
	@for f in $$(cd dist/$(TITLE_ID) && find . -type f); do \
		curl -sS -f --ftp-create-dirs -T "dist/$(TITLE_ID)/$$f" "ftp://$(PS5_HOST):$(UPLOAD_PORT)/data/homebrew/$(TITLE_ID)/$${f#./}" \
			|| { echo "upload of $$f failed"; exit 1; }; \
	done; echo "uploaded"
	@echo "local  eboot.bin sha256: $$(sha256sum dist/$(TITLE_ID)/eboot.bin | cut -c1-16)"
	@echo "remote eboot.bin sha256: $$(curl -sS -f "ftp://$(PS5_HOST):$(UPLOAD_PORT)/data/homebrew/$(TITLE_ID)/eboot.bin" | sha256sum | cut -c1-16)"
UPLOAD_PORT ?= 1337

# --- websrv / elfldr payload build and desktop build (Makefile.payload) ----
payload:
	@$(MAKE) -f Makefile.payload

native:
	@$(MAKE) -f Makefile.payload native

# Regenerate the logo header and the icon (needs python3-pil).
assets:
	tools/make_logo.py
	tools/make_icon.py

clean:
	@printf '%s\n' '==> [clean] Removing generated build outputs'
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)
	@$(MAKE) -f Makefile.payload clean

distclean: clean
	@printf '%s\n' '==> [distclean] Removing downloaded dependency caches'
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make                 Build the native title folder dist/PPSA01153 and the zip' \
	  'make deps            Build libxmp-lite and fetch the SDK and PacBrew (sdl2)' \
	  'make ffpfsc          Also build the compressed .ffpfsc image' \
	  'make upload          Build and copy the folder to the PS5 over FTP (port 1337)' \
	  'make payload         Build the websrv/elfldr eboot.elf (Makefile.payload)' \
	  'make native          Build the desktop test binary' \
	  'make assets          Regenerate logo header and icon' \
	  'make doctor          Check the host toolchain' \
	  'make clean           Remove build outputs'
