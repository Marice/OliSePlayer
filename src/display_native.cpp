#if defined(OLISE_NATIVE) && !defined(OLISE_GL)
/* VideoOut display backend for the native PS5 title.
 *
 * Used when the app is built without OpenGL. With OLISE_GL the frame goes
 * through EGL in display_gl.cpp instead, which keeps the same interface.
 *
 * SDL's PS5 video driver does not run inside a sandboxed title, so the frame
 * goes straight to libSceVideoOut the way the ps5-native-app-boilerplate
 * does it (Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later): two
 * 1080p buffers in write-combined direct memory, registered with the tiled
 * RGBA8 layout, flipped on vblank. The 640x360 framebuffer is scaled 3x with
 * nearest neighbour while it is converted to the tiled layout. */
#include "display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "gfx.h"

extern "C" {
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t length,
                                  size_t alignment, int memory_type, int64_t* physical_address);
int sceKernelMapDirectMemory(void** address, size_t length, int protection, int flags,
                             int64_t physical_address, size_t alignment);
int sceSystemServiceHideSplashScreen(void);
int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void* param);
int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index, uint32_t flip_mode, int64_t flip_argument);
int sceVideoOutWaitVblank(int32_t handle);

struct VideoBuffer { void* data; void* metadata; void* reserved0; void* reserved1; };
struct VideoAttribute { uint8_t reserved[80]; };
void sceVideoOutSetBufferAttribute2(VideoAttribute* attribute, uint64_t pixel_format, uint32_t tiling_mode,
                                    uint32_t width, uint32_t height, uint64_t option, uint32_t dcc_control,
                                    uint64_t dcc_clear_color);
int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set_index, int32_t buffer_index_start, VideoBuffer* buffers,
                                int32_t buffer_count, VideoAttribute* attribute, int32_t category, void* option);
}

namespace {

constexpr unsigned FRAME_W = 1920;
constexpr unsigned FRAME_H = 1080;
constexpr size_t FRAME_BYTES = 0x1000000;            /* 16 MiB per tiled buffer */
constexpr size_t MEMORY_BYTES = FRAME_BYTES * 2;
constexpr size_t MEMORY_ALIGNMENT = 0x200000;
constexpr int MEMORY_TYPE_WC_GARLIC = 3;
constexpr int MAP_PROTECTION = 0x33;
constexpr uint64_t PIXEL_FORMAT_RGBA8_SRGB = UINT64_C(0x8000000022000000);
constexpr int SCALE = 3;                              /* 640x360 -> 1920x1080 */

int g_video = -1;
uint8_t* g_buffers[2] = { nullptr, nullptr };
int g_back = 0;
int64_t g_frame = 0;
uint16_t g_tile[128 * 128];                           /* byte offset inside a 128x128 block */

/* Byte offset of pixel (x, y) in the tiled 1080p buffer (boilerplate layout). */
constexpr uint32_t tile_offset(unsigned x, unsigned y)
{
	return ((y << 4) & 0x70U) ^ ((y << 5) & 0xf00U) ^ ((y << 9) & 0x1000U) ^ ((y << 8) & 0x4000U) ^
	       ((x << 2) & 0xcU) ^ ((x << 5) & 0x380U) ^ ((x << 4) & 0x400U) ^ ((x << 6) & 0x800U) ^
	       ((x << 9) & 0xa000U);
}

constexpr uint32_t BLOCKS_PER_ROW = (FRAME_W + 127U) >> 7;

/* 0x00RRGGBB (framebuffer) -> 0xAABBGGRR (VideoOut). */
inline uint32_t to_abgr(uint32_t c)
{
	return 0xff000000u | ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff);
}

} // namespace

