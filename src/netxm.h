#ifndef NETXM_H
#define NETXM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What to fetch from The Mod Archive. The site has no public JSON API
   without a key, so this uses the regular web pages over plain HTTP:
   - random pick: index.php?request=view_random[&format=XM]
   - genre:       index.php?request=search&search_type=genre&query=<id>[&format=..]&page=N
   - charts:      index.php?request=view_chart&query=featured|topscore
   - download:    api.modarchive.org/downloads.php?moduleid=<id>            */
enum NetxmKind {
	NETXM_RANDOM = 0,
	NETXM_GENRE = 1,
	NETXM_FEATURED = 2,
	NETXM_TOPSCORE = 3
};

typedef struct {
	int kind;             /* NetxmKind */
	int genre_id;         /* NETXM_GENRE */
	const char* format;   /* "MOD", "XM", "S3M", "IT" or NULL for any supported */
} NetxmRequest;

typedef struct {
	uint8_t* data;        /* malloc'd module bytes, caller frees */
	size_t len;
	long module_id;
	char title[96];       /* song title from the site, or the file name */
	char filename[96];
	char format[8];       /* from the file extension, upper case */
	char genre[48];       /* when the page told us, else empty */
	char artist[48];      /* when the page told us, else empty */
} NetxmResult;

/* Blocking; call from a worker thread. Returns 0 on success, negative on
   any error (no network, page changed, nothing supported on the page).
   The desktop build compiles a stub that always fails. */
int netxm_fetch(const NetxmRequest* req, NetxmResult* out);

#ifdef __cplusplus
}
#endif

#endif /* NETXM_H */
