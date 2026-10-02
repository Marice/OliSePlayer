#include "radio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netxm.h"
#include "player.h"

static const size_t HISTORY_BYTES_CAP = 40u * 1024u * 1024u;
static const int HISTORY_CAP = 20;

Radio::~Radio()
{
	if (thread_) SDL_WaitThread(thread_, nullptr);
	free(pending_.data);
	for (RadioTrack& t : hist_) free(t.data);
}

bool Radio::available()
{
#ifdef OLISE_NET
	return true;
#else
	return false;
#endif
}

RadioState Radio::state() const
{
	return (RadioState)SDL_AtomicGet(const_cast<SDL_atomic_t*>(&state_));
}

int Radio::worker(void* arg)
{
	Radio* self = (Radio*)arg;
	RadioTrack t;
	int rc = netxm_fetch_random(&t.data, &t.len, t.title, sizeof(t.title), &t.module_id);
	if (rc != 0) {
		snprintf(self->error_, sizeof(self->error_), "DOWNLOAD FAILED (%d)", rc);
		SDL_AtomicSet(&self->state_, (int)RadioState::Failed);
		return 0;
	}
	char name[XMP_NAME_SIZE], type[XMP_NAME_SIZE];
	if (!Player::test_memory(t.data, t.len, name, type)) {
		snprintf(self->error_, sizeof(self->error_), "UNSUPPORTED MODULE #%ld", t.module_id);
		free(t.data);
		SDL_AtomicSet(&self->state_, (int)RadioState::Failed);
		return 0;
	}
	/* Prefer the title stored inside the module; fall back to the file name. */
	for (char* p = name; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
	size_t n = strlen(name);
	while (n > 0 && name[n - 1] == ' ') name[--n] = 0;
	if (n > 0) strncpy(t.title, name, sizeof(t.title) - 1);
	strncpy(t.type, type, sizeof(t.type) - 1);
	self->pending_ = t;
	SDL_AtomicSet(&self->state_, (int)RadioState::Ready);
	return 0;
}

bool Radio::fetch()
{
	if (!available()) {
		snprintf(error_, sizeof(error_), "NO NETWORK SUPPORT IN THIS BUILD");
		SDL_AtomicSet(&state_, (int)RadioState::Failed);
		return false;
	}
	if (state() == RadioState::Loading) return false;
	if (thread_) {
		SDL_WaitThread(thread_, nullptr);
		thread_ = nullptr;
	}
	free(pending_.data);
	pending_ = RadioTrack();
	SDL_AtomicSet(&state_, (int)RadioState::Loading);
	thread_ = SDL_CreateThread(worker, "radio", this);
	if (!thread_) {
		snprintf(error_, sizeof(error_), "THREAD FAILED");
		SDL_AtomicSet(&state_, (int)RadioState::Failed);
		return false;
	}
	return true;
}

bool Radio::take(RadioTrack& t)
{
	if (state() != RadioState::Ready) return false;
	if (thread_) {
		SDL_WaitThread(thread_, nullptr);
		thread_ = nullptr;
	}
	t = pending_;
	pending_ = RadioTrack();
	SDL_AtomicSet(&state_, (int)RadioState::Idle);
	return true;
}

void Radio::ack_failed()
{
	if (state() == RadioState::Failed) SDL_AtomicSet(&state_, (int)RadioState::Idle);
}

void Radio::remember(RadioTrack& t)
{
	/* Drop anything after the current index (new branch after "previous"). */
	while ((int)hist_.size() > idx_ + 1) {
		hist_bytes_ -= hist_.back().len;
		free(hist_.back().data);
		hist_.pop_back();
	}
	hist_.push_back(t);
	hist_bytes_ += t.len;
	t = RadioTrack();
	while ((hist_.size() > (size_t)HISTORY_CAP || hist_bytes_ > HISTORY_BYTES_CAP) && hist_.size() > 1) {
		hist_bytes_ -= hist_.front().len;
		free(hist_.front().data);
		hist_.erase(hist_.begin());
	}
	idx_ = (int)hist_.size() - 1;
}

const RadioTrack* Radio::prev()
{
	if (!has_prev()) return nullptr;
	idx_--;
	return &hist_[idx_];
}

const RadioTrack* Radio::next()
{
	if (!has_next()) return nullptr;
	idx_++;
	return &hist_[idx_];
}
