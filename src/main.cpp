/* OliSe Player: a FastTracker II flavoured tracker radio for the PS5.
 *
 * Plays MOD / XM / S3M / IT modules (libxmp-lite), streams random tracks
 * from The Mod Archive, and shows the pattern data live, demoscene style.
 */
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dbg.h"
#include "display.h"
#include "version.h"
#include "effects.h"
#ifdef OLISE_GL
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#include "appdir.h"
#include "elevation.hpp"
#include "visualiser.h"
#ifdef OLISE_PROJECTM
#include "milkdrop.h"
#endif
#endif
#include "app.h"
#include "gfx.h"
#include "library.h"
#include "player.h"
#include "radio.h"
#include "stations.h"
#include "ui.h"


void show_toast(App& a, const char* l1, const char* l2)
{
	strncpy(a.toast1, l1, sizeof(a.toast1) - 1);
	strncpy(a.toast2, l2 ? l2 : "", sizeof(a.toast2) - 1);
	a.toast_frames = 4 * FPS;
}


static void app_dir(const char* argv0, char* out, size_t n)
{
	out[0] = 0;
	if (!argv0) return;
	const char* slash = strrchr(argv0, '/');
	if (!slash) return;
	size_t len = (size_t)(slash - argv0);
	if (len >= n) len = n - 1;
	memcpy(out, argv0, len);
	out[len] = 0;
}

/* Keep a fatal error on screen long enough to read before the title exits. */
static int fatal(const char* what)
{
	dbg_error("%s: %s", what, SDL_GetError());
	SDL_Delay(4000);
	return 1;
}

int main(int argc, char** argv)
{
	dbg_checkpoint("start (argc=%d)", argc);
	SDL_SetMainReady();
	dbg_checkpoint("main ready");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	dbg_checkpoint("hints set");
	/* Initialise the SDL subsystems one at a time so a failing one is named. */
	if (SDL_Init(0) != 0) return fatal("SDL core init failed");
	dbg_checkpoint("sdl core ok");
	if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) return fatal("SDL timer init failed");
	dbg_checkpoint("sdl timer ok");
	if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) return fatal("SDL events init failed");
	dbg_checkpoint("sdl events ok");
	/* Display: SDL window on the desktop/payload, direct VideoOut in the title. */
	char derr[160];
	if (!display_init(derr, sizeof(derr))) {
		dbg_error("%s", derr);
		SDL_Delay(4000);
		return 1;
	}
	dbg_checkpoint("display ok");
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) dbg_error("joystick init failed: %s", SDL_GetError());
	else dbg_checkpoint("sdl joystick ok");
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) dbg_error("audio init failed: %s", SDL_GetError());
	else dbg_checkpoint("sdl audio ok");
	SDL_Joystick* joy = SDL_JoystickOpen(0);
	dbg_checkpoint("joystick %s", joy ? "opened" : "not found");

	/* App holds a few hundred KB of tables; keep it off the (small) main stack. */
	App* app = new App();
	App& a = *app;
	app_dir(argc > 0 ? argv[0] : nullptr, a.appdir, sizeof(a.appdir));
#ifdef OLISE_NATIVE
	/* Native title: the app folder is mounted read-only at /app0. */
	if (!a.appdir[0]) strncpy(a.appdir, "/app0", sizeof(a.appdir) - 1);
#endif
	fx::init((unsigned)time(NULL));
	dbg_checkpoint("app dir '%s'", a.appdir);
