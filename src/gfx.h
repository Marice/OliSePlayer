/* Software framebuffer and FastTracker II style drawing primitives. */
#ifndef GFX_H
#define GFX_H

#include <stddef.h>
#include <stdint.h>

namespace gfx {

constexpr int W = 640;
constexpr int H = 360;

extern uint32_t fb[W * H];

/* The FT2 palette: greys with a blue tint, one blue highlight bar. */
constexpr uint32_t BLACK       = 0x000000;
constexpr uint32_t PANEL       = 0x7b7b9c;
constexpr uint32_t PANEL_LIGHT = 0xbcbcd4;
constexpr uint32_t PANEL_DARK  = 0x4a4a66;
constexpr uint32_t PANEL_DEEP  = 0x2e2e44;
constexpr uint32_t TEXT        = 0xe0e0ff;
constexpr uint32_t TEXT_DIM    = 0x8080a0;
constexpr uint32_t TEXT_DARK   = 0x50506a;
constexpr uint32_t HILITE      = 0x3c5c9c;
constexpr uint32_t HILITE_EDGE = 0x5c80c8;
constexpr uint32_t ROWNUM      = 0xc0c0e0;
constexpr uint32_t WHITE       = 0xffffff;
constexpr uint32_t NOTE        = 0xf0f0ff;
constexpr uint32_t INSTR       = 0xa0c8ff;
constexpr uint32_t VOLUME      = 0x80e0a0;
constexpr uint32_t EFFECT      = 0xe0c080;
constexpr uint32_t SCOPE       = 0x80ff80;
constexpr uint32_t VU_LOW      = 0x40c040;
constexpr uint32_t VU_HIGH     = 0xe0e040;
constexpr uint32_t VU_PEAK     = 0xff6040;

inline void put(int x, int y, uint32_t c)
{
	if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = c;
}

uint32_t blend(uint32_t a, uint32_t b, int t256);   /* a*(1-t) + b*t */
uint32_t shade(uint32_t c, int mul256);             /* c * mul/256, clamped */

void clear(uint32_t c);
void hline(int x0, int x1, int y, uint32_t c);
void vline(int x, int y0, int y1, uint32_t c);
void rect(int x, int y, int w, int h, uint32_t c);
void fill(int x, int y, int w, int h, uint32_t c);
void fill_alpha(int x, int y, int w, int h, uint32_t c, int alpha256);
void dim(int x, int y, int w, int h, int keep256);

/* FT2 panel: flat fill with a light top/left and dark bottom/right edge. */
void bevel(int x, int y, int w, int h, bool raised = true);
/* Sunken black field inside a panel (pattern area, list boxes). */
void field(int x, int y, int w, int h);

/* 8x8 text. Characters outside 32..126 are skipped. */
void text(int x, int y, const char* s, uint32_t c);
void text_scaled(int x, int y, const char* s, int scale, uint32_t c);
void text_outlined(int x, int y, const char* s, uint32_t c, uint32_t outline = BLACK);
void textf(int x, int y, uint32_t c, const char* fmt, ...);
int text_width(const char* s);
/* Raw 8 byte-rows of a glyph (bit n = column n). */
const unsigned char* glyph(unsigned char ch);
/* Draw one glyph column-row at any scale with a per-row colour table. */
void glyph_scaled(int x, int y, unsigned char ch, int scale, const uint32_t* row_colors, uint32_t outline, bool with_outline);

/* ARGB blit with alpha; mul256 darkens/brightens (256 = unchanged). */
void blit(int x, int y, int w, int h, const uint32_t* px, int mul256 = 256);

} /* namespace gfx */

#endif
