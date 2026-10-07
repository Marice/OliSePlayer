# OliSe Player - Linux/WSL build entry points.
#
# Native PS5 title build (eboot.bin + sce_sys, for ShadowMountPlus and the
# homebrew.page catalog) based on the ps5-native-app-boilerplate tooling:
# Copyright (C) 2026 BlackBearReloaded, SPDX-License-Identifier: GPL-3.0-or-later.
# `make native` builds a desktop test binary with the system SDL2.

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

# --- OpenGL build (make gl) ------------------------------------------------
# Renders through EGL instead of VideoOut, which is what the visualiser needs.
# Point PS5_OPENGL_PREFIX at the sdk/ directory of a ps5-opengl release:
#   https://github.com/blackbearreloaded/ps5-opengl/releases
# The SDK links Mesa statically, so the app grows from about 2.6 MB to 21 MB.
PS5_OPENGL_PREFIX ?= deps/ps5-opengl/sdk
# TRACE=1 turns every startup checkpoint into an on-screen notification.
# Use it to find where a crash happens; leave it off for a release.
TRACE ?= 0
# projectM (LGPL-2.1) runs the original MilkDrop presets. Optional: without
# the library the app keeps its built-in visualiser. Build it with
# tools/build-projectm.sh, which also documents the three porting fixes.
PROJECTM_DIR := deps/projectm
PROJECTM := $(PROJECTM_DIR)/lib/libprojectm.a

GL_DEFINITIONS := OLISE_NET OLISE_NATIVE OLISE_GL
ifeq ($(TRACE),1)
GL_DEFINITIONS += OLISE_TRACE_STARTUP
endif
# Only compile the projectM path when the library is actually there.
HAVE_PROJECTM := $(wildcard $(PROJECTM))
ifneq ($(HAVE_PROJECTM),)
GL_DEFINITIONS += OLISE_PROJECTM
endif
GL_INCLUDE_PATHS := deps/ps5/include/libxmp-lite $(PS5_OPENGL_PREFIX)/include src/elevation
ifneq ($(HAVE_PROJECTM),)
GL_INCLUDE_PATHS += $(PROJECTM_DIR)/include
endif
# The SDK allocator owns malloc for the whole executable; wrapping it only in
# part is unsafe, so the complete set goes together (consumer-build.md).
GL_WRAP_SYMBOLS := malloc calloc realloc free posix_memalign malloc_usable_size
# Mesa uses std::mutex, std::vector and libc calls (popen, mkstemps, syslog)
# that the boilerplate's minimal runtime leaves out on purpose, so the OpenGL
# build links the payload SDK's full libc and libc++ too. tools/build.sh only
# accepts archive paths below the project and without "+" in the name, hence
# the symlinks in deps/ps5-runtime that the gl-runtime target creates.
GL_RUNTIME_DIR := deps/ps5-runtime
GL_ARCHIVES := deps/ps5/lib/libxmp-lite.a \
	$(PS5_OPENGL_PREFIX)/lib/libPS5OpenGL.a \
	$(PS5_OPENGL_PREFIX)/lib/libSceAgc.so \
	$(PS5_OPENGL_PREFIX)/lib/libSceAgcDriver.so \
	$(GL_RUNTIME_DIR)/libcxx.a \
	$(GL_RUNTIME_DIR)/libcxxabi.a \
	$(GL_RUNTIME_DIR)/libc.a \
	$(GL_RUNTIME_DIR)/libpthread.a \
	$(GL_RUNTIME_DIR)/libunwind.a
ifneq ($(HAVE_PROJECTM),)
# Before the runtime archives: projectM pulls symbols out of libc++ and libc.
GL_ARCHIVES := $(PROJECTM_DIR)/lib/libprojectm.a $(PROJECTM_DIR)/lib/libprojectmeval.a $(GL_ARCHIVES)
endif
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
XMP_NATIVE := deps/native/lib/libxmp-lite.a
VERSION_H := src/version.h

.PHONY: all app gl gl-runtime presets presets-folder presets-zip app-zip build init doctor deps pacbrew pacbrew-list assets-check libc ffpkg ffpfsc packages deploy undeploy \
	native version assets upload music-folder clean distclean help

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

# The MilkDrop preset pack is not in the repository; this fetches it.
presets:
	@bash tools/fetch-presets.sh

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

# src/version.h carries contentVersion from sce_sys/param.json (shown in the app).
$(VERSION_H): sce_sys/param.json
	@python3 -c 'import json; p=json.load(open("sce_sys/param.json")); open("$(VERSION_H)","w").write("/* Generated from sce_sys/param.json by make. Do not edit. */\n#ifndef VERSION_H\n#define VERSION_H\n#define OLISE_VERSION \"%s\"\n#define OLISE_TITLE_ID \"%s\"\n#endif\n" % (p["contentVersion"], p["titleId"])); print("==> [version]", p["contentVersion"], p["titleId"])'