#ifdef OLISE_GL
	/* ShadowMountPlus 1.7 lets a sandboxed title open files but refuses to
	   list a folder (EPERM), which leaves music/ and presets/ unreadable.
	   The boilerplate's elevation client lifts that: it hands lapy.elf to the
	   local elfldr on port 9021 and waits until /data lists. Failure is not
	   fatal, the app simply has no local files then. */
	{
		/* etaHEN offers jailbreak-on-demand: a title writes its pid to
		   /download0/etahen_jailbreak inside its own sandbox, and the daemon
		   raises that process's credentials and removes the file. That is
		   what lifts the EPERM on listing folders. */
		bool elevated = false;
		FILE* req = fopen("/download0/etahen_jailbreak", "w");
		if (req) {
			fprintf(req, "%d", (int)getpid());
			fclose(req);
			/* The daemon polls, so give it a moment and check by listing a
			   folder that was refused before. */
			for (int i = 0; i < 40 && !elevated; i++) {
				SDL_Delay(50);
				DIR* probe = opendir("/data");
				if (probe) { closedir(probe); elevated = true; }
			}
			dbg_log("elevation: etahen request %s after %s",
			        elevated ? "granted" : "ignored", elevated ? "poll" : "2s");
		} else {
			dbg_log("elevation: cannot write /download0/etahen_jailbreak (errno=%d)", errno);
		}

		if (!elevated) {
			/* Fall back to the boilerplate client, which sends a helper ELF
			   to the local elfldr. */
			char helper[256];
			snprintf(helper, sizeof(helper), "%s/lapy.elf", app_folder());
			const elevation::Status st =
			    elevation::request(elevation::Capability::filesystem, helper);
			dbg_log("elevation: client status=%u via %s", (unsigned)st, elevation::path());
			elevated = st == elevation::Status::ok;
		}
		if (!elevated) dbg_log("elevation: unavailable, using index.txt for local files");
	}
	/* Only now may folders be read, so this is where projectM scans presets. */
	display_start_visualiser();
#endif
	if (!a.player.init()) dbg_error("player init failed, continuing without audio: %s", SDL_GetError());
	else dbg_checkpoint("player ok");
	a.library.scan(a.appdir);
	dbg_checkpoint("library ok (%d files)", a.library.count());

	/* Start: a local file if there is one, and kick off the radio. */
	if (a.library.count() > 0) play_local(a, 0);
	if (Radio::available()) start_radio_fetch(a);
	else show_toast(a, "DESKTOP BUILD: NO MOD ARCHIVE", "USE LOCAL FILES");

	/* OLISE_SCREENSHOT=<path.bmp> saves a frame after a few seconds and exits
	   (used for README screenshots and headless checks). */
	const char* shot_path = SDL_getenv("OLISE_SCREENSHOT");
	int shot_frame = 240;
	if (SDL_getenv("OLISE_SCREENSHOT_FRAME")) shot_frame = atoi(SDL_getenv("OLISE_SCREENSHOT_FRAME"));

	int16_t scope_buf[SCOPE_SAMPLES];
	bool running = true;
	Uint64 perf_freq = SDL_GetPerformanceFrequency();
	Uint64 next_frame = SDL_GetPerformanceCounter();
	Uint64 frame_budget = perf_freq / FPS;
	Uint32 fps_t0 = SDL_GetTicks();
	int fps_frames = 0;

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) {
				running = false;
			} else if (ev.type == SDL_JOYBUTTONDOWN) {
				handle_button(a, ev.jbutton.button, running);
			} else if (ev.type == SDL_JOYAXISMOTION) {
				if (ev.jaxis.axis == AXIS_R2) {
					bool down = ev.jaxis.value > 20000;
					if (down && !a.r2_down && !a.l2_down) a.crt = !a.crt;
					a.r2_down = down;
				} else if (ev.jaxis.axis == AXIS_L2) {
					a.l2_down = ev.jaxis.value > 20000;
				}
			} else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
				handle_key(a, ev.key.keysym.sym, running);
			}
		}

		poll_radio(a);
		if (a.player.track_ended()) {
			if (a.radio_mode && a.source == Source::Radio) {
				next_track(a);
			} else if (a.source == Source::Local && a.library.count() > 0) {
				play_local(a, a.library.next());
			}
		}

		/* Snapshot the player state once per frame. */
		a.player.snapshot(a.snap);
		Uint32 now = SDL_GetTicks();
		if (a.snap.row != a.last_row || a.snap.pos != a.last_pos) {
			a.last_row = a.snap.row;
			a.last_pos = a.snap.pos;
			a.row_change_ms = now;
		}
		float row_frac = 0.f;
		if (a.snap.loaded && !a.snap.paused && a.snap.speed > 0 && a.snap.frame_time_us > 0) {
			float row_ms = a.snap.speed * a.snap.frame_time_us / 1000.f;
			row_frac = (now - a.row_change_ms) / row_ms;
			if (row_frac > 1.f) row_frac = 1.f;
			if (row_frac < 0.f) row_frac = 0.f;
		}
		int energy = 0;
		for (int c = 0; c < a.snap.chn; c++) energy += a.snap.ch[c].volume;
		if (a.snap.chn) energy /= a.snap.chn;

