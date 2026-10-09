/* What plays, and what plays next.
 *
 * The radio and the local library are two different ways of choosing a
 * module; everything below hides that difference from the rest of the app. */
#include "app.h"

#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "effects.h"
#include "stations.h"
#include "ui.h"

static void on_track_started(App& a)
{
	a.card_frames = 6 * FPS;
	a.ch_offset = 0;
	a.last_row = -1;
	/* The scroller is rebuilt at the end of its pass rather than here: cutting
	   a line off halfway reads worse than showing the previous track for a few
	   seconds longer. The now playing card covers the gap. */
	a.scroller_stale = true;
	fx::warp();
	fprintf(stderr, "NOW PLAYING: %s [%s] %d ch (%s)\n", a.player.info().title, a.player.info().type,
	        a.player.info().channels, a.source_text);
}

bool play_local(App& a, int index)
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
	/* Modland has no module numbers: its tracks are files in an archive, so
	   the name of the source is all there is to show. The Mod Archive numbers
	   every module and that number is what a listener looks one up by. */
	if (t.module_id > 0)
		snprintf(a.source_text, sizeof(a.source_text), "%.20s #%ld", source_name(t.source), t.module_id);
	else
		snprintf(a.source_text, sizeof(a.source_text), "%.30s", source_name(t.source));

	char id[24] = "";
	if (t.module_id > 0) snprintf(id, sizeof(id), "#%ld ", t.module_id);
	if (t.genre[0] && t.artist[0]) snprintf(a.detail_text, sizeof(a.detail_text), "%s%s BY %s", id, t.genre, t.artist);
	else if (t.genre[0]) snprintf(a.detail_text, sizeof(a.detail_text), "%s%s", id, t.genre);
	else if (t.artist[0]) snprintf(a.detail_text, sizeof(a.detail_text), "%sBY %s", id, t.artist);
	else snprintf(a.detail_text, sizeof(a.detail_text), "%s%.20s (%s)", id, source_name(t.source), t.format);
	for (char* q = a.detail_text; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
	on_track_started(a);
	return true;
}

void start_radio_fetch(App& a)
{
	if (a.radio.state() == RadioState::Loading) return;
	if (!a.radio.fetch()) {
		show_toast(a, "RADIO UNAVAILABLE", a.radio.error());
		a.radio.ack_failed();
	}
}

void next_track(App& a)
{
	if (a.radio.has_next()) {
		const RadioTrack* t = a.radio.next();
		if (t) play_radio_track(a, *t);
		return;
	}
	start_radio_fetch(a);
}

void prev_track(App& a)
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

void poll_radio(App& a)
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

void tune_in(App& a, int index)
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

/* Writes the playing module into music/ and adds its name to index.txt, so a
   tune from the radio can be kept instead of scrolling out of the 20 track
   history. Writing a file is allowed in the title sandbox; listing a folder is
   not, which is why the index has to be updated by hand here too. */
bool keep_current_track(App& a)
{
	const RadioTrack* t = a.radio.current();
	if (a.source != Source::Radio || !t || !t->data || t->len == 0) {
		show_toast(a, "NOTHING TO KEEP", "ONLY RADIO TRACKS");
		return false;
	}

	/* Build a file name from the title: keep it plain, because this ends up
	   on a console file system and in a text index. */
	char base[64];
	int n = 0;
	for (const char* p = t->title; *p && n < (int)sizeof(base) - 1; p++) {
		char c = *p;
		if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
		if (ok) base[n++] = c;
		else if (n > 0 && base[n - 1] != '-') base[n++] = '-';
	}
	while (n > 0 && base[n - 1] == '-') n--;
	if (n == 0) n = snprintf(base, sizeof(base), "module-%ld", t->module_id);
	base[n] = 0;

	char ext[8];
	snprintf(ext, sizeof(ext), "%s", t->format[0] ? t->format : "mod");
	for (char* p = ext; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);

	char name[96], path[512];
	snprintf(name, sizeof(name), "%s.%s", base, ext);
	snprintf(path, sizeof(path), "%s/music/%s", a.appdir, name);

	/* Do not overwrite: the same title can come round twice with different
	   music behind it. */
	FILE* probe = fopen(path, "rb");
	if (probe) {
		fclose(probe);
		show_toast(a, "ALREADY KEPT", name);
		return false;
	}

	FILE* f = fopen(path, "wb");
	if (!f) {
		dbg_log("keep: cannot write %s", path);
		show_toast(a, "COULD NOT SAVE", "music/ IS NOT WRITABLE");
		return false;
	}
	const size_t written = fwrite(t->data, 1, t->len, f);
	fclose(f);
	if (written != t->len) {
		remove(path);
		show_toast(a, "COULD NOT SAVE", "WRITE FAILED");
		return false;
	}

	/* Append to the index the app reads at startup. */
	char index_path[512];
	snprintf(index_path, sizeof(index_path), "%s/music/index.txt", a.appdir);
	FILE* idx = fopen(index_path, "a");
	if (idx) {
		fprintf(idx, "%s\n", name);
		fclose(idx);
	} else {
		dbg_log("keep: saved %s but could not update index.txt", name);
	}

	a.library.scan(a.appdir);
	dbg_log("keep: %s (%zu bytes)", path, t->len);
	show_toast(a, "KEPT IN music/", name);
	return true;
}

