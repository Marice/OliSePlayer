#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects.h"
#include "gfx.h"
#include "stations.h"

namespace ui {

/* Pattern view geometry: 2-digit hex row numbers left and right, channel
   columns of 92 px: NOTE(24) INS(16) VOL(16) FX(24) with 4 px gaps. */
constexpr int ROWNUM_W = 24;
constexpr int CH_W = 96;
constexpr int ROW_H = 8;
constexpr int FIELD_X = 2;
constexpr int FIELD_W = gfx::W - 4;

int pattern_channels_visible()
{
	return (FIELD_W - 2 * ROWNUM_W - 4) / CH_W; /* 6 */
}

static const char* NOTE_NAMES[12] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };

static void note_text(char* out, int note)
{
	if (note == 0) { strcpy(out, "..."); return; }
	if (note == XMP_KEY_OFF || note >= 0x80) { strcpy(out, "==="); return; }
	int n = note - 1;
	snprintf(out, 4, "%s%d", NOTE_NAMES[n % 12], (n / 12) % 10);
}

static char fx_char(int fxt)
{
	static const char* tab = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	return fxt >= 0 && fxt < 36 ? tab[fxt] : '?';
}

static void draw_event(int x, int y, const xmp_event& e, uint32_t mul)
{
	char buf[8];
	note_text(buf, e.note);
	gfx::text(x, y, buf, e.note ? gfx::shade(gfx::NOTE, mul) : gfx::shade(gfx::TEXT_DARK, mul));
	if (e.ins) snprintf(buf, sizeof(buf), "%02X", e.ins);
	else strcpy(buf, "..");
	gfx::text(x + 28, y, buf, e.ins ? gfx::shade(gfx::INSTR, mul) : gfx::shade(gfx::TEXT_DARK, mul));
	if (e.vol) snprintf(buf, sizeof(buf), "%02X", (e.vol - 1) & 0xff);
	else strcpy(buf, "..");
	gfx::text(x + 48, y, buf, e.vol ? gfx::shade(gfx::VOLUME, mul) : gfx::shade(gfx::TEXT_DARK, mul));
	if (e.fxt || e.fxp) snprintf(buf, sizeof(buf), "%c%02X", fx_char(e.fxt), e.fxp);
	else strcpy(buf, "000");
	gfx::text(x + 68, y, buf, (e.fxt || e.fxp) ? gfx::shade(gfx::EFFECT, mul) : gfx::shade(gfx::TEXT_DARK, mul));
}

