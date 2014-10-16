/*
 * qemu/kvm integration, x86 specific code
 *
 * Copyright (C) 2006-2008 Qumranet Technologies
 *
 * Licensed under the terms of the GNU GPL version 2 or higher.
 */

#include "config.h"
#include "config-host.h"

#include <string.h>
#include "hw/hw.h"
#include "gdbstub.h"
#include <sys/io.h>

#include "qemu-kvm.h"
#include "libkvm.h"
#include <pthread.h>
#include <sys/utsname.h>
#include <linux/kvm_para.h>
#include <sys/ioctl.h>

#include "kvm.h"
#include "hw/pc.h"

#define MSR_IA32_TSC		0x10

static struct kvm_msr_list *kvm_msr_list;
extern unsigned int kvm_shadow_memory;
static int kvm_has_msr_star;
static int kvm_has_vm_hsave_pa;

static int lm_capable_kernel;

int kvm_set_tss_addr(kvm_context_t kvm, unsigned long addr)
{
#ifdef KVM_CAP_SET_TSS_ADDR
	int r;

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_TSS_ADDR);
	if (r > 0) {
		r = kvm_vm_ioctl(kvm_state, KVM_SET_TSS_ADDR, addr);
		if (r < 0) {
			fprintf(stderr, "kvm_set_tss_addr: %m\n");
			return r;
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

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_TSS_ADDR);
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

static int kvm_set_identity_map_addr(kvm_context_t kvm, unsigned long addr)
{
#ifdef KVM_CAP_SET_IDENTITY_MAP_ADDR
	int r;

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_IDENTITY_MAP_ADDR);
	if (r > 0) {
		r = kvm_vm_ioctl(kvm_state, KVM_SET_IDENTITY_MAP_ADDR, &addr);
		if (r == -1) {
			fprintf(stderr, "kvm_set_identity_map_addr: %m\n");
			return -errno;
		}
		return 0;
	}
#endif
	return -ENOSYS;
}

