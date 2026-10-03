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
#include "gfx.h"
#include "library.h"
#include "player.h"
#include "radio.h"
#include "stations.h"
#include "ui.h"

/* DualSense button indices as SDL reports them. */
#define BTN_CROSS     0
#define BTN_CIRCLE    1
#define BTN_SQUARE    2
#define BTN_TRIANGLE  3
#define BTN_TOUCHPAD  4
#define BTN_OPTIONS   6
#define BTN_L3        7
#define BTN_R3        8
#define BTN_L1        9
#define BTN_R1        10
#define BTN_DUP       11
#define BTN_DDOWN     12
#define BTN_DLEFT     13
#define BTN_DRIGHT    14
#define AXIS_L2       4
#define AXIS_R2       5

#define FPS 60

static const char GREETINGS[] =
	"      *** OLISE PLAYER ***   TRACKER RADIO FOR THE PLAYSTATION 5 ... "
	"RANDOM MODULES STRAIGHT FROM THE MOD ARCHIVE ... PRESS R3 FOR THE NEXT TRACK, L3 TO GO BACK ... "
	"GREETINGS TO ALL TRACKER MUSICIANS AND THE PS5 HOMEBREW SCENE ... "
	"OLIVIER <3 - ELISE <3 - CAROLIEN <3 ... MADE BY MARICE IN 2026 ...      ";

enum class Source { None, Local, Radio };

struct App {
	Player player;
	Radio radio;
	Library library;
	Snapshot snap;

	Source source = Source::None;
	char source_text[48] = "NO SOURCE";
	bool radio_mode = true;        /* auto-next on track end */
	int fx_level = 2;              /* 2 full, 1 calm, 0 off */
	bool crt = false;
	bool show_scopes = false;
	bool help = false;
	bool browser = false;
	int browser_sel = 0, browser_first = 0;
	bool picker = false;
	int picker_sel = 0, picker_first = 0;
	char detail_text[64] = "";
	int ch_offset = 0;
	int card_frames = 0;           /* now-playing card countdown */
	int toast_frames = 0;
	char toast1[64] = "", toast2[64] = "";
	int retry_frames = 0;          /* radio backoff */
	int retry_delay = 10 * FPS;
	bool l2_down = false, r2_down = false;
	float scroll_x = gfx::W;
	Uint32 row_change_ms = 0;
	int last_row = -1, last_pos = -1;
	int frame = 0;
	char appdir[512] = "";
};

static void show_toast(App& a, const char* l1, const char* l2)
{
	strncpy(a.toast1, l1, sizeof(a.toast1) - 1);
	strncpy(a.toast2, l2 ? l2 : "", sizeof(a.toast2) - 1);
	a.toast_frames = 4 * FPS;
}

static void on_track_started(App& a)
{
	a.card_frames = 6 * FPS;
	a.ch_offset = 0;
	a.last_row = -1;
	fx::warp();
	fprintf(stderr, "NOW PLAYING: %s [%s] %d ch (%s)\n", a.player.info().title, a.player.info().type,
	        a.player.info().channels, a.source_text);
}

static bool play_local(App& a, int index)
{
	if (index < 0 || index >= a.library.count()) return false;
	a.library.set_current(index);
	if (!a.player.load_file(a.library.path(index), a.radio_mode ? 1 : 0)) {
		show_toast(a, "COULD NOT LOAD FILE", a.library.name(index));
		return false;
	}
	a.source = Source::Local;
	snprintf(a.source_text, sizeof(a.source_text), "LOCAL %d/%d", index + 1, a.library.count());
	snprintf(a.detail_text, sizeof(a.detail_text), "LOCAL FILE: %.40s", a.library.name(index));
	on_track_started(a);
	return true;
}