void pattern_view(const Player& p, const Snapshot& s, int ch_offset, float row_frac, int frame, int energy, int fx_level)
{
	const int y0 = PATTERN_Y, h = PATTERN_H;
	gfx::bevel(0, y0, gfx::W, h);
	gfx::field(FIELD_X, y0 + 2, FIELD_W, h - 4);
	const int HEAD_H = 12;
	const int fy0 = y0 + 3 + HEAD_H, fy1 = y0 + h - 4;
	fx::raster_bars(FIELD_X + 1, fy0, FIELD_W - 2, fy1, frame, energy, fx_level);

	const xmp_module* mod = p.module();
	int visible = pattern_channels_visible();
	int rows_visible = (fy1 - fy0) / ROW_H;      /* 19 */
	int center = rows_visible / 2;               /* 9 */
	int cx0 = FIELD_X + 2 + ROWNUM_W + 2;

	/* Highlight bar for the current row, FT2 blue. */
	int bar_y = fy0 + center * ROW_H;
	gfx::fill(FIELD_X + 1, bar_y - 1, FIELD_W - 2, ROW_H + 1, gfx::HILITE);
	gfx::hline(FIELD_X + 1, FIELD_X + FIELD_W - 2, bar_y - 1, gfx::HILITE_EDGE);
	gfx::hline(FIELD_X + 1, FIELD_X + FIELD_W - 2, bar_y + ROW_H - 1, gfx::HILITE_EDGE);

	if (!mod || !s.loaded) {
		gfx::text(gfx::W / 2 - 13 * 4, bar_y, "NO MODULE LOADED", gfx::TEXT_DIM);
		return;
	}

	int chn = mod->chn;
	if (ch_offset > chn - visible) ch_offset = chn - visible;
	if (ch_offset < 0) ch_offset = 0;
	int shown = chn - ch_offset < visible ? chn - ch_offset : visible;
	int scroll = (int)(row_frac * ROW_H);

	/* Column separators and channel header ticks. */
	for (int c = 0; c <= shown; c++) {
		int x = cx0 + c * CH_W - 3;
		gfx::vline(x, fy0, fy1, gfx::PANEL_DEEP);
	}
	gfx::vline(FIELD_X + 1 + ROWNUM_W + 1, fy0, fy1, gfx::PANEL_DARK);
	gfx::vline(FIELD_X + FIELD_W - 2 - ROWNUM_W - 1, fy0, fy1, gfx::PANEL_DARK);

	for (int r = -1; r <= rows_visible; r++) {
		int row = s.row + (r - center);
		int y = fy0 + r * ROW_H - scroll;
		if (y < fy0 - ROW_H || y > fy1) continue;
		/* Rows outside the current pattern show the neighbouring pattern. */
		int pat = s.pattern;
		int num_rows = mod->xxp[pat]->rows;
		int pos = s.pos;
		while (row < 0) {
			if (pos <= 0) { row = -1; break; }
			pos--;
			pat = mod->xxo[pos];
			if (pat >= mod->pat) { row = -1; break; }
			row += mod->xxp[pat]->rows;
		}
		while (row >= 0 && row >= num_rows) {
			row -= num_rows;
			if (pos + 1 >= mod->len) { row = -1; break; }
			pos++;
			pat = mod->xxo[pos];
			if (pat >= mod->pat) { row = -1; break; }
			num_rows = mod->xxp[pat]->rows;
		}
		if (row < 0) continue;
		bool is_current = (r == center);
		int dist = abs(r - center);
		int mul = is_current ? 256 : (dist > 6 ? 150 : 256 - dist * 10);
		bool beat = (row % 4) == 0;
		uint32_t numcol = is_current ? gfx::WHITE : (beat ? gfx::ROWNUM : gfx::TEXT_DIM);
		/* Clip text to the field vertically by skipping partially visible rows at the top edge. */
		if (y < fy0) continue;
		if (y + ROW_H - 1 > fy1) continue;
		gfx::textf(FIELD_X + 3, y, gfx::shade(numcol, mul), "%02X", row & 0xff);
		gfx::textf(FIELD_X + FIELD_W - 2 - ROWNUM_W + 2, y, gfx::shade(numcol, mul), "%02X", row & 0xff);
		for (int c = 0; c < shown; c++) {
			int ch = ch_offset + c;
			int trk = mod->xxp[pat]->index[ch];
			const xmp_event& e = mod->xxt[trk]->event[row];
			draw_event(cx0 + c * CH_W, y, e, mul);
		}
	}

	/* Header strip: channel number and a small VU bar per column, FT2 has
	   its scopes up there. */
	int hy = y0 + 3;
	gfx::fill(FIELD_X + 1, hy, FIELD_W - 2, HEAD_H, gfx::PANEL_DEEP);
	gfx::hline(FIELD_X + 1, FIELD_X + FIELD_W - 2, hy + HEAD_H - 1, gfx::PANEL_DARK);
	for (int c = 0; c < shown; c++) {
		int ch = ch_offset + c;
		int x = cx0 + c * CH_W;
		gfx::textf(x, hy + 2, s.ch[ch].volume > 0 ? gfx::WHITE : gfx::TEXT_DIM, "%02d", ch + 1);
		int bw = CH_W - 28;
		int v = s.ch[ch].volume * bw / 64;
		int pk = s.ch[ch].peak * bw / 64;
		gfx::fill(x + 20, hy + 3, bw, 6, gfx::BLACK);
		for (int k = 0; k < v; k++) gfx::vline(x + 20 + k, hy + 3, hy + 8, gfx::blend(gfx::VU_LOW, gfx::VU_HIGH, k * 256 / bw));
		if (pk > 0) gfx::vline(x + 20 + pk, hy + 3, hy + 8, gfx::WHITE);
	}
	/* Arrows in the row-number columns hint at channels off-screen. */
	if (ch_offset > 0) gfx::text(FIELD_X + 10, hy + 2, "<", gfx::WHITE);
	if (ch_offset + shown < chn) gfx::text(FIELD_X + FIELD_W - 2 - ROWNUM_W + 8, hy + 2, ">", gfx::WHITE);
}

static void fmt_time(char* out, size_t n, int ms)
{
	if (ms < 0) ms = 0;
	int sec = ms / 1000;
	snprintf(out, n, "%d:%02d", sec / 60, sec % 60);
}

