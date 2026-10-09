/* The Mod Archive fetcher for OliSe Player.
 *
 * Uses the PS5 system HTTP client (libSceHttp2) the same way the
 * ps5-payload-sdk http2_get sample does. All endpoints work over plain
 * HTTP, so no TLS is involved. Pages are parsed with plain string scanning;
 * only the download link, the pagination links and a few labels are used.
 */
#include "netxm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef OLISE_NET

#define URL_RANDOM   "http://modarchive.org/index.php?request=view_random"
#define URL_GENRE    "http://modarchive.org/index.php?request=search&search_type=genre&query=%d"
#define URL_CHART    "http://modarchive.org/index.php?request=view_chart&query=%s"
#define URL_DOWNLOAD "http://api.modarchive.org/downloads.php?moduleid=%ld"
/* Modland: playlists are plain text, one absolute URL per track. They point at
   a mirror that is often slow or down, so the host is rewritten to the main
   server before downloading. */
#define URL_PLAYLIST "http://ftp.modland.com/pub/playlists/%s.m3u"
#define MODLAND_MIRROR "http://ftp.amigascne.org/mirrors/ftp.modland.com"
#define MODLAND_HOST   "http://ftp.modland.com"
#define PLAYLIST_CAP (512u * 1024u)
#define USER_AGENT   "OliSePlayer/1.0 (PS5 homebrew)"
#define PAGE_CAP     (2u * 1024u * 1024u)
#define MODULE_CAP   (16u * 1024u * 1024u)
#define MAX_ENTRIES  64

static const char* const SUPPORTED[] = { "MOD", "XM", "S3M", "IT" };
#define NUM_SUPPORTED 4

/* Prototypes of the Sony libraries; the SDK ships no headers for them. */
int sceNetInit(void);
int sceNetPoolCreate(const char*, int, int);
int sceSslInit(size_t);
int sceHttp2Init(int, int, size_t, int);
int sceHttp2CreateTemplate(int, const char*, int, int);
int sceHttp2CreateRequestWithURL(int, const char*, const char*, uint64_t);
int sceHttp2DeleteRequest(int);
int sceHttp2SendRequest(int, const void*, size_t);
int sceHttp2GetStatusCode(int, int*);
int sceHttp2ReadData(int, void*, size_t);

static int g_tmpl = -1;

/* ------------------------------------------------------------- network */

static int net_init(void)
{
	int pool, ssl, http;

	if (g_tmpl >= 0) return 0;

	if (sceNetInit() != 0) {
		fprintf(stderr, "netxm: sceNetInit failed\n");
		return -1;
	}
	if ((pool = sceNetPoolCreate("oliseplayer", 32 * 1024, 0)) < 0) {
		fprintf(stderr, "netxm: sceNetPoolCreate failed\n");
		return -1;
	}
	if ((ssl = sceSslInit(256 * 1024)) < 0) {
		fprintf(stderr, "netxm: sceSslInit failed\n");
		return -1;
	}
	if ((http = sceHttp2Init(pool, ssl, 256 * 1024, 1)) < 0) {
		fprintf(stderr, "netxm: sceHttp2Init failed\n");
		return -1;
	}
	if ((g_tmpl = sceHttp2CreateTemplate(http, USER_AGENT, 3, 1)) < 0) {
		fprintf(stderr, "netxm: sceHttp2CreateTemplate failed\n");
		g_tmpl = -1;
		return -1;
	}
	return 0;
}

/* GET url into a malloc'd, NUL-terminated buffer (cap bytes max).
   Returns 0 on HTTP 200, negative otherwise. */
