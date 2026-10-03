#ifndef OLISE_NATIVE
/* SDL2 display backend: streaming texture scaled to the window, optional
   scanline overlay texture. Used by the desktop build and the websrv payload. */
#include "display.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;
static SDL_Texture* g_scanlines = nullptr;

bool display_init(char* err, size_t err_len)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		snprintf(err, err_len, "SDL video init failed: %s", SDL_GetError());
		return false;
	}
	g_window = SDL_CreateWindow("OliSe Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                            gfx::W * 2, gfx::H * 2, SDL_WINDOW_FULLSCREEN_DESKTOP);
	if (!g_window) {
		snprintf(err, err_len, "window failed: %s", SDL_GetError());
		return false;
	}
	g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
	if (!g_renderer) {
		snprintf(err, err_len, "renderer failed: %s", SDL_GetError());
		return false;
	}
	g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, gfx::W, gfx::H);
	if (!g_texture) {
		snprintf(err, err_len, "texture failed: %s", SDL_GetError());
		return false;
	}
	/* CRT scanlines at output resolution: one column, stretched full width. */
	int out_w = 0, out_h = 0;
	SDL_GetRendererOutputSize(g_renderer, &out_w, &out_h);
	if (out_h <= 0) out_h = 1080;
	g_scanlines = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, out_h);
	if (g_scanlines) {
		uint32_t* rows = (uint32_t*)malloc(out_h * sizeof(uint32_t));
		if (rows) {
			for (int i = 0; i < out_h; i++) rows[i] = (i % 3 == 2) ? 0x50000000 : 0x00000000;
			SDL_UpdateTexture(g_scanlines, NULL, rows, sizeof(uint32_t));
			free(rows);
		}
		SDL_SetTextureBlendMode(g_scanlines, SDL_BLENDMODE_BLEND);
	}
	return true;
}

void display_present(const uint32_t* fb, bool crt)
{
	SDL_UpdateTexture(g_texture, NULL, fb, gfx::W * sizeof(uint32_t));
	SDL_RenderClear(g_renderer);
	SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
	if (crt && g_scanlines) SDL_RenderCopy(g_renderer, g_scanlines, NULL, NULL);
	SDL_RenderPresent(g_renderer);
}

void display_shutdown()
{
	if (g_scanlines) SDL_DestroyTexture(g_scanlines);
	if (g_texture) SDL_DestroyTexture(g_texture);
	if (g_renderer) SDL_DestroyRenderer(g_renderer);
	if (g_window) SDL_DestroyWindow(g_window);
	g_scanlines = nullptr;
	g_texture = nullptr;
	g_renderer = nullptr;
	g_window = nullptr;
}

bool display_screenshot(const uint32_t* fb, const char* path)
{
	SDL_Surface* shot = SDL_CreateRGBSurfaceWithFormat(0, gfx::W, gfx::H, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!shot) return false;
	memcpy(shot->pixels, fb, gfx::W * gfx::H * sizeof(uint32_t));
	bool ok = SDL_SaveBMP(shot, path) == 0;
	SDL_FreeSurface(shot);
	return ok;
}
#endif
