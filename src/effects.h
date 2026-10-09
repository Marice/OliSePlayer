/* Demoscene effects: starfield, logo, twister scroller, raster bars, scope. */
#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>

#include "player.h"

namespace fx {

void init(unsigned seed);

/* 3D starfield in the band y0..y1. energy 0..64 nudges the speed. */
void stars_update(int energy);
void stars_draw(int y0, int y1);
void warp(); /* short hyperspace burst, e.g. on a track change */

/* The chrome logo centred on cx, cy, bobbing and with a shine sweep. */
void logo(int cx, int cy, int frame, int level);
int logo_width();
int logo_height();

/* Twisting ribbon with 3x chrome text on it. base_y is the ribbon centre. */
/* fade 0..256 dims the whole ribbon, for handing over to a new line. */
void twister(const char* text, float scroll_x, int frame, int base_y, int level, int fade = 256);
int twister_text_width(const char* text);

/* Copper bars inside y0..y1, pulsing with the music energy. */
void raster_bars(int x, int y0, int w, int y1, int frame, int energy, int level);

/* Oscilloscope and VU meters, drawn into a black field. */
void scope(int x, int y, int w, int h, const int16_t* samples, int n);
void vu_bars(int x, int y, int w, int h, const Snapshot& s);

} /* namespace fx */

#endif
