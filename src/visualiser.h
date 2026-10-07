/* MilkDrop style visualiser: a feedback buffer warped on the GPU.
 *
 * Only built with OLISE_GL; without OpenGL there is no visualiser. */
#ifndef VISUALISER_H
#define VISUALISER_H

#include <stdint.h>

namespace vis {

/* Number of presets the user can cycle through. */
int preset_count();
const char* preset_name(int index);

/* Per frame: the mono waveform (oldest first), the master level 0..255 and
 * whether the music just hit a beat. Call before display_present(). */
struct Frame {
	const int16_t* samples;
	int sample_count;
	int level;        /* 0..255, drives zoom and brightness */
	bool beat;        /* a new row at the pattern's speed */
	int preset;       /* 0 .. preset_count()-1 */
	float seconds;    /* wall clock, drives the slow rotation */
	int screen_w;     /* output size, so the final draw covers the screen */
	int screen_h;
};

/* Draws the visualiser over the whole screen. Returns false and leaves the
   screen untouched when the GPU resources could not be created; the caller
   then falls back to the plain framebuffer. */
bool draw(const Frame& f);

/* Frees the textures. Safe to call when draw() was never used. */
void shutdown();

} /* namespace vis */

#endif