void info_panel(const TrackInfo& ti, const Snapshot& s, const char* detail, int volume, bool radio_on, int ch_first, int ch_shown)
{
	int x = LEFT_X, y = PANEL_Y, w = PANEL_W, h = PANEL_H;
	gfx::bevel(x, y, w, h);
	int tx = x + 6, ty = y + 3;
	char buf[64], t1[16], t2[16];
	if (!ti.loaded) {
		gfx::text(tx, ty, "NO MODULE", gfx::TEXT_DIM);
		gfx::text(tx, ty + 10, "R3: RANDOM TRACK FROM THE MOD ARCHIVE", gfx::TEXT);
		gfx::text(tx, ty + 20, "TRIANGLE: PICK A STATION / GENRE", gfx::TEXT);
		gfx::text(tx, ty + 30, "CIRCLE: LOCAL FILES", gfx::TEXT);
		return;
	}
	snprintf(buf, sizeof(buf), "%.37s", ti.title[0] ? ti.title : "(untitled)");
	gfx::text(tx, ty, buf, gfx::WHITE);
	snprintf(buf, sizeof(buf), "%.24s %2dCH %02dINS", ti.type, ti.channels, ti.instruments);
	gfx::text(tx, ty + 10, buf, gfx::TEXT);
	fmt_time(t1, sizeof(t1), s.time_ms);
	fmt_time(t2, sizeof(t2), s.total_ms);
	if (ti.channels > ch_shown)
		snprintf(buf, sizeof(buf), "POS %02X/%02X PAT %02X ROW %02X CH %02d-%02d", s.pos, ti.length ? ti.length - 1 : 0, s.pattern, s.row, ch_first + 1, ch_first + ch_shown);
	else
		snprintf(buf, sizeof(buf), "POS %02X/%02X PAT %02X ROW %02X", s.pos, ti.length ? ti.length - 1 : 0, s.pattern, s.row);
	gfx::text(tx, ty + 20, buf, gfx::TEXT);
	snprintf(buf, sizeof(buf), "SPD %02d BPM %03d %s/%s VOL %d%% %s", s.speed, s.bpm, t1, t2, volume,
	         s.paused ? "PAUSE" : (radio_on ? "RADIO" : "LOOP"));
	gfx::text(tx, ty + 30, buf, s.paused ? gfx::EFFECT : gfx::TEXT);
	snprintf(buf, sizeof(buf), "%.37s", detail);
	gfx::text(tx, ty + 40, buf, gfx::INSTR);
	/* Progress line along the bottom edge. */
	if (s.total_ms > 0) {
		int pw = (w - 12) * (s.time_ms > s.total_ms ? s.total_ms : s.time_ms) / s.total_ms;
		gfx::hline(x + 6, x + w - 7, y + h - 3, gfx::PANEL_DEEP);
		gfx::hline(x + 6, x + 6 + pw, y + h - 3, gfx::HILITE_EDGE);
	}
}

void instrument_panel(const Player& p, const Snapshot& s, int frame)
{
	int x = RIGHT_X, y = PANEL_Y, w = PANEL_W, h = PANEL_H;
	gfx::bevel(x, y, w, h);
	gfx::field(x + 3, y + 3, w - 6, h - 6);
	const xmp_module* mod = p.module();
	if (!mod) return;
	int lines = (h - 10) / 8; /* 5 */
	int n = mod->ins;
	/* Slow auto-scroll through the instrument list, FT2 shows 8 at a time. */
	int first = 0;
	if (n > lines) first = (frame / 180) % (n - lines + 1);
	for (int i = 0; i < lines && first + i < n; i++) {
		int idx = first + i;
		bool active = false;
		for (int c = 0; c < s.chn; c++) if (s.ch[c].volume > 0 && s.ch[c].instrument == idx) { active = true; break; }
		char name[32];
		strncpy(name, mod->xxi[idx].name, 22);
		name[22] = 0;
		for (char* q = name; *q; q++) if ((unsigned char)*q < 32) *q = ' ';
		gfx::textf(x + 6, y + 5 + i * 8, active ? gfx::WHITE : gfx::TEXT_DIM, "%02X %s", idx + 1, name);
		if (active) gfx::fill(x + w - 14, y + 5 + i * 8 + 2, 6, 4, gfx::VU_LOW);
	}
	/* Tiny VU strip at the right side of the panel. */
	int vx = x + w - 60, vy = y + 5, vw = 44, vh = h - 10;
	gfx::fill(vx, vy, vw, vh, gfx::BLACK);
	fx::vu_bars(vx + 1, vy, vw - 2, vh, s);
}

