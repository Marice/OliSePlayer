#include "library.h"

#include "appdir.h"
#include "dbg.h"
#include "version.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ASCII-only extension match, for the same reason as in milkdrop.cpp:
   strcasecmp depends on a locale the title sandbox never sets up. */
static bool ends_with_ci(const char* name, const char* ext)
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

static bool has_module_ext(const char* name)
{
	return ends_with_ci(name, ".mod") || ends_with_ci(name, ".xm") ||
	       ends_with_ci(name, ".s3m") || ends_with_ci(name, ".it");
}

void Library::scan_dir(const char* dir)
{
	DIR* d = opendir(dir);
	if (!d) {
		dbg_log("library: %s: cannot open (errno=%d)", dir, errno);
		if (report_len_ < (int)sizeof(report_) - 24)
			report_len_ += snprintf(report_ + report_len_, sizeof(report_) - report_len_,
			                        "%s%.14s:X", report_len_ ? "  " : "", dir);
		return;
	}
	const int before = count_;
	int entries = 0;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr && count_ < LIBRARY_MAX) {
		if (e->d_name[0] == '.') continue;
		/* Same verbatim trace as the preset scan: the names as readdir hands
		   them over, with the extension test's answer beside each. */
		if (++entries <= 10)
			dbg_log("  [%d] '%s' len=%zu mod=%d", entries, e->d_name,
			        strlen(e->d_name), has_module_ext(e->d_name) ? 1 : 0);
		if (!has_module_ext(e->d_name)) continue;
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
	dbg_log("library: %s: %d module(s)", dir, count_ - before);
	/* Keep a short trace for the on-screen message: the last path segment and
	   how many modules it held, so an empty list says where it looked. */
	if (report_len_ < (int)sizeof(report_) - 24) {
		const char* tail = strrchr(dir, '/');
		tail = tail ? tail : dir;
		report_len_ += snprintf(report_ + report_len_, sizeof(report_) - report_len_,
		                        "%s%.14s:%d", report_len_ ? "  " : "", dir, count_ - before);
	}
	/* Report the folder the files actually came from, not merely the first
	   directory that happened to open: an empty /app0/music would otherwise
	   be shown while the modules sit elsewhere. */
	if (count_ > before && !folder_[0]) strncpy(folder_, dir, sizeof(folder_) - 1);
}

static int compare_entries(const void* a, const void* b)
{
	const char* na = (const char*)a; /* Entry starts with name */
	const char* nb = (const char*)b;
	return strcasecmp(na, nb);
}

/* Reads <dir>/index.txt: one filename per line, blank lines and lines starting
   with # ignored. Only files that actually open are kept, so a stale index
   cannot fill the list with names that are no longer there. */
void Library::read_index(const char* dir)
{
	char path[LIBRARY_PATH];
	snprintf(path, sizeof(path), "%s/index.txt", dir);
	FILE* f = fopen(path, "r");
	if (!f) {
		dbg_log("library: %s: no index (errno=%d)", path, errno);
		return;
	}

	char line[256];
	while (fgets(line, sizeof(line), f) && count_ < LIBRARY_MAX) {
		size_t n = strlen(line);
		while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = 0;
		if (!n || line[0] == '#' || !has_module_ext(line)) continue;

		Entry& en = files_[count_];
		snprintf(en.path, sizeof(en.path), "%s/%s", dir, line);
		FILE* probe = fopen(en.path, "rb");
		if (!probe) continue;
		fclose(probe);
		snprintf(en.name, sizeof(en.name), "%s", line);
		count_++;
	}
	fclose(f);
	if (!folder_[0] && count_) strncpy(folder_, dir, sizeof(folder_) - 1);
	dbg_log("library: index %s: %d module(s)", path, count_);
}

void Library::scan(const char* appdir)
{
	count_ = 0;
	folder_[0] = 0;
	report_[0] = 0;
	report_len_ = 0;
	if (appdir && *appdir) {
		char dir[LIBRARY_PATH];
		snprintf(dir, sizeof(dir), "%s/music", appdir);
		scan_dir(dir);
	}
	/* The app folder is located once, by looking for this app's own
	   eboot.bin; /data is out of reach from the sandbox without elevated
	   privileges, so modules have to live next to the app. */
	char dir2[LIBRARY_PATH];
	snprintf(dir2, sizeof(dir2), "%s/music", app_folder());
	scan_dir(dir2);
	/* ShadowMountPlus refuses to list a folder from inside a title sandbox
	   (EPERM), while opening a file by name works. The build therefore writes
	   music/index.txt with one filename per line, and that index is read when
	   the directory scan comes up empty. */
	if (count_ == 0) read_index(dir2);
	qsort(files_, (size_t)count_, sizeof(Entry), compare_entries);
	if (current_ >= count()) current_ = 0;
	dbg_log("library: %d module(s) in %s", count(), folder_[0] ? folder_ : "(no folder)");
}
