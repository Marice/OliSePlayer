/* Local module files in the music/ folder next to the app.
 * Plain arrays, no C++ runtime library: the native title build links no libc++. */
#ifndef LIBRARY_H
#define LIBRARY_H

constexpr int LIBRARY_MAX = 512;
constexpr int LIBRARY_PATH = 512;
constexpr int LIBRARY_NAME = 128;

class Library {
public:
	/* Scans <appdir>/music plus the fixed PS5 locations. */
	void scan(const char* appdir);
	int count() const { return count_; }
	const char* path(int i) const { return files_[i].path; }
	const char* name(int i) const { return files_[i].name; }
	int current() const { return current_; }
	void set_current(int i) { current_ = i; }
	int next() { return count() ? (current_ = (current_ + 1) % count()) : -1; }
	int prev() { return count() ? (current_ = (current_ + count() - 1) % count()) : -1; }
	const char* folder() const { return folder_; }

private:
	struct Entry {
		char name[LIBRARY_NAME];
		char path[LIBRARY_PATH];
	};
	void scan_dir(const char* dir);
	Entry files_[LIBRARY_MAX];
	int count_ = 0;
	char folder_[LIBRARY_PATH] = {0};
	int current_ = 0;
};

#endif
