#include "libkvm.h"
#include "kvm-x86.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int kvm_set_tss_addr(kvm_context_t kvm, unsigned long addr)
{
#ifdef KVM_CAP_SET_TSS_ADDR
	int r;

	r = ioctl(kvm->fd, KVM_CHECK_EXTENSION, KVM_CAP_SET_TSS_ADDR);
	if (r > 0) {
		r = ioctl(kvm->vm_fd, KVM_SET_TSS_ADDR, addr);
		if (r == -1) {
			fprintf(stderr, "kvm_set_tss_addr: %m\n");
			return -errno;
		}
		return 0;
	}
#endif
	return -ENOSYS;
}

static int kvm_init_tss(kvm_context_t kvm)
{
#ifdef KVM_CAP_SET_TSS_ADDR
	int r;

	r = ioctl(kvm->fd, KVM_CHECK_EXTENSION, KVM_CAP_SET_TSS_ADDR);
	if (r > 0) {
		/*
		 * this address is 3 pages before the bios, and the bios should present
		 * as unavaible memory
		 */
		r = kvm_set_tss_addr(kvm, 0xfffbd000);
		if (r < 0) {
			fprintf(stderr, "kvm_init_tss: unable to set tss addr\n");
			return r;
		}

	}
#endif
	return 0;
}

static int kvm_create_pit(kvm_context_t kvm)
{
#ifdef KVM_CAP_PIT
	int r;

	kvm->pit_in_kernel = 0;
	if (!kvm->no_pit_creation) {
#ifdef KVM_CAP_PIT2
		struct kvm_pit_config config = { .flags = 0 };

		r = ioctl(kvm->fd, KVM_CHECK_EXTENSION, KVM_CAP_PIT2);
		if (r > 0)
			r = ioctl(kvm->vm_fd, KVM_CREATE_PIT2, &config);
		else
#endif
		{
			r = ioctl(kvm->fd, KVM_CHECK_EXTENSION, KVM_CAP_PIT);
			if (r <= 0)
				return 0;

			r = ioctl(kvm->vm_fd, KVM_CREATE_PIT);
		}
		if (r < 0) {
			fprintf(stderr, "Create kernel PIC irqchip failed\n");
			return r;
		}
		kvm->pit_in_kernel = 1;
	}
#endif
	return 0;
}

int kvm_arch_create(kvm_context_t kvm, unsigned long phys_mem_bytes,
 			void **vm_mem)
{
	int r = 0;

	r = kvm_init_tss(kvm);
	if (r < 0)
		return r;

	r = kvm_create_pit(kvm);
	if (r < 0)
		return r;

	r = kvm_init_coalesced_mmio(kvm);
	if (r < 0)
		return r;

	return 0;
}

#ifdef KVM_EXIT_TPR_ACCESS

static int handle_tpr_access(kvm_context_t kvm, struct kvm_run *run, int vcpu)
{
	return kvm->callbacks->tpr_access(kvm->opaque, vcpu,
					  run->tpr_access.rip,
					  run->tpr_access.is_write);
}


int kvm_enable_vapic(kvm_context_t kvm, int vcpu, uint64_t vapic)
{
	int r;
	struct kvm_vapic_addr va = {
		.vapic_addr = vapic,
	};

	r = ioctl(kvm->vcpu_fd[vcpu], KVM_SET_VAPIC_ADDR, &va);
	if (r == -1) {
		r = -errno;
		perror("kvm_enable_vapic");
		return r;
	}
	return 0;
}

#endif

int kvm_arch_run(struct kvm_run *run,kvm_context_t kvm, int vcpu)
{
	int r = 0;

	switch (run->exit_reason) {
#ifdef KVM_EXIT_SET_TPR
		case KVM_EXIT_SET_TPR:
			break;
#endif
#ifdef KVM_EXIT_TPR_ACCESS
		case KVM_EXIT_TPR_ACCESS:
			r = handle_tpr_access(kvm, run, vcpu);
			break;
#endif
		default:
			r = 1;
			break;
	}

	return r;
}