version: $(VERSION_H)

app: $(RUNTIME) $(XMP_PS5) $(VERSION_H)
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder
	@$(MAKE) --no-print-directory music-folder app-zip

# OpenGL build: same app, rendered through EGL instead of VideoOut.
gl: $(RUNTIME) $(XMP_PS5) $(VERSION_H) gl-runtime
	@test -d "$(PS5_OPENGL_PREFIX)/include/EGL" || { \
		echo "PS5 OpenGL SDK not found at $(PS5_OPENGL_PREFIX)"; \
		echo "Download a release from https://github.com/blackbearreloaded/ps5-opengl/releases"; \
		echo "and unpack it so that $(PS5_OPENGL_PREFIX)/include/EGL exists,"; \
		echo "or build with: make gl PS5_OPENGL_PREFIX=/path/to/sdk"; \
		exit 1; }
	@printf '%s\n' '==> [gl] Compiling and linking the OpenGL build'
	@APP_DEFINITIONS="$(GL_DEFINITIONS)" \
	 APP_INCLUDE_PATHS="$(GL_INCLUDE_PATHS)" \
	 APP_STATIC_ARCHIVES="$(GL_ARCHIVES)" \
	 APP_WRAP_SYMBOLS="$(GL_WRAP_SYMBOLS)" \
	 APP_UNDEFINED_SYMBOLS="ps5_agc_gate2_run" \
	 APP_LIBRARY_PATHS="$(PS5_PAYLOAD_SDK)/target/lib" \
	 bash tools/build.sh Folder
	@$(MAKE) --no-print-directory music-folder presets-folder app-zip
	@printf '==> [gl] eboot.bin is %s bytes\n' "$$(stat -c%s dist/$(TITLE_ID)/eboot.bin)"

# The payload SDK archives under names tools/build.sh accepts, plus the two
# OpenGL SDK link stubs. The converter resolves every needed module by SONAME
# from one stub directory, and libSceAgc.prx and libSceAgcDriver.prx are not
# part of the payload SDK. Both are system modules present on the console, so
# only the stubs are needed here; nothing extra ships with the app.
# Also strips mman.o from libc.a: a sandboxed title must reach mmap and
# mprotect through libScePosixForWebKit, which wraps them. The raw versions in
# mman.o call the kernel directly, and the title is killed with 0xa002030a
# (SYSTEM_ILLEGAL_FUNCTION_CALL) as soon as the allocator maps its heap.
# Without the object both symbols stay undefined and resolve to the system
# module again, exactly as the VideoOut build already did.
gl-runtime:
	@mkdir -p $(GL_RUNTIME_DIR)
	@cp -u $(PS5_OPENGL_PREFIX)/lib/libSceAgc.so $(PS5_OPENGL_PREFIX)/lib/libSceAgcDriver.so \
		.deps/native/ps5-payload-sdk/target/lib/ 2>/dev/null || true
	@ln -sf $(PS5_PAYLOAD_SDK)/target/lib/libc++.a $(GL_RUNTIME_DIR)/libcxx.a
	@ln -sf $(PS5_PAYLOAD_SDK)/target/lib/libc++abi.a $(GL_RUNTIME_DIR)/libcxxabi.a
	@cp -f $(PS5_PAYLOAD_SDK)/target/lib/libc.a $(GL_RUNTIME_DIR)/libc.a
	@chmod u+w $(GL_RUNTIME_DIR)/libc.a
	@$(PS5_PAYLOAD_SDK)/bin/prospero-ar d $(GL_RUNTIME_DIR)/libc.a mman.o
	@ln -sf $(PS5_PAYLOAD_SDK)/target/lib/libpthread.a $(GL_RUNTIME_DIR)/libpthread.a
	@ln -sf $(PS5_PAYLOAD_SDK)/target/lib/libunwind.a $(GL_RUNTIME_DIR)/libunwind.a

# The elevation helper ships beside the app: the client sends it to the local
# elfldr so a sandboxed title may list folders. Taken from ProsperoStore, which
# runs it on this firmware. Without it the app still runs, with no local files.
LAPY := deps/lapy/lapy.elf

