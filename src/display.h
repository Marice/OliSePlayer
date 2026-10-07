/* Output of the 640x360 framebuffer to the screen.
 *
 * Two backends: SDL2 window + software renderer (desktop and websrv payload),
 * and direct VideoOut for the native PS5 title, where SDL's video driver
 * cannot run inside the sandbox. */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/* Opens the display. On failure fills err and returns false. */
bool display_init(char* err, size_t err_len);
/* Shows one frame; crt adds dark scanlines. Blocks until the frame is queued. */
void display_present(const uint32_t* fb, bool crt);
/* True when this backend can run the visualiser (OpenGL only). */
bool display_has_gpu();

#ifdef OLISE_GL
/* Starts the preset-based visualiser. Call after the app has asked for
   elevation: it reads a folder, which a sandboxed title may not do before. */
void display_start_visualiser();

namespace vis { struct Frame; }
/* Run the visualiser behind the interface on the next display_present().
   Pass nullptr to turn it off. interface_opacity 0..1 fades the interface;
   at less than 1 its black background drops out so the effect shows through. */
void display_set_visualiser(const vis::Frame* frame, float interface_opacity);
#endif
void display_shutdown();
/* Desktop only: saves the last presented framebuffer as BMP. */
bool display_screenshot(const uint32_t* fb, const char* path);

#endif
