/* Buttons and keys.
 *
 * Overlays are handled first and return early: while the file list or a
 * picker is open the same buttons mean something else. */
#include "app.h"

#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "display.h"
#include "stations.h"
#include "ui.h"
#ifdef OLISE_GL
#include "visualiser.h"
#ifdef OLISE_PROJECTM
#include "milkdrop.h"
#endif
#endif

/* One button for the whole look. Square walks through the demo effects and
   then the visualiser, so everything that changes how busy the screen is sits
   on a single key instead of a chord. Without OpenGL the visualiser steps are
   skipped, which leaves the three effect levels this app always had. */
static void cycle_look(App& a)
{
#ifdef OLISE_GL
	/* Square is the visualiser button: off, behind the interface, full screen.
	   The demo effects keep their own level, so the scroller and the starfield
	   are not disturbed by looking for the visualiser. */
	if (display_has_gpu()) {
		a.vis_mode = (a.vis_mode + 1) % 3;
		if (a.vis_mode == 0) show_toast(a, "VISUALISER", "OFF");
		else show_toast(a, a.vis_mode == 1 ? "VISUALISER" : "VISUALISER FULL",
		                vis::preset_name(a.vis_preset));
		return;
	}
#endif
	/* Without OpenGL there is no visualiser, so the button keeps its old job. */
	a.fx_level = (a.fx_level + 2) % 3;
	show_toast(a, "EFFECTS", a.fx_level == 2 ? "FULL" : (a.fx_level == 1 ? "CALM" : "OFF"));
}

void handle_button(App& a, int b, bool& running)
{
	if (a.source_picker) {
		switch (b) {
		case BTN_DUP:   if (a.source_sel > 0) a.source_sel--; break;
		case BTN_DDOWN: if (a.source_sel + 1 < NUM_SOURCES) a.source_sel++; break;
		case BTN_CROSS:
			/* Picking a source opens its stations: the two steps are one
			   decision, so the user should not have to press Triangle again. */
			a.radio.set_source(a.source_sel);
			a.source_picker = false;
			a.picker = true;
			a.picker_sel = a.radio.station();
			a.picker_first = 0;
			break;
		case BTN_CIRCLE:
		case BTN_TRIANGLE:
		case BTN_TOUCHPAD:
			a.source_picker = false;
			break;
		}
		return;
	}
	if (a.picker) {
		switch (b) {
		case BTN_DUP:   if (a.picker_sel > 0) a.picker_sel--; break;
		case BTN_DDOWN: if (a.picker_sel + 1 < station_count_for(a.radio.source())) a.picker_sel++; break;
		case BTN_L1:    a.picker_sel = a.picker_sel > 10 ? a.picker_sel - 10 : 0; break;
		case BTN_R1: {
			const int n = station_count_for(a.radio.source());
			a.picker_sel = a.picker_sel + 10 < n ? a.picker_sel + 10 : n - 1;
			break;
		}
		case BTN_CROSS:
			tune_in(a, a.picker_sel);
			a.picker = false;
			break;
		case BTN_CIRCLE:
			/* Back to the source list rather than straight out: that is where
			   this screen was opened from. */
			a.picker = false;
			a.source_picker = true;
			a.source_sel = a.radio.source();
			break;
		case BTN_TRIANGLE:
		case BTN_TOUCHPAD:
			a.picker = false;
			break;
		}
		int lines = 18;
		if (a.picker_sel < a.picker_first) a.picker_first = a.picker_sel;
		if (a.picker_sel >= a.picker_first + lines) a.picker_first = a.picker_sel - lines + 1;
		return;
	}
	if (a.browser) {
		switch (b) {
		case BTN_DUP:   if (a.browser_sel > 0) a.browser_sel--; break;
		case BTN_DDOWN: if (a.browser_sel + 1 < a.library.count()) a.browser_sel++; break;
		case BTN_CROSS:
			if (play_local(a, a.browser_sel)) a.browser = false;
			break;
		case BTN_CIRCLE:
		case BTN_TOUCHPAD:
			a.browser = false;
			break;
		}
		int lines = 18;
		if (a.browser_sel < a.browser_first) a.browser_first = a.browser_sel;
		if (a.browser_sel >= a.browser_first + lines) a.browser_first = a.browser_sel - lines + 1;
		return;
	}
	if (a.help) {
		a.help = false;
		return;
	}
	switch (b) {
	case BTN_R3:       next_track(a); break;
	case BTN_L3:       prev_track(a); break;
	case BTN_CROSS:    a.player.set_paused(!a.player.paused()); break;
	case BTN_CIRCLE:
		/* Held with L2 it keeps the playing radio track in music/; on its own
		   it opens the local file list. */
		if (a.l2_down) {
			keep_current_track(a);
			break;
		}
		a.library.scan(a.appdir);
		a.browser = true;
		a.browser_sel = a.library.current();
		break;
	case BTN_L1:       if (a.library.count()) play_local(a, a.library.prev()); break;
	case BTN_R1:       if (a.library.count()) play_local(a, a.library.next()); break;
	case BTN_TRIANGLE:
#ifdef OLISE_GL
		/* While the visualiser is on, Triangle picks the next preset: that is
		   what the user is looking at, so that is what the button acts on.
		   With the visualiser off it opens the genre picker as it always did. */
		if (a.vis_mode > 0) {
			/* Held with L2, Triangle jumps back instead of forward: with
			   hundreds of presets, stepping past the one you wanted is
			   otherwise a very long way round. */
			const int delta = a.l2_down ? -1 : 1;
			/* Picking by hand restarts the clock: an automatic change right
			   after a deliberate one is just annoying. */
			a.vis_auto_frames = a.vis_auto_seconds * FPS;
#ifdef OLISE_PROJECTM
			if (milkdrop::available()) {
				/* With hundreds of presets, stepping is for browsing and R2
				   is for getting somewhere else entirely. */
				if (a.r2_down) milkdrop::shuffle();
				else milkdrop::step(delta);
				show_toast(a, "PRESET", milkdrop::current_name());
				break;
			}
#endif
			const int n = vis::preset_count();
			a.vis_preset = (a.vis_preset + (delta > 0 ? 1 : n - 1)) % n;
			show_toast(a, "PRESET", vis::preset_name(a.vis_preset));
			break;
		}
#endif
		a.source_picker = true;
		a.source_sel = a.radio.source();
		break;
	case BTN_OPTIONS:
#ifdef OLISE_GL
		/* Held with L2 it turns the automatic preset change on or off; on its
		   own it keeps swapping the right-hand panel. */
		if (a.l2_down) {
			a.vis_auto_seconds = a.vis_auto_seconds > 0 ? 0 : 60;
			a.vis_auto_frames = a.vis_auto_seconds * FPS;
			show_toast(a, "AUTO PRESET", a.vis_auto_seconds > 0 ? "EVERY MINUTE" : "OFF");
			break;
		}
#endif
		a.show_scopes = !a.show_scopes;
		break;
	case BTN_SQUARE:   cycle_look(a); break;
	case BTN_DLEFT:    if (a.ch_offset > 0) a.ch_offset--; break;
	case BTN_DRIGHT:   a.ch_offset++; break;
	case BTN_DUP:
		if (a.l2_down) a.player.seek_positions(1);
		else a.player.set_volume(a.player.volume() + 5);
		break;
	case BTN_DDOWN:
		if (a.l2_down) a.player.seek_positions(-1);
		else a.player.set_volume(a.player.volume() - 5);
		break;
	case BTN_TOUCHPAD: a.help = true; break;
	default:
		fprintf(stderr, "input: unmapped joystick button %d\n", b);
		break;
	}
	(void)running;
}

