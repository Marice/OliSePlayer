/* Random tracker module downloader for OliSe Player.
 *
 * Uses the PS5 system HTTP client (libSceHttp2) the same way the
 * ps5-payload-sdk http2_get sample does. Both Mod Archive endpoints work
 * over plain HTTP, so no TLS is involved:
 *   1. http://modarchive.org/index.php?request=view_random&format=<FMT>
 *      -> HTML page containing downloads.php?moduleid=NNN#name.ext
 *   2. http://api.modarchive.org/downloads.php?moduleid=NNN
 *      -> the module bytes
 * The format is picked at random from what libxmp-lite plays (MOD, XM,
 * S3M, IT). The caller validates the bytes with xmp_test_module_from_memory.
 */
#include "netxm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef OLISE_NET

#define RANDOM_URL   "http://modarchive.org/index.php?request=view_random&format=%s"
#define DOWNLOAD_URL "http://api.modarchive.org/downloads.php?moduleid=%ld"
#define USER_AGENT   "OliSePlayer/1.0 (PS5 homebrew)"
#define PAGE_CAP     (2u * 1024u * 1024u)
#define MODULE_CAP   (16u * 1024u * 1024u)

static const char* const formats[] = { "MOD", "XM", "S3M", "IT" };

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

/* One-time library setup, kept for the lifetime of the process. */
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

/* Decode %XX and '+' in place; the fragment is a file name. */
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

/* Find "downloads.php?moduleid=NNN#name.xm" in the random page. */
static int parse_random_page(const char* html, long* module_id, char* title, size_t title_len)
{
	const char* p = strstr(html, "downloads.php?moduleid=");
	const char* frag;
	size_t n;

	if (!p) return -1;
	p += strlen("downloads.php?moduleid=");
	*module_id = strtol(p, (char**)&frag, 10);
	if (*module_id <= 0) return -1;

	title[0] = 0;
	if (*frag == '#') {
		frag++;
		n = strcspn(frag, "\"'<> ");
		if (n >= title_len) n = title_len - 1;
		memcpy(title, frag, n);
		title[n] = 0;
		url_decode(title);
		/* Drop the extension for display; libxmp reports the format. */
		n = strlen(title);
		while (n > 0 && title[n - 1] != '.') n--;
		if (n > 1) title[n - 1] = 0;
	}
	if (!title[0]) snprintf(title, title_len, "MODULE %ld", *module_id);
	return 0;
}

int netxm_fetch_random(uint8_t** data, size_t* len, char* title, size_t title_len, long* module_id_out)
{
	uint8_t* page = NULL;
	size_t page_len = 0;
	long module_id = 0;
	char url[160];
	const char* fmt = formats[rand() % (sizeof(formats) / sizeof(formats[0]))];

	*data = NULL;
	*len = 0;

	if (net_init() != 0) return -1;

	snprintf(url, sizeof(url), RANDOM_URL, fmt);
	if (http_get(url, &page, &page_len, PAGE_CAP) != 0) return -2;
	if (parse_random_page((const char*)page, &module_id, title, title_len) != 0) {
		fprintf(stderr, "netxm: no module link in random page\n");
		free(page);
		return -3;
	}
	free(page);
	if (module_id_out) *module_id_out = module_id;

	snprintf(url, sizeof(url), DOWNLOAD_URL, module_id);
	fprintf(stderr, "netxm: downloading %s (%s)\n", url, title);
	if (http_get(url, data, len, MODULE_CAP) != 0) return -4;
	if (*len < 64) {
		fprintf(stderr, "netxm: module %ld is too small (%zu bytes)\n", module_id, *len);
		free(*data);
		*data = NULL;
		*len = 0;
		return -5;
	}
	return 0;
}

#else /* desktop build: no Sony network libraries */

int netxm_fetch_random(uint8_t** data, size_t* len, char* title, size_t title_len, long* module_id)
{
	if (module_id) *module_id = 0;
	*data = NULL;
	*len = 0;
	if (title && title_len) title[0] = 0;
	fprintf(stderr, "netxm: network download not available in this build\n");
	return -1;
}

#endif