bool display_init(char* err, size_t err_len)
{
	for (unsigned y = 0; y < 128; y++)
		for (unsigned x = 0; x < 128; x++) g_tile[y * 128 + x] = (uint16_t)tile_offset(x, y);

	(void)sceSystemServiceHideSplashScreen();
	g_video = sceVideoOutOpen(0xff, 0, 0, nullptr);
	if (g_video < 0) {
		snprintf(err, err_len, "sceVideoOutOpen failed: 0x%x", (unsigned)g_video);
		return false;
	}
	size_t pool = sceKernelGetDirectMemorySize();
	if (pool < MEMORY_BYTES) {
		snprintf(err, err_len, "direct memory too small: %zu", pool);
		return false;
	}
	int64_t physical = 0;
	int rc = sceKernelAllocateDirectMemory(0, (int64_t)pool, MEMORY_BYTES, MEMORY_ALIGNMENT, MEMORY_TYPE_WC_GARLIC, &physical);
	if (rc < 0) {
		snprintf(err, err_len, "direct memory alloc failed: 0x%x", (unsigned)rc);
		return false;
	}
	void* mapped = nullptr;
	rc = sceKernelMapDirectMemory(&mapped, MEMORY_BYTES, MAP_PROTECTION, 0, physical, MEMORY_ALIGNMENT);
	if (rc < 0) {
		snprintf(err, err_len, "direct memory map failed: 0x%x", (unsigned)rc);
		return false;
	}
	g_buffers[0] = (uint8_t*)mapped;
	g_buffers[1] = (uint8_t*)mapped + FRAME_BYTES;
	/* Start black: the tiled buffer covers every pixel, so a plain memset works. */
	for (int b = 0; b < 2; b++) {
		uint32_t* p = (uint32_t*)g_buffers[b];
		for (size_t i = 0; i < FRAME_BYTES / 4; i++) p[i] = 0xff000000u;
	}
	__asm__ volatile("mfence" ::: "memory");

	VideoBuffer buffers[2] = { { g_buffers[0], nullptr, nullptr, nullptr }, { g_buffers[1], nullptr, nullptr, nullptr } };
	VideoAttribute attribute = {};
	(void)sceVideoOutSetFlipRate(g_video, 0);
	sceVideoOutSetBufferAttribute2(&attribute, PIXEL_FORMAT_RGBA8_SRGB, 0, FRAME_W, FRAME_H, 0, 0, 0);
	rc = sceVideoOutRegisterBuffers2(g_video, 0, 0, buffers, 2, &attribute, 0, nullptr);
	if (rc < 0) {
		snprintf(err, err_len, "buffer registration failed: 0x%x", (unsigned)rc);
		return false;
	}
	rc = sceVideoOutSubmitFlip(g_video, 0, 1, 0);
	if (rc < 0) {
		snprintf(err, err_len, "initial flip failed: 0x%x", (unsigned)rc);
		return false;
	}
	(void)sceVideoOutWaitVblank(g_video);
	g_back = 1;
	return true;
}

void display_present(const uint32_t* fb, bool crt)
{
	if (g_video < 0) return;
	uint8_t* dst = g_buffers[g_back];
	/* Walk the 1080p target row by row; each source pixel covers a 3x3 block. */
	for (unsigned y = 0; y < FRAME_H; y++) {
		const uint32_t* src_row = fb + (y / SCALE) * gfx::W;
		const uint32_t block_row = (y >> 7) * BLOCKS_PER_ROW;
		const uint16_t* tile_row = g_tile + (y & 127) * 128;
		const bool dark = crt && (y % 3 == 2);
		for (unsigned x = 0; x < FRAME_W; x++) {
			uint32_t c = src_row[x / SCALE];
			if (dark) c = (c >> 1) & 0x7f7f7f;
			uint8_t* block = dst + ((size_t)(block_row + (x >> 7)) << 16);
			*(uint32_t*)(block + tile_row[x & 127]) = to_abgr(c);
		}
	}
	__asm__ volatile("mfence" ::: "memory");
	(void)sceVideoOutSubmitFlip(g_video, g_back, 1, g_frame++);
	(void)sceVideoOutWaitVblank(g_video);
	g_back ^= 1;
}

void display_shutdown()
{
	/* The title keeps its video output until the shell closes the process. */
}

bool display_has_gpu()
{
	return false; /* software path: the visualiser needs OpenGL */
}

bool display_screenshot(const uint32_t* fb, const char* path)
{
	(void)fb;
	(void)path;
	return false;
}
#endif
