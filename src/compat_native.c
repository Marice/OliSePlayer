/* Small libc pieces the native title build needs but the platform does not
 * export: libiconv (pulled in by SDL2) asks for the locale charset and the
 * multibyte width. The app only ever uses UTF-8. */
#ifdef OLISE_NATIVE

#include <stddef.h>

int ___mb_cur_max(void)
{
	return 4;
}

char* nl_langinfo(int item)
{
	(void)item;
	return (char*)"UTF-8";
}

#endif