static bool play_radio_track(App& a, const RadioTrack& t)
{
	if (!a.player.load_memory(t.data, t.len, 1)) {
		show_toast(a, "MODULE FAILED TO LOAD", t.title);
		return false;
	}
	a.source = Source::Radio;
	snprintf(a.source_text, sizeof(a.source_text), "MOD ARCHIVE #%ld", t.module_id);
	if (t.genre[0] && t.artist[0]) snprintf(a.detail_text, sizeof(a.detail_text), "#%ld %s BY %s", t.module_id, t.genre, t.artist);
	else if (t.genre[0]) snprintf(a.detail_text, sizeof(a.detail_text), "#%ld %s", t.module_id, t.genre);
	else if (t.artist[0]) snprintf(a.detail_text, sizeof(a.detail_text), "#%ld BY %s", t.module_id, t.artist);
	else snprintf(a.detail_text, sizeof(a.detail_text), "MOD ARCHIVE #%ld (%s)", t.module_id, t.format);
	for (char* q = a.detail_text; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
	on_track_started(a);
	return true;
}

static void start_radio_fetch(App& a)
{
	if (a.radio.state() == RadioState::Loading) return;
	if (!a.radio.fetch()) {
		show_toast(a, "RADIO UNAVAILABLE", a.radio.error());
		a.radio.ack_failed();
	}
}

static void next_track(App& a)
{
	if (a.radio.has_next()) {
		const RadioTrack* t = a.radio.next();
		if (t) play_radio_track(a, *t);
		return;
	}
	start_radio_fetch(a);
}

static void prev_track(App& a)
{
	if (a.radio.has_prev()) {
		const RadioTrack* t = a.radio.prev();
		if (t) play_radio_track(a, *t);
	} else if (a.source == Source::Local && a.library.count() > 0) {
		play_local(a, a.library.prev());
	} else {
		show_toast(a, "NO PREVIOUS TRACK", nullptr);
	}
}

static void poll_radio(App& a)
{
	RadioState st = a.radio.state();
	if (st == RadioState::Ready) {
		RadioTrack t;
		if (a.radio.take(t)) {
			if (play_radio_track(a, t)) {
				a.radio.remember(t);
				a.retry_delay = 10 * FPS;
			} else {
				free(t.data);
			}
		}
	} else if (st == RadioState::Failed) {
		fprintf(stderr, "radio: %s\n", a.radio.error());
		a.radio.ack_failed();
		char l2[64];
		snprintf(l2, sizeof(l2), "RETRY IN %d S", a.retry_delay / FPS);
		show_toast(a, a.radio.error(), l2);
		a.retry_frames = a.retry_delay;
		if (a.retry_delay < 60 * FPS) a.retry_delay *= 2;
		/* Keep the music going with a local file while offline. */
		if (!a.player.info().loaded && a.library.count() > 0) play_local(a, a.library.current());
	}
	if (a.retry_frames > 0 && --a.retry_frames == 0 && a.radio_mode && a.source != Source::Local) {
		start_radio_fetch(a);
	}
}

static void tune_in(App& a, int index)
{
	a.radio.set_station(index);
	Station st = a.radio.station_info();
	char name[48];
	snprintf(name, sizeof(name), "%.40s", st.name);
	for (char* q = name; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
	show_toast(a, "TUNING IN", name);
	a.radio_mode = true;
	a.retry_frames = 0;
	start_radio_fetch(a);
}

static void handle_button(App& a, int b, bool& running)
{
	if (a.picker) {
		switch (b) {
		case BTN_DUP:   if (a.picker_sel > 0) a.picker_sel--; break;
		case BTN_DDOWN: if (a.picker_sel + 1 < station_count()) a.picker_sel++; break;
		case BTN_L1:    a.picker_sel = a.picker_sel > 10 ? a.picker_sel - 10 : 0; break;
		case BTN_R1:    a.picker_sel = a.picker_sel + 10 < station_count() ? a.picker_sel + 10 : station_count() - 1; break;
		case BTN_CROSS:
			tune_in(a, a.picker_sel);
			a.picker = false;
			break;
		case BTN_CIRCLE:
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
		a.library.scan(a.appdir);
		a.browser = true;
		a.browser_sel = a.library.current();
		break;
	case BTN_L1:       if (a.library.count()) play_local(a, a.library.prev()); break;
	case BTN_R1:       if (a.library.count()) play_local(a, a.library.next()); break;
	case BTN_TRIANGLE:
		a.picker = true;
		a.picker_sel = a.radio.station();
		break;
	case BTN_OPTIONS:  a.show_scopes = !a.show_scopes; break;
	case BTN_SQUARE:   a.fx_level = (a.fx_level + 2) % 3; break;
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

static void handle_key(App& a, SDL_Keycode k, bool& running)
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

		/* ---- compose the frame ---- */
		gfx::clear(gfx::BLACK);
		fx::stars_update(energy);
		fx::stars_draw(0, ui::PANEL_Y - 2);
		fx::logo(gfx::W / 2, 50, a.frame, a.fx_level);
		/* Version, small, to the right of the logo at its baseline. */
		gfx::text_outlined(gfx::W / 2 + fx::logo_width() / 2 + 6, 50 + fx::logo_height() / 2 - 12, "V" OLISE_VERSION, gfx::TEXT_DIM);
		if (a.fx_level > 0) {
			char text[512];
			snprintf(text, sizeof(text), "%s", GREETINGS);
			fx::twister(text, a.scroll_x, a.frame, 118, a.fx_level);
			a.scroll_x -= a.fx_level > 1 ? 2.2f : 1.6f;
			if (a.scroll_x < -fx::twister_text_width(text)) a.scroll_x = gfx::W;
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

		if (a.radio.state() == RadioState::Loading) ui::loading_badge(a.frame);
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
		if (a.picker) ui::station_picker(a.picker_sel, a.picker_first, a.radio.station());
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