void handle_key(App& a, SDL_Keycode k, bool& running)
{
	switch (k) {
	case SDLK_ESCAPE: running = false; break;
	case SDLK_n:      handle_button(a, BTN_R3, running); break;
	case SDLK_p:      handle_button(a, BTN_L3, running); break;
	case SDLK_SPACE:  handle_button(a, BTN_CROSS, running); break;
	case SDLK_o:      handle_button(a, BTN_CIRCLE, running); break;
	case SDLK_COMMA:  handle_button(a, BTN_L1, running); break;
	case SDLK_PERIOD: handle_button(a, BTN_R1, running); break;
	case SDLK_s:      handle_button(a, BTN_OPTIONS, running); break;
	case SDLK_g:      handle_button(a, BTN_TRIANGLE, running); break;
	case SDLK_f:      handle_button(a, BTN_SQUARE, running); break;
	case SDLK_c:      a.crt = !a.crt; break;
	case SDLK_k:      keep_current_track(a); break;
#ifdef OLISE_GL
	case SDLK_b:
		a.vis_preset = (a.vis_preset + 1) % vis::preset_count();
		show_toast(a, "PRESET", vis::preset_name(a.vis_preset));
		break;
#endif
	case SDLK_LEFT:   handle_button(a, BTN_DLEFT, running); break;
	case SDLK_RIGHT:  handle_button(a, BTN_DRIGHT, running); break;
	case SDLK_UP:     handle_button(a, BTN_DUP, running); break;
	case SDLK_DOWN:   handle_button(a, BTN_DDOWN, running); break;
	case SDLK_RETURN:
		if (a.browser || a.picker) handle_button(a, BTN_CROSS, running);
		else a.card_frames = 6 * FPS;
		break;
	case SDLK_h:      handle_button(a, BTN_TOUCHPAD, running); break;
	}
}

/* Directory of the executable, so music/ next to eboot.elf is found. */
