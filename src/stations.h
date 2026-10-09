/* Radio stations: what to ask The Mod Archive for.
 *
 * The genre table mirrors https://modarchive.org/index.php?request=view_genres
 * (ids are stable, they are used in the site's search URLs). */
#ifndef STATIONS_H
#define STATIONS_H

enum StationKind {
	STATION_RANDOM = 0,   /* random module, any supported format or one format */
	STATION_GENRE,        /* random module from one genre */
	STATION_FEATURED,     /* random pick from the "featured" chart */
	STATION_TOPSCORE,     /* random pick from the "top rated" chart */
	STATION_PLAYLIST      /* random pick from a Modland playlist */
};

/* Where the music comes from. The picker asks this first, because the two
   archives are organised differently: the Mod Archive by genre, Modland by
   curated playlist. One list of genres covering both would be a lie. */
enum SourceId {
	SOURCE_MODARCHIVE = 0,
	SOURCE_MODLAND = 1
};

/* Named NetSource because main.cpp already has a Source for "is the current
   track local or from the radio", which is a different question. */
struct NetSource {
	SourceId id;
	const char* name;
	const char* detail;
};

static const NetSource SOURCES[] = {
	{ SOURCE_MODARCHIVE, "THE MOD ARCHIVE", "160.000 MODULES, BY GENRE AND CHART" },
	{ SOURCE_MODLAND,    "MODLAND",         "CURATED PLAYLISTS, CHIPTUNE AND CLASSICS" },
};
static const int NUM_SOURCES = (int)(sizeof(SOURCES) / sizeof(SOURCES[0]));

/* The name to put in front of the user: "THE MOD ARCHIVE" or "MODLAND". */
static inline const char* source_name(int source)
{
	if (source < 0 || source >= NUM_SOURCES) source = SOURCE_MODARCHIVE;
	return SOURCES[source].name;
}

struct Station {
	StationKind kind;
	int genre_id;          /* STATION_GENRE only */
	const char* format;    /* "MOD", "XM", "S3M", "IT" or nullptr for any */
	const char* name;      /* label shown in the UI */
	const char* playlist;  /* STATION_PLAYLIST: path below /pub/playlists/ */
};

struct GenreEntry {
	int id;
	const char* name;
};

static const GenreEntry GENRES[] = {
	{ 54, "Chiptune" }, { 55, "Demo Style" }, { 8, "Video Game" }, { 53, "One Hour Compo" },
	{ 1, "Electronic (general)" }, { 2, "Electronic - Ambient" }, { 3, "Electronic - Dance" },
	{ 6, "Electronic - Drum & Bass" }, { 7, "Electronic - Techno" }, { 9, "Electronic - Breakbeat" },
	{ 10, "Electronic - House" }, { 11, "Electronic - Progressive" }, { 34, "Electronic - Industrial" },
	{ 39, "Electronic - Hardcore" }, { 40, "Electronic - Gabber" }, { 60, "Electronic - Jungle" },
	{ 65, "Electronic - Rave" }, { 99, "Electronic - IDM" }, { 100, "Electronic - Other" },
	{ 101, "Electronic - Minimal" },
	{ 71, "Trance (general)" }, { 63, "Trance - Acid" }, { 64, "Trance - Hard" }, { 66, "Trance - Goa" },
	{ 67, "Trance - Dream" }, { 70, "Trance - Tribal" }, { 85, "Trance - Progressive" },
	{ 12, "Pop (general)" }, { 61, "Pop - Synth" }, { 62, "Pop - Soft" }, { 58, "Disco" },
	{ 13, "Rock (general)" }, { 14, "Rock - Hard" }, { 15, "Rock - Soft" }, { 48, "Alternative" },
	{ 103, "Grunge" }, { 35, "Punk" }, { 36, "Metal (general)" }, { 37, "Metal - Extreme" }, { 38, "Gothic" },
	{ 29, "Jazz (general)" }, { 30, "Jazz - Acid" }, { 31, "Jazz - Modern" }, { 32, "Funk" }, { 25, "Soul" },
	{ 26, "R & B" }, { 22, "Hip-Hop" }, { 112, "Lofi - HipHop" }, { 111, "Lofi - Ambient" }, { 106, "Chillout" },
	{ 107, "Easy Listening" }, { 74, "Big Band" }, { 75, "Swing" }, { 102, "Fusion" }, { 19, "Blues" },
	{ 18, "Country" }, { 105, "Bluegrass" }, { 21, "Folk" }, { 42, "World" }, { 27, "Reggae" }, { 24, "Ska" },
	{ 20, "Classical" }, { 50, "Orchestral" }, { 59, "Piano" }, { 28, "Medieval" }, { 52, "Fantasy" },
	{ 43, "Soundtrack" }, { 44, "New Age" }, { 56, "Ballad" }, { 76, "Vocal Montage" },
	{ 45, "Comedy" }, { 46, "Experimental" }, { 47, "Spiritual" }, { 49, "Religious" },
	{ 72, "Christmas" }, { 82, "Halloween" }, { 41, "Other" },
};
static const int NUM_GENRES = (int)(sizeof(GENRES) / sizeof(GENRES[0]));

