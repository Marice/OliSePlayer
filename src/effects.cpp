#include "effects.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "logo_data.h"

namespace fx {

/* ---------------------------------------------------------------- stars */

constexpr int NUM_STARS = 220;
struct Star { float x, y, z; };
static Star stars[NUM_STARS];
static int warp_left = 0;

static float frand(float lo, float hi)
{
	return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

static void respawn(Star& s, bool anywhere)
{
	s.x = frand(-1.f, 1.f);
	s.y = frand(-1.f, 1.f);
	s.z = anywhere ? frand(0.05f, 1.f) : 1.f;
}

void init(unsigned seed)
{
	srand(seed);
	for (int i = 0; i < NUM_STARS; i++) respawn(stars[i], true);
}

void warp()
{
	warp_left = 70;
}

void stars_update(int energy)
{
	float speed = 0.0035f + energy * 0.00004f;
	if (warp_left > 0) {
		speed += 0.03f * (warp_left / 70.f);
		warp_left--;
	}
	for (int i = 0; i < NUM_STARS; i++) {
		stars[i].z -= speed;
		if (stars[i].z <= 0.03f) respawn(stars[i], false);
	}
}

void stars_draw(int y0, int y1)
{
	const float cx = gfx::W / 2.f, cy = (y0 + y1) / 2.f;
	const float sx = gfx::W * 0.55f, sy = (y1 - y0) * 0.9f;
	for (int i = 0; i < NUM_STARS; i++) {
		const Star& s = stars[i];
		int px = (int)(cx + s.x / s.z * sx);
		int py = (int)(cy + s.y / s.z * sy);
		if (px < 0 || px >= gfx::W || py < y0 || py > y1) continue;
		int b = (int)(255 * (1.f - s.z));
		if (b < 40) b = 40;
		uint32_t c = ((uint32_t)b << 16) | ((uint32_t)b << 8) | (uint32_t)(b > 215 ? 255 : b + 40);
		gfx::put(px, py, c);
		if (s.z < 0.25f) {
			gfx::put(px + 1, py, gfx::shade(c, 160));
			gfx::put(px, py + 1, gfx::shade(c, 160));
		}
		if (warp_left > 0) {
			/* Streak towards the centre during the warp. */
			int len = (int)(warp_left / 6.f * (1.f - s.z));
			for (int k = 1; k <= len; k++) {
				int tx = (int)(cx + s.x / (s.z + k * 0.01f) * sx);
				int ty = (int)(cy + s.y / (s.z + k * 0.01f) * sy);
				if (ty >= y0 && ty <= y1) gfx::put(tx, ty, gfx::shade(c, 256 - k * 256 / (len + 1)));
			}
		}
	}
}

/* ----------------------------------------------------------------- logo */

int logo_width() { return LOGO_W; }
int logo_height() { return LOGO_H; }

void logo(int cx, int cy, int frame, int level)
{
	int bob = level > 0 ? (int)(3.f * sinf(frame * 0.035f)) : 0;
	int x0 = cx - LOGO_W / 2;
	int y0 = cy - LOGO_H / 2 + bob;

	/* Drop shadow. */
	if (level > 0) {
		for (int y = 0; y < LOGO_H; y++) {
			for (int x = 0; x < LOGO_W; x++) {
				if (LOGO_PX[y * LOGO_W + x] >> 24) {
					int dx = x0 + x + 4, dy = y0 + y + 5;
					if ((unsigned)dx < (unsigned)gfx::W && (unsigned)dy < (unsigned)gfx::H)
						gfx::fb[dy * gfx::W + dx] = gfx::shade(gfx::fb[dy * gfx::W + dx], 90);
				}
			}
		}
	}

	/* Shine band sweeping left to right every ~6 seconds. */
	int period = 360;
	int band = (frame % period) * (LOGO_W + 160) / period - 80;
	for (int y = 0; y < LOGO_H; y++) {
		int dy = y0 + y;
		if ((unsigned)dy >= (unsigned)gfx::H) continue;
		for (int x = 0; x < LOGO_W; x++) {
			uint32_t s = LOGO_PX[y * LOGO_W + x];
			if (!(s >> 24)) continue;
			int dx = x0 + x;
			if ((unsigned)dx >= (unsigned)gfx::W) continue;
			uint32_t c = s & 0xffffff;
			if (level > 1) {
				int d = abs((x - y / 2) - band); /* slanted band */
				if (d < 14) {
					int lum = ((c >> 16) & 0xff) + ((c >> 8) & 0xff) + (c & 0xff);
					if (lum > 300) c = gfx::blend(c, gfx::WHITE, (14 - d) * 14);
				}
			}
			gfx::fb[dy * gfx::W + dx] = c;
		}
	}
}

/* -------------------------------------------------------------- twister */

constexpr int TW_SCALE = 3;
constexpr int TW_GLYPH = 8 * TW_SCALE;   /* 24 px */
constexpr int TW_RIBBON = 21;            /* ribbon half height at full twist */

static const uint32_t chrome8[8] = {
	0x9c9cb8, 0xd8d8ec, 0xf4f4ff, 0xc0c0d8, 0x707090, 0x585878, 0x9c9cb8, 0xd0d0e4
};

static uint32_t chrome_row(int r /* 0..23 */)
{
	float t = r / 23.f * 7.f;
	int i = (int)t;
	int f = (int)((t - i) * 256);
	return gfx::blend(chrome8[i], chrome8[i + 1 > 7 ? 7 : i + 1], f);
}

int twister_text_width(const char* text)
{
	return (int)strlen(text) * TW_GLYPH;
}

void twister(const char* text, float scroll_x, int frame, int base_y, int level)
{
	int len = (int)strlen(text);
	if (len == 0) return;
	float t = frame * 0.03f;
	float twist_speed = level > 1 ? 0.045f : 0.0f;
	float spatial = 0.014f;

	/* Pass 1: the twisting ribbon. Front side bluish, back side purple. */
	for (int x = 0; x < gfx::W; x++) {
		float cy = base_y + 9.f * sinf(x * 0.018f + t * 1.7f) + 5.f * sinf(x * 0.041f - t * 2.3f);
		float theta = x * spatial - frame * twist_speed;
		float s = level > 1 ? cosf(theta) : 1.f;
		float sn = level > 1 ? sinf(theta) : 0.f;
		float as = fabsf(s);
		if (as < 0.04f) continue;
		int half = (int)(TW_RIBBON * as);
		uint32_t band = s >= 0 ? 0x2c4c8c : 0x3a2c5c;
		int light = 170 + (int)(70 * sn);
		band = gfx::shade(band, light);
		int ytop = (int)(cy - half), ybot = (int)(cy + half);
		for (int y = ytop; y <= ybot; y++) {
			if ((unsigned)y >= (unsigned)gfx::H) continue;
			int edge = (y == ytop || y == ybot) ? 1 : 0;
			uint32_t c = edge ? gfx::blend(band, gfx::WHITE, 90) : band;
			gfx::fb[y * gfx::W + x] = gfx::blend(gfx::fb[y * gfx::W + x], c, edge ? 230 : 150);
		}
	}

	/* Pass 2: big chrome letters riding the same wave, never squashed.
	   Each glyph follows the ribbon centre at its own middle column, with a
	   full black outline so it reads on any background. */
	uint32_t rows[TW_GLYPH];
	for (int r = 0; r < TW_GLYPH; r++) rows[r] = chrome_row(r);
	for (int ci = 0; ci < len; ci++) {
		int gx = (int)floorf(scroll_x) + ci * TW_GLYPH;
		if (gx + TW_GLYPH < 0 || gx >= gfx::W) continue;
		unsigned char ch = (unsigned char)text[ci];
		if (ch == ' ' || ch > 127) continue;
		float mx = gx + TW_GLYPH / 2.f;
		float cy = base_y + 7.f * sinf(mx * 0.018f + t * 1.7f) + 3.f * sinf(mx * 0.041f - t * 2.3f);
		int gy = (int)(cy - TW_GLYPH / 2);
		gfx::glyph_scaled(gx, gy, ch, TW_SCALE, rows, gfx::BLACK, true);
	}
}

/* ---------------------------------------------------------- raster bars */

void raster_bars(int x, int y0, int w, int y1, int frame, int energy, int level)
{
	if (level <= 0) return;
	static const uint32_t cols[4] = { 0x30406c, 0x4a3a7a, 0x2c5c7c, 0x5a4a8a };
	int bars = level > 1 ? 4 : 2;
	int span = y1 - y0;
	for (int b = 0; b < bars; b++) {
		float ph = frame * (0.012f + b * 0.003f) + b * 1.9f;
		int cy = y0 + span / 2 + (int)((span / 2 - 10) * sinf(ph));
		int half = 5 + energy / 12;
		for (int y = cy - half; y <= cy + half; y++) {
			if (y < y0 || y > y1) continue;
			int d = abs(y - cy);
			int a = (half - d) * 110 / (half + 1) + 20;
			gfx::fill_alpha(x, y, w, 1, cols[b], a);
		}
	}
}

/* ---------------------------------------------------------- scope / VU */

void scope(int x, int y, int w, int h, const int16_t* samples, int n)
{
	int mid = y + h / 2;
	gfx::hline(x, x + w - 1, mid, gfx::PANEL_DEEP);
	int prev_y = mid;
	for (int i = 0; i < w; i++) {
		int si = i * n / w;
		int v = samples[si] * (h / 2 - 1) / 32768;
		int yy = mid - v;
		if (yy < y) yy = y;
		if (yy > y + h - 1) yy = y + h - 1;
		if (i > 0) gfx::vline(x + i, prev_y, yy, gfx::SCOPE);
		else gfx::put(x + i, yy, gfx::SCOPE);
		prev_y = yy;
	}
}

void vu_bars(int x, int y, int w, int h, const Snapshot& s)
{
	int n = s.chn > 0 ? s.chn : 1;
	if (n > 32) n = 32;
	int bw = w / n;
	if (bw < 3) bw = 3;
	int inner = bw > 6 ? bw - 2 : bw - 1;
	for (int c = 0; c < n; c++) {
		int bx = x + c * bw;
		int vh = s.ch[c].volume * (h - 2) / 64;
		int ph = s.ch[c].peak * (h - 2) / 64;
		for (int k = 0; k < vh; k++) {
			int t = k * 256 / (h - 1);
			uint32_t col = gfx::blend(gfx::VU_LOW, gfx::VU_HIGH, t);
			if (t > 200) col = gfx::blend(col, gfx::VU_PEAK, (t - 200) * 4);
			gfx::hline(bx, bx + inner - 1, y + h - 2 - k, col);
		}
		if (ph > 0) gfx::hline(bx, bx + inner - 1, y + h - 2 - ph, gfx::WHITE);
	}
}

} /* namespace fx */
