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

void Radio::set_station(int index)
{
	if (index < 0) index = 0;
	if (index >= station_count()) index = station_count() - 1;
	station_ = index;
}

RadioState Radio::state() const
{
	return (RadioState)SDL_AtomicGet(const_cast<SDL_atomic_t*>(&state_));
}

static void clean_name(char* s)
{
	for (char* p = s; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
	size_t n = strlen(s);
	while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
}

int Radio::worker(void* arg)
{
	Radio* self = (Radio*)arg;
	Station st = station_at(self->job_station_);
	NetxmRequest req;
	NetxmResult res;
	req.kind = (int)st.kind;
	req.genre_id = st.genre_id;
	req.format = st.format;

	int rc = netxm_fetch(&req, &res);
	if (rc != 0) {
		snprintf(self->error_, sizeof(self->error_), "DOWNLOAD FAILED (%d)", rc);
		SDL_AtomicSet(&self->state_, (int)RadioState::Failed);
		return 0;
	}
	char name[XMP_NAME_SIZE], type[XMP_NAME_SIZE];
	if (!Player::test_memory(res.data, res.len, name, type)) {
		snprintf(self->error_, sizeof(self->error_), "UNSUPPORTED MODULE #%ld", res.module_id);
		free(res.data);
		SDL_AtomicSet(&self->state_, (int)RadioState::Failed);
		return 0;
	}
	RadioTrack t;
	t.data = res.data;
	t.len = res.len;
	t.module_id = res.module_id;
	t.station = self->job_station_;
	/* Prefer the title stored inside the module, then the site's title. */
	clean_name(name);
	strncpy(t.title, name[0] ? name : res.title, sizeof(t.title) - 1);
	strncpy(t.type, type, sizeof(t.type) - 1);
	strncpy(t.format, res.format, sizeof(t.format) - 1);
	strncpy(t.genre, res.genre, sizeof(t.genre) - 1);
	strncpy(t.artist, res.artist, sizeof(t.artist) - 1);
	if (t.genre[0] == 0 && st.kind == STATION_GENRE) strncpy(t.genre, st.name, sizeof(t.genre) - 1);
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
	job_station_ = station_;
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