static int http_get(const char* url, uint8_t** out, size_t* out_len, size_t cap)
{
	int req, status = 0, n, rc = -1;
	uint8_t* buf = NULL;
	size_t len = 0, size = 0;

	*out = NULL;
	*out_len = 0;

	if ((req = sceHttp2CreateRequestWithURL(g_tmpl, "GET", url, 0)) < 0) {
		fprintf(stderr, "netxm: create request failed for %s\n", url);
		return -1;
	}
	if (sceHttp2SendRequest(req, NULL, 0) != 0) {
		fprintf(stderr, "netxm: send failed for %s\n", url);
		goto done;
	}
	if (sceHttp2GetStatusCode(req, &status) != 0 || status != 200) {
		fprintf(stderr, "netxm: HTTP %d for %s\n", status, url);
		goto done;
	}
	for (;;) {
		if (len + 0x4000 + 1 > size) {
			size_t nsize = size ? size * 2 : 0x10000;
			uint8_t* nbuf;
			if (nsize > cap + 1) nsize = cap + 1;
			if (len + 0x4000 + 1 > nsize) {
				fprintf(stderr, "netxm: response larger than cap (%zu)\n", cap);
				goto done;
			}
			nbuf = (uint8_t*)realloc(buf, nsize);
			if (!nbuf) goto done;
			buf = nbuf;
			size = nsize;
		}
		n = sceHttp2ReadData(req, buf + len, 0x4000);
		if (n < 0) {
			fprintf(stderr, "netxm: read failed for %s\n", url);
			goto done;
		}
		if (n == 0) break;
		len += (size_t)n;
	}
	buf[len] = 0;
	*out = buf;
	*out_len = len;
	buf = NULL;
	rc = 0;

done:
	free(buf);
	sceHttp2DeleteRequest(req);
	return rc;
}

/* -------------------------------------------------------------- parsing */

/* Decode %XX and '+' in place (file names in URL fragments). */
static void url_decode(char* s)
{
	char* d = s;
	while (*s) {
		if (*s == '%' && s[1] && s[2]) {
			char hex[3] = { s[1], s[2], 0 };
			*d++ = (char)strtol(hex, NULL, 16);
			s += 3;
		} else if (*s == '+') {
			*d++ = ' ';
			s++;
		} else {
			*d++ = *s++;
		}
	}
	*d = 0;
}

/* Decode the handful of HTML entities the site uses in titles. */
static void html_decode(char* s)
{
	static const struct { const char* ent; char ch; } tab[] = {
		{ "&amp;", '&' }, { "&quot;", '"' }, { "&#039;", '\'' }, { "&#39;", '\'' },
		{ "&lt;", '<' }, { "&gt;", '>' }, { "&nbsp;", ' ' },
	};
	char* d = s;
	while (*s) {
		if (*s == '&') {
			size_t i;
			int hit = 0;
			for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
				size_t n = strlen(tab[i].ent);
				if (strncmp(s, tab[i].ent, n) == 0) {
					*d++ = tab[i].ch;
					s += n;
					hit = 1;
					break;
				}
			}
			if (hit) continue;
		}
		*d++ = *s++;
	}
	*d = 0;
}

static void trim(char* s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == '\t')) s[--n] = 0;
	size_t i = 0;
	while (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t') i++;
	if (i) memmove(s, s + i, n - i + 1);
}

/* Copy up to n chars of src that stop at any character in `stop`. */
static void copy_until(char* dst, size_t dst_len, const char* src, const char* stop)
{
	size_t n = strcspn(src, stop);
	if (n >= dst_len) n = dst_len - 1;
	memcpy(dst, src, n);
	dst[n] = 0;
}

/* Upper-case file extension without the dot, "" when none. */
static void extension_of(const char* fname, char* out, size_t out_len)
{
	const char* dot = strrchr(fname, '.');
	size_t i;
	out[0] = 0;
	if (!dot) return;
	dot++;
	for (i = 0; dot[i] && i + 1 < out_len; i++) {
		char c = dot[i];
		out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
	}
	out[i] = 0;
}

static int format_supported(const char* fmt)
{
	int i;
	for (i = 0; i < NUM_SUPPORTED; i++) if (strcmp(fmt, SUPPORTED[i]) == 0) return 1;
	return 0;
}

typedef struct {
	long id;
	char filename[96];
	char title[96];
	char format[8];
} Entry;

/* Collect "downloads.php?moduleid=NNN#file" links with their song title
   (the title="" of the matching view_by_moduleid link that follows). */
