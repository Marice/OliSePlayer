/* FastTracker II style screen parts. */
#ifndef UI_H
#define UI_H

#include "library.h"
#include "player.h"

namespace ui {

/* Layout constants shared with main.cpp (640x360). */
constexpr int PATTERN_Y = 200;
constexpr int PATTERN_H = 160;
constexpr int PANEL_Y = 144;
constexpr int PANEL_H = 56;
constexpr int PANEL_W = 312;
constexpr int LEFT_X = 4;
constexpr int RIGHT_X = 324;

/* Number of channel columns that fit in the pattern view. */
int pattern_channels_visible();
/* The pattern editor view: row numbers, channel columns, highlight bar.
   row_frac 0..1 scrolls smoothly towards the next row. */
void pattern_view(const Player& p, const Snapshot& s, int ch_offset, float row_frac, int frame, int energy, int fx_level);

void info_panel(const TrackInfo& ti, const Snapshot& s, const char* detail, int volume, bool radio_on, int ch_first, int ch_shown);
void instrument_panel(const Player& p, const Snapshot& s, int frame);
void scope_panel(const int16_t* samples, int n, const Snapshot& s);
void copyright_line(int y);

/* Overlays. */
void now_playing_card(const TrackInfo& ti, const char* source, const char* station, int alpha256);
void toast(const char* line1, const char* line2, int alpha256);
void file_browser(const Library& lib, int sel, int first);
/* Station list: fixed stations followed by all genres. */
void station_picker(int source, int sel, int first, int current);
/* Which archive to listen to; shown before the station list. */
void source_picker(int sel, int current);
void help_overlay();
/* "DOWNLOADING FROM ..." with the name of the source in use. */
void loading_badge(int frame, int source);

} /* namespace ui */

#endif
