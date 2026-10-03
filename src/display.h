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
void display_shutdown();
/* Desktop only: saves the last presented framebuffer as BMP. */
bool display_screenshot(const uint32_t* fb, const char* path);

#endif