#define MAX_ALIAS_SLOTS 4
static struct {
	uint64_t start;
	uint64_t len;
} kvm_aliases[MAX_ALIAS_SLOTS];

static int get_alias_slot(uint64_t start)
{
	int i;

	for (i=0; i<MAX_ALIAS_SLOTS; i++)
		if (kvm_aliases[i].start == start)
			return i;
	return -1;
}
static int get_free_alias_slot(void)
{
        int i;

        for (i=0; i<MAX_ALIAS_SLOTS; i++)
                if (kvm_aliases[i].len == 0)
                        return i;
        return -1;
}

static void register_alias(int slot, uint64_t start, uint64_t len)
{
	kvm_aliases[slot].start = start;
	kvm_aliases[slot].len   = len;
}

int kvm_create_memory_alias(kvm_context_t kvm,
			    uint64_t phys_start,
			    uint64_t len,
			    uint64_t target_phys)
{
	struct kvm_memory_alias alias = {
		.flags = 0,
		.guest_phys_addr = phys_start,
		.memory_size = len,
		.target_phys_addr = target_phys,
	};
	int fd = kvm->vm_fd;
	int r;
	int slot;

	slot = get_alias_slot(phys_start);
	if (slot < 0)
		slot = get_free_alias_slot();
	if (slot < 0)
		return -EBUSY;
	alias.slot = slot;

	r = ioctl(fd, KVM_SET_MEMORY_ALIAS, &alias);
	if (r == -1)
	    return -errno;

	register_alias(slot, phys_start, len);
	return 0;
}

int kvm_destroy_memory_alias(kvm_context_t kvm, uint64_t phys_start)
{
	return kvm_create_memory_alias(kvm, phys_start, 0, 0);
}

#ifdef KVM_CAP_IRQCHIP

int kvm_get_lapic(kvm_context_t kvm, int vcpu, struct kvm_lapic_state *s)
{
	int r;
	if (!kvm->irqchip_in_kernel)
		return 0;
	r = ioctl(kvm->vcpu_fd[vcpu], KVM_GET_LAPIC, s);
	if (r == -1) {
		r = -errno;
		perror("kvm_get_lapic");
	}
	return r;
}

int kvm_set_lapic(kvm_context_t kvm, int vcpu, struct kvm_lapic_state *s)
{
	int r;
	if (!kvm->irqchip_in_kernel)
		return 0;
	r = ioctl(kvm->vcpu_fd[vcpu], KVM_SET_LAPIC, s);
	if (r == -1) {
		r = -errno;
		perror("kvm_set_lapic");
	}
	return r;
}

#endif

#ifdef KVM_CAP_PIT

int kvm_get_pit(kvm_context_t kvm, struct kvm_pit_state *s)
{
	int r;
	if (!kvm->pit_in_kernel)
		return 0;
	r = ioctl(kvm->vm_fd, KVM_GET_PIT, s);
	if (r == -1) {
		r = -errno;
		perror("kvm_get_pit");
	}
	return r;
}

int kvm_set_pit(kvm_context_t kvm, struct kvm_pit_state *s)
{
	int r;
	if (!kvm->pit_in_kernel)
		return 0;
	r = ioctl(kvm->vm_fd, KVM_SET_PIT, s);
	if (r == -1) {
		r = -errno;
		perror("kvm_set_pit");
	}
	return r;
}

#endif

void kvm_show_code(kvm_context_t kvm, int vcpu)
{
#define SHOW_CODE_LEN 50
	int fd = kvm->vcpu_fd[vcpu];
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r, n;
	int back_offset;
	unsigned char code;
	char code_str[SHOW_CODE_LEN * 3 + 1];
	unsigned long rip;

	r = ioctl(fd, KVM_GET_SREGS, &sregs);
	if (r == -1) {
		perror("KVM_GET_SREGS");
		return;
	}
	r = ioctl(fd, KVM_GET_REGS, &regs);
	if (r == -1) {
		perror("KVM_GET_REGS");
		return;
	}
	rip = sregs.cs.base + regs.rip;
	back_offset = regs.rip;
	if (back_offset > 20)
	    back_offset = 20;
	*code_str = 0;
	for (n = -back_offset; n < SHOW_CODE_LEN-back_offset; ++n) {
		if (n == 0)
			strcat(code_str, " -->");
		r = kvm->callbacks->mmio_read(kvm->opaque, rip + n, &code, 1);
		if (r < 0) {
			strcat(code_str, " xx");
			continue;
		}
		sprintf(code_str + strlen(code_str), " %02x", code);
	}
	fprintf(stderr, "code:%s\n", code_str);
}