static int parse_entries(const char* html, Entry* out, int max, int supported_only)
{
	const char* p = html;
	int n = 0;
	while (n < max && (p = strstr(p, "downloads.php?moduleid=")) != NULL) {
		Entry e;
		char* end;
		p += strlen("downloads.php?moduleid=");
		memset(&e, 0, sizeof(e));
		e.id = strtol(p, &end, 10);
		if (e.id <= 0) { p = end; continue; }
		if (*end == '#') copy_until(e.filename, sizeof(e.filename), end + 1, "\"'<> ");
		url_decode(e.filename);
		extension_of(e.filename, e.format, sizeof(e.format));
		p = end;

		/* Skip duplicates (the same module can appear twice per row). */
		{
			int dup = 0, i;
			for (i = 0; i < n; i++) if (out[i].id == e.id) { dup = 1; break; }
			if (dup) continue;
		}
		if (supported_only && !format_supported(e.format)) continue;

		/* Song title: title="..." on the detail link for this id, if nearby. */
		{
			char needle[64];
			const char* q;
			snprintf(needle, sizeof(needle), "view_by_moduleid&amp;query=%ld\"", e.id);
			q = strstr(p, needle);
			if (q && (size_t)(q - p) < 2000) {
				q = strstr(q, "title=\"");
				if (q) {
					copy_until(e.title, sizeof(e.title), q + 7, "\"");
					html_decode(e.title);
					trim(e.title);
				}
			}
		}
		if (!e.title[0]) {
			strncpy(e.title, e.filename, sizeof(e.title) - 1);
			char* dot = strrchr(e.title, '.');
			if (dot && dot != e.title) *dot = 0;
		}
		out[n++] = e;
	}
	return n;
}

/* Highest N in "page=N#mods" pagination links; 1 when there is none. */
static int parse_max_page(const char* html)
{
	const char* p = html;
	int best = 1;
	while ((p = strstr(p, "page=")) != NULL) {
		char* end;
		long v = strtol(p + 5, &end, 10);
		if (v > best && strncmp(end, "#mods", 5) == 0) best = (int)v;
		p = end;
	}
	return best;
}

/* Text after a label such as "Genre:", skipping tags and entities. */
static void parse_label(const char* html, const char* label, char* out, size_t out_len)
{
	const char* p = strstr(html, label);
	out[0] = 0;
	if (!p) return;
	p += strlen(label);
	for (;;) {
		while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
		if (strncmp(p, "&nbsp;", 6) == 0) { p += 6; continue; }
		if (*p == '<') {
			const char* gt = strchr(p, '>');
			if (!gt) return;
			p = gt + 1;
			continue;
		}
		break;
	}
	copy_until(out, out_len, p, "<\n\r");
	html_decode(out);
	trim(out);
}

/* --------------------------------------------------------------- fetch */

/* Remember how many pages a genre (and format) listing has. */
typedef struct { int genre_id; int fmt; int pages; } PageCache;
static PageCache g_pages[64];
static int g_pages_n = 0;

static int cached_pages(int genre_id, int fmt)
{
	int i;
	for (i = 0; i < g_pages_n; i++)
		if (g_pages[i].genre_id == genre_id && g_pages[i].fmt == fmt) return g_pages[i].pages;
	return 0;
}

static void cache_pages(int genre_id, int fmt, int pages)
{
	if (g_pages_n < (int)(sizeof(g_pages) / sizeof(g_pages[0]))) {
		g_pages[g_pages_n].genre_id = genre_id;
		g_pages[g_pages_n].fmt = fmt;
		g_pages[g_pages_n].pages = pages;
		g_pages_n++;
	}
}

static int format_index(const char* fmt)
{
	int i;
	if (!fmt) return -1;
	for (i = 0; i < NUM_SUPPORTED; i++) if (strcmp(fmt, SUPPORTED[i]) == 0) return i;
	return -1;
}

static int download(long module_id, NetxmResult* out)
{
	char url[160];
	snprintf(url, sizeof(url), URL_DOWNLOAD, module_id);
	fprintf(stderr, "netxm: downloading %s (%s)\n", url, out->title);
	if (http_get(url, &out->data, &out->len, MODULE_CAP) != 0) return -4;
	if (out->len < 64) {
		fprintf(stderr, "netxm: module %ld is too small (%zu bytes)\n", module_id, out->len);
		free(out->data);
		out->data = NULL;
		out->len = 0;
		return -5;
	}
	out->module_id = module_id;
	return 0;
}