void scope_panel(const int16_t* samples, int n, const Snapshot& s)
{
	int x = RIGHT_X, y = PANEL_Y, w = PANEL_W, h = PANEL_H;
	gfx::bevel(x, y, w, h);
	gfx::field(x + 3, y + 3, w / 2 - 4, h - 6);
	fx::scope(x + 4, y + 4, w / 2 - 6, h - 8, samples, n);
	gfx::field(x + w / 2 + 1, y + 3, w / 2 - 4, h - 6);
	fx::vu_bars(x + w / 2 + 3, y + 4, w / 2 - 8, h - 8, s);
}

void copyright_line(int y)
{
	const char* l = "(C) 2026 OLISE PLAYER - TRACKER RADIO FOR PS5 - MUSIC BY THE MOD ARCHIVE";
	gfx::text_outlined(gfx::W / 2 - gfx::text_width(l) / 2, y, l, gfx::ROWNUM);
}

void now_playing_card(const TrackInfo& ti, const char* source, const char* station, int alpha)
{
	if (alpha <= 0) return;
	int w = 440, h = 74;
	int x = gfx::W / 2 - w / 2, y = PATTERN_Y + 36;
	gfx::fill_alpha(x + 4, y + 4, w, h, gfx::BLACK, alpha / 2);
	gfx::fill_alpha(x, y, w, h, gfx::PANEL, alpha);
	gfx::fill_alpha(x, y, w, 2, gfx::PANEL_LIGHT, alpha);
	gfx::fill_alpha(x, y, 2, h, gfx::PANEL_LIGHT, alpha);
	gfx::fill_alpha(x, y + h - 2, w, 2, gfx::PANEL_DARK, alpha);
	gfx::fill_alpha(x + w - 2, y, 2, h, gfx::PANEL_DARK, alpha);
	if (alpha < 128) return; /* text pops in/out, FT2 had no fades */
	gfx::text(x + 10, y + 8, "NOW PLAYING", gfx::TEXT_DIM);
	char buf[64];
	snprintf(buf, sizeof(buf), "%.26s", ti.title[0] ? ti.title : "(untitled)");
	gfx::text_scaled(x + 10, y + 20, buf, 2, gfx::WHITE);
	snprintf(buf, sizeof(buf), "%.22s  %d CHANNELS  %d INSTR", ti.type, ti.channels, ti.instruments);
	gfx::text(x + 10, y + 42, buf, gfx::TEXT);
	char t[16];
	fmt_time(t, sizeof(t), ti.total_time_ms);
	snprintf(buf, sizeof(buf), "%.30s  LENGTH %s", source, t);
	gfx::text(x + 10, y + 54, buf, gfx::INSTR);
	if (station && *station) {
		snprintf(buf, sizeof(buf), "%.24s", station);
		gfx::text(x + w - 10 - gfx::text_width(buf), y + 8, buf, gfx::VOLUME);
	}
}