/*
 * Returns available msr list.  User must free.
 */
struct kvm_msr_list *kvm_get_msr_list(kvm_context_t kvm)
{
	struct kvm_msr_list sizer, *msrs;
	int r, e;

	sizer.nmsrs = 0;
	r = ioctl(kvm->fd, KVM_GET_MSR_INDEX_LIST, &sizer);
	if (r == -1 && errno != E2BIG)
		return NULL;
	msrs = malloc(sizeof *msrs + sizer.nmsrs * sizeof *msrs->indices);
	if (!msrs) {
		errno = ENOMEM;
		return NULL;
	}
	msrs->nmsrs = sizer.nmsrs;
	r = ioctl(kvm->fd, KVM_GET_MSR_INDEX_LIST, msrs);
	if (r == -1) {
		e = errno;
		free(msrs);
		errno = e;
		return NULL;
	}
	return msrs;
}

int kvm_get_msrs(kvm_context_t kvm, int vcpu, struct kvm_msr_entry *msrs,
		 int n)
{
    struct kvm_msrs *kmsrs = malloc(sizeof *kmsrs + n * sizeof *msrs);
    int r, e;

    if (!kmsrs) {
	errno = ENOMEM;
	return -1;
    }
    kmsrs->nmsrs = n;
    memcpy(kmsrs->entries, msrs, n * sizeof *msrs);
    r = ioctl(kvm->vcpu_fd[vcpu], KVM_GET_MSRS, kmsrs);
    e = errno;
    memcpy(msrs, kmsrs->entries, n * sizeof *msrs);
    free(kmsrs);
    errno = e;
    return r;
}

int kvm_set_msrs(kvm_context_t kvm, int vcpu, struct kvm_msr_entry *msrs,
		 int n)
{
    struct kvm_msrs *kmsrs = malloc(sizeof *kmsrs + n * sizeof *msrs);
    int r, e;

    if (!kmsrs) {
	errno = ENOMEM;
	return -1;
    }
    kmsrs->nmsrs = n;
    memcpy(kmsrs->entries, msrs, n * sizeof *msrs);
    r = ioctl(kvm->vcpu_fd[vcpu], KVM_SET_MSRS, kmsrs);
    e = errno;
    free(kmsrs);
    errno = e;
    return r;
}

static void print_seg(FILE *file, const char *name, struct kvm_segment *seg)
{
    	fprintf(stderr,
		"%s %04x (%08llx/%08x p %d dpl %d db %d s %d type %x l %d"
		" g %d avl %d)\n",
		name, seg->selector, seg->base, seg->limit, seg->present,
		seg->dpl, seg->db, seg->s, seg->type, seg->l, seg->g,
		seg->avl);
}

static void print_dt(FILE *file, const char *name, struct kvm_dtable *dt)
{
    	fprintf(stderr, "%s %llx/%x\n", name, dt->base, dt->limit);
}