#ifdef OLISE_GL
		/* The visualiser runs on the GPU behind the interface. In full mode
		   only the overlays stay on top, so the effect fills the screen. */
		vis::Frame vf = {};
		bool vis_on = a.vis_mode > 0 && display_has_gpu();
		if (vis_on) {
			a.player.scope(scope_buf, SCOPE_SAMPLES);
			vf.samples = scope_buf;
			vf.sample_count = SCOPE_SAMPLES;
			vf.level = energy > 255 ? 255 : energy;
			/* A new pattern row is the closest thing a tracker gives us to a
			   beat, and it lands exactly on the music. */
			vf.beat = a.snap.row != a.last_row;
			vf.preset = a.vis_preset;
			vf.seconds = (float)a.frame / (float)FPS;
			/* Behind: the interface stays readable over the effect. Full: the
			   visualiser owns the screen and the player is gone, except while
			   a toast or the now playing card is up, so changing preset still
			   tells you what you picked. */
			/* Full mode draws no interface, so only the toasts remain; 0.95
			   keeps them bright while the black around them is keyed out. */
			display_set_visualiser(&vf, a.vis_mode == 2 ? 0.95f : 0.55f);
		} else {
			display_set_visualiser(nullptr, 1.0f);
		}
#endif

		/* ---- compose the frame ---- */
		gfx::clear(gfx::BLACK);
		/* In full visualiser mode the player is not drawn at all: only the
		   toasts and the now playing card go over the effect, and the black
		   around them is keyed out by the backend. */
#if defined(OLISE_GL) && defined(OLISE_PROJECTM)
		/* A preset every minute, the way MilkDrop moves on by itself. The
		   timer only runs while the visualiser is on screen, so it does not
		   quietly walk through the list in the background. */
		if (a.vis_mode > 0 && a.vis_auto_seconds > 0 && milkdrop::available()) {
			if (--a.vis_auto_frames <= 0) {
				a.vis_auto_frames = a.vis_auto_seconds * FPS;
				milkdrop::step(1);
			}
		}
