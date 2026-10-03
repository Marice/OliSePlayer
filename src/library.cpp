#include "library.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool has_module_ext(const char* name)
{
	const char* dot = strrchr(name, '.');
	if (!dot) return false;
	return strcasecmp(dot, ".mod") == 0 || strcasecmp(dot, ".xm") == 0 ||
	       strcasecmp(dot, ".s3m") == 0 || strcasecmp(dot, ".it") == 0;
}

void Library::scan_dir(const char* dir)
{
	DIR* d = opendir(dir);
	if (!d) return;
	if (!folder_[0]) strncpy(folder_, dir, sizeof(folder_) - 1);
	struct dirent* e;
	while ((e = readdir(d)) != nullptr && count_ < LIBRARY_MAX) {
		if (e->d_name[0] == '.' || !has_module_ext(e->d_name)) continue;
		/* Skip duplicates when two scan roots point at the same folder. */
		bool dup = false;
		for (int i = 0; i < count_; i++) if (strcmp(files_[i].name, e->d_name) == 0) { dup = true; break; }
		if (dup) continue;
		Entry& en = files_[count_];
		snprintf(en.name, sizeof(en.name), "%s", e->d_name);
		snprintf(en.path, sizeof(en.path), "%s/%s", dir, e->d_name);
		count_++;
	}
	closedir(d);
}

static int compare_entries(const void* a, const void* b)
{
	const char* na = (const char*)a; /* Entry starts with name */
	const char* nb = (const char*)b;
	return strcasecmp(na, nb);
}

void Library::scan(const char* appdir)
{
	count_ = 0;
	folder_[0] = 0;
	if (appdir && *appdir) {
		char dir[LIBRARY_PATH];
		snprintf(dir, sizeof(dir), "%s/music", appdir);
		scan_dir(dir);
	}
	scan_dir("/data/homebrew/OliSePlayer/music");
	scan_dir("/app0/music");
	scan_dir("music"); /* desktop build run from the repo root */
	qsort(files_, (size_t)count_, sizeof(Entry), compare_entries);
	if (current_ >= count()) current_ = 0;
	fprintf(stderr, "library: %d module(s) in %s\n", count(), folder_[0] ? folder_ : "(no folder)");
}
