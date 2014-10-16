#include "libcflat.h"

void panic(char *fmt, ...)
{
	va_list va;
	char buf[2000];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	puts(buf);
	exit(-1);
}
