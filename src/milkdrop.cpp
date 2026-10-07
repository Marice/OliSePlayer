#ifdef OLISE_PROJECTM
/* projectM integration: the real MilkDrop presets.
 *
 * projectM owns its own OpenGL state while it renders, so this file keeps the
 * app's side of the contract: render projectM first, then let the interface
 * draw over it, and put the viewport back afterwards.
 *
 * Everything here is defensive. The console refuses things the desktop allows,
 * and a preset is a script written by someone else: anything that goes wrong
 * must leave the app playing music with the built-in visualiser, not crash it. */
#include "milkdrop.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

#include <projectM-4/projectM.h>

#include "appdir.h"
#include "dbg.h"
#include "version.h"

namespace milkdrop {
namespace {

constexpr int MAX_PRESETS = 1024;
constexpr int PATH_MAX_LEN = 320;
/* The mesh is the grid projectM evaluates the per-pixel scripts on. 32x24 is
   what MilkDrop used on the hardware of its day; larger is smoother and much
   more expensive, and this has to share the console with the music. */
constexpr int MESH_W = 32;
constexpr int MESH_H = 24;

struct Entry {
	char path[PATH_MAX_LEN];
	char name[64];
};

projectm_handle g_pm = nullptr;
Entry* g_presets = nullptr;
int g_count = 0;
int g_current = -1;
bool g_ready = false;
int g_screen_w = 0, g_screen_h = 0;

/* ASCII-only extension match. strcasecmp goes through the C library's locale
   handling, which a title sandbox does not set up, so the comparison is done
   here instead: it cannot be affected by what the console does or does not
   initialise. */
bool ends_with_ci(const char* name, const char* ext)
{
	const size_t n = strlen(name), e = strlen(ext);
	if (n < e) return false;
	const char* p = name + (n - e);
	for (size_t i = 0; i < e; i++) {
		char a = p[i], b = ext[i];
		if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
		if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
		if (a != b) return false;
	}
	return true;
}

bool is_preset(const char* name)
{
	return ends_with_ci(name, ".milk");
}

/* Adds every .milk file in one directory. Recursion is deliberate: the
   curated packs ship their presets in themed subfolders. */
void scan_dir(const char* dir, int depth)
{
	if (depth > 3 || g_count >= MAX_PRESETS) return;
	DIR* d = opendir(dir);
	if (!d) {
		if (depth == 0) dbg_log("milkdrop: %s: cannot open (errno=%d)", dir, errno);
		return;
	}
	const int before = g_count;
	int entries = 0;
	char first[64] = {0};

	struct dirent* e;
	while ((e = readdir(d)) != nullptr && g_count < MAX_PRESETS) {
		if (e->d_name[0] == '.') continue;
		/* Count what readdir actually hands back, and keep the first name:
		   "the folder opens but holds nothing" and "it holds files whose
		   names do not end in .milk" need different fixes. */
		entries++;
		if (!first[0]) snprintf(first, sizeof(first), "%s", e->d_name);
		/* Write the first entries out verbatim, with the result of the
		   extension test next to each one. Whatever is wrong, it shows here:
		   an empty folder, mangled names, or a test that says no to a name
		   that plainly ends in .milk. */
		if (depth == 0 && entries <= 10)
			dbg_log("  [%d] '%s' len=%zu milk=%d", entries, e->d_name,
			        strlen(e->d_name), is_preset(e->d_name) ? 1 : 0);
		char full[PATH_MAX_LEN];
		if (snprintf(full, sizeof(full), "%s/%s", dir, e->d_name) >= (int)sizeof(full))
			continue; /* too long to open anyway */

		if (is_preset(e->d_name)) {
			Entry& en = g_presets[g_count];
			snprintf(en.path, sizeof(en.path), "%s", full);
			/* Show the preset name without the author prefix or extension:
			   "Geiss - Spiral Artifact.milk" becomes "SPIRAL ARTIFACT". */
			const char* label = strstr(e->d_name, " - ");
			label = label ? label + 3 : e->d_name;
			snprintf(en.name, sizeof(en.name), "%s", label);
			char* dot = strrchr(en.name, '.');
			if (dot) *dot = 0;
			for (char* p = en.name; *p; p++)
				if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
			g_count++;
		} else if (e->d_type == DT_DIR || e->d_type == DT_UNKNOWN) {
			/* Not every file system fills in d_type, and a wrong guess here
			   would silently skip a whole folder; opendir settles it. */
			scan_dir(full, depth + 1);
		}
	}
	closedir(d);
	if (depth == 0) {
		dbg_log("milkdrop: %s: %d entries, %d preset(s), first '%s'",
		        dir, entries, g_count - before, first);
		if (g_count == before && entries)
			dbg_log("milkdrop: %d entries but no .milk, first '%s'", entries, first);
	}
}

/* Reads <dir>/index.txt: one preset filename per line. The names are taken on
   trust here, unlike the music index: checking 552 files one by one costs a
   visible pause at startup, and projectM reports a file it cannot read. */
void read_index(const char* dir)
{
	char path[PATH_MAX_LEN];
	snprintf(path, sizeof(path), "%s/index.txt", dir);
	FILE* f = fopen(path, "r");
	if (!f) {
		dbg_log("milkdrop: %s: no index (errno=%d)", path, errno);
		return;
	}

	char line[256];
	while (fgets(line, sizeof(line), f) && g_count < MAX_PRESETS) {
		size_t n = strlen(line);
		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = 0;
		if (!n || line[0] == '#' || !is_preset(line)) continue;

		Entry& en = g_presets[g_count];
		if (snprintf(en.path, sizeof(en.path), "%s/%s", dir, line) >= (int)sizeof(en.path))
			continue;
		const char* label = strstr(line, " - ");
		label = label ? label + 3 : line;
		snprintf(en.name, sizeof(en.name), "%s", label);
		char* dot = strrchr(en.name, '.');
		if (dot) *dot = 0;
		for (char* p = en.name; *p; p++)
			if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
		g_count++;
	}
	fclose(f);
	dbg_log("milkdrop: index %s: %d preset(s)", path, g_count);
}

void load(int index)
{
	if (!g_pm || index < 0 || index >= g_count) return;
	g_current = index;
	/* smooth_transition 0: a hard cut. The blend needs a second render target
	   and more time per frame than this console has to spare here. */
	projectm_load_preset_file(g_pm, g_presets[index].path, false);
	dbg_log("milkdrop: %s", g_presets[index].path);
}

} /* namespace */

bool init(int screen_w, int screen_h)
{
	if (g_ready) return true;

	g_presets = (Entry*)calloc(MAX_PRESETS, sizeof(Entry));
	if (!g_presets) return false;

	/* The app folder is located once, by looking for this app's own
	   eboot.bin. /data cannot be read from a title sandbox without elevated
	   privileges, which is why presets live next to the app. */
	char dir[PATH_MAX_LEN];
	snprintf(dir, sizeof(dir), "%s/presets", app_folder());
	scan_dir(dir, 0);
	/* ShadowMountPlus refuses to list a folder from inside a title sandbox
	   (EPERM), while opening a file by name works. The build writes
	   presets/index.txt with one name per line for exactly that case. */
	if (g_count == 0) read_index(dir);

	if (g_count == 0) {
		dbg_log("milkdrop: no presets in %s, using the built-in visualiser", dir);
		free(g_presets);
		g_presets = nullptr;
		return false;
	}

	g_pm = projectm_create();
	if (!g_pm) {
		dbg_log("milkdrop: projectm_create failed");
		free(g_presets);
		g_presets = nullptr;
		return false;
	}

	g_screen_w = screen_w;
	g_screen_h = screen_h;
	projectm_set_window_size(g_pm, (size_t)screen_w, (size_t)screen_h);
	projectm_set_mesh_size(g_pm, MESH_W, MESH_H);
	projectm_set_fps(g_pm, 60);
	/* The app changes presets itself, so projectM must not do it on a timer. */
	projectm_set_preset_duration(g_pm, 1.0e6);
	projectm_set_soft_cut_duration(g_pm, 0.0);

	load(0);
	g_ready = true;
	dbg_log("milkdrop: ready, %d preset(s)", g_count);
	return true;
}

bool available() { return g_ready; }
int count() { return g_count; }

const char* current_name()
{
	if (!g_ready || g_current < 0) return "";
	return g_presets[g_current].name;
}

void step(int delta)
{
	if (!g_ready || g_count == 0) return;
	int next = (g_current + delta) % g_count;
	if (next < 0) next += g_count;
	load(next);
}

void shuffle()
{
	if (!g_ready || g_count == 0) return;
	load(rand() % g_count);
}

void add_audio(const int16_t* samples, int n)
{
	if (!g_ready || !samples || n <= 0) return;
	projectm_pcm_add_int16(g_pm, samples, (unsigned)n, PROJECTM_MONO);
}

void render()
{
	if (!g_ready) return;
	/* projectM leaves its own viewport and bindings behind, so the caller
	   restores what it needs afterwards. */
	glViewport(0, 0, g_screen_w, g_screen_h);
	projectm_opengl_render_frame(g_pm);
}

void shutdown()
{
	if (g_pm) { projectm_destroy(g_pm); g_pm = nullptr; }
	if (g_presets) { free(g_presets); g_presets = nullptr; }
	g_count = 0;
	g_current = -1;
	g_ready = false;
}

} /* namespace milkdrop */

#endif /* OLISE_PROJECTM */