/* What the scroller says. The greeting runs once at startup, because that is
   the moment it is worth reading; after that the band is better used for what
   is playing, which the pattern view and the small panels cannot spell out at
   this size.

   libxmp reports the tracker that wrote a module, its channel count and how
   many instruments and patterns it holds. For a demoscene player those are
   the interesting numbers, so they go in. */
void build_scroller_text(App& a)
{
	a.scroller_stale = false;
	if (!a.scroller_greeted && !a.scroller_greeting) {
		/* Only marks the greeting as showing; the main loop sets greeted once
		   the line has actually run off the screen. */
		a.scroller_greeting = true;
		snprintf(a.scroller, sizeof(a.scroller), "%s", GREETINGS);
		return;
	}
	a.scroller_greeting = false;

	const TrackInfo& t = a.player.info();
	if (!t.loaded) {
		/* Nothing playing: say so rather than scrolling an empty band. */
		snprintf(a.scroller, sizeof(a.scroller),
		         "      *** OLISE PLAYER ***   PRESS R3 FOR A RANDOM TRACK, "
		         "TRIANGLE TO PICK A SOURCE AND STATION, CIRCLE FOR LOCAL FILES ...      ");
		return;
	}

	char extra[160] = "";
	int n = 0;
	if (t.channels > 0)
		n += snprintf(extra + n, sizeof(extra) - n, "%d CHANNELS", t.channels);
	if (t.instruments > 0 && n < (int)sizeof(extra) - 24)
		n += snprintf(extra + n, sizeof(extra) - n, "%s%d INSTRUMENTS", n ? " - " : "", t.instruments);
	if (t.patterns > 0 && n < (int)sizeof(extra) - 20)
		n += snprintf(extra + n, sizeof(extra) - n, "%s%d PATTERNS", n ? " - " : "", t.patterns);
	if (t.total_time_ms > 0 && n < (int)sizeof(extra) - 14) {
		const int secs = t.total_time_ms / 1000;
		snprintf(extra + n, sizeof(extra) - n, "%s%d:%02d", n ? " - " : "", secs / 60, secs % 60);
	}

	/* Sections are joined one at a time so an empty one leaves no dangling
	   separator, and the genre/artist line is skipped when it only repeats the
	   source, which is what Modland's own metadata amounts to. */
	const char* parts[5];
	int count = 0;
	parts[count++] = t.title[0] ? t.title : "UNTITLED";
	parts[count++] = a.source_text[0] ? a.source_text : "LOCAL FILE";
	if (t.type[0]) parts[count++] = t.type;
	if (extra[0]) parts[count++] = extra;
	if (a.detail_text[0] && strstr(a.detail_text, a.source_text) == NULL)
		parts[count++] = a.detail_text;

	int n2 = snprintf(a.scroller, sizeof(a.scroller), "      NOW PLAYING: ");
	for (int i = 0; i < count && n2 < (int)sizeof(a.scroller) - 32; i++)
		n2 += snprintf(a.scroller + n2, sizeof(a.scroller) - n2, "%s%.80s",
		               i ? "   ***   " : "", parts[i]);
	snprintf(a.scroller + n2, sizeof(a.scroller) - n2, " ...      ");

	for (char* p = a.scroller; *p; p++)
		if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
}
