/* The application state, shared by the pieces that act on it.
 *
 * main.cpp sets this up and runs the frame loop; track.cpp decides what plays
 * next and input.cpp turns button presses into those decisions. They all work
 * on one App, passed by reference, because the player, the radio and the
 * library are single instances that every part needs to see. */
#ifndef APP_H
#define APP_H

#include <SDL2/SDL.h>

#include "gfx.h"
#include "library.h"
#include "player.h"
#include "radio.h"

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

/* Where the track that is playing came from. */
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
	int vis_mode = 0;              /* 0 off, 1 behind the interface, 2 full screen */
	int vis_preset = 0;
	int vis_auto_seconds = 60;     /* 0 is off; otherwise a new preset this often */
	int vis_auto_frames = 0;       /* counts down to the next automatic change */
	bool crt = false;
	bool show_scopes = false;
	bool help = false;
	bool browser = false;
	int browser_sel = 0, browser_first = 0;
	bool picker = false;
	int picker_sel = 0, picker_first = 0;
	bool source_picker = false;    /* shown before the station list */
	int source_sel = 0;
	char detail_text[64] = "";
	int ch_offset = 0;
	int card_frames = 0;           /* now-playing card countdown */
	int toast_frames = 0;
	char toast1[64] = "", toast2[64] = "";
	int retry_frames = 0;          /* radio backoff */
	int retry_delay = 10 * FPS;
	bool l2_down = false, r2_down = false;
	float scroll_x = gfx::W;
	/* The scroller shows the greeting once, then what is playing. The text is
	   built when a pass ends rather than every frame, because measuring it is
	   the expensive part. */
	/* Two different questions: has the greeting been put on the band, and has
	   it finished its pass. Treating them as one cut the greeting short as
	   soon as the first track arrived. */
	bool scroller_greeting = false;   /* the greeting is the line on screen */
	bool scroller_greeted = false;    /* its one pass has finished */
	char scroller[512] = "";
	bool scroller_stale = false;   /* a new track wants a new line */
	/* 256 is fully visible. A new track fades the band out, swaps the text at
	   zero and fades back in, so a line is never cut off mid sentence. The
	   greeting is exempt: it runs its one pass whatever starts playing. */
	int scroller_fade = 256;
	int scroller_fade_dir = 0;     /* -1 fading out, +1 fading in, 0 steady */
	Uint32 row_change_ms = 0;
	int last_row = -1, last_pos = -1;
	int frame = 0;
	char appdir[512] = "";
};

/* The scroller greeting, shown once at startup. */
static const char GREETINGS[] =
	"      *** OLISE PLAYER ***   TRACKER RADIO FOR THE PLAYSTATION 5 ... "
	"RANDOM MODULES STRAIGHT FROM THE MOD ARCHIVE AND MODLAND ... PRESS R3 FOR THE NEXT TRACK, L3 TO GO BACK ... "
	"GREETINGS TO ALL TRACKER MUSICIANS AND THE PS5 HOMEBREW SCENE ... "
	"OLIVIER <3 - ELISE <3 - CAROLIEN <3 ... MADE BY MARICE IN 2026 ...      ";

/* Fills a.scroller with what is playing, or the greeting the first time. */
void build_scroller_text(App& a);

/* Two lines in the corner for four seconds. */
void show_toast(App& a, const char* l1, const char* l2);

/* --- track.cpp: what plays, and what plays next ------------------------- */

/* Starts a module from the local library. False when it will not load. */
bool play_local(App& a, int index);
/* Next and previous within the current source: the radio walks its history,
   the local library its file list. */
void next_track(App& a);
void prev_track(App& a);
/* Picks up a finished download, a failed one, or a track that ended. */
void poll_radio(App& a);
/* Asks the radio for the next module from the current station. */
void start_radio_fetch(App& a);
/* Switches to a station and starts fetching from it. */
void tune_in(App& a, int index);
/* Writes the playing module into music/ and adds it to the index, so a track
   from the radio can be kept. */
bool keep_current_track(App& a);

/* --- input.cpp: buttons and keys ---------------------------------------- */

void handle_button(App& a, int b, bool& running);
void handle_key(App& a, SDL_Keycode k, bool& running);

#endif
