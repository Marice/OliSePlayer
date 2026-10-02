#ifndef NETXM_H
#define NETXM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Download a random tracker module (MOD, XM, S3M or IT) from The Mod Archive.
 *
 * Blocking; call it from a worker thread. On success returns 0, stores a
 * malloc'd copy of the module in *data / *len (caller frees) and writes a
 * human readable title into title and the Mod Archive id into *module_id. Returns a negative value on any error
 * (no network, bad page, empty file); *data is then NULL. The caller
 * validates the bytes with libxmp before playing them.
 *
 * The desktop build has no Sony network libraries and compiles a stub that
 * always fails. */
int netxm_fetch_random(uint8_t** data, size_t* len, char* title, size_t title_len, long* module_id);

#ifdef __cplusplus
}
#endif

#endif /* NETXM_H */
