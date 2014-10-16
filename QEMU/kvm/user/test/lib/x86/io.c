#include "libcflat.h"
#include "smp.h"

static struct spinlock lock;

static void print_serial(const char *buf)
{
	unsigned long len = strlen(buf);

	asm volatile ("rep/outsb" : "+S"(buf), "+c"(len) : "d"(0xf1));
}

void puts(const char *s)
{
	spin_lock(&lock);
	print_serial(s);
	spin_unlock(&lock);
}

void exit(int code)
{
        asm volatile("out %0, %1" : : "a"(code), "d"((short)0xf4));
}