void station_picker(int sel, int first, int current)
{
	int w = 480, h = 232;
	int x = gfx::W / 2 - w / 2, y = 64;
	gfx::dim(0, 0, gfx::W, gfx::H, 110);
	gfx::bevel(x, y, w, h);
	gfx::text(x + 8, y + 6, "MOD ARCHIVE STATIONS", gfx::WHITE);
	char hdr[32];
	snprintf(hdr, sizeof(hdr), "%d/%d", sel + 1, station_count());
	gfx::text(x + w - 8 - gfx::text_width(hdr), y + 6, hdr, gfx::TEXT_DIM);
	gfx::field(x + 6, y + 18, w - 12, h - 44);
	int lines = (h - 48) / 10;
	for (int i = 0; i < lines && first + i < station_count(); i++) {
		int idx = first + i;
		int ly = y + 22 + i * 10;
		Station st = station_at(idx);
		if (idx == sel) gfx::fill(x + 8, ly - 1, w - 16, 10, gfx::HILITE);
		bool genre = st.kind == STATION_GENRE;
		if (genre && idx == NUM_FIXED_STATIONS) gfx::text(x + 14, ly, "GENRES:", gfx::TEXT_DIM);
		char name[52];
		snprintf(name, sizeof(name), "%.44s", st.name);
		for (char* q = name; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
		gfx::text(x + (genre ? 78 : 14), ly, name, idx == sel ? gfx::WHITE : (genre ? gfx::TEXT : gfx::INSTR));
		if (idx == current) gfx::text(x + w - 30, ly, ">", gfx::VU_LOW);
	}
	gfx::text(x + 8, y + h - 20, "CROSS: TUNE IN   CIRCLE: CLOSE   UP/DOWN: SELECT", gfx::TEXT_DIM);
}

void toast(const char* line1, const char* line2, int alpha)
{
	if (alpha <= 0) return;
	int w = 360, h = line2 && *line2 ? 36 : 24;
	int x = gfx::W / 2 - w / 2, y = PATTERN_Y - h - 6;
	gfx::fill_alpha(x, y, w, h, gfx::PANEL_DEEP, alpha);
	gfx::fill_alpha(x, y, w, 1, gfx::HILITE_EDGE, alpha);
	gfx::fill_alpha(x, y + h - 1, w, 1, gfx::HILITE_EDGE, alpha);
	if (alpha < 128) return;
	gfx::text(x + (w - gfx::text_width(line1)) / 2, y + 8, line1, gfx::WHITE);
	if (line2 && *line2) gfx::text(x + (w - gfx::text_width(line2)) / 2, y + 20, line2, gfx::TEXT_DIM);
}

void file_browser(const Library& lib, int sel, int first)
{
	int w = 480, h = 232;
	int x = gfx::W / 2 - w / 2, y = 64;
	gfx::dim(0, 0, gfx::W, gfx::H, 110);
	gfx::bevel(x, y, w, h);
	gfx::text(x + 8, y + 6, "LOCAL MODULES", gfx::WHITE);
	gfx::textf(x + w - 8 - 8 * 20, y + 6, gfx::TEXT_DIM, "%20.20s", lib.folder());
	gfx::field(x + 6, y + 18, w - 12, h - 44);
	int lines = (h - 48) / 10;
	if (lib.count() == 0) {
		gfx::text(x + 14, y + 24, "NO FILES. PUT .MOD .XM .S3M .IT IN music/", gfx::TEXT_DIM);
	}
	for (int i = 0; i < lines && first + i < lib.count(); i++) {
		int idx = first + i;
		int ly = y + 22 + i * 10;
		if (idx == sel) gfx::fill(x + 8, ly - 1, w - 16, 10, gfx::HILITE);
		char name[52];
		snprintf(name, sizeof(name), "%.50s", lib.name(idx));
		gfx::text(x + 14, ly, name, idx == sel ? gfx::WHITE : gfx::TEXT);
		if (idx == lib.current()) gfx::text(x + w - 30, ly, ">", gfx::VU_LOW);
	}
	gfx::text(x + 8, y + h - 20, "CROSS: PLAY   CIRCLE: CLOSE   UP/DOWN: SELECT", gfx::TEXT_DIM);
}

void help_overlay()
{
	static const char* lines[] = {
		"R3 ............ NEXT RANDOM TRACK (MOD ARCHIVE)",
		"L3 ............ PREVIOUS TRACK",
		"CROSS ......... PAUSE / PLAY",
		"TRIANGLE ...... STATIONS: GENRES, FORMATS, CHARTS",
		"CIRCLE ........ LOCAL FILE LIST",
		"L1 / R1 ....... PREVIOUS / NEXT LOCAL FILE",
		"OPTIONS ....... INSTRUMENTS <-> SCOPES",
		"SQUARE ........ EFFECTS: FULL / CALM / OFF",
		"R2 ............ CRT SCANLINES",
		"D-PAD L/R ..... SCROLL CHANNELS",
		"D-PAD U/D ..... VOLUME  (HOLD L2: SEEK)",
		"TOUCHPAD ...... THIS HELP",
		"L2 + R2 ....... EXIT",
	};
	int n = (int)(sizeof(lines) / sizeof(lines[0]));
	int w = 420, h = 24 + n * 10 + 8;
	int x = gfx::W / 2 - w / 2, y = gfx::H / 2 - h / 2;
	gfx::dim(0, 0, gfx::W, gfx::H, 100);
	gfx::bevel(x, y, w, h);
	gfx::text(x + 10, y + 7, "OLISE PLAYER - CONTROLS", gfx::WHITE);
	for (int i = 0; i < n; i++) gfx::text(x + 10, y + 22 + i * 10, lines[i], gfx::TEXT);
}

void loading_badge(int frame)
{
	static const char spin[4] = { '|', '/', '-', '\\' };
	char buf[40];
	snprintf(buf, sizeof(buf), "%c DOWNLOADING FROM THE MOD ARCHIVE", spin[(frame / 6) & 3]);
	int w = gfx::text_width(buf) + 12;
	int x = gfx::W / 2 - w / 2, y = PATTERN_Y - 20;
	gfx::fill(x, y, w, 14, gfx::PANEL_DEEP);
	gfx::rect(x, y, w, 14, gfx::HILITE_EDGE);
	gfx::text(x + 6, y + 3, buf, gfx::WHITE);
}

} /* namespace ui */
