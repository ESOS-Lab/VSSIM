#include "libcflat.h"

typedef unsigned long long u64;

u64 rdtsc(void)
{
	unsigned a, d;

	asm volatile("rdtsc" : "=a"(a), "=d"(d));
	return a | (u64)d << 32;
}

void wrtsc(u64 tsc)
{
	unsigned a = tsc, d = tsc >> 32;

	asm volatile("wrmsr" : : "a"(a), "d"(d), "c"(0x10));
}

void test_wrtsc(u64 t1)
{
	u64 t2;

	wrtsc(t1);
	t2 = rdtsc();
	printf("rdtsc after wrtsc(%lld): %lld\n", t1, t2);
}

int main()
{
	u64 t1, t2;

	t1 = rdtsc();
	t2 = rdtsc();
	printf("rdtsc latency %lld\n", (unsigned)(t2 - t1));

	test_wrtsc(0);
	test_wrtsc(100000000000ull);
	return 0;
}
