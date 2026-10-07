#include "dbg.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef OLISE_NATIVE
#include "appdir.h"
#endif

#ifdef OLISE_NATIVE
extern "C" int sceKernelSendNotificationRequest(uint32_t device, void* request, size_t size, int blocking);

/* Layout used by the ps5-native-app-boilerplate: 45 reserved bytes, then text. */
struct NotificationRequest {
	uint8_t reserved[45];
	char message[3075];
};
static NotificationRequest g_notification;
#endif

static void vformat(char* out, size_t n, const char* fmt, va_list ap)
{
	vsnprintf(out, n, fmt, ap);
}

static void write_log(const char* line)
{
	fprintf(stderr, "%s\n", line);
#ifdef OLISE_NATIVE
	/* stderr goes nowhere in a native title, so the same lines also land in a
	   file next to the app. That folder is writable (ShadowMount mounts it
	   from /data/homebrew), and it is the only way to read a diagnosis back
	   over FTP instead of copying it off the screen by hand. */
	static FILE* file = nullptr;
	static bool tried = false;
	if (!tried) {
		tried = true;
		char path[192];
		snprintf(path, sizeof(path), "%s/olise.log", app_folder());
		file = fopen(path, "w");
	}
	if (file) {
		fprintf(file, "%s\n", line);
		fflush(file); /* a crash must not take the last lines with it */
	}
#endif
}

static void show_toast(const char* text)
{
#ifdef OLISE_NATIVE
	memset(&g_notification, 0, sizeof(g_notification));
	snprintf(g_notification.message, sizeof(g_notification.message), "OliSe Player: %s", text);
	(void)sceKernelSendNotificationRequest(0, &g_notification, sizeof(g_notification), 0);
#else
	fprintf(stderr, "[toast] %s\n", text);
#endif
}

void dbg_log(const char* fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vformat(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write_log(buf);
}

void dbg_toast(const char* fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vformat(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	show_toast(buf);
}

void dbg_checkpoint(const char* fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vformat(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write_log(buf);
#ifdef OLISE_TRACE_STARTUP
	/* Startup tracing: stderr goes nowhere in a native title, so every
	   checkpoint becomes a notification. The last one on screen is the step
	   before the crash. Built with `make gl TRACE=1`; never in a release. */
	show_toast(buf);
#endif
}

void dbg_error(const char* fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vformat(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write_log(buf);
	show_toast(buf);
}
