/* Small libc pieces the native title build needs but the platform does not
 * export: libiconv (pulled in by SDL2) asks for the locale charset and the
 * multibyte width. The app only ever uses UTF-8.
 *
 * The OpenGL build links the payload SDK's full libc, which defines both of
 * these, so this file steps aside there to avoid duplicate symbols. */
#if defined(OLISE_NATIVE) && !defined(OLISE_GL)

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
