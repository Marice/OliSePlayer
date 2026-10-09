/* Mod Archive radio: background downloads, stations and a history. */
#ifndef RADIO_H
#define RADIO_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>

#include "stations.h"

struct RadioTrack {
	uint8_t* data = nullptr;
	size_t len = 0;
	long module_id = 0;
	char title[96] = {0};
	char type[64] = {0};     /* tracker name as libxmp reports it */
	char format[8] = {0};    /* MOD / XM / S3M / IT */
	char genre[48] = {0};    /* when the site told us */
	char artist[48] = {0};
	int station = 0;         /* station index it was fetched with */
};

enum class RadioState { Idle, Loading, Ready, Failed };

class Radio {
public:
	~Radio();

	/* True when the build has network support. */
	static bool available();

	/* Station selection (index into stations.h). */
	void set_station(int index);
	int station() const { return station_; }
	int source() const { return source_; }
	void set_source(int source);
	Station station_info() const { return station_at_for(source_, station_); }

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
	bool has_next() const { return idx_ + 1 < hist_n_; }
	const RadioTrack* prev();
	const RadioTrack* next();
	const RadioTrack* current() const { return (idx_ >= 0 && idx_ < hist_n_) ? &hist_[idx_] : nullptr; }
	int history_size() const { return hist_n_; }
	int history_index() const { return idx_; }

private:
	static int worker(void* arg);

	SDL_Thread* thread_ = nullptr;
	SDL_atomic_t state_ = {0};
	RadioTrack pending_;
	int station_ = 0;
	int job_station_ = 0;
	int source_ = SOURCE_MODARCHIVE;
	int job_source_ = SOURCE_MODARCHIVE;
	char error_[128] = {0};
	static constexpr int HISTORY_CAP = 20;
	RadioTrack hist_[HISTORY_CAP];
	int hist_n_ = 0;
	int idx_ = -1;
	size_t hist_bytes_ = 0;
};

#endif