/* Fixed stations ahead of the genre list in the picker. */
static const Station FIXED_STATIONS[] = {
	{ STATION_RANDOM,   0, nullptr, "RANDOM - ANY FORMAT", nullptr },
	{ STATION_FEATURED, 0, nullptr, "FEATURED PICKS", nullptr },
	{ STATION_TOPSCORE, 0, nullptr, "TOP RATED", nullptr },
	{ STATION_RANDOM,   0, "MOD",   "RANDOM - MOD (PROTRACKER)", nullptr },
	{ STATION_RANDOM,   0, "XM",    "RANDOM - XM (FASTTRACKER II)", nullptr },
	{ STATION_RANDOM,   0, "S3M",   "RANDOM - S3M (SCREAM TRACKER)", nullptr },
	{ STATION_RANDOM,   0, "IT",    "RANDOM - IT (IMPULSE TRACKER)", nullptr },
};
static const int NUM_FIXED_STATIONS = (int)(sizeof(FIXED_STATIONS) / sizeof(FIXED_STATIONS[0]));

/* Modland stations. Each is one playlist, filtered by format where that
   leaves enough to choose from: the favourites hold 1253 MOD and 398 XM, the
   other formats only a handful, so those are not offered on their own. */
static const Station MODLAND_STATIONS[] = {
	{ STATION_PLAYLIST, 0, nullptr, "FAVOURITES - ANY FORMAT",   "favourites_by_coma" },
	{ STATION_PLAYLIST, 0, "MOD",   "FAVOURITES - MOD",          "favourites_by_coma" },
	{ STATION_PLAYLIST, 0, "XM",    "FAVOURITES - XM",           "favourites_by_coma" },
	{ STATION_PLAYLIST, 0, nullptr, "CHIPFEST - CHIPTUNES",      "chipfest_by_coma" },
	{ STATION_PLAYLIST, 0, nullptr, "MONO - NETLABEL RELEASES",  "mono_netlabel" },
	{ STATION_PLAYLIST, 0, nullptr, "CHRISTMAS CHIPS",           "musicdisks/christmas_chips" },
	{ STATION_PLAYLIST, 0, nullptr, "STAGE NINE - MUSICDISK",    "musicdisks/stage_nine" },
	{ STATION_PLAYLIST, 0, nullptr, "MEKKA 96 - DEMOPARTY",      "compos/mekka_96" },
};
static const int NUM_MODLAND_STATIONS =
	(int)(sizeof(MODLAND_STATIONS) / sizeof(MODLAND_STATIONS[0]));

/* How many stations a source offers, and the one at an index within it. */
static inline int station_count_for(int source)
{
	return source == SOURCE_MODLAND ? NUM_MODLAND_STATIONS
	                                : NUM_FIXED_STATIONS + NUM_GENRES;
}

/* Total number of selectable stations: fixed ones followed by the genres. */
static inline int station_count() { return NUM_FIXED_STATIONS + NUM_GENRES; }

static inline Station station_at_for(int source, int index)
{
	if (source == SOURCE_MODLAND) {
		if (index < 0) index = 0;
		if (index >= NUM_MODLAND_STATIONS) index = NUM_MODLAND_STATIONS - 1;
		return MODLAND_STATIONS[index];
	}
	if (index < NUM_FIXED_STATIONS) return FIXED_STATIONS[index];
	int g = index - NUM_FIXED_STATIONS;
	if (g < 0) g = 0;
	if (g >= NUM_GENRES) g = NUM_GENRES - 1;
	Station s = { STATION_GENRE, GENRES[g].id, nullptr, GENRES[g].name, nullptr };
	return s;
}

static inline Station station_at(int index)
{
	return station_at_for(SOURCE_MODARCHIVE, index);
}

#endif
