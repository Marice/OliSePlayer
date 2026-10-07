/* Finds the folder the running title was mounted into.
 *
 * The method comes from ProsperoStore (src/system/app_folder.cpp,
 * Copyright (C) 2026 BlackBearReloaded, GPL-3.0-or-later): check the
 * candidates for this app's own eboot.bin instead of trusting one layout.
 * Testing for the file rather than the directory matters, because an empty
 * directory with the right name would otherwise win. */
#include "appdir.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "dbg.h"
#include "version.h"

namespace {

char g_folder[128] = "";

bool holds_eboot(const char* dir)
{
	char path[160];
	if (snprintf(path, sizeof(path), "%s/eboot.bin", dir) >= (int)sizeof(path)) return false;
	struct stat info;
	return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

} /* namespace */

const char* app_folder()
{
	if (g_folder[0]) return g_folder;

	static const char* const CANDIDATES[] = {
		"/app0",
		"/system_ex/app/" OLISE_TITLE_ID,
		"/mnt/sandbox/" OLISE_TITLE_ID "_000/app0",
	};
	/* Report what each candidate looked like, so a wrong guess is visible on
	   the console instead of only in a log nobody there can read. */
	char seen[200] = {0};
	int seen_len = 0;
	for (size_t i = 0; i < sizeof(CANDIDATES) / sizeof(CANDIDATES[0]); i++) {
		struct stat info;
		const bool dir_exists = stat(CANDIDATES[i], &info) == 0 && S_ISDIR(info.st_mode);
		const bool has_eboot = holds_eboot(CANDIDATES[i]);
		if (seen_len < (int)sizeof(seen) - 8)
			seen_len += snprintf(seen + seen_len, sizeof(seen) - seen_len, "%s%zu%c%c",
			                     seen_len ? " " : "", i,
			                     dir_exists ? 'd' : '-', has_eboot ? 'e' : '-');
		if (has_eboot) {
			snprintf(g_folder, sizeof(g_folder), "%s", CANDIDATES[i]);
			dbg_log("appdir: %s (%s)", g_folder, seen);
			return g_folder;
		}
	}
	dbg_log("appdir: no eboot.bin found (%s)", seen);

	/* The desktop build runs from the repo root, where there is no eboot.bin;
	   "." then makes music/ and presets/ resolve the way they do in the tree. */
	struct stat info;
	if (stat("music", &info) == 0 || stat("presets", &info) == 0) {
		snprintf(g_folder, sizeof(g_folder), "%s", ".");
	} else {
		snprintf(g_folder, sizeof(g_folder), "%s", "/app0");
	}
	dbg_log("appdir: no eboot.bin found, using %s", g_folder);
	return g_folder;
}