void kvm_show_regs(kvm_context_t kvm, int vcpu)
{
	int fd = kvm->vcpu_fd[vcpu];
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r;

	r = ioctl(fd, KVM_GET_REGS, &regs);
	if (r == -1) {
		perror("KVM_GET_REGS");
		return;
	}
	fprintf(stderr,
		"rax %016llx rbx %016llx rcx %016llx rdx %016llx\n"
		"rsi %016llx rdi %016llx rsp %016llx rbp %016llx\n"
		"r8  %016llx r9  %016llx r10 %016llx r11 %016llx\n"
		"r12 %016llx r13 %016llx r14 %016llx r15 %016llx\n"
		"rip %016llx rflags %08llx\n",
		regs.rax, regs.rbx, regs.rcx, regs.rdx,
		regs.rsi, regs.rdi, regs.rsp, regs.rbp,
		regs.r8,  regs.r9,  regs.r10, regs.r11,
		regs.r12, regs.r13, regs.r14, regs.r15,
		regs.rip, regs.rflags);
	r = ioctl(fd, KVM_GET_SREGS, &sregs);
	if (r == -1) {
		perror("KVM_GET_SREGS");
		return;
	}
	print_seg(stderr, "cs", &sregs.cs);
	print_seg(stderr, "ds", &sregs.ds);
	print_seg(stderr, "es", &sregs.es);
	print_seg(stderr, "ss", &sregs.ss);
	print_seg(stderr, "fs", &sregs.fs);
	print_seg(stderr, "gs", &sregs.gs);
	print_seg(stderr, "tr", &sregs.tr);
	print_seg(stderr, "ldt", &sregs.ldt);
	print_dt(stderr, "gdt", &sregs.gdt);
	print_dt(stderr, "idt", &sregs.idt);
	fprintf(stderr, "cr0 %llx cr2 %llx cr3 %llx cr4 %llx cr8 %llx"
		" efer %llx\n",
		sregs.cr0, sregs.cr2, sregs.cr3, sregs.cr4, sregs.cr8,
		sregs.efer);
}

uint64_t kvm_get_apic_base(kvm_context_t kvm, int vcpu)
{
	struct kvm_run *run = kvm->run[vcpu];

	return run->apic_base;
}

void kvm_set_cr8(kvm_context_t kvm, int vcpu, uint64_t cr8)
{
	struct kvm_run *run = kvm->run[vcpu];

	run->cr8 = cr8;
}

__u64 kvm_get_cr8(kvm_context_t kvm, int vcpu)
{
	return kvm->run[vcpu]->cr8;
}

int kvm_setup_cpuid(kvm_context_t kvm, int vcpu, int nent,
		    struct kvm_cpuid_entry *entries)
{
	struct kvm_cpuid *cpuid;
	int r;

	cpuid = malloc(sizeof(*cpuid) + nent * sizeof(*entries));
	if (!cpuid)
		return -ENOMEM;

	cpuid->nent = nent;
	memcpy(cpuid->entries, entries, nent * sizeof(*entries));
	r = ioctl(kvm->vcpu_fd[vcpu], KVM_SET_CPUID, cpuid);

	free(cpuid);
	return r;
}

int kvm_setup_cpuid2(kvm_context_t kvm, int vcpu, int nent,
		     struct kvm_cpuid_entry2 *entries)
{
	struct kvm_cpuid2 *cpuid;
	int r;

	cpuid = malloc(sizeof(*cpuid) + nent * sizeof(*entries));
	if (!cpuid)
		return -ENOMEM;

	cpuid->nent = nent;
	memcpy(cpuid->entries, entries, nent * sizeof(*entries));
	r = ioctl(kvm->vcpu_fd[vcpu], KVM_SET_CPUID2, cpuid);
	if (r == -1) {
		fprintf(stderr, "kvm_setup_cpuid2: %m\n");
		r = -errno;
	}
	free(cpuid);
	return r;
}

int kvm_set_shadow_pages(kvm_context_t kvm, unsigned int nrshadow_pages)
{
#ifdef KVM_CAP_MMU_SHADOW_CACHE_CONTROL
	int r;

	r = ioctl(kvm->fd, KVM_CHECK_EXTENSION,
		  KVM_CAP_MMU_SHADOW_CACHE_CONTROL);
	if (r > 0) {
		r = ioctl(kvm->vm_fd, KVM_SET_NR_MMU_PAGES, nrshadow_pages);
		if (r == -1) {
			fprintf(stderr, "kvm_set_shadow_pages: %m\n");
			return -errno;
		}
		return 0;
	}
#endif
	return -1;
}

