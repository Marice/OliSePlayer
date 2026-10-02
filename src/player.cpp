#include "player.h"

#include <stdio.h>
#include <string.h>

bool Player::init()
{
	ctx_ = xmp_create_context();
	if (!ctx_) {
		fprintf(stderr, "player: xmp_create_context failed\n");
		return false;
	}
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = PLAYER_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = audio_cb;
	want.userdata = this;
	dev_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!dev_) {
		fprintf(stderr, "player: no audio device: %s\n", SDL_GetError());
		return false;
	}
	SDL_PauseAudioDevice(dev_, 0);
	return true;
}

void Player::shutdown()
{
	if (dev_) {
		SDL_CloseAudioDevice(dev_); /* stops the callback first */
		dev_ = 0;
	}
	unload();
	if (ctx_) {
		xmp_free_context(ctx_);
		ctx_ = nullptr;
	}
}

bool Player::test_memory(const void* data, size_t len, char* name, char* type)
{
	struct xmp_test_info ti;
	if (xmp_test_module_from_memory(data, (long)len, &ti) != 0) return false;
	if (name) strncpy(name, ti.name, XMP_NAME_SIZE);
	if (type) strncpy(type, ti.type, XMP_NAME_SIZE);
	return true;
}

void Player::unload()
{
	if (!ctx_) return;
	SDL_LockAudioDevice(dev_);
	if (loaded_) {
		xmp_end_player(ctx_);
		xmp_release_module(ctx_);
	}
	loaded_ = false;
	mod_ = nullptr;
	info_ = TrackInfo();
	SDL_UnlockAudioDevice(dev_);
}

bool Player::apply_loaded(int loop)
{
	struct xmp_module_info mi;
	if (xmp_start_player(ctx_, PLAYER_RATE, 0) != 0) {
		xmp_release_module(ctx_);
		return false;
	}
	xmp_set_player(ctx_, XMP_PLAYER_VOLUME, volume_);
	xmp_get_module_info(ctx_, &mi);
	mod_ = mi.mod;
	strncpy(info_.title, mi.mod->name, sizeof(info_.title) - 1);
	strncpy(info_.type, mi.mod->type, sizeof(info_.type) - 1);
	info_.channels = mi.mod->chn;
	info_.instruments = mi.mod->ins;
	info_.patterns = mi.mod->pat;
	info_.length = mi.mod->len;
	struct xmp_frame_info fi;
	xmp_get_frame_info(ctx_, &fi);
	info_.total_time_ms = fi.total_time;
	info_.loaded = true;
	loop_ = loop;
	loaded_ = true;
	memset(peak_, 0, sizeof(peak_));
	SDL_AtomicSet(&ended_, 0);
	return true;
}

bool Player::load_memory(const void* data, size_t len, int loop)
{
	if (!ctx_) return false;
	SDL_LockAudioDevice(dev_);
	if (loaded_) {
		xmp_end_player(ctx_);
		xmp_release_module(ctx_);
		loaded_ = false;
		mod_ = nullptr;
	}
	int rc = xmp_load_module_from_memory(ctx_, data, (long)len);
	bool ok = (rc == 0) && apply_loaded(loop);
	SDL_UnlockAudioDevice(dev_);
	if (!ok) fprintf(stderr, "player: load from memory failed (%d)\n", rc);
	return ok;
}

bool Player::load_file(const char* path, int loop)
{
	if (!ctx_) return false;
	SDL_LockAudioDevice(dev_);
	if (loaded_) {
		xmp_end_player(ctx_);
		xmp_release_module(ctx_);
		loaded_ = false;
		mod_ = nullptr;
	}
	int rc = xmp_load_module(ctx_, path);
	bool ok = (rc == 0) && apply_loaded(loop);
	SDL_UnlockAudioDevice(dev_);
	if (!ok) fprintf(stderr, "player: load %s failed (%d)\n", path, rc);
	return ok;
}

void Player::set_paused(bool p)
{
	paused_ = p;
}

void Player::set_volume(int percent)
{
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	volume_ = percent;
	if (ctx_ && loaded_) {
		SDL_LockAudioDevice(dev_);
		xmp_set_player(ctx_, XMP_PLAYER_VOLUME, volume_);
		SDL_UnlockAudioDevice(dev_);
	}
}

void Player::seek_positions(int delta)
{
	if (!ctx_ || !loaded_) return;
	SDL_LockAudioDevice(dev_);
	struct xmp_frame_info fi;
	xmp_get_frame_info(ctx_, &fi);
	int pos = fi.pos + delta;
	if (pos < 0) pos = 0;
	if (pos >= info_.length) pos = info_.length - 1;
	xmp_set_position(ctx_, pos);
	SDL_UnlockAudioDevice(dev_);
}

bool Player::track_ended()
{
	return SDL_AtomicSet(&ended_, 0) != 0;
}

void Player::audio_cb(void* userdata, Uint8* stream, int len)
{
	Player* self = (Player*)userdata;
	bool silent = true;
	if (self->loaded_ && !self->paused_) {
		int rc = xmp_play_buffer(self->ctx_, stream, len, self->loop_);
		if (rc == 0) {
			silent = false;
		} else if (rc == -XMP_END) {
			SDL_AtomicSet(&self->ended_, 1);
		}
	}
	if (silent) memset(stream, 0, len);

	/* Feed the oscilloscope ring with a mono mix. */
	const int16_t* s = (const int16_t*)stream;
	int frames = len / 4;
	int pos = SDL_AtomicGet(&self->ring_pos_);
	for (int i = 0; i < frames; i++) {
		self->ring_[pos] = (int16_t)(((int)s[2 * i] + (int)s[2 * i + 1]) / 2);
		pos = (pos + 1) % (SCOPE_SAMPLES * 2);
	}
	SDL_AtomicSet(&self->ring_pos_, pos);
}

void Player::snapshot(Snapshot& out)
{
	memset(&out, 0, sizeof(out));
	out.paused = paused_;
	if (!ctx_ || !loaded_) return;
	struct xmp_frame_info fi;
	SDL_LockAudioDevice(dev_);
	xmp_get_frame_info(ctx_, &fi);
	SDL_UnlockAudioDevice(dev_);
	out.loaded = true;
	out.pos = fi.pos;
	out.pattern = fi.pattern;
	out.row = fi.row;
	out.num_rows = fi.num_rows;
	out.speed = fi.speed;
	out.bpm = fi.bpm;
	out.time_ms = fi.time;
	out.total_ms = fi.total_time;
	out.frame_time_us = fi.frame_time;
	out.chn = info_.channels;
	for (int c = 0; c < out.chn && c < XMP_MAX_CHANNELS; c++) {
		const struct xmp_channel_info& ci = fi.channel_info[c];
		int v = paused_ ? 0 : ci.volume;
		if (v > 64) v = 64;
		if (v > peak_[c]) peak_[c] = v;
		else if (peak_[c] > 0) peak_[c] -= 1;
		out.ch[c].volume = v;
		out.ch[c].note = ci.note;
		out.ch[c].instrument = ci.instrument;
		out.ch[c].peak = peak_[c];
	}
}

void Player::scope(int16_t* out, int n)
{
	int pos = SDL_AtomicGet(&ring_pos_);
	int start = (pos - n + SCOPE_SAMPLES * 2) % (SCOPE_SAMPLES * 2);
	for (int i = 0; i < n; i++) out[i] = ring_[(start + i) % (SCOPE_SAMPLES * 2)];
}
