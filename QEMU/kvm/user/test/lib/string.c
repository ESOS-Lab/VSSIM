#include "libcflat.h"

unsigned long strlen(const char *buf)
{
    unsigned long len = 0;

    while (*buf++)
	++len;
    return len;
}

char *strcat(char *dest, const char *src)
{
    char *p = dest;

    while (*p)
	++p;
    while ((*p++ = *src++) != 0)
	;
    return dest;
}