# MilkDrop presets ship inside the app folder: a title sandbox cannot read
# /data without elevated privileges, so this is the only place the app can
# reach them. Skipped when presets/ is empty.
presets-folder:
	@if [ -f $(LAPY) ]; then cp -u $(LAPY) dist/$(TITLE_ID)/lapy.elf; \
		printf '==> [lapy] elevation helper added\n'; fi
	@if [ -d presets ] && ls presets/*.milk >/dev/null 2>&1; then \
		mkdir -p dist/$(TITLE_ID)/presets; \
		cp -u presets/*.milk dist/$(TITLE_ID)/presets/; \
		(cd dist/$(TITLE_ID)/presets && ls -1 *.milk > index.txt); \
		cp -f presets/PRESETS-LICENSE.md dist/$(TITLE_ID)/presets/ 2>/dev/null || true; \
		if [ -d presets/textures ]; then \
			mkdir -p dist/$(TITLE_ID)/presets/textures; \
			cp -u presets/textures/* dist/$(TITLE_ID)/presets/textures/; \
		fi; \
		printf '==> [presets] %s preset(s) added to the app folder\n' \
			"$$(ls dist/$(TITLE_ID)/presets/*.milk 2>/dev/null | wc -l)"; \
	fi

# A separate archive of just the presets, to attach to a release. The app
# works without it (it falls back to the built-in visualiser), so it is a
# download of its own rather than part of the app zip.
presets-zip: presets-folder
	@test -d dist/$(TITLE_ID)/presets || { echo "no presets/ to package"; exit 1; }
	@rm -f dist/OliSePlayer-presets.zip
	@cd dist/$(TITLE_ID) && python3 -m zipfile -c ../OliSePlayer-presets.zip presets
	@printf '==> [presets] dist/OliSePlayer-presets.zip (%s)\n' \
		"$$(du -h dist/OliSePlayer-presets.zip | cut -f1)"

# Local modules live in <title>/music; ship the folder with a note and re-zip.
# index.txt lists what is in the folder. A title sandbox may open a file but
# not list a directory (ShadowMountPlus returns EPERM), so the app reads this
# index when its own scan finds nothing.
music-folder:
	@mkdir -p dist/$(TITLE_ID)/music
	@cp music/README.txt dist/$(TITLE_ID)/music/README.txt
	@rm -f dist/$(TITLE_ID)/music/*.mod dist/$(TITLE_ID)/music/*.xm \
		dist/$(TITLE_ID)/music/*.s3m dist/$(TITLE_ID)/music/*.it 2>/dev/null; true
	@: > dist/$(TITLE_ID)/music/index.txt
	@printf '%s\n' '==> [music] Empty music/ with a note; the modules in music/ are test files'

# The zip is what gets installed, so it is built last, after the music note and
# the presets are in place. Modules are not shipped: they are test material and
# the user puts their own in music/, listing them in music/index.txt.
app-zip:
	@cd dist && rm -f $(TITLE_ID).zip && python3 -m zipfile -c $(TITLE_ID).zip $(TITLE_ID)
	@printf '==> [zip] dist/$(TITLE_ID).zip (%s)\n' "$$(du -h dist/$(TITLE_ID).zip | cut -f1)"

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

# --- Desktop test build (needs libsdl2-dev; plays files from music/) -------
DESKTOP_SRCS := $(filter-out src/heap_native.c src/compat_native.c src/netxm.c,$(wildcard src/*.cpp))
NATIVE_CXXFLAGS := -O2 -Wall -std=c++17 -I$(dir $(XMP_NATIVE))../include/libxmp-lite $(shell sdl2-config --cflags)
native: $(VERSION_H) $(XMP_NATIVE)
	gcc -O2 -Wall -c src/netxm.c -o build/netxm-desktop.o
	g++ $(NATIVE_CXXFLAGS) -o OliSePlayer-native $(DESKTOP_SRCS) build/netxm-desktop.o \
		$(XMP_NATIVE) $(shell sdl2-config --libs) -lm

$(XMP_NATIVE): $(XMP_PS5)

# Regenerate the logo header and the icon (needs python3-pil).
assets:
	tools/make_logo.py
	tools/make_icon.py

clean:
	@printf '%s\n' '==> [clean] Removing generated build outputs'
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME) $(VERSION_H) OliSePlayer-native

distclean: clean
	@printf '%s\n' '==> [distclean] Removing downloaded dependency caches'
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make                 Build the native title folder dist/PPSA01153 and the zip' \
	  'make deps            Build libxmp-lite and fetch the SDK and PacBrew (sdl2)' \
	  'make ffpfsc          Also build the compressed .ffpfsc image' \
	  'make upload          Build and copy the folder to the PS5 over FTP (port 1337)' \
	  'make native          Build the desktop test binary' \
	  'make assets          Regenerate logo header and icon' \
	  'make doctor          Check the host toolchain' \
	  'make clean           Remove build outputs'
