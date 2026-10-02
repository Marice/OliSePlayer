#include "library.h"

#include <algorithm>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool has_module_ext(const char* name)
{
	const char* dot = strrchr(name, '.');
	if (!dot) return false;
	return strcasecmp(dot, ".mod") == 0 || strcasecmp(dot, ".xm") == 0 ||
	       strcasecmp(dot, ".s3m") == 0 || strcasecmp(dot, ".it") == 0;
}

void Library::scan_dir(const std::string& dir)
{
	DIR* d = opendir(dir.c_str());
	if (!d) return;
	if (folder_.empty()) folder_ = dir;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr) {
		if (e->d_name[0] == '.' || !has_module_ext(e->d_name)) continue;
		Entry en;
		en.name = e->d_name;
		en.path = dir + "/" + e->d_name;
		/* Skip duplicates when both scan roots point at the same folder. */
		bool dup = false;
		for (const Entry& x : files_) if (x.name == en.name) { dup = true; break; }
		if (!dup) files_.push_back(en);
	}
	closedir(d);
}

void Library::scan(const char* appdir)
{
	files_.clear();
	folder_.clear();
	if (appdir && *appdir) scan_dir(std::string(appdir) + "/music");
	scan_dir("/data/homebrew/OliSePlayer/music");
	scan_dir("music"); /* desktop build run from the repo root */
	std::sort(files_.begin(), files_.end(),
	          [](const Entry& a, const Entry& b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
	if (current_ >= count()) current_ = 0;
	fprintf(stderr, "library: %d module(s) in %s\n", count(), folder_.empty() ? "(no folder)" : folder_.c_str());
}