#endif

		const bool draw_ui = a.vis_mode != 2;
		if (draw_ui) {
		fx::stars_update(energy);
		fx::stars_draw(0, ui::PANEL_Y - 2);
		fx::logo(gfx::W / 2, 50, a.frame, a.fx_level);
		/* Version, small, to the right of the logo at its baseline. */
		gfx::text_outlined(gfx::W / 2 + fx::logo_width() / 2 + 6, 50 + fx::logo_height() / 2 - 12, "V" OLISE_VERSION, gfx::TEXT_DIM);
		if (a.fx_level > 0) {
			if (!a.scroller[0]) build_scroller_text(a);

			/* A new track asks for a new line, but the greeting is never cut
			   short: it runs its single pass whatever starts playing. */
			if (a.scroller_stale && !a.scroller_greeting && a.scroller_fade_dir == 0)
				a.scroller_fade_dir = -1;

			if (a.scroller_fade_dir < 0) {
				a.scroller_fade -= 12;
				if (a.scroller_fade <= 0) {
					a.scroller_fade = 0;
					build_scroller_text(a);
					a.scroll_x = gfx::W;
					a.scroller_fade_dir = 1;
				}
			} else if (a.scroller_fade_dir > 0) {
				a.scroller_fade += 12;
				if (a.scroller_fade >= 256) {
					a.scroller_fade = 256;
					a.scroller_fade_dir = 0;
				}
			}

			fx::twister(a.scroller, a.scroll_x, a.frame, 118, a.fx_level, a.scroller_fade);
			a.scroll_x -= a.fx_level > 1 ? 2.2f : 1.6f;
			if (a.scroll_x < -fx::twister_text_width(a.scroller)) {
				/* The line ran off the screen. Only here is the greeting
				   really done, which is what lets a track that arrived
				   halfway through wait for it. */
				if (a.scroller_greeting) a.scroller_greeted = true;
				build_scroller_text(a);
				a.scroll_x = gfx::W;
			}
		} else {
			ui::copyright_line(114);
		}
		int vis = ui::pattern_channels_visible();
		int shown = a.player.info().channels - a.ch_offset < vis ? a.player.info().channels - a.ch_offset : vis;
		ui::info_panel(a.player.info(), a.snap, a.detail_text, a.player.volume(), a.radio_mode, a.ch_offset, shown);
		if (a.show_scopes) {
			a.player.scope(scope_buf, SCOPE_SAMPLES);
			ui::scope_panel(scope_buf, SCOPE_SAMPLES, a.snap);
		} else {
			ui::instrument_panel(a.player, a.snap, a.frame);
		}
		ui::pattern_view(a.player, a.snap, a.ch_offset, row_frac, a.frame, energy, a.fx_level);
		if (a.ch_offset > a.player.info().channels - ui::pattern_channels_visible())
			a.ch_offset = a.player.info().channels - ui::pattern_channels_visible();
		if (a.ch_offset < 0) a.ch_offset = 0;
		} /* draw_ui */

		if (a.radio.state() == RadioState::Loading) ui::loading_badge(a.frame, a.radio.source());
		if (a.card_frames > 0) {
			int alpha = a.card_frames > 5 * FPS + 50 ? (6 * FPS - a.card_frames) * 256 / 10 : (a.card_frames < 20 ? a.card_frames * 256 / 20 : 256);
			char station[48] = "";
			if (a.source == Source::Radio) {
				snprintf(station, sizeof(station), "%.40s", a.radio.station_info().name);
				for (char* q = station; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
			}
			ui::now_playing_card(a.player.info(), a.source_text, station, alpha > 256 ? 256 : alpha);
			a.card_frames--;
		}
		if (a.toast_frames > 0) {
			int alpha = a.toast_frames < 20 ? a.toast_frames * 256 / 20 : 256;
			ui::toast(a.toast1, a.toast2, alpha);
			a.toast_frames--;
		}
		if (a.browser) ui::file_browser(a.library, a.browser_sel, a.browser_first);
		if (a.source_picker) ui::source_picker(a.source_sel, a.radio.source());
		if (a.picker) ui::station_picker(a.radio.source(), a.picker_sel, a.picker_first, a.radio.station());
		if (a.help) ui::help_overlay();

		if (shot_path && a.frame == shot_frame) {
			if (display_screenshot(gfx::fb, shot_path)) fprintf(stderr, "screenshot saved to %s\n", shot_path);
			running = false;
		}
		display_present(gfx::fb, a.crt);

		if (a.frame == 0) {
			dbg_checkpoint("first frame presented");
			dbg_toast("Dedicated to my children, Olivier & Elise");
		}
		a.frame++;
		fps_frames++;
		if (now - fps_t0 >= 10000) {
			dbg_log("fps: %.1f", fps_frames * 1000.f / (now - fps_t0));
			fps_t0 = now;
			fps_frames = 0;
		}

		/* Frame pacing without drift. */
		next_frame += frame_budget;
		Uint64 cur = SDL_GetPerformanceCounter();
		if (next_frame > cur) {
			Uint32 wait_ms = (Uint32)((next_frame - cur) * 1000 / perf_freq);
			if (wait_ms > 0) SDL_Delay(wait_ms);
		} else if (cur - next_frame > frame_budget * 4) {
			next_frame = cur; /* fell far behind, resync */
		}
	}

	a.player.shutdown();
#ifdef OLISE_NATIVE
	/* Returning from main crashes the title launch context (boilerplate note):
	   leave the last frame up and let the user close the app from the PS menu. */
	dbg_toast("music stopped, close the app with the PS button");
	for (;;) SDL_Delay(1000);
#else
	display_shutdown();
	if (joy) SDL_JoystickClose(joy);
	SDL_Quit();
	delete app;
	return 0;
#endif
}
