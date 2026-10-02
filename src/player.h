/* libxmp wrapper: owns the player context and the SDL audio device. */
#ifndef PLAYER_H
#define PLAYER_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>

#include "xmp.h"

constexpr int PLAYER_RATE = 48000;
constexpr int SCOPE_SAMPLES = 1024;

struct TrackInfo {
	char title[64];
	char type[64];
	int channels;
	int instruments;
	int patterns;
	int length;        /* song length in positions */
	int total_time_ms; /* estimated */
	bool loaded;
};

struct ChannelState {
	int volume;     /* 0..64 */
	int note;       /* base note, 0 if none */
	int instrument;
	int peak;       /* decaying peak for the VU meters, 0..64 */
};

struct Snapshot {
	bool loaded;
	bool paused;
	int pos, pattern, row, num_rows;
	int speed, bpm;
	int time_ms, total_ms;
	int frame_time_us; /* duration of one tick */
	int chn;
	ChannelState ch[XMP_MAX_CHANNELS];
};

class Player {
public:
	bool init();
	void shutdown();

	/* Validate a module in memory; fills name/type when given. */
	static bool test_memory(const void* data, size_t len, char* name, char* type);

	/* loop: 0 = play forever, 1 = play once (then track_ended() turns true). */
	bool load_memory(const void* data, size_t len, int loop);
	bool load_file(const char* path, int loop);
	void unload();

	void set_paused(bool p);
	bool paused() const { return paused_; }
	void set_volume(int percent); /* 0..100 */
	int volume() const { return volume_; }
	void seek_positions(int delta);

	/* True once after a loop==1 track reached its end. */
	bool track_ended();

	void snapshot(Snapshot& s);
	const TrackInfo& info() const { return info_; }
	/* Pattern data; stable while the module is loaded (main thread only). */
	const xmp_module* module() const { return mod_; }

	/* Most recent mono samples for the oscilloscope, oldest first. */
	void scope(int16_t* out, int n);

private:
	static void audio_cb(void* userdata, Uint8* stream, int len);
	bool apply_loaded(int loop);

	xmp_context ctx_ = nullptr;
	SDL_AudioDeviceID dev_ = 0;
	const xmp_module* mod_ = nullptr;
	TrackInfo info_ = {};
	int loop_ = 0;
	int volume_ = 80;
	bool paused_ = false;
	bool loaded_ = false;
	SDL_atomic_t ended_ = {0};
	int16_t ring_[SCOPE_SAMPLES * 2] = {0};
	SDL_atomic_t ring_pos_ = {0};
	int peak_[XMP_MAX_CHANNELS] = {0};
};

#endif
