#include "dbg.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
