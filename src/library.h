/* Local module files in the music/ folder next to eboot.elf. */
#ifndef LIBRARY_H
#define LIBRARY_H

#include <string>
#include <vector>

class Library {
public:
	/* Scans <appdir>/music and the fixed PS5 homebrew location. */
	void scan(const char* appdir);
	int count() const { return (int)files_.size(); }
	const char* path(int i) const { return files_[i].path.c_str(); }
	const char* name(int i) const { return files_[i].name.c_str(); }
	int current() const { return current_; }
	void set_current(int i) { current_ = i; }
	int next() { return count() ? (current_ = (current_ + 1) % count()) : -1; }
	int prev() { return count() ? (current_ = (current_ + count() - 1) % count()) : -1; }
	const char* folder() const { return folder_.c_str(); }

private:
	struct Entry { std::string name, path; };
	void scan_dir(const std::string& dir);
	std::vector<Entry> files_;
	std::string folder_;
	int current_ = 0;
};

#endif