static void use_entry(NetxmResult* out, const Entry* e)
{
	strncpy(out->title, e->title, sizeof(out->title) - 1);
	strncpy(out->filename, e->filename, sizeof(out->filename) - 1);
	strncpy(out->format, e->format, sizeof(out->format) - 1);
}

static int fetch_random(const NetxmRequest* req, NetxmResult* out)
{
	uint8_t* page = NULL;
	size_t page_len = 0;
	char url[160];
	Entry e[2];
	const char* fmt = req->format ? req->format : SUPPORTED[rand() % NUM_SUPPORTED];

	snprintf(url, sizeof(url), URL_RANDOM "&format=%s", fmt);
	if (http_get(url, &page, &page_len, PAGE_CAP) != 0) return -2;
	if (parse_entries((const char*)page, e, 1, 0) < 1) {
		fprintf(stderr, "netxm: no module link in random page\n");
		free(page);
		return -3;
	}
	use_entry(out, &e[0]);
	/* The random page is a detail page: it also tells genre and artist. */
	parse_label((const char*)page, "Genre:", out->genre, sizeof(out->genre));
	parse_label((const char*)page, "Artist(s):", out->artist, sizeof(out->artist));
	{
		/* The <title> carries "song title - file.ext (FMT)"; prefer that name. */
		const char* t = strstr((const char*)page, "modules - ");
		if (t) {
			char tmp[160];
			copy_until(tmp, sizeof(tmp), t + 10, "<");
			char* sep = strstr(tmp, " - ");
			if (sep) { *sep = 0; html_decode(tmp); trim(tmp); if (tmp[0]) strncpy(out->title, tmp, sizeof(out->title) - 1); }
		}
	}
	free(page);
	return download(e[0].id, out);
}

static int fetch_listing(const char* url_base, int genre_id, int fmt_idx, NetxmResult* out)
{
	uint8_t* page = NULL;
	size_t page_len = 0;
	char url[220];
	Entry e[MAX_ENTRIES];
	int n, pages, pick, rc;

	pages = genre_id ? cached_pages(genre_id, fmt_idx) : 1;
	if (pages <= 0) {
		/* First visit: page 1 tells how many pages there are. */
		snprintf(url, sizeof(url), "%s&page=1", url_base);
		if (http_get(url, &page, &page_len, PAGE_CAP) != 0) return -2;
		pages = parse_max_page((const char*)page);
		cache_pages(genre_id, fmt_idx, pages);
		pick = 1 + rand() % pages;
		if (pick != 1) {
			free(page);
			page = NULL;
		}
	} else {
		pick = 1 + rand() % pages;
	}
	if (!page) {
		snprintf(url, sizeof(url), "%s&page=%d", url_base, pick);
		if (http_get(url, &page, &page_len, PAGE_CAP) != 0) return -2;
	}
	n = parse_entries((const char*)page, e, MAX_ENTRIES, 1);
	free(page);
	if (n <= 0) {
		fprintf(stderr, "netxm: no supported modules on %s (page %d of %d)\n", url_base, pick, pages);
		return -3;
	}
	pick = rand() % n;
	use_entry(out, &e[pick]);
	rc = download(e[pick].id, out);
	return rc;
}

/* Picks a random supported module out of a Modland playlist and downloads it.
   A playlist is at most a few hundred kilobytes, so it is read whole and the
   usable lines are counted; reservoir sampling then picks one without keeping
   the line numbers around. */