static int kvm_init_identity_map_page(kvm_context_t kvm)
{
#ifdef KVM_CAP_SET_IDENTITY_MAP_ADDR
	int r;

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_IDENTITY_MAP_ADDR);
	if (r > 0) {
		/*
		 * this address is 4 pages before the bios, and the bios should present
		 * as unavaible memory
		 */
		r = kvm_set_identity_map_addr(kvm, 0xfffbc000);
		if (r < 0) {
			fprintf(stderr, "kvm_init_identity_map_page: "
				"unable to set identity mapping addr\n");
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
		r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_PIT);
		if (r > 0) {
			r = kvm_vm_ioctl(kvm_state, KVM_CREATE_PIT);
			if (r >= 0)
				kvm->pit_in_kernel = 1;
			else {
				fprintf(stderr, "Create kernel PIC irqchip failed\n");
				return r;
			}
		}
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

	r = kvm_init_identity_map_page(kvm);
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

static int kvm_handle_tpr_access(kvm_vcpu_context_t vcpu)
{
	struct kvm_run *run = vcpu->run;
	kvm_tpr_access_report(cpu_single_env,
                         run->tpr_access.rip,
                         run->tpr_access.is_write);
    return 0;
}


int kvm_enable_vapic(kvm_vcpu_context_t vcpu, uint64_t vapic)
{
	int r;
	struct kvm_vapic_addr va = {
		.vapic_addr = vapic,
	};

	r = ioctl(vcpu->fd, KVM_SET_VAPIC_ADDR, &va);
	if (r == -1) {
		r = -errno;
		perror("kvm_enable_vapic");
		return r;
	}
	return 0;
}

#endif

int kvm_arch_run(kvm_vcpu_context_t vcpu)
{
	int r = 0;
	struct kvm_run *run = vcpu->run;


	switch (run->exit_reason) {
#ifdef KVM_EXIT_SET_TPR
		case KVM_EXIT_SET_TPR:
			break;
#endif
#ifdef KVM_EXIT_TPR_ACCESS
		case KVM_EXIT_TPR_ACCESS:
			r = kvm_handle_tpr_access(vcpu);
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
	int r;
	int slot;

	slot = get_alias_slot(phys_start);
	if (slot < 0)
		slot = get_free_alias_slot();
	if (slot < 0)
		return -EBUSY;
	alias.slot = slot;

	r = kvm_vm_ioctl(kvm_state, KVM_SET_MEMORY_ALIAS, &alias);
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

int kvm_get_lapic(kvm_vcpu_context_t vcpu, struct kvm_lapic_state *s)
{
	int r;
	if (!kvm_irqchip_in_kernel(vcpu->kvm))
		return 0;
	r = ioctl(vcpu->fd, KVM_GET_LAPIC, s);
	if (r == -1) {
		r = -errno;
		perror("kvm_get_lapic");
	}
	return r;
}

int kvm_set_lapic(kvm_vcpu_context_t vcpu, struct kvm_lapic_state *s)
{
	int r;
	if (!kvm_irqchip_in_kernel(vcpu->kvm))
		return 0;
	r = ioctl(vcpu->fd, KVM_SET_LAPIC, s);
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
	if (!kvm->pit_in_kernel)
		return 0;
	return kvm_vm_ioctl(kvm_state, KVM_GET_PIT, s);
}

int kvm_set_pit(kvm_context_t kvm, struct kvm_pit_state *s)
{
	if (!kvm->pit_in_kernel)
		return 0;
	return kvm_vm_ioctl(kvm_state, KVM_SET_PIT, s);
}

#ifdef KVM_CAP_PIT_STATE2
int kvm_get_pit2(kvm_context_t kvm, struct kvm_pit_state2 *ps2)
{
	if (!kvm->pit_in_kernel)
		return 0;
	return kvm_vm_ioctl(kvm_state, KVM_GET_PIT2, ps2);
}

int kvm_set_pit2(kvm_context_t kvm, struct kvm_pit_state2 *ps2)
{
	if (!kvm->pit_in_kernel)
		return 0;
	return kvm_vm_ioctl(kvm_state, KVM_SET_PIT2, ps2);
}

#endif
#endif

int kvm_has_pit_state2(kvm_context_t kvm)
{
	int r = 0;

#ifdef KVM_CAP_PIT_STATE2
	r = kvm_check_extension(kvm_state, KVM_CAP_PIT_STATE2);
#endif
	return r;
}

void kvm_show_code(kvm_vcpu_context_t vcpu)
{
#define SHOW_CODE_LEN 50
	int fd = vcpu->fd;
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int r, n;
	int back_offset;
	unsigned char code;
	char code_str[SHOW_CODE_LEN * 3 + 1];
	unsigned long rip;
	kvm_context_t kvm = vcpu->kvm;

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
		r = kvm_mmio_read(kvm->opaque, rip + n, &code, 1);
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
	int r;

	sizer.nmsrs = 0;
	r = kvm_ioctl(kvm_state, KVM_GET_MSR_INDEX_LIST, &sizer);
	if (r < 0 && r != -E2BIG)
		return NULL;
	/* Old kernel modules had a bug and could write beyond the provided
	   memory. Allocate at least a safe amount of 1K. */
	msrs = qemu_malloc(MAX(1024, sizeof(*msrs) +
				       sizer.nmsrs * sizeof(*msrs->indices)));

	msrs->nmsrs = sizer.nmsrs;
	r = kvm_ioctl(kvm_state, KVM_GET_MSR_INDEX_LIST, msrs);
	if (r < 0) {
		free(msrs);
		errno = r;
		return NULL;
	}
	return msrs;
}

int kvm_get_msrs(kvm_vcpu_context_t vcpu, struct kvm_msr_entry *msrs, int n)
{
    struct kvm_msrs *kmsrs = qemu_malloc(sizeof *kmsrs + n * sizeof *msrs);
    int r, e;

    kmsrs->nmsrs = n;
    memcpy(kmsrs->entries, msrs, n * sizeof *msrs);
    r = ioctl(vcpu->fd, KVM_GET_MSRS, kmsrs);
    e = errno;
    memcpy(msrs, kmsrs->entries, n * sizeof *msrs);
    free(kmsrs);
    errno = e;
    return r;
}

int kvm_set_msrs(kvm_vcpu_context_t vcpu, struct kvm_msr_entry *msrs, int n)
{
    struct kvm_msrs *kmsrs = qemu_malloc(sizeof *kmsrs + n * sizeof *msrs);
    int r, e;

    kmsrs->nmsrs = n;
    memcpy(kmsrs->entries, msrs, n * sizeof *msrs);
    r = ioctl(vcpu->fd, KVM_SET_MSRS, kmsrs);
    e = errno;
    free(kmsrs);
    errno = e;
    return r;
}

int kvm_get_mce_cap_supported(kvm_context_t kvm, uint64_t *mce_cap,
                              int *max_banks)
{
#ifdef KVM_CAP_MCE
    int r;

    r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_MCE);
    if (r > 0) {
        *max_banks = r;
        return kvm_ioctl(kvm_state, KVM_X86_GET_MCE_CAP_SUPPORTED, mce_cap);
    }
#endif
    return -ENOSYS;
}

int kvm_setup_mce(kvm_vcpu_context_t vcpu, uint64_t *mcg_cap)
{
#ifdef KVM_CAP_MCE
    return ioctl(vcpu->fd, KVM_X86_SETUP_MCE, mcg_cap);
#else
    return -ENOSYS;
#endif
}

int kvm_set_mce(kvm_vcpu_context_t vcpu, struct kvm_x86_mce *m)
{
#ifdef KVM_CAP_MCE
    return ioctl(vcpu->fd, KVM_X86_SET_MCE, m);
#else
    return -ENOSYS;
#endif
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

void kvm_show_regs(kvm_vcpu_context_t vcpu)
{
	int fd = vcpu->fd;
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

uint64_t kvm_get_apic_base(kvm_vcpu_context_t vcpu)
{
	return vcpu->run->apic_base;
}

void kvm_set_cr8(kvm_vcpu_context_t vcpu, uint64_t cr8)
{
	vcpu->run->cr8 = cr8;
}

__u64 kvm_get_cr8(kvm_vcpu_context_t vcpu)
{
	return vcpu->run->cr8;
}

int kvm_setup_cpuid(kvm_vcpu_context_t vcpu, int nent,
		    struct kvm_cpuid_entry *entries)
{
	struct kvm_cpuid *cpuid;
	int r;

	cpuid = qemu_malloc(sizeof(*cpuid) + nent * sizeof(*entries));

	cpuid->nent = nent;
	memcpy(cpuid->entries, entries, nent * sizeof(*entries));
	r = ioctl(vcpu->fd, KVM_SET_CPUID, cpuid);

	free(cpuid);
	return r;
}

int kvm_setup_cpuid2(kvm_vcpu_context_t vcpu, int nent,
		     struct kvm_cpuid_entry2 *entries)
{
	struct kvm_cpuid2 *cpuid;
	int r;

	cpuid = qemu_malloc(sizeof(*cpuid) + nent * sizeof(*entries));

	cpuid->nent = nent;
	memcpy(cpuid->entries, entries, nent * sizeof(*entries));
	r = ioctl(vcpu->fd, KVM_SET_CPUID2, cpuid);
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

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION,
		  KVM_CAP_MMU_SHADOW_CACHE_CONTROL);
	if (r > 0) {
		r = kvm_vm_ioctl(kvm_state, KVM_SET_NR_MMU_PAGES, nrshadow_pages);
		if (r < 0) {
			fprintf(stderr, "kvm_set_shadow_pages: %m\n");
			return r;
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

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION,
		  KVM_CAP_MMU_SHADOW_CACHE_CONTROL);
	if (r > 0) {
		*nrshadow_pages = kvm_vm_ioctl(kvm_state, KVM_GET_NR_MMU_PAGES);
		return 0;
	}
#endif
	return -1;
}

#ifdef KVM_CAP_VAPIC

static int tpr_access_reporting(kvm_vcpu_context_t vcpu, int enabled)
{
	int r;
	struct kvm_tpr_access_ctl tac = {
		.enabled = enabled,
	};

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_VAPIC);
	if (r <= 0)
		return -ENOSYS;
	r = ioctl(vcpu->fd, KVM_TPR_ACCESS_REPORTING, &tac);
	if (r == -1) {
		r = -errno;
		perror("KVM_TPR_ACCESS_REPORTING");
		return r;
	}
	return 0;
}

int kvm_enable_tpr_access_reporting(kvm_vcpu_context_t vcpu)
{
	return tpr_access_reporting(vcpu, 1);
}

int kvm_disable_tpr_access_reporting(kvm_vcpu_context_t vcpu)
{
	return tpr_access_reporting(vcpu, 0);
}

#endif

#ifdef KVM_CAP_EXT_CPUID

static struct kvm_cpuid2 *try_get_cpuid(kvm_context_t kvm, int max)
{
	struct kvm_cpuid2 *cpuid;
	int r, size;

	size = sizeof(*cpuid) + max * sizeof(*cpuid->entries);
	cpuid = qemu_malloc(size);
	cpuid->nent = max;
	r = kvm_ioctl(kvm_state, KVM_GET_SUPPORTED_CPUID, cpuid);
	if (r == 0 && cpuid->nent >= max)
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

	if (!kvm_check_extension(kvm_state, KVM_CAP_EXT_CPUID)) {
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
int kvm_qemu_create_memory_alias(uint64_t phys_start,
                                 uint64_t len,
                                 uint64_t target_phys)
{
    return kvm_create_memory_alias(kvm_context, phys_start, len, target_phys);
}

int kvm_qemu_destroy_memory_alias(uint64_t phys_start)
{
	return kvm_destroy_memory_alias(kvm_context, phys_start);
}

int kvm_arch_qemu_create_context(void)
{
    int i;
    struct utsname utsname;

    uname(&utsname);
    lm_capable_kernel = strcmp(utsname.machine, "x86_64") == 0;

    if (kvm_shadow_memory)
        kvm_set_shadow_pages(kvm_context, kvm_shadow_memory);

    kvm_msr_list = kvm_get_msr_list(kvm_context);
    if (!kvm_msr_list)
		return -1;
    for (i = 0; i < kvm_msr_list->nmsrs; ++i) {
	if (kvm_msr_list->indices[i] == MSR_STAR)
	    kvm_has_msr_star = 1;
        if (kvm_msr_list->indices[i] == MSR_VM_HSAVE_PA)
            kvm_has_vm_hsave_pa = 1;
    }

    return 0;
}

static void set_msr_entry(struct kvm_msr_entry *entry, uint32_t index,
                          uint64_t data)
{
    entry->index = index;
    entry->data  = data;
}

/* returns 0 on success, non-0 on failure */
static int get_msr_entry(struct kvm_msr_entry *entry, CPUState *env)
{
        switch (entry->index) {
        case MSR_IA32_SYSENTER_CS:
            env->sysenter_cs  = entry->data;
            break;
        case MSR_IA32_SYSENTER_ESP:
            env->sysenter_esp = entry->data;
            break;
        case MSR_IA32_SYSENTER_EIP:
            env->sysenter_eip = entry->data;
            break;
        case MSR_STAR:
            env->star         = entry->data;
            break;
#ifdef TARGET_X86_64
        case MSR_CSTAR:
            env->cstar        = entry->data;
            break;
        case MSR_KERNELGSBASE:
            env->kernelgsbase = entry->data;
            break;
        case MSR_FMASK:
            env->fmask        = entry->data;
            break;
        case MSR_LSTAR:
            env->lstar        = entry->data;
            break;
#endif
        case MSR_IA32_TSC:
            env->tsc          = entry->data;
            break;
        case MSR_VM_HSAVE_PA:
            env->vm_hsave     = entry->data;
            break;
        default:
            printf("Warning unknown msr index 0x%x\n", entry->index);
            return 1;
        }
        return 0;
}

#ifdef TARGET_X86_64
#define MSR_COUNT 9
#else
#define MSR_COUNT 5
#endif

static void set_v8086_seg(struct kvm_segment *lhs, const SegmentCache *rhs)
{
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->type = 3;
    lhs->present = 1;
    lhs->dpl = 3;
    lhs->db = 0;
    lhs->s = 1;
    lhs->l = 0;
    lhs->g = 0;
    lhs->avl = 0;
    lhs->unusable = 0;
}

static void set_seg(struct kvm_segment *lhs, const SegmentCache *rhs)
{
    unsigned flags = rhs->flags;
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->type = (flags >> DESC_TYPE_SHIFT) & 15;
    lhs->present = (flags & DESC_P_MASK) != 0;
    lhs->dpl = rhs->selector & 3;
    lhs->db = (flags >> DESC_B_SHIFT) & 1;
    lhs->s = (flags & DESC_S_MASK) != 0;
    lhs->l = (flags >> DESC_L_SHIFT) & 1;
    lhs->g = (flags & DESC_G_MASK) != 0;
    lhs->avl = (flags & DESC_AVL_MASK) != 0;
    lhs->unusable = 0;
}

static void get_seg(SegmentCache *lhs, const struct kvm_segment *rhs)
{
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->flags =
	(rhs->type << DESC_TYPE_SHIFT)
	| (rhs->present * DESC_P_MASK)
	| (rhs->dpl << DESC_DPL_SHIFT)
	| (rhs->db << DESC_B_SHIFT)
	| (rhs->s * DESC_S_MASK)
	| (rhs->l << DESC_L_SHIFT)
	| (rhs->g * DESC_G_MASK)
	| (rhs->avl * DESC_AVL_MASK);
}

void kvm_arch_load_regs(CPUState *env)
{
    struct kvm_regs regs;
    struct kvm_fpu fpu;
    struct kvm_sregs sregs;
    struct kvm_msr_entry msrs[MSR_COUNT];
    int rc, n, i;

    regs.rax = env->regs[R_EAX];
    regs.rbx = env->regs[R_EBX];
    regs.rcx = env->regs[R_ECX];
    regs.rdx = env->regs[R_EDX];
    regs.rsi = env->regs[R_ESI];
    regs.rdi = env->regs[R_EDI];
    regs.rsp = env->regs[R_ESP];
    regs.rbp = env->regs[R_EBP];
#ifdef TARGET_X86_64
    regs.r8 = env->regs[8];
    regs.r9 = env->regs[9];
    regs.r10 = env->regs[10];
    regs.r11 = env->regs[11];
    regs.r12 = env->regs[12];
    regs.r13 = env->regs[13];
    regs.r14 = env->regs[14];
    regs.r15 = env->regs[15];
#endif

    regs.rflags = env->eflags;
    regs.rip = env->eip;

    kvm_set_regs(env->kvm_cpu_state.vcpu_ctx, &regs);

    memset(&fpu, 0, sizeof fpu);
    fpu.fsw = env->fpus & ~(7 << 11);
    fpu.fsw |= (env->fpstt & 7) << 11;
    fpu.fcw = env->fpuc;
    for (i = 0; i < 8; ++i)
	fpu.ftwx |= (!env->fptags[i]) << i;
    memcpy(fpu.fpr, env->fpregs, sizeof env->fpregs);
    memcpy(fpu.xmm, env->xmm_regs, sizeof env->xmm_regs);
    fpu.mxcsr = env->mxcsr;
    kvm_set_fpu(env->kvm_cpu_state.vcpu_ctx, &fpu);

    memcpy(sregs.interrupt_bitmap, env->interrupt_bitmap, sizeof(sregs.interrupt_bitmap));

    if ((env->eflags & VM_MASK)) {
	    set_v8086_seg(&sregs.cs, &env->segs[R_CS]);
	    set_v8086_seg(&sregs.ds, &env->segs[R_DS]);
	    set_v8086_seg(&sregs.es, &env->segs[R_ES]);
	    set_v8086_seg(&sregs.fs, &env->segs[R_FS]);
	    set_v8086_seg(&sregs.gs, &env->segs[R_GS]);
	    set_v8086_seg(&sregs.ss, &env->segs[R_SS]);
    } else {
	    set_seg(&sregs.cs, &env->segs[R_CS]);
	    set_seg(&sregs.ds, &env->segs[R_DS]);
	    set_seg(&sregs.es, &env->segs[R_ES]);
	    set_seg(&sregs.fs, &env->segs[R_FS]);
	    set_seg(&sregs.gs, &env->segs[R_GS]);
	    set_seg(&sregs.ss, &env->segs[R_SS]);

	    if (env->cr[0] & CR0_PE_MASK) {
		/* force ss cpl to cs cpl */
		sregs.ss.selector = (sregs.ss.selector & ~3) |
			(sregs.cs.selector & 3);
		sregs.ss.dpl = sregs.ss.selector & 3;
	    }
    }

    set_seg(&sregs.tr, &env->tr);
    set_seg(&sregs.ldt, &env->ldt);

    sregs.idt.limit = env->idt.limit;
    sregs.idt.base = env->idt.base;
    sregs.gdt.limit = env->gdt.limit;
    sregs.gdt.base = env->gdt.base;

    sregs.cr0 = env->cr[0];
    sregs.cr2 = env->cr[2];
    sregs.cr3 = env->cr[3];
    sregs.cr4 = env->cr[4];

    sregs.cr8 = cpu_get_apic_tpr(env);
    sregs.apic_base = cpu_get_apic_base(env);

    sregs.efer = env->efer;

    kvm_set_sregs(env->kvm_cpu_state.vcpu_ctx, &sregs);

    /* msrs */
    n = 0;
    set_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_CS,  env->sysenter_cs);
    set_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_ESP, env->sysenter_esp);
    set_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_EIP, env->sysenter_eip);
    if (kvm_has_msr_star)
	set_msr_entry(&msrs[n++], MSR_STAR,              env->star);
    if (kvm_has_vm_hsave_pa)
        set_msr_entry(&msrs[n++], MSR_VM_HSAVE_PA, env->vm_hsave);
#ifdef TARGET_X86_64
    if (lm_capable_kernel) {
        set_msr_entry(&msrs[n++], MSR_CSTAR,             env->cstar);
        set_msr_entry(&msrs[n++], MSR_KERNELGSBASE,      env->kernelgsbase);
        set_msr_entry(&msrs[n++], MSR_FMASK,             env->fmask);
        set_msr_entry(&msrs[n++], MSR_LSTAR  ,           env->lstar);
    }
#endif

    rc = kvm_set_msrs(env->kvm_cpu_state.vcpu_ctx, msrs, n);
    if (rc == -1)
        perror("kvm_set_msrs FAILED");
}

void kvm_load_tsc(CPUState *env)
{
    int rc;
    struct kvm_msr_entry msr;

    set_msr_entry(&msr, MSR_IA32_TSC, env->tsc);

    rc = kvm_set_msrs(env->kvm_cpu_state.vcpu_ctx, &msr, 1);
    if (rc == -1)
        perror("kvm_set_tsc FAILED.\n");
}

void kvm_arch_save_mpstate(CPUState *env)
{
#ifdef KVM_CAP_MP_STATE
    int r;
    struct kvm_mp_state mp_state;

    r = kvm_get_mpstate(env->kvm_cpu_state.vcpu_ctx, &mp_state);
    if (r < 0)
        env->mp_state = -1;
    else
        env->mp_state = mp_state.mp_state;
#endif
}

void kvm_arch_load_mpstate(CPUState *env)
{
#ifdef KVM_CAP_MP_STATE
    struct kvm_mp_state mp_state = { .mp_state = env->mp_state };

    /*
     * -1 indicates that the host did not support GET_MP_STATE ioctl,
     *  so don't touch it.
     */
    if (env->mp_state != -1)
        kvm_set_mpstate(env->kvm_cpu_state.vcpu_ctx, &mp_state);
#endif
}

void kvm_arch_save_regs(CPUState *env)
{
    struct kvm_regs regs;
    struct kvm_fpu fpu;
    struct kvm_sregs sregs;
    struct kvm_msr_entry msrs[MSR_COUNT];
    uint32_t hflags;
    uint32_t i, n, rc;

    kvm_get_regs(env->kvm_cpu_state.vcpu_ctx, &regs);

    env->regs[R_EAX] = regs.rax;
    env->regs[R_EBX] = regs.rbx;
    env->regs[R_ECX] = regs.rcx;
    env->regs[R_EDX] = regs.rdx;
    env->regs[R_ESI] = regs.rsi;
    env->regs[R_EDI] = regs.rdi;
    env->regs[R_ESP] = regs.rsp;
    env->regs[R_EBP] = regs.rbp;
#ifdef TARGET_X86_64
    env->regs[8] = regs.r8;
    env->regs[9] = regs.r9;
    env->regs[10] = regs.r10;
    env->regs[11] = regs.r11;
    env->regs[12] = regs.r12;
    env->regs[13] = regs.r13;
    env->regs[14] = regs.r14;
    env->regs[15] = regs.r15;
#endif

    env->eflags = regs.rflags;
    env->eip = regs.rip;

    kvm_get_fpu(env->kvm_cpu_state.vcpu_ctx, &fpu);
    env->fpstt = (fpu.fsw >> 11) & 7;
    env->fpus = fpu.fsw;
    env->fpuc = fpu.fcw;
    for (i = 0; i < 8; ++i)
	env->fptags[i] = !((fpu.ftwx >> i) & 1);
    memcpy(env->fpregs, fpu.fpr, sizeof env->fpregs);
    memcpy(env->xmm_regs, fpu.xmm, sizeof env->xmm_regs);
    env->mxcsr = fpu.mxcsr;

    kvm_get_sregs(env->kvm_cpu_state.vcpu_ctx, &sregs);

    memcpy(env->interrupt_bitmap, sregs.interrupt_bitmap, sizeof(env->interrupt_bitmap));

    get_seg(&env->segs[R_CS], &sregs.cs);
    get_seg(&env->segs[R_DS], &sregs.ds);
    get_seg(&env->segs[R_ES], &sregs.es);
    get_seg(&env->segs[R_FS], &sregs.fs);
    get_seg(&env->segs[R_GS], &sregs.gs);
    get_seg(&env->segs[R_SS], &sregs.ss);

    get_seg(&env->tr, &sregs.tr);
    get_seg(&env->ldt, &sregs.ldt);

    env->idt.limit = sregs.idt.limit;
    env->idt.base = sregs.idt.base;
    env->gdt.limit = sregs.gdt.limit;
    env->gdt.base = sregs.gdt.base;

    env->cr[0] = sregs.cr0;
    env->cr[2] = sregs.cr2;
    env->cr[3] = sregs.cr3;
    env->cr[4] = sregs.cr4;

    cpu_set_apic_base(env, sregs.apic_base);

    env->efer = sregs.efer;
    //cpu_set_apic_tpr(env, sregs.cr8);

#define HFLAG_COPY_MASK ~( \
			HF_CPL_MASK | HF_PE_MASK | HF_MP_MASK | HF_EM_MASK | \
			HF_TS_MASK | HF_TF_MASK | HF_VM_MASK | HF_IOPL_MASK | \
			HF_OSFXSR_MASK | HF_LMA_MASK | HF_CS32_MASK | \
			HF_SS32_MASK | HF_CS64_MASK | HF_ADDSEG_MASK)



    hflags = (env->segs[R_CS].flags >> DESC_DPL_SHIFT) & HF_CPL_MASK;
    hflags |= (env->cr[0] & CR0_PE_MASK) << (HF_PE_SHIFT - CR0_PE_SHIFT);
    hflags |= (env->cr[0] << (HF_MP_SHIFT - CR0_MP_SHIFT)) &
	    (HF_MP_MASK | HF_EM_MASK | HF_TS_MASK);
    hflags |= (env->eflags & (HF_TF_MASK | HF_VM_MASK | HF_IOPL_MASK));
    hflags |= (env->cr[4] & CR4_OSFXSR_MASK) <<
	    (HF_OSFXSR_SHIFT - CR4_OSFXSR_SHIFT);

    if (env->efer & MSR_EFER_LMA) {
        hflags |= HF_LMA_MASK;
    }

    if ((hflags & HF_LMA_MASK) && (env->segs[R_CS].flags & DESC_L_MASK)) {
        hflags |= HF_CS32_MASK | HF_SS32_MASK | HF_CS64_MASK;
    } else {
        hflags |= (env->segs[R_CS].flags & DESC_B_MASK) >>
		(DESC_B_SHIFT - HF_CS32_SHIFT);
        hflags |= (env->segs[R_SS].flags & DESC_B_MASK) >>
		(DESC_B_SHIFT - HF_SS32_SHIFT);
        if (!(env->cr[0] & CR0_PE_MASK) ||
                   (env->eflags & VM_MASK) ||
                   !(hflags & HF_CS32_MASK)) {
                hflags |= HF_ADDSEG_MASK;
            } else {
                hflags |= ((env->segs[R_DS].base |
                                env->segs[R_ES].base |
                                env->segs[R_SS].base) != 0) <<
                    HF_ADDSEG_SHIFT;
            }
    }
    env->hflags = (env->hflags & HFLAG_COPY_MASK) | hflags;

    /* msrs */
    n = 0;
    msrs[n++].index = MSR_IA32_SYSENTER_CS;
    msrs[n++].index = MSR_IA32_SYSENTER_ESP;
    msrs[n++].index = MSR_IA32_SYSENTER_EIP;
    if (kvm_has_msr_star)
	msrs[n++].index = MSR_STAR;
    msrs[n++].index = MSR_IA32_TSC;
    if (kvm_has_vm_hsave_pa)
        msrs[n++].index = MSR_VM_HSAVE_PA;
#ifdef TARGET_X86_64
    if (lm_capable_kernel) {
        msrs[n++].index = MSR_CSTAR;
        msrs[n++].index = MSR_KERNELGSBASE;
        msrs[n++].index = MSR_FMASK;
        msrs[n++].index = MSR_LSTAR;
    }
#endif
    rc = kvm_get_msrs(env->kvm_cpu_state.vcpu_ctx, msrs, n);
    if (rc == -1) {
        perror("kvm_get_msrs FAILED");
    }
    else {
        n = rc; /* actual number of MSRs */
        for (i=0 ; i<n; i++) {
            if (get_msr_entry(&msrs[i], env))
                return;
        }
    }
}

static void do_cpuid_ent(struct kvm_cpuid_entry2 *e, uint32_t function,
                         uint32_t count, CPUState *env)
{
    env->regs[R_EAX] = function;
    env->regs[R_ECX] = count;
    qemu_kvm_cpuid_on_env(env);
    e->function = function;
    e->flags = 0;
    e->index = 0;
    e->eax = env->regs[R_EAX];
    e->ebx = env->regs[R_EBX];
    e->ecx = env->regs[R_ECX];
    e->edx = env->regs[R_EDX];
}

struct kvm_para_features {
	int cap;
	int feature;
} para_features[] = {
#ifdef KVM_CAP_CLOCKSOURCE
	{ KVM_CAP_CLOCKSOURCE, KVM_FEATURE_CLOCKSOURCE },
#endif
#ifdef KVM_CAP_NOP_IO_DELAY
	{ KVM_CAP_NOP_IO_DELAY, KVM_FEATURE_NOP_IO_DELAY },
#endif
#ifdef KVM_CAP_PV_MMU
	{ KVM_CAP_PV_MMU, KVM_FEATURE_MMU_OP },
#endif
#ifdef KVM_CAP_CR3_CACHE
	{ KVM_CAP_CR3_CACHE, KVM_FEATURE_CR3_CACHE },
#endif
	{ -1, -1 }
};

static int get_para_features(kvm_context_t kvm_context)
{
	int i, features = 0;

	for (i = 0; i < ARRAY_SIZE(para_features)-1; i++) {
		if (kvm_check_extension(kvm_state, para_features[i].cap))
			features |= (1 << para_features[i].feature);
	}

	return features;
}

static void kvm_trim_features(uint32_t *features, uint32_t supported)
{
    int i;
    uint32_t mask;

    for (i = 0; i < 32; ++i) {
        mask = 1U << i;
        if ((*features & mask) && !(supported & mask)) {
            *features &= ~mask;
        }
    }
}

int kvm_arch_qemu_init_env(CPUState *cenv)
{
    struct kvm_cpuid_entry2 cpuid_ent[100];
#ifdef KVM_CPUID_SIGNATURE
    struct kvm_cpuid_entry2 *pv_ent;
    uint32_t signature[3];
#endif
    int cpuid_nent = 0;
    CPUState copy;
    uint32_t i, j, limit;

    qemu_kvm_load_lapic(cenv);


#ifdef KVM_CPUID_SIGNATURE
    /* Paravirtualization CPUIDs */
    memcpy(signature, "KVMKVMKVM\0\0\0", 12);
    pv_ent = &cpuid_ent[cpuid_nent++];
    memset(pv_ent, 0, sizeof(*pv_ent));
    pv_ent->function = KVM_CPUID_SIGNATURE;
    pv_ent->eax = 0;
    pv_ent->ebx = signature[0];
    pv_ent->ecx = signature[1];
    pv_ent->edx = signature[2];

    pv_ent = &cpuid_ent[cpuid_nent++];
    memset(pv_ent, 0, sizeof(*pv_ent));
    pv_ent->function = KVM_CPUID_FEATURES;
    pv_ent->eax = get_para_features(kvm_context);
#endif

    kvm_trim_features(&cenv->cpuid_features,
                      kvm_arch_get_supported_cpuid(cenv, 1, R_EDX));

    /* prevent the hypervisor bit from being cleared by the kernel */
    i = cenv->cpuid_ext_features & CPUID_EXT_HYPERVISOR;
    kvm_trim_features(&cenv->cpuid_ext_features,
                      kvm_arch_get_supported_cpuid(cenv, 1, R_ECX));
    cenv->cpuid_ext_features |= i;

    kvm_trim_features(&cenv->cpuid_ext2_features,
                      kvm_arch_get_supported_cpuid(cenv, 0x80000001, R_EDX));
    kvm_trim_features(&cenv->cpuid_ext3_features,
                      kvm_arch_get_supported_cpuid(cenv, 0x80000001, R_ECX));

    copy = *cenv;

    copy.regs[R_EAX] = 0;
    qemu_kvm_cpuid_on_env(&copy);
    limit = copy.regs[R_EAX];

    for (i = 0; i <= limit; ++i) {
        if (i == 4 || i == 0xb || i == 0xd) {
            for (j = 0; ; ++j) {
                do_cpuid_ent(&cpuid_ent[cpuid_nent], i, j, &copy);

                cpuid_ent[cpuid_nent].flags = KVM_CPUID_FLAG_SIGNIFCANT_INDEX;
                cpuid_ent[cpuid_nent].index = j;

                cpuid_nent++;

                if (i == 4 && copy.regs[R_EAX] == 0)
                    break;
                if (i == 0xb && !(copy.regs[R_ECX] & 0xff00))
                    break;
                if (i == 0xd && copy.regs[R_EAX] == 0)
                    break;
            }
        } else
            do_cpuid_ent(&cpuid_ent[cpuid_nent++], i, 0, &copy);
    }

    copy.regs[R_EAX] = 0x80000000;
    qemu_kvm_cpuid_on_env(&copy);
    limit = copy.regs[R_EAX];

    for (i = 0x80000000; i <= limit; ++i)
	do_cpuid_ent(&cpuid_ent[cpuid_nent++], i, 0, &copy);

    kvm_setup_cpuid2(cenv->kvm_cpu_state.vcpu_ctx, cpuid_nent, cpuid_ent);

#ifdef KVM_CAP_MCE
    if (((cenv->cpuid_version >> 8)&0xF) >= 6
        && (cenv->cpuid_features&(CPUID_MCE|CPUID_MCA)) == (CPUID_MCE|CPUID_MCA)
        && kvm_check_extension(kvm_state, KVM_CAP_MCE) > 0) {
        uint64_t mcg_cap;
        int banks;

        if (kvm_get_mce_cap_supported(kvm_context, &mcg_cap, &banks))
            perror("kvm_get_mce_cap_supported FAILED");
        else {
            if (banks > MCE_BANKS_DEF)
                banks = MCE_BANKS_DEF;
            mcg_cap &= MCE_CAP_DEF;
            mcg_cap |= banks;
            if (kvm_setup_mce(cenv->kvm_cpu_state.vcpu_ctx, &mcg_cap))
                perror("kvm_setup_mce FAILED");
            else
                cenv->mcg_cap = mcg_cap;
        }
    }
#endif

    return 0;
}

int kvm_arch_halt(void *opaque, kvm_vcpu_context_t vcpu)
{
    CPUState *env = cpu_single_env;

    if (!((env->interrupt_request & CPU_INTERRUPT_HARD) &&
	  (env->eflags & IF_MASK)) &&
	!(env->interrupt_request & CPU_INTERRUPT_NMI)) {
            env->halted = 1;
    }
    return 1;
}

void kvm_arch_pre_kvm_run(void *opaque, CPUState *env)
{
    if (!kvm_irqchip_in_kernel(kvm_context))
	kvm_set_cr8(env->kvm_cpu_state.vcpu_ctx, cpu_get_apic_tpr(env));
}

void kvm_arch_post_kvm_run(void *opaque, CPUState *env)
{
    cpu_single_env = env;

    env->eflags = kvm_get_interrupt_flag(env->kvm_cpu_state.vcpu_ctx)
	? env->eflags | IF_MASK : env->eflags & ~IF_MASK;

    cpu_set_apic_tpr(env, kvm_get_cr8(env->kvm_cpu_state.vcpu_ctx));
    cpu_set_apic_base(env, kvm_get_apic_base(env->kvm_cpu_state.vcpu_ctx));
}

int kvm_arch_has_work(CPUState *env)
{
    if (((env->interrupt_request & CPU_INTERRUPT_HARD) &&
	 (env->eflags & IF_MASK)) ||
	(env->interrupt_request & CPU_INTERRUPT_NMI))
	return 1;
    return 0;
}

int kvm_arch_try_push_interrupts(void *opaque)
{
    CPUState *env = cpu_single_env;
    int r, irq;

    if (kvm_is_ready_for_interrupt_injection(env->kvm_cpu_state.vcpu_ctx) &&
        (env->interrupt_request & CPU_INTERRUPT_HARD) &&
        (env->eflags & IF_MASK)) {
            env->interrupt_request &= ~CPU_INTERRUPT_HARD;
	    irq = cpu_get_pic_interrupt(env);
	    if (irq >= 0) {
		r = kvm_inject_irq(env->kvm_cpu_state.vcpu_ctx, irq);
		if (r < 0)
		    printf("cpu %d fail inject %x\n", env->cpu_index, irq);
	    }
    }

    return (env->interrupt_request & CPU_INTERRUPT_HARD) != 0;
}

#ifdef KVM_CAP_USER_NMI
void kvm_arch_push_nmi(void *opaque)
{
    CPUState *env = cpu_single_env;
    int r;

    if (likely(!(env->interrupt_request & CPU_INTERRUPT_NMI)))
        return;

    env->interrupt_request &= ~CPU_INTERRUPT_NMI;
    r = kvm_inject_nmi(env->kvm_cpu_state.vcpu_ctx);
    if (r < 0)
        printf("cpu %d fail inject NMI\n", env->cpu_index);
}
#endif /* KVM_CAP_USER_NMI */

void kvm_arch_update_regs_for_sipi(CPUState *env)
{
    SegmentCache cs = env->segs[R_CS];

    kvm_arch_save_regs(env);
    env->segs[R_CS] = cs;
    env->eip = 0;
    kvm_arch_load_regs(env);
}

void kvm_arch_cpu_reset(CPUState *env)
{
    kvm_arch_load_regs(env);
    if (!cpu_is_bsp(env)) {
	if (kvm_irqchip_in_kernel(kvm_context)) {
#ifdef KVM_CAP_MP_STATE
	    kvm_reset_mpstate(env->kvm_cpu_state.vcpu_ctx);
#endif
	} else {
	    env->interrupt_request &= ~CPU_INTERRUPT_HARD;
	    env->halted = 1;
	}
    }
}

int kvm_arch_insert_sw_breakpoint(CPUState *env, struct kvm_sw_breakpoint *bp)
{
    uint8_t int3 = 0xcc;

    if (cpu_memory_rw_debug(env, bp->pc, (uint8_t *)&bp->saved_insn, 1, 0) ||
        cpu_memory_rw_debug(env, bp->pc, &int3, 1, 1))
        return -EINVAL;
    return 0;
}

int kvm_arch_remove_sw_breakpoint(CPUState *env, struct kvm_sw_breakpoint *bp)
{
    uint8_t int3;

    if (cpu_memory_rw_debug(env, bp->pc, &int3, 1, 0) || int3 != 0xcc ||
        cpu_memory_rw_debug(env, bp->pc, (uint8_t *)&bp->saved_insn, 1, 1))
        return -EINVAL;
    return 0;
}

#ifdef KVM_CAP_SET_GUEST_DEBUG
static struct {
    target_ulong addr;
    int len;
    int type;
} hw_breakpoint[4];

static int nb_hw_breakpoint;

static int find_hw_breakpoint(target_ulong addr, int len, int type)
{
    int n;

    for (n = 0; n < nb_hw_breakpoint; n++)
	if (hw_breakpoint[n].addr == addr && hw_breakpoint[n].type == type &&
	    (hw_breakpoint[n].len == len || len == -1))
	    return n;
    return -1;
}

int kvm_arch_insert_hw_breakpoint(target_ulong addr,
                                  target_ulong len, int type)
{
    switch (type) {
    case GDB_BREAKPOINT_HW:
	len = 1;
	break;
    case GDB_WATCHPOINT_WRITE:
    case GDB_WATCHPOINT_ACCESS:
	switch (len) {
	case 1:
	    break;
	case 2:
	case 4:
	case 8:
	    if (addr & (len - 1))
		return -EINVAL;
	    break;
	default:
	    return -EINVAL;
	}
	break;
    default:
	return -ENOSYS;
    }

    if (nb_hw_breakpoint == 4)
        return -ENOBUFS;

    if (find_hw_breakpoint(addr, len, type) >= 0)
        return -EEXIST;

    hw_breakpoint[nb_hw_breakpoint].addr = addr;
    hw_breakpoint[nb_hw_breakpoint].len = len;
    hw_breakpoint[nb_hw_breakpoint].type = type;
    nb_hw_breakpoint++;

    return 0;
}

int kvm_arch_remove_hw_breakpoint(target_ulong addr,
                                  target_ulong len, int type)
{
    int n;

    n = find_hw_breakpoint(addr, (type == GDB_BREAKPOINT_HW) ? 1 : len, type);
    if (n < 0)
        return -ENOENT;

    nb_hw_breakpoint--;
    hw_breakpoint[n] = hw_breakpoint[nb_hw_breakpoint];

    return 0;
}

void kvm_arch_remove_all_hw_breakpoints(void)
{
    nb_hw_breakpoint = 0;
}

static CPUWatchpoint hw_watchpoint;

int kvm_arch_debug(struct kvm_debug_exit_arch *arch_info)
{
    int handle = 0;
    int n;

    if (arch_info->exception == 1) {
	if (arch_info->dr6 & (1 << 14)) {
	    if (cpu_single_env->singlestep_enabled)
		handle = 1;
	} else {
	    for (n = 0; n < 4; n++)
		if (arch_info->dr6 & (1 << n))
		    switch ((arch_info->dr7 >> (16 + n*4)) & 0x3) {
		    case 0x0:
			handle = 1;
			break;
		    case 0x1:
			handle = 1;
			cpu_single_env->watchpoint_hit = &hw_watchpoint;
			hw_watchpoint.vaddr = hw_breakpoint[n].addr;
			hw_watchpoint.flags = BP_MEM_WRITE;
			break;
		    case 0x3:
			handle = 1;
			cpu_single_env->watchpoint_hit = &hw_watchpoint;
			hw_watchpoint.vaddr = hw_breakpoint[n].addr;
			hw_watchpoint.flags = BP_MEM_ACCESS;
			break;
		    }
	}
    } else if (kvm_find_sw_breakpoint(cpu_single_env, arch_info->pc))
	handle = 1;

    if (!handle)
	kvm_update_guest_debug(cpu_single_env,
			(arch_info->exception == 1) ?
			KVM_GUESTDBG_INJECT_DB : KVM_GUESTDBG_INJECT_BP);

    return handle;
}

void kvm_arch_update_guest_debug(CPUState *env, struct kvm_guest_debug *dbg)
{
    const uint8_t type_code[] = {
	[GDB_BREAKPOINT_HW] = 0x0,
	[GDB_WATCHPOINT_WRITE] = 0x1,
	[GDB_WATCHPOINT_ACCESS] = 0x3
    };
    const uint8_t len_code[] = {
	[1] = 0x0, [2] = 0x1, [4] = 0x3, [8] = 0x2
    };
    int n;

    if (kvm_sw_breakpoints_active(env))
	dbg->control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_SW_BP;

    if (nb_hw_breakpoint > 0) {
	dbg->control |= KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_USE_HW_BP;
	dbg->arch.debugreg[7] = 0x0600;
	for (n = 0; n < nb_hw_breakpoint; n++) {
	    dbg->arch.debugreg[n] = hw_breakpoint[n].addr;
	    dbg->arch.debugreg[7] |= (2 << (n * 2)) |
		(type_code[hw_breakpoint[n].type] << (16 + n*4)) |
		(len_code[hw_breakpoint[n].len] << (18 + n*4));
	}
    }
}
#endif

void kvm_arch_do_ioperm(void *_data)
{
    struct ioperm_data *data = _data;
    ioperm(data->start_port, data->num, data->turn_on);
}

/*
 * Setup x86 specific IRQ routing
 */
int kvm_arch_init_irq_routing(void)
{
    int i, r;

    if (kvm_irqchip && kvm_has_gsi_routing(kvm_context)) {
        kvm_clear_gsi_routes(kvm_context);
        for (i = 0; i < 8; ++i) {
            if (i == 2)
                continue;
            r = kvm_add_irq_route(kvm_context, i, KVM_IRQCHIP_PIC_MASTER, i);
            if (r < 0)
                return r;
        }
        for (i = 8; i < 16; ++i) {
            r = kvm_add_irq_route(kvm_context, i, KVM_IRQCHIP_PIC_SLAVE, i - 8);
            if (r < 0)
                return r;
        }
        for (i = 0; i < 24; ++i) {
            if (i == 0) {
                r = kvm_add_irq_route(kvm_context, i, KVM_IRQCHIP_IOAPIC, 2);
            } else if (i != 2) {
                r = kvm_add_irq_route(kvm_context, i, KVM_IRQCHIP_IOAPIC, i);
            }
            if (r < 0)
                return r;
        }
        kvm_commit_irq_routes(kvm_context);
    }
    return 0;
}

uint32_t kvm_arch_get_supported_cpuid(CPUState *env, uint32_t function,
                                      int reg)
{
    return kvm_get_supported_cpuid(kvm_context, function, reg);
}

void kvm_arch_process_irqchip_events(CPUState *env)
{
    kvm_arch_save_regs(env);
    if (env->interrupt_request & CPU_INTERRUPT_INIT)
        do_cpu_init(env);
    if (env->interrupt_request & CPU_INTERRUPT_SIPI)
        do_cpu_sipi(env);
    kvm_arch_load_regs(env);
}
