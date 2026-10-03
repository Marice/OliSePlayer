/* Diagnostics: stderr for progress, and on-screen notifications (PS5) for
 * errors and the few messages the user should see. */
#ifndef DBG_H
#define DBG_H

/* Print a line to stderr. */
void dbg_log(const char* fmt, ...);
/* Show a system notification toast (PS5) or print to stderr (desktop). */
void dbg_toast(const char* fmt, ...);
/* Startup checkpoint, stderr only. */
void dbg_checkpoint(const char* fmt, ...);
/* Log and toast: something went wrong that the user should see. */
void dbg_error(const char* fmt, ...);

#endif
