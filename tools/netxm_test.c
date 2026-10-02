/* Offline test for the Mod Archive page parsers in src/netxm.c.
 *
 * Compiles netxm.c with OLISE_NET and fakes the Sony HTTP functions so
 * requests are served from saved HTML files and a local module:
 *
 *   gcc -DOLISE_NET -Isrc tools/netxm_test.c src/netxm.c -o netxm_test
 *   ./netxm_test <dir with random.html search.html chart.html module.bin>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netxm.h"

static const char* g_dir = ".";
static FILE* g_req = NULL;
static char g_last_url[512];

int sceNetInit(void) { return 0; }
int sceNetPoolCreate(const char* n, int a, int b) { (void)n; (void)a; (void)b; return 1; }
int sceSslInit(size_t s) { (void)s; return 2; }
int sceHttp2Init(int a, int b, size_t c, int d) { (void)a; (void)b; (void)c; (void)d; return 3; }
int sceHttp2CreateTemplate(int a, const char* ua, int b, int c) { (void)a; (void)ua; (void)b; (void)c; return 4; }
int sceHttp2DeleteRequest(int r) { (void)r; if (g_req) fclose(g_req); g_req = NULL; return 0; }
int sceHttp2SendRequest(int r, const void* d, size_t n) { (void)r; (void)d; (void)n; return 0; }
int sceHttp2GetStatusCode(int r, int* status) { (void)r; *status = g_req ? 200 : 404; return 0; }
int sceHttp2ReadData(int r, void* buf, size_t n) { (void)r; return g_req ? (int)fread(buf, 1, n, g_req) : -1; }

int sceHttp2CreateRequestWithURL(int tmpl, const char* method, const char* url, uint64_t len)
{
	char path[1024];
	const char* file;
	(void)tmpl; (void)method; (void)len;
	strncpy(g_last_url, url, sizeof(g_last_url) - 1);
	if (strstr(url, "view_random")) file = "random.html";
	else if (strstr(url, "search_type=genre")) file = "search.html";
	else if (strstr(url, "view_chart")) file = "chart.html";
	else if (strstr(url, "downloads.php")) file = "module.bin";
	else file = "missing";
	snprintf(path, sizeof(path), "%s/%s", g_dir, file);
	g_req = fopen(path, "rb");
	printf("  GET %s -> %s%s\n", url, file, g_req ? "" : " (missing)");
	return 5;
}

static int run(const char* label, NetxmRequest req)
{
	NetxmResult res;
	int rc;
	printf("== %s\n", label);
	rc = netxm_fetch(&req, &res);
	if (rc != 0) {
		printf("  FAILED rc=%d\n", rc);
		return 1;
	}
	printf("  ok: id=%ld title='%s' file='%s' fmt=%s genre='%s' artist='%s' bytes=%zu\n",
	       res.module_id, res.title, res.filename, res.format, res.genre, res.artist, res.len);
	free(res.data);
	return 0;
}

int main(int argc, char** argv)
{
	int fails = 0;
	if (argc > 1) g_dir = argv[1];
	srand(1);
	NetxmRequest r1 = { NETXM_RANDOM, 0, NULL };
	NetxmRequest r2 = { NETXM_RANDOM, 0, "XM" };
	NetxmRequest r3 = { NETXM_GENRE, 54, NULL };
	NetxmRequest r4 = { NETXM_GENRE, 54, "XM" };
	NetxmRequest r5 = { NETXM_FEATURED, 0, NULL };
	NetxmRequest r6 = { NETXM_TOPSCORE, 0, NULL };
	fails += run("random any", r1);
	fails += run("random XM", r2);
	fails += run("genre 54 (first visit, learns page count)", r3);
	fails += run("genre 54 again (cached)", r3);
	fails += run("genre 54 + XM", r4);
	fails += run("featured chart", r5);
	fails += run("top rated chart", r6);
	printf("%s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
	return fails ? 1 : 0;
}