int kvm_get_shadow_pages(kvm_context_t kvm, unsigned int *nrshadow_pages)
{
#ifdef KVM_CAP_MMU_SHADOW_CACHE_CONTROL
	int r;

	r = ioctl(kvm->fd, KVM_CHECK_EXTENSION,
		  KVM_CAP_MMU_SHADOW_CACHE_CONTROL);
	if (r > 0) {
		*nrshadow_pages = ioctl(kvm->vm_fd, KVM_GET_NR_MMU_PAGES);
		return 0;
	}
#endif
	return -1;
}

#ifdef KVM_CAP_VAPIC

static int tpr_access_reporting(kvm_context_t kvm, int vcpu, int enabled)
{
	int r;
	struct kvm_tpr_access_ctl tac = {
		.enabled = enabled,
	};

	r = ioctl(kvm->fd, KVM_CHECK_EXTENSION, KVM_CAP_VAPIC);
	if (r == -1 || r == 0)
		return -ENOSYS;
	r = ioctl(kvm->vcpu_fd[vcpu], KVM_TPR_ACCESS_REPORTING, &tac);
	if (r == -1) {
		r = -errno;
		perror("KVM_TPR_ACCESS_REPORTING");
		return r;
	}
	return 0;
}

int kvm_enable_tpr_access_reporting(kvm_context_t kvm, int vcpu)
{
	return tpr_access_reporting(kvm, vcpu, 1);
}

int kvm_disable_tpr_access_reporting(kvm_context_t kvm, int vcpu)
{
	return tpr_access_reporting(kvm, vcpu, 0);
}

#endif

#ifdef KVM_CAP_EXT_CPUID

static struct kvm_cpuid2 *try_get_cpuid(kvm_context_t kvm, int max)
{
	struct kvm_cpuid2 *cpuid;
	int r, size;

	size = sizeof(*cpuid) + max * sizeof(*cpuid->entries);
	cpuid = (struct kvm_cpuid2 *)malloc(size);
	cpuid->nent = max;
	r = ioctl(kvm->fd, KVM_GET_SUPPORTED_CPUID, cpuid);
	if (r == -1)
		r = -errno;
	else if (r == 0 && cpuid->nent >= max)
		r = -E2BIG;
	if (r < 0) {
		if (r == -E2BIG) {
			free(cpuid);
			return NULL;
		} else {
			fprintf(stderr, "KVM_GET_SUPPORTED_CPUID failed: %s\n",
				strerror(-r));
			exit(1);
		}
	}
	return cpuid;
}

#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6
#define R_EDI 7

uint32_t kvm_get_supported_cpuid(kvm_context_t kvm, uint32_t function, int reg)
{
	struct kvm_cpuid2 *cpuid;
	int i, max;
	uint32_t ret = 0;
	uint32_t cpuid_1_edx;

	if (!kvm_check_extension(kvm, KVM_CAP_EXT_CPUID)) {
		return -1U;
	}

	max = 1;
	while ((cpuid = try_get_cpuid(kvm, max)) == NULL) {
		max *= 2;
	}

	for (i = 0; i < cpuid->nent; ++i) {
		if (cpuid->entries[i].function == function) {
			switch (reg) {
			case R_EAX:
				ret = cpuid->entries[i].eax;
				break;
			case R_EBX:
				ret = cpuid->entries[i].ebx;
				break;
			case R_ECX:
				ret = cpuid->entries[i].ecx;
				break;
			case R_EDX:
				ret = cpuid->entries[i].edx;
                                if (function == 1) {
                                    /* kvm misreports the following features
                                     */
                                    ret |= 1 << 12; /* MTRR */
                                    ret |= 1 << 16; /* PAT */
                                    ret |= 1 << 7;  /* MCE */
                                    ret |= 1 << 14; /* MCA */
                                }

				/* On Intel, kvm returns cpuid according to
				 * the Intel spec, so add missing bits
				 * according to the AMD spec:
				 */
				if (function == 0x80000001) {
					cpuid_1_edx = kvm_get_supported_cpuid(kvm, 1, R_EDX);
					ret |= cpuid_1_edx & 0xdfeff7ff;
				}
				break;
			}
		}
	}

	free(cpuid);

	return ret;
}

#else

uint32_t kvm_get_supported_cpuid(kvm_context_t kvm, uint32_t function, int reg)
{
	return -1U;
}

#endif
