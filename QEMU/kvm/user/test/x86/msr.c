/* msr tests */

#include "libcflat.h"

#define MSR_KERNEL_GS_BASE	0xc0000102 /* SwapGS GS shadow */

int nr_passed, nr_tests;

#ifdef __x86_64__
static void report(const char *name, int passed)
{
	++nr_tests;
	if (passed)
		++nr_passed;
	printf("%s: %s\n", name, passed ? "PASS" : "FAIL");
}

static void wrmsr(unsigned index, unsigned long long value)
{
	asm volatile ("wrmsr" : : "c"(index), "A"(value));
}

static unsigned long long rdmsr(unsigned index)
{
	unsigned long long value;

	asm volatile ("rdmsr" : "=A"(value) : "c"(index));

	return value;
}
#endif

static void test_kernel_gs_base(void)
{
#ifdef __x86_64__
	unsigned long long v1 = 0x123456789abcdef, v2;

	wrmsr(MSR_KERNEL_GS_BASE, v1);
	v2 = rdmsr(MSR_KERNEL_GS_BASE);
	report("MSR_KERNEL_GS_BASE", v1 == v2);
#endif
}

int main(int ac, char **av)
{
	test_kernel_gs_base();

	printf("%d tests, %d failures\n", nr_tests, nr_tests - nr_passed);

	return nr_passed == nr_tests ? 0 : 1;
}

