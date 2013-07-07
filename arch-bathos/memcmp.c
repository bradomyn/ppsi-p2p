
#include <ppsi/lib.h>

/* We currently miss memcmp in bathos, so provide it here */

/*
 * Code from uClibc-0.9.32. LGPL V2.1 (see ../lib/libc-functions.c
 */

/* libc/string/memcmp.c */
int memcmp(const void *s1, const void *s2, size_t n)
{
	register const unsigned char *r1 = s1;
	register const unsigned char *r2 = s2;
	int r = 0;

	while (n-- && ((r = ((int)(*r1++)) - *r2++) == 0))
		;
	return r;
}
