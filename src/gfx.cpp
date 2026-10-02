#include "gfx.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "font8x8_basic.h"

namespace gfx {

uint32_t fb[W * H];

uint32_t blend(uint32_t a, uint32_t b, int t)
{
	if (t <= 0) return a;
	if (t >= 256) return b;
	int s = 256 - t;
	uint32_t r = (((a >> 16) & 0xff) * s + ((b >> 16) & 0xff) * t) >> 8;
	uint32_t g = (((a >> 8) & 0xff) * s + ((b >> 8) & 0xff) * t) >> 8;
	uint32_t bl = ((a & 0xff) * s + (b & 0xff) * t) >> 8;
	return (r << 16) | (g << 8) | bl;
}

uint32_t shade(uint32_t c, int mul)
{
	int r = (((c >> 16) & 0xff) * mul) >> 8;
	int g = (((c >> 8) & 0xff) * mul) >> 8;
	int b = ((c & 0xff) * mul) >> 8;
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void clear(uint32_t c)
{
	for (int i = 0; i < W * H; i++) fb[i] = c;
}

void hline(int x0, int x1, int y, uint32_t c)
{
	if ((unsigned)y >= (unsigned)H) return;
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (x0 < 0) x0 = 0;
	if (x1 >= W) x1 = W - 1;
	uint32_t* p = fb + y * W;
	for (int x = x0; x <= x1; x++) p[x] = c;
}

void vline(int x, int y0, int y1, uint32_t c)
{
	if ((unsigned)x >= (unsigned)W) return;
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	if (y0 < 0) y0 = 0;
	if (y1 >= H) y1 = H - 1;
	for (int y = y0; y <= y1; y++) fb[y * W + x] = c;
}

void rect(int x, int y, int w, int h, uint32_t c)
{
	if (w <= 0 || h <= 0) return;
	hline(x, x + w - 1, y, c);
	hline(x, x + w - 1, y + h - 1, c);
	vline(x, y, y + h - 1, c);
	vline(x + w - 1, y, y + h - 1, c);
}

void fill(int x, int y, int w, int h, uint32_t c)
{
	for (int yy = y; yy < y + h; yy++) hline(x, x + w - 1, yy, c);
}

void fill_alpha(int x, int y, int w, int h, uint32_t c, int alpha)
{
	if (alpha <= 0) return;
	if (alpha >= 256) { fill(x, y, w, h, c); return; }
	int x0 = x < 0 ? 0 : x, x1 = x + w > W ? W : x + w;
	int y0 = y < 0 ? 0 : y, y1 = y + h > H ? H : y + h;
	for (int yy = y0; yy < y1; yy++) {
		uint32_t* p = fb + yy * W;
		for (int xx = x0; xx < x1; xx++) p[xx] = blend(p[xx], c, alpha);
	}
}

void dim(int x, int y, int w, int h, int keep)
{
	int x0 = x < 0 ? 0 : x, x1 = x + w > W ? W : x + w;
	int y0 = y < 0 ? 0 : y, y1 = y + h > H ? H : y + h;
	for (int yy = y0; yy < y1; yy++) {
		uint32_t* p = fb + yy * W;
		for (int xx = x0; xx < x1; xx++) p[xx] = shade(p[xx], keep);
	}
}

void bevel(int x, int y, int w, int h, bool raised)
{
	uint32_t light = raised ? PANEL_LIGHT : PANEL_DARK;
	uint32_t dark = raised ? PANEL_DARK : PANEL_LIGHT;
	fill(x, y, w, h, PANEL);
	hline(x, x + w - 1, y, light);
	vline(x, y, y + h - 1, light);
	hline(x, x + w - 1, y + h - 1, dark);
	vline(x + w - 1, y, y + h - 1, dark);
	/* FT2 has a second, softer inner edge. */
	hline(x + 1, x + w - 2, y + 1, blend(PANEL, light, 128));
	vline(x + 1, y + 1, y + h - 2, blend(PANEL, light, 128));
	hline(x + 1, x + w - 2, y + h - 2, blend(PANEL, dark, 128));
	vline(x + w - 2, y + 1, y + h - 2, blend(PANEL, dark, 128));
}

void field(int x, int y, int w, int h)
{
	fill(x, y, w, h, BLACK);
	hline(x, x + w - 1, y, PANEL_DARK);
	vline(x, y, y + h - 1, PANEL_DARK);
	hline(x, x + w - 1, y + h - 1, PANEL_LIGHT);
	vline(x + w - 1, y, y + h - 1, PANEL_LIGHT);
}

const unsigned char* glyph(unsigned char ch)
{
	return (ch < 128) ? font8x8_basic[ch] : font8x8_basic[(unsigned char)'?'];
}

static inline const unsigned char* glyph_rows(unsigned char ch) { return glyph(ch); }

void text(int x, int y, const char* s, uint32_t c)
{
	for (; *s; s++, x += 8) {
		unsigned char ch = (unsigned char)*s;
		if (ch == ' ') continue;
		const unsigned char* g = glyph_rows(ch);
		for (int row = 0; row < 8; row++) {
			int bits = (unsigned char)g[row];
			if (!bits) continue;
			int yy = y + row;
			if ((unsigned)yy >= (unsigned)H) continue;
			for (int col = 0; col < 8; col++) {
				if (bits & (1 << col)) put(x + col, yy, c);
			}
		}
	}
}

void glyph_scaled(int x, int y, unsigned char ch, int scale, const uint32_t* row_colors, uint32_t outline, bool with_outline)
{
	const unsigned char* g = glyph_rows(ch);
	if (with_outline) {
		for (int row = 0; row < 8; row++) {
			int bits = (unsigned char)g[row];
			if (!bits) continue;
			for (int col = 0; col < 8; col++) {
				if (!(bits & (1 << col))) continue;
				fill(x + col * scale - 1, y + row * scale - 1, scale + 2, scale + 2, outline);
			}
		}
	}
	for (int row = 0; row < 8; row++) {
		int bits = (unsigned char)g[row];
		if (!bits) continue;
		for (int sy = 0; sy < scale; sy++) {
			uint32_t c = row_colors[row * scale + sy];
			int yy = y + row * scale + sy;
			for (int col = 0; col < 8; col++) {
				if (!(bits & (1 << col))) continue;
				hline(x + col * scale, x + col * scale + scale - 1, yy, c);
			}
		}
	}
}

void text_scaled(int x, int y, const char* s, int scale, uint32_t c)
{
	uint32_t rows[8 * 8];
	for (int i = 0; i < 8 * scale && i < 64; i++) rows[i] = c;
	for (; *s; s++, x += 8 * scale) {
		if (*s == ' ') continue;
		glyph_scaled(x, y, (unsigned char)*s, scale, rows, BLACK, false);
	}
}

void text_outlined(int x, int y, const char* s, uint32_t c, uint32_t outline)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (dx || dy) text(x + dx, y + dy, s, outline);
	text(x, y, s, c);
}

void textf(int x, int y, uint32_t c, const char* fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	text(x, y, buf, c);
}

int text_width(const char* s)
{
	return (int)strlen(s) * 8;
}

void blit(int x, int y, int w, int h, const uint32_t* px, int mul)
{
	for (int yy = 0; yy < h; yy++) {
		int dy = y + yy;
		if ((unsigned)dy >= (unsigned)H) continue;
		const uint32_t* src = px + yy * w;
		uint32_t* dst = fb + dy * W;
		for (int xx = 0; xx < w; xx++) {
			int dx = x + xx;
			if ((unsigned)dx >= (unsigned)W) continue;
			uint32_t s = src[xx];
			int a = (int)(s >> 24);
			if (!a) continue;
			uint32_t c = mul == 256 ? (s & 0xffffff) : shade(s & 0xffffff, mul);
			dst[dx] = a >= 255 ? c : blend(dst[dx], c, a + 1);
		}
	}
}

} /* namespace gfx */