static int fetch_playlist(const NetxmRequest* req, NetxmResult* out)
{
	char url[256];
	uint8_t* text = NULL;
	size_t len = 0;

	snprintf(url, sizeof(url), URL_PLAYLIST, req->playlist ? req->playlist : "favourites_by_coma");
	if (http_get(url, &text, &len, PLAYLIST_CAP) != 0 || !text || len == 0) {
		free(text);
		fprintf(stderr, "netxm: playlist %s unavailable\n", url);
		return -2;
	}

	const int want = format_index(req->format);   /* -1 = any supported */
	char chosen[512] = {0};
	int seen = 0;

	char* save = NULL;
	for (char* line = strtok_r((char*)text, "\r\n", &save); line;
	     line = strtok_r(NULL, "\r\n", &save)) {
		if (line[0] == '#' || line[0] == 0) continue;      /* M3U comment */
		if (strncmp(line, "http://", 7) != 0) continue;

		char ext[8];
		extension_of(line, ext, sizeof(ext));
		if (!format_supported(ext)) continue;
		if (want >= 0 && strcmp(ext, SUPPORTED[want]) != 0) continue;

		/* Reservoir sampling: every line gets an equal chance in one pass. */
		seen++;
		if (rand() % seen == 0) snprintf(chosen, sizeof(chosen), "%s", line);
	}
	free(text);

	if (!chosen[0]) {
		fprintf(stderr, "netxm: playlist held no supported module\n");
		return -3;
	}

	/* The listed mirror is usually unreachable; the main server is not. */
	char direct[512];
	const size_t mirror_len = strlen(MODLAND_MIRROR);
	if (strncmp(chosen, MODLAND_MIRROR, mirror_len) == 0)
		snprintf(direct, sizeof(direct), "%s%s", MODLAND_HOST, chosen + mirror_len);
	else
		snprintf(direct, sizeof(direct), "%s", chosen);

	if (http_get(direct, &out->data, &out->len, MODULE_CAP) != 0 || !out->data || out->len == 0) {
		free(out->data);
		out->data = NULL;
		fprintf(stderr, "netxm: module download failed\n");
		return -4;
	}

	/* Name and format come from the path; Modland has no metadata to read.
	   The folder above the file is the artist, which is worth showing. */
	const char* slash = strrchr(direct, '/');
	snprintf(out->filename, sizeof(out->filename), "%s", slash ? slash + 1 : direct);
	url_decode(out->filename);
	snprintf(out->title, sizeof(out->title), "%s", out->filename);
	char* dot = strrchr(out->title, '.');
	if (dot) *dot = 0;
	extension_of(out->filename, out->format, sizeof(out->format));

	if (slash) {
		const char* start = direct;
		for (const char* p = direct; p < slash; p++)
			if (*p == '/') start = p + 1;
		if (start < slash) {
			size_t n = (size_t)(slash - start);
			if (n >= sizeof(out->artist)) n = sizeof(out->artist) - 1;
			memcpy(out->artist, start, n);
			out->artist[n] = 0;
			url_decode(out->artist);
		}
	}
	snprintf(out->genre, sizeof(out->genre), "%s", "Modland");
	return 0;
}

int netxm_fetch(const NetxmRequest* req, NetxmResult* out)
{
	char url_base[200];
	int fmt_idx;

	memset(out, 0, sizeof(*out));
	if (net_init() != 0) return -1;

	switch (req->kind) {
	case NETXM_RANDOM:
		return fetch_random(req, out);
	case NETXM_GENRE:
		fmt_idx = format_index(req->format);
		if (fmt_idx >= 0) snprintf(url_base, sizeof(url_base), URL_GENRE "&format=%s", req->genre_id, SUPPORTED[fmt_idx]);
		else snprintf(url_base, sizeof(url_base), URL_GENRE, req->genre_id);
		return fetch_listing(url_base, req->genre_id, fmt_idx, out);
	case NETXM_FEATURED:
		snprintf(url_base, sizeof(url_base), URL_CHART, "featured");
		return fetch_listing(url_base, 0, -1, out);
	case NETXM_TOPSCORE:
		snprintf(url_base, sizeof(url_base), URL_CHART, "topscore");
		return fetch_listing(url_base, 0, -1, out);
	case NETXM_PLAYLIST:
		return fetch_playlist(req, out);
	default:
		return -6;
	}
}

#else /* desktop build: no Sony network libraries */

int netxm_fetch(const NetxmRequest* req, NetxmResult* out)
{
	(void)req;
	memset(out, 0, sizeof(*out));
	fprintf(stderr, "netxm: network download not available in this build\n");
	return -1;
}

#endif
