/* Real MilkDrop presets through projectM.
 *
 * projectM (LGPL-2.1) runs the original .milk presets, scripts and shaders
 * included. It is optional: when the library is missing, fails to start or
 * finds no presets, the app falls back to the built-in visualiser, so a
 * problem here never costs more than the fancier effect.
 *
 * Only built with OLISE_PROJECTM. */
#ifndef MILKDROP_H
#define MILKDROP_H

#include <stdint.h>

namespace milkdrop {

/* Starts projectM and scans the preset folders. Returns false when it cannot
   run, which is not an error: the caller then keeps the built-in visualiser.
   Must be called with the OpenGL context current. */
bool init(int screen_w, int screen_h);

/* True once init() succeeded and at least one preset loaded. */
bool available();

/* Number of presets found, and the name of the one playing. */
int count();
const char* current_name();

/* Switches preset. delta +1 is the next one, -1 the previous. */
void step(int delta);
/* Picks a random preset, which is how MilkDrop itself moves on. */
void shuffle();

/* Feeds the latest audio. Mono samples, as the oscilloscope ring provides. */
void add_audio(const int16_t* samples, int count);

/* Renders one frame over the whole screen. */
void render();

void shutdown();

} /* namespace milkdrop */

#endif
