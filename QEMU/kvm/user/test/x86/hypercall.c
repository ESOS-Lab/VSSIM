#include "libcflat.h"

#define KVM_HYPERCALL_INTEL ".byte 0x0f,0x01,0xc1"
#define KVM_HYPERCALL_AMD ".byte 0x0f,0x01,0xd9"

static inline long kvm_hypercall0_intel(unsigned int nr)
{
	long ret;
	asm volatile(KVM_HYPERCALL_INTEL
		     : "=a"(ret)
		     : "a"(nr));
	return ret;
}

static inline long kvm_hypercall0_amd(unsigned int nr)
{
	long ret;
	asm volatile(KVM_HYPERCALL_AMD
		     : "=a"(ret)
		     : "a"(nr));
	return ret;
}

int main(int ac, char **av)
{
	kvm_hypercall0_intel(-1u);
	printf("Hypercall via VMCALL: OK\n");
	kvm_hypercall0_amd(-1u);
	printf("Hypercall via VMMCALL: OK\n");
	return 0;
}
