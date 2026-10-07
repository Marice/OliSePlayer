# Decision log

## 2026-10-07: Render through OpenGL instead of VideoOut

**Trigger**

A MilkDrop style visualiser needs per-pixel work on a feedback buffer every
frame. The software renderer draws 640x360 on the CPU and has no shaders, so
the effect would be limited and slow.

**Options**

1. Keep the software renderer and write the effect by hand on the CPU.
   Pro: no new dependency, the app stays 2.6 MB.
   Con: no shaders, so the result stays far from what MilkDrop does.
2. Render through the PS5 OpenGL SDK (Mesa, OpenGL 4.6, GLSL 4.60).
   Pro: real shaders, runtime compiled. The SDK passes the full Khronos
   conformance run on a PS5 and its showcase app runs on this console.
   Con: the app grows to about 25 MB because Mesa links statically. The
   display layer has to change, and the SDK brings its own allocator.
3. Build the visualiser as a separate app next to the player.
   Pro: the player keeps working untouched.
   Con: two apps, and the music and the picture live apart.

**Advice**

Option 2 if the size is acceptable, because it is the only route to the
effect that was asked for. Option 3 is the safer path.

**Choice**

Option 2, chosen by Marice Lamain on 2026-10-07, with the visualiser reacting
to the module the player itself plays.

## 2026-10-07: Use the OpenGL SDK allocator, not the one in heap_native.c

**Trigger**

Both the app and the SDK replace malloc, for the same reason: the libc heap
inside a sandboxed title is too small. Two allocators cannot both own malloc.

**Options**

1. Keep heap_native.c and leave the SDK allocator out.
   Con: the SDK documents that wrapping malloc only in part is unsafe, and
   Mesa allocates through the wrapped names.
2. Use the SDK allocator in the OpenGL build.
   Pro: one owner, and the SDK reserves 128 MiB through direct memory, which
   is what Mesa is tested against. The size is adjustable.
   Con: two allocators in the tree, each used by one build.

**Advice and choice**

Option 2. heap_native.c now compiles only without OLISE_GL, and the OpenGL
build raises the reservation to 256 MiB because libxmp keeps whole modules in
memory. Decided by Marice Lamain on 2026-10-07.

## 2026-10-07: Four changes to the vendored boilerplate

**Trigger**

The ps5-native-app-boilerplate tooling could not link an app against the
OpenGL SDK. Four limits had to be lifted. These files come from an upstream
project, so the changes are listed here to make them easy to re-apply.

**Changes**

1. `tools/build.sh` accepts `.so` link stubs next to `.a` archives. The SDK
   ships libSceAgc and libSceAgcDriver that way.
2. `tools/build.sh` gained `APP_LIBRARY_PATHS` and `APP_UNDEFINED_SYMBOLS`,
   for the search path the SDK's libc++ needs ("pthread" is named inside the
   archive) and for the `-u ps5_agc_gate2_run` the SDK requires.
3. `tooling/native/ps5-pie.ld` defines the four `__eh_frame*` bounds. On this
   target libunwind cannot use dl_iterate_phdr and reads these instead.
   PROVIDE_HIDDEN leaves builds that do not use them unchanged.
4. `tooling/native/sce_module_writer.cpp` skips a weak undefined symbol that
   no stub exports, rather than failing. The SDK relies on this for the TLS
   wrapper `_ZTH23_mesa_glapi_tls_Context`: its C++ objects reference it while
   its C objects use emulated TLS. The ordinary linker leaves it weak and
   undefined, and the SDK's own triangle example links the same way.

**Risk**

Change 4 is the one to watch. It makes the converter accept a program it
previously rejected. A weak symbol that stays unresolved is normal, but it
also means a genuinely missing weak symbol no longer stops the build.

**Choice**

Applied by Marice Lamain on 2026-10-07. All four are additive: without
OLISE_GL the build produces the same output as before.

## 2026-10-07: Port projectM rather than convert the preset files

**Trigger**

The hand written presets look the part, but the request was for a real
MilkDrop preset collection. Those exist: 552 original presets plus curated
packs.

**What a .milk file actually holds**

Partly plain values (zoom, rot, warp, decay) that map onto the hand written
presets one to one, and partly a scripting language:

    per_frame_3=bpulse=band(above(le,bth),above(le-bth,bblock));

A typical preset carries 27 to 50 such lines, often per-pixel code, and many
carry HLSL shaders. Running them needs an expression compiler and an HLSL to
GLSL translator.

**Options**

1. Convert only the fixed values and drop the scripts.
   Pro: a few hours of work, no new dependency.
   Con: presets that get their character from their scripts fall flat.
2. Port projectM (LGPL-2.1, compatible with this project's GPL-3.0).
   Pro: every preset works as intended, scripts and shaders included.
   Con: 41,000 lines across 400 files, and an unknown amount of porting.
3. Keep writing presets by hand.

**Choice**

Option 2, chosen by Marice Lamain on 2026-10-07.

**Feasibility, measured rather than estimated**

projectM 4.1.8 builds completely for the PS5 with the payload SDK toolchain
and the PS5 OpenGL SDK. Every target compiles: the expression compiler, the
HLSL parser, SOIL2, MilkdropPreset, Renderer and the public API. The result
is 2.9 MB of static libraries carrying projectm_create,
projectm_opengl_render_frame and projectm_load_preset_file.

Three things had to be dealt with:

1. CMake's find_package(OpenGL) looks for GLX, which the console does not
   have. A small shim file predefines the OpenGL::GL target against the PS5
   OpenGL SDK, so the search is skipped. No projectM source is touched.
2. SOIL2 treats every FreeBSD derived system as an X11 platform and includes
   GL/glx.h. One patched condition (SOIL_NO_X11) makes it fall back to plain
   GL/gl.h, which the file already supports.
3. The HLSL parser uses exceptions, which the boilerplate disables with
   -fno-exceptions. The OpenGL build already links the full libc++, so the
   support is present; the flag has to be lifted for these sources.

Bison and flex are not needed: the generated parser and scanner ship with the
sources, and the build only regenerates them when the tools are found.

**Still unproven**

That it builds does not mean it runs. Memory use, frame rate at 1080p and
whether the sandbox allows what Mesa and projectM ask for are open questions
until this runs on the console.
