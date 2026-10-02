/* Mod Archive radio: background downloads plus a history for "previous". */
#ifndef RADIO_H
#define RADIO_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

struct RadioTrack {
	uint8_t* data = nullptr;
	size_t len = 0;
	long module_id = 0;
	char title[96] = {0};
	char type[64] = {0};
};

enum class RadioState { Idle, Loading, Ready, Failed };

class Radio {
public:
	~Radio();

	/* True when the build has network support. */
	static bool available();

	/* Start a download unless one is running. */
	bool fetch();
	RadioState state() const;
	/* Hand over a finished download (state goes back to Idle). The caller
	   owns t.data until it passes the track to remember(). */
	bool take(RadioTrack& t);
	const char* error() const { return error_; }
	void ack_failed();

	/* History of played Mod Archive tracks; remember() takes ownership. */
	void remember(RadioTrack& t);
	bool has_prev() const { return idx_ > 0; }
	bool has_next() const { return idx_ + 1 < (int)hist_.size(); }
	const RadioTrack* prev();
	const RadioTrack* next();
	const RadioTrack* current() const { return (idx_ >= 0 && idx_ < (int)hist_.size()) ? &hist_[idx_] : nullptr; }
	int history_size() const { return (int)hist_.size(); }
	int history_index() const { return idx_; }

private:
	static int worker(void* arg);

	SDL_Thread* thread_ = nullptr;
	SDL_atomic_t state_ = {0};
	RadioTrack pending_;
	char error_[128] = {0};
	std::vector<RadioTrack> hist_;
	int idx_ = -1;
	size_t hist_bytes_ = 0;
};

#endif
