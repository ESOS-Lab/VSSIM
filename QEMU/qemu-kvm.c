/*
 * qemu/kvm integration
 *
 * Copyright (C) 2006-2008 Qumranet Technologies
 *
 * Licensed under the terms of the GNU GPL version 2 or higher.
 */
#include "config.h"
#include "config-host.h"

#include <assert.h>
#include <string.h>
#include "hw/hw.h"
#include "sysemu.h"
#include "qemu-common.h"
#include "console.h"
#include "block.h"
#include "compatfd.h"
#include "gdbstub.h"

#include "qemu-kvm.h"
#include "libkvm.h"

#include <pthread.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>

#define false 0
#define true 1

#define EXPECTED_KVM_API_VERSION 12

#if EXPECTED_KVM_API_VERSION != KVM_API_VERSION
#error libkvm: userspace and kernel version mismatch
#endif

/* Check QEMU overhead */
int64_t qemu_overhead=0;
int64_t _get_usec(void)
{
        int64_t t = 0;
        struct timeval tv;
        struct timezone tz;

        gettimeofday(&tv, &tz);
        t = tv.tv_sec;

	t *= 1000000;
        t += tv.tv_usec;

        return t;
}

int kvm_allowed = 1;
int kvm_irqchip = 1;
int kvm_pit = 1;
int kvm_pit_reinject = 1;
int kvm_nested = 0;


KVMState *kvm_state;
kvm_context_t kvm_context;

pthread_mutex_t qemu_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t qemu_vcpu_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t qemu_system_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t qemu_pause_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t qemu_work_cond = PTHREAD_COND_INITIALIZER;
__thread CPUState *current_env;

static int qemu_system_ready;

#define SIG_IPI (SIGRTMIN+4)

pthread_t io_thread;
static int io_thread_fd = -1;
static int io_thread_sigfd = -1;

static CPUState *kvm_debug_cpu_requested;

static uint64_t phys_ram_size;

/* The list of ioperm_data */
static LIST_HEAD(, ioperm_data) ioperm_head;

//#define DEBUG_MEMREG
#ifdef	DEBUG_MEMREG
#define DPRINTF(fmt, args...) \
	do { fprintf(stderr, "%s:%d " fmt , __func__, __LINE__, ##args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while (0)
#endif

#define ALIGN(x, y) (((x)+(y)-1) & ~((y)-1))

int kvm_abi = EXPECTED_KVM_API_VERSION;
int kvm_page_size;

#ifdef KVM_CAP_SET_GUEST_DEBUG
static int kvm_debug(void *opaque, void *data,
                     struct kvm_debug_exit_arch *arch_info)
{
    int handle = kvm_arch_debug(arch_info);
    CPUState *env = data;

    if (handle) {
	kvm_debug_cpu_requested = env;
	env->stopped = 1;
    }
    return handle;
}
#endif

int kvm_mmio_read(void *opaque, uint64_t addr, uint8_t *data, int len)
{
	cpu_physical_memory_rw(addr, data, len, 0);
	return 0;
}

int kvm_mmio_write(void *opaque, uint64_t addr, uint8_t *data, int len)
{
	cpu_physical_memory_rw(addr, data, len, 1);
	return 0;
}

static int handle_unhandled(uint64_t reason)
{
    fprintf(stderr, "kvm: unhandled exit %"PRIx64"\n", reason);
    return -EINVAL;
}


static inline void set_gsi(kvm_context_t kvm, unsigned int gsi)
{
	uint32_t *bitmap = kvm->used_gsi_bitmap;

	if (gsi < kvm->max_gsi)
		bitmap[gsi / 32] |= 1U << (gsi % 32);
	else
		DPRINTF("Invalid GSI %d\n");
}

static inline void clear_gsi(kvm_context_t kvm, unsigned int gsi)
{
	uint32_t *bitmap = kvm->used_gsi_bitmap;

	if (gsi < kvm->max_gsi)
		bitmap[gsi / 32] &= ~(1U << (gsi % 32));
	else
		DPRINTF("Invalid GSI %d\n");
}

struct slot_info {
	unsigned long phys_addr;
	unsigned long len;
	unsigned long userspace_addr;
	unsigned flags;
	int logging_count;
};

struct slot_info slots[KVM_MAX_NUM_MEM_REGIONS];

static void init_slots(void)
{
	int i;

	for (i = 0; i < KVM_MAX_NUM_MEM_REGIONS; ++i)
		slots[i].len = 0;
}

static int get_free_slot(kvm_context_t kvm)
{
	int i;
	int tss_ext;

#if defined(KVM_CAP_SET_TSS_ADDR) && !defined(__s390__)
	tss_ext = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_TSS_ADDR);
#else
	tss_ext = 0;
#endif

	/*
	 * on older kernels where the set tss ioctl is not supprted we must save
	 * slot 0 to hold the extended memory, as the vmx will use the last 3
	 * pages of this slot.
	 */
	if (tss_ext > 0)
		i = 0;
	else
		i = 1;

	for (; i < KVM_MAX_NUM_MEM_REGIONS; ++i)
		if (!slots[i].len)
			return i;
	return -1;
}

static void register_slot(int slot, unsigned long phys_addr, unsigned long len,
		   unsigned long userspace_addr, unsigned flags)
{
	slots[slot].phys_addr = phys_addr;
	slots[slot].len = len;
	slots[slot].userspace_addr = userspace_addr;
        slots[slot].flags = flags;
}

static void free_slot(int slot)
{
	slots[slot].len = 0;
	slots[slot].logging_count = 0;
}

static int get_slot(unsigned long phys_addr)
{
	int i;

	for (i = 0; i < KVM_MAX_NUM_MEM_REGIONS ; ++i) {
		if (slots[i].len && slots[i].phys_addr <= phys_addr &&
		    (slots[i].phys_addr + slots[i].len-1) >= phys_addr)
			return i;
	}
	return -1;
}

/* Returns -1 if this slot is not totally contained on any other,
 * and the number of the slot otherwise */
static int get_container_slot(uint64_t phys_addr, unsigned long size)
{
	int i;

	for (i = 0; i < KVM_MAX_NUM_MEM_REGIONS ; ++i)
		if (slots[i].len && slots[i].phys_addr <= phys_addr &&
		    (slots[i].phys_addr + slots[i].len) >= phys_addr + size)
			return i;
	return -1;
}

int kvm_is_containing_region(kvm_context_t kvm, unsigned long phys_addr, unsigned long size)
{
	int slot = get_container_slot(phys_addr, size);
	if (slot == -1)
		return 0;
	return 1;
}

/*
 * dirty pages logging control
 */
static int kvm_dirty_pages_log_change(kvm_context_t kvm,
				      unsigned long phys_addr,
				      unsigned flags,
				      unsigned mask)
{
	int r = -1;
	int slot = get_slot(phys_addr);

	if (slot == -1) {
		fprintf(stderr, "BUG: %s: invalid parameters\n", __FUNCTION__);
		return 1;
	}

	flags = (slots[slot].flags & ~mask) | flags;
	if (flags == slots[slot].flags)
		return 0;
	slots[slot].flags = flags;

	{
		struct kvm_userspace_memory_region mem = {
			.slot = slot,
			.memory_size = slots[slot].len,
			.guest_phys_addr = slots[slot].phys_addr,
			.userspace_addr = slots[slot].userspace_addr,
			.flags = slots[slot].flags,
		};


		DPRINTF("slot %d start %llx len %llx flags %x\n",
			mem.slot,
			mem.guest_phys_addr,
			mem.memory_size,
			mem.flags);
		r = kvm_vm_ioctl(kvm_state, KVM_SET_USER_MEMORY_REGION, &mem);
		if (r < 0)
			fprintf(stderr, "%s: %m\n", __FUNCTION__);
	}
	return r;
}

static int kvm_dirty_pages_log_change_all(kvm_context_t kvm,
					  int (*change)(kvm_context_t kvm,
							uint64_t start,
							uint64_t len))
{
	int i, r;

	for (i=r=0; i<KVM_MAX_NUM_MEM_REGIONS && r==0; i++) {
		if (slots[i].len)
			r = change(kvm, slots[i].phys_addr, slots[i].len);
	}
	return r;
}

int kvm_dirty_pages_log_enable_slot(kvm_context_t kvm,
				    uint64_t phys_addr,
				    uint64_t len)
{
	int slot = get_slot(phys_addr);

	DPRINTF("start %"PRIx64" len %"PRIx64"\n", phys_addr, len);
	if (slot == -1) {
		fprintf(stderr, "BUG: %s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (slots[slot].logging_count++)
		return 0;

	return kvm_dirty_pages_log_change(kvm, slots[slot].phys_addr,
					  KVM_MEM_LOG_DIRTY_PAGES,
					  KVM_MEM_LOG_DIRTY_PAGES);
}

int kvm_dirty_pages_log_disable_slot(kvm_context_t kvm,
				     uint64_t phys_addr,
				     uint64_t len)
{
	int slot = get_slot(phys_addr);

	if (slot == -1) {
		fprintf(stderr, "BUG: %s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (--slots[slot].logging_count)
		return 0;

	return kvm_dirty_pages_log_change(kvm, slots[slot].phys_addr,
					  0,
					  KVM_MEM_LOG_DIRTY_PAGES);
}

/**
 * Enable dirty page logging for all memory regions
 */
int kvm_dirty_pages_log_enable_all(kvm_context_t kvm)
{
	if (kvm->dirty_pages_log_all)
		return 0;
	kvm->dirty_pages_log_all = 1;
	return kvm_dirty_pages_log_change_all(kvm,
					      kvm_dirty_pages_log_enable_slot);
}

/**
 * Enable dirty page logging only for memory regions that were created with
 *     dirty logging enabled (disable for all other memory regions).
 */
int kvm_dirty_pages_log_reset(kvm_context_t kvm)
{
	if (!kvm->dirty_pages_log_all)
		return 0;
	kvm->dirty_pages_log_all = 0;
	return kvm_dirty_pages_log_change_all(kvm,
					      kvm_dirty_pages_log_disable_slot);
}


static int kvm_create_context(void);

int kvm_init(int smp_cpus)
{
	int fd;
	int r, gsi_count;


	fd = open("/dev/kvm", O_RDWR);
	if (fd == -1) {
		perror("open /dev/kvm");
		return -1;
	}
	r = ioctl(fd, KVM_GET_API_VERSION, 0);
	if (r == -1) {
	    fprintf(stderr, "kvm kernel version too old: "
		    "KVM_GET_API_VERSION ioctl not supported\n");
	    goto out_close;
	}
	if (r < EXPECTED_KVM_API_VERSION) {
		fprintf(stderr, "kvm kernel version too old: "
			"We expect API version %d or newer, but got "
			"version %d\n",
			EXPECTED_KVM_API_VERSION, r);
	    goto out_close;
	}
	if (r > EXPECTED_KVM_API_VERSION) {
	    fprintf(stderr, "kvm userspace version too old\n");
	    goto out_close;
	}
	kvm_abi = r;
	kvm_page_size = getpagesize();
	kvm_state = qemu_mallocz(sizeof(*kvm_state));
    kvm_context = &kvm_state->kvm_context;

	kvm_state->fd = fd;
	kvm_state->vmfd = -1;
	kvm_context->opaque = cpu_single_env;
	kvm_context->dirty_pages_log_all = 0;
	kvm_context->no_irqchip_creation = 0;
	kvm_context->no_pit_creation = 0;

#ifdef KVM_CAP_SET_GUEST_DEBUG
    TAILQ_INIT(&kvm_state->kvm_sw_breakpoints);
#endif

	gsi_count = kvm_get_gsi_count(kvm_context);
	if (gsi_count > 0) {
		int gsi_bits, i;

		/* Round up so we can search ints using ffs */
		gsi_bits = ALIGN(gsi_count, 32);
		kvm_context->used_gsi_bitmap = qemu_mallocz(gsi_bits / 8);
		kvm_context->max_gsi = gsi_bits;

		/* Mark any over-allocated bits as already in use */
		for (i = gsi_count; i < gsi_bits; i++)
			set_gsi(kvm_context, i);
	}

    pthread_mutex_lock(&qemu_mutex);
    return kvm_create_context();

 out_close:
	close(fd);
	return -1;
}

static void kvm_finalize(KVMState *s)
{
	/* FIXME
	if (kvm->vcpu_fd[0] != -1)
		close(kvm->vcpu_fd[0]);
	if (kvm->vm_fd != -1)
		close(kvm->vm_fd);
	*/
	close(s->fd);
	free(s);
}

void kvm_disable_irqchip_creation(kvm_context_t kvm)
{
	kvm->no_irqchip_creation = 1;
}

void kvm_disable_pit_creation(kvm_context_t kvm)
{
	kvm->no_pit_creation = 1;
}

kvm_vcpu_context_t kvm_create_vcpu(CPUState *env, int id)
{
	long mmap_size;
	int r;
	kvm_vcpu_context_t vcpu_ctx = qemu_malloc(sizeof(struct kvm_vcpu_context));
    kvm_context_t kvm = kvm_context;

	vcpu_ctx->kvm = kvm;
	vcpu_ctx->id = id;

	r = kvm_vm_ioctl(kvm_state, KVM_CREATE_VCPU, id);
	if (r < 0) {
		fprintf(stderr, "kvm_create_vcpu: %m\n");
		goto err;
	}
	vcpu_ctx->fd = r;

    env->kvm_fd = r;
    env->kvm_state = kvm_state;

	mmap_size = kvm_ioctl(kvm_state, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (mmap_size < 0) {
		fprintf(stderr, "get vcpu mmap size: %m\n");
		goto err_fd;
	}
	vcpu_ctx->run = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED,
			      vcpu_ctx->fd, 0);
	if (vcpu_ctx->run == MAP_FAILED) {
		fprintf(stderr, "mmap vcpu area: %m\n");
		goto err_fd;
	}
	return vcpu_ctx;
err_fd:
	close(vcpu_ctx->fd);
err:
	free(vcpu_ctx);
	return NULL;
}

static int kvm_set_boot_vcpu_id(kvm_context_t kvm, uint32_t id)
{
#ifdef KVM_CAP_SET_BOOT_CPU_ID
    int r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SET_BOOT_CPU_ID);
    if (r > 0)
        return kvm_vm_ioctl(kvm_state, KVM_SET_BOOT_CPU_ID, id);
    return -ENOSYS;
#else
    return -ENOSYS;
#endif
}

int kvm_create_vm(kvm_context_t kvm)
{
    int fd;
#ifdef KVM_CAP_IRQ_ROUTING
	kvm->irq_routes = qemu_mallocz(sizeof(*kvm->irq_routes));
	kvm->nr_allocated_irq_routes = 0;
#endif

	fd = kvm_ioctl(kvm_state, KVM_CREATE_VM, 0);
	if (fd < 0) {
		fprintf(stderr, "kvm_create_vm: %m\n");
		return -1;
	}
	kvm_state->vmfd = fd;
	return 0;
}

static int kvm_create_default_phys_mem(kvm_context_t kvm,
				       unsigned long phys_mem_bytes,
				       void **vm_mem)
{
#ifdef KVM_CAP_USER_MEMORY
	int r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY);
	if (r > 0)
		return 0;
	fprintf(stderr, "Hypervisor too old: KVM_CAP_USER_MEMORY extension not supported\n");
#else
#error Hypervisor too old: KVM_CAP_USER_MEMORY extension not supported
#endif
	return -1;
}

void kvm_create_irqchip(kvm_context_t kvm)
{
	int r;

	kvm->irqchip_in_kernel = 0;
#ifdef KVM_CAP_IRQCHIP
	if (!kvm->no_irqchip_creation) {
		r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_IRQCHIP);
		if (r > 0) {	/* kernel irqchip supported */
			r = kvm_vm_ioctl(kvm_state, KVM_CREATE_IRQCHIP);
			if (r >= 0) {
				kvm->irqchip_inject_ioctl = KVM_IRQ_LINE;
#if defined(KVM_CAP_IRQ_INJECT_STATUS) && defined(KVM_IRQ_LINE_STATUS)
				r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION,
			  KVM_CAP_IRQ_INJECT_STATUS);
				if (r > 0)
					kvm->irqchip_inject_ioctl = KVM_IRQ_LINE_STATUS;
#endif
				kvm->irqchip_in_kernel = 1;
			}
			else
				fprintf(stderr, "Create kernel PIC irqchip failed\n");
		}
	}
#endif
}

int kvm_create(kvm_context_t kvm, unsigned long phys_mem_bytes, void **vm_mem)
{
	int r;

	r = kvm_create_vm(kvm);
	if (r < 0)
	        return r;
	r = kvm_arch_create(kvm, phys_mem_bytes, vm_mem);
	if (r < 0)
		return r;
	init_slots();
	r = kvm_create_default_phys_mem(kvm, phys_mem_bytes, vm_mem);
	if (r < 0)
	        return r;
	kvm_create_irqchip(kvm);

	return 0;
}


int kvm_register_phys_mem(kvm_context_t kvm,
			  unsigned long phys_start, void *userspace_addr,
			  unsigned long len, int log)
{

	struct kvm_userspace_memory_region memory = {
		.memory_size = len,
		.guest_phys_addr = phys_start,
		.userspace_addr = (unsigned long)(intptr_t)userspace_addr,
		.flags = log ? KVM_MEM_LOG_DIRTY_PAGES : 0,
	};
	int r;

	memory.slot = get_free_slot(kvm);
	DPRINTF("memory: gpa: %llx, size: %llx, uaddr: %llx, slot: %x, flags: %lx\n",
		memory.guest_phys_addr, memory.memory_size,
		memory.userspace_addr, memory.slot, memory.flags);
	r = kvm_vm_ioctl(kvm_state, KVM_SET_USER_MEMORY_REGION, &memory);
	if (r < 0) {
		fprintf(stderr, "create_userspace_phys_mem: %s\n", strerror(-r));
		return -1;
	}
	register_slot(memory.slot, memory.guest_phys_addr, memory.memory_size,
		      memory.userspace_addr, memory.flags);
        return 0;
}


/* destroy/free a whole slot.
 * phys_start, len and slot are the params passed to kvm_create_phys_mem()
 */
void kvm_destroy_phys_mem(kvm_context_t kvm, unsigned long phys_start,
			  unsigned long len)
{
	int slot;
	int r;
	struct kvm_userspace_memory_region memory = {
		.memory_size = 0,
		.guest_phys_addr = phys_start,
		.userspace_addr = 0,
		.flags = 0,
	};

	slot = get_slot(phys_start);

	if ((slot >= KVM_MAX_NUM_MEM_REGIONS) || (slot == -1)) {
		fprintf(stderr, "BUG: %s: invalid parameters (slot=%d)\n",
			__FUNCTION__, slot);
		return;
	}
	if (phys_start != slots[slot].phys_addr) {
		fprintf(stderr,
			"WARNING: %s: phys_start is 0x%lx expecting 0x%lx\n",
			__FUNCTION__, phys_start, slots[slot].phys_addr);
		phys_start = slots[slot].phys_addr;
	}

	memory.slot = slot;
	DPRINTF("slot %d start %llx len %llx flags %x\n",
		memory.slot,
		memory.guest_phys_addr,
		memory.memory_size,
		memory.flags);
	r = kvm_vm_ioctl(kvm_state, KVM_SET_USER_MEMORY_REGION, &memory);
	if (r < 0) {
		fprintf(stderr, "destroy_userspace_phys_mem: %s",
			strerror(-r));
		return;
	}

	free_slot(memory.slot);
}

void kvm_unregister_memory_area(kvm_context_t kvm, uint64_t phys_addr, unsigned long size)
{

	int slot = get_container_slot(phys_addr, size);

	if (slot != -1) {
		DPRINTF("Unregistering memory region %llx (%lx)\n", phys_addr, size);
		kvm_destroy_phys_mem(kvm, phys_addr, size);
		return;
	}
}

static int kvm_get_map(kvm_context_t kvm, int ioctl_num, int slot, void *buf)
{
	int r;
	struct kvm_dirty_log log = {
		.slot = slot,
	};

	log.dirty_bitmap = buf;

	r = kvm_vm_ioctl(kvm_state, ioctl_num, &log);
	if (r < 0)
		return r;
	return 0;
}

int kvm_get_dirty_pages(kvm_context_t kvm, unsigned long phys_addr, void *buf)
{
	int slot;

	slot = get_slot(phys_addr);
	return kvm_get_map(kvm, KVM_GET_DIRTY_LOG, slot, buf);
}

int kvm_get_dirty_pages_range(kvm_context_t kvm, unsigned long phys_addr,
			      unsigned long len, void *opaque,
			      int (*cb)(unsigned long start, unsigned long len,
					void*bitmap, void *opaque))
{
	int i;
	int r;
	unsigned long end_addr = phys_addr + len;
        void *buf;

	for (i = 0; i < KVM_MAX_NUM_MEM_REGIONS; ++i) {
		if ((slots[i].len && (uint64_t)slots[i].phys_addr >= phys_addr)
		    && ((uint64_t)slots[i].phys_addr + slots[i].len  <= end_addr)) {
			buf = qemu_malloc((slots[i].len / 4096 + 7) / 8 + 2);
			r = kvm_get_map(kvm, KVM_GET_DIRTY_LOG, i, buf);
			if (r) {
				qemu_free(buf);
				return r;
			}
			r = cb(slots[i].phys_addr, slots[i].len, buf, opaque);
			qemu_free(buf);
			if (r)
				return r;
		}
	}
	return 0;
}

#ifdef KVM_CAP_IRQCHIP

int kvm_set_irq_level(kvm_context_t kvm, int irq, int level, int *status)
{
	struct kvm_irq_level event;
	int r;

	if (!kvm->irqchip_in_kernel)
		return 0;
	event.level = level;
	event.irq = irq;
	r = kvm_vm_ioctl(kvm_state, kvm->irqchip_inject_ioctl, &event);
	if (r < 0)
		perror("kvm_set_irq_level");

	if (status) {
#ifdef KVM_CAP_IRQ_INJECT_STATUS
		*status = (kvm->irqchip_inject_ioctl == KVM_IRQ_LINE) ?
			1 : event.status;
#else
		*status = 1;
#endif
	}

	return 1;
}

int kvm_get_irqchip(kvm_context_t kvm, struct kvm_irqchip *chip)
{
	int r;

	if (!kvm->irqchip_in_kernel)
		return 0;
	r = kvm_vm_ioctl(kvm_state, KVM_GET_IRQCHIP, chip);
	if (r < 0) {
		perror("kvm_get_irqchip\n");
	}
	return r;
}

int kvm_set_irqchip(kvm_context_t kvm, struct kvm_irqchip *chip)
{
	int r;

	if (!kvm->irqchip_in_kernel)
		return 0;
	r = kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, chip);
	if (r < 0) {
		perror("kvm_set_irqchip\n");
	}
	return r;
}

#endif

static int handle_io(kvm_vcpu_context_t vcpu)
{
	struct kvm_run *run = vcpu->run;
	kvm_context_t kvm = vcpu->kvm;
	uint16_t addr = run->io.port;
	int i;
	void *p = (void *)run + run->io.data_offset;
	for (i = 0; i < run->io.count; ++i) {
		switch (run->io.direction) {
		case KVM_EXIT_IO_IN:
			switch (run->io.size) {
			case 1:
				*(uint8_t *)p = cpu_inb(kvm->opaque, addr);
				break;
			case 2:
				*(uint16_t *)p = cpu_inw(kvm->opaque, addr);
				break;
			case 4:
				*(uint32_t *)p = cpu_inl(kvm->opaque, addr);
				break;
			default:
				fprintf(stderr, "bad I/O size %d\n", run->io.size);
				return -EMSGSIZE;
			}
			break;
		case KVM_EXIT_IO_OUT:
			switch (run->io.size) {
			case 1:
				cpu_outb(kvm->opaque, addr, *(uint8_t *)p);
				break;
			case 2:
				cpu_outw(kvm->opaque, addr, *(uint16_t *)p);
				break;
			case 4:
				cpu_outl(kvm->opaque, addr, *(uint32_t *)p);
				break;
			default:
				fprintf(stderr, "bad I/O size %d\n", run->io.size);
				return -EMSGSIZE;
			}
			break;
		default:
			fprintf(stderr, "bad I/O direction %d\n", run->io.direction);
			return -EPROTO;
		}

		p += run->io.size;
	}

	return 0;
}

int handle_debug(kvm_vcpu_context_t vcpu, void *env)
{
#ifdef KVM_CAP_SET_GUEST_DEBUG
    struct kvm_run *run = vcpu->run;
    kvm_context_t kvm = vcpu->kvm;

    return kvm_debug(kvm->opaque, env, &run->debug.arch);
#else
    return 0;
#endif
}

int kvm_get_regs(kvm_vcpu_context_t vcpu, struct kvm_regs *regs)
{
    return ioctl(vcpu->fd, KVM_GET_REGS, regs);
}

int kvm_set_regs(kvm_vcpu_context_t vcpu, struct kvm_regs *regs)
{
    return ioctl(vcpu->fd, KVM_SET_REGS, regs);
}

int kvm_get_fpu(kvm_vcpu_context_t vcpu, struct kvm_fpu *fpu)
{
    return ioctl(vcpu->fd, KVM_GET_FPU, fpu);
}

int kvm_set_fpu(kvm_vcpu_context_t vcpu, struct kvm_fpu *fpu)
{
    return ioctl(vcpu->fd, KVM_SET_FPU, fpu);
}

int kvm_get_sregs(kvm_vcpu_context_t vcpu, struct kvm_sregs *sregs)
{
    return ioctl(vcpu->fd, KVM_GET_SREGS, sregs);
}

int kvm_set_sregs(kvm_vcpu_context_t vcpu, struct kvm_sregs *sregs)
{
    return ioctl(vcpu->fd, KVM_SET_SREGS, sregs);
}

#ifdef KVM_CAP_MP_STATE
int kvm_get_mpstate(kvm_vcpu_context_t vcpu, struct kvm_mp_state *mp_state)
{
    int r;

    r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_MP_STATE);
    if (r > 0)
        return ioctl(vcpu->fd, KVM_GET_MP_STATE, mp_state);
    return -ENOSYS;
}

int kvm_set_mpstate(kvm_vcpu_context_t vcpu, struct kvm_mp_state *mp_state)
{
    int r;

    r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_MP_STATE);
    if (r > 0)
        return ioctl(vcpu->fd, KVM_SET_MP_STATE, mp_state);
    return -ENOSYS;
}
#endif

static int handle_mmio(kvm_vcpu_context_t vcpu)
{
	unsigned long addr = vcpu->run->mmio.phys_addr;
	kvm_context_t kvm = vcpu->kvm;
	struct kvm_run *kvm_run = vcpu->run;
	void *data = kvm_run->mmio.data;

	/* hack: Red Hat 7.1 generates these weird accesses. */
	if ((addr > 0xa0000-4 && addr <= 0xa0000) && kvm_run->mmio.len == 3)
	    return 0;

	if (kvm_run->mmio.is_write)
		return kvm_mmio_write(kvm->opaque, addr, data,
					kvm_run->mmio.len);
	else
		return kvm_mmio_read(kvm->opaque, addr, data,
					kvm_run->mmio.len);
}

int handle_io_window(kvm_context_t kvm)
{
    return 1;
}

int handle_halt(kvm_vcpu_context_t vcpu)
{
	return kvm_arch_halt(vcpu->kvm->opaque, vcpu);
}

int handle_shutdown(kvm_context_t kvm, CPUState *env)
{
    /* stop the current vcpu from going back to guest mode */
    env->stopped = 1;

    qemu_system_reset_request();
    return 1;
}

static inline void push_nmi(kvm_context_t kvm)
{
#ifdef KVM_CAP_USER_NMI
	kvm_arch_push_nmi(kvm->opaque);
#endif /* KVM_CAP_USER_NMI */
}

void post_kvm_run(kvm_context_t kvm, CPUState *env)
{
    pthread_mutex_lock(&qemu_mutex);
    kvm_arch_post_kvm_run(kvm->opaque, env);
}

int pre_kvm_run(kvm_context_t kvm, CPUState *env)
{
    kvm_arch_pre_kvm_run(kvm->opaque, env);

    pthread_mutex_unlock(&qemu_mutex);
    return 0;
}

int kvm_get_interrupt_flag(kvm_vcpu_context_t vcpu)
{
	return vcpu->run->if_flag;
}

int kvm_is_ready_for_interrupt_injection(kvm_vcpu_context_t vcpu)
{
	return vcpu->run->ready_for_interrupt_injection;
}

int kvm_run(kvm_vcpu_context_t vcpu, void *env)
{
	int r;
	int fd = vcpu->fd;
	struct kvm_run *run = vcpu->run;
	kvm_context_t kvm = vcpu->kvm;

	/* QEMU Overhead _ JS */
	int64_t start_temp, end_temp;

again:
	push_nmi(kvm);
#if !defined(__s390__)
	if (!kvm->irqchip_in_kernel)
		run->request_interrupt_window = kvm_arch_try_push_interrupts(env);
#endif
	r = pre_kvm_run(kvm, env);
	if (r)
	    return r;
	/* QEMU Overhead Check Start _ JS */
	start_temp = _get_usec();

	r = ioctl(fd, KVM_RUN, 0);

	if (r == -1 && errno != EINTR && errno != EAGAIN) {
		r = -errno;
		post_kvm_run(kvm, env);
		fprintf(stderr, "kvm_run: %s\n", strerror(-r));
		return r;
	}

	post_kvm_run(kvm, env);

#if defined(KVM_CAP_COALESCED_MMIO)
	if (kvm->coalesced_mmio) {
	        struct kvm_coalesced_mmio_ring *ring = (void *)run +
						kvm->coalesced_mmio * PAGE_SIZE;
		while (ring->first != ring->last) {
			kvm_mmio_write(kvm->opaque,
				 ring->coalesced_mmio[ring->first].phys_addr,
				&ring->coalesced_mmio[ring->first].data[0],
				 ring->coalesced_mmio[ring->first].len);
			smp_wmb();
			ring->first = (ring->first + 1) %
							KVM_COALESCED_MMIO_MAX;
		}
	}
#endif
	/* QEMU Overhead Check End _ JS */
	end_temp = _get_usec();
	if( run->exit_reason == KVM_EXIT_IO && ((end_temp - start_temp) > qemu_overhead || qemu_overhead == 0)){
		qemu_overhead = end_temp - start_temp;	
	}
/*	if( run->exit_reason == KVM_EXIT_IO && ((200 + end_temp - start_temp) > qemu_overhead || qemu_overhead == 0)){
		qemu_overhead = 200 + end_temp - start_temp;	
	}
*/
#if !defined(__s390__)
	if (r == -1) {
		r = handle_io_window(kvm);
		goto more;
	}
#endif
	if (1) {
		switch (run->exit_reason) {
		case KVM_EXIT_UNKNOWN:
			r = handle_unhandled(run->hw.hardware_exit_reason);
			break;
		case KVM_EXIT_FAIL_ENTRY:
			r = handle_unhandled(run->fail_entry.hardware_entry_failure_reason);
			break;
		case KVM_EXIT_EXCEPTION:
			fprintf(stderr, "exception %d (%x)\n",
			       run->ex.exception,
			       run->ex.error_code);
			kvm_show_regs(vcpu);
			kvm_show_code(vcpu);
			abort();
			break;
		case KVM_EXIT_IO:
			r = handle_io(vcpu);
			break;
		case KVM_EXIT_DEBUG:
			r = handle_debug(vcpu, env);
			break;
		case KVM_EXIT_MMIO:
			r = handle_mmio(vcpu);
			break;
		case KVM_EXIT_HLT:
			r = handle_halt(vcpu);
			break;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			break;
		case KVM_EXIT_SHUTDOWN:
			r = handle_shutdown(kvm, env);
			break;
#if defined(__s390__)
		case KVM_EXIT_S390_SIEIC:
			r = kvm_s390_handle_intercept(kvm, vcpu,
				run);
			break;
		case KVM_EXIT_S390_RESET:
			r = kvm_s390_handle_reset(kvm, vcpu, run);
			break;
#endif
		default:
			if (kvm_arch_run(vcpu)) {
				fprintf(stderr, "unhandled vm exit: 0x%x\n",
							run->exit_reason);
				kvm_show_regs(vcpu);
				abort();
			}
			break;
		}
	}

	/* reset qemu-overhead _ JS */
//TEMPS
//	if(qemu_overhead > 1000000)
//		qemu_overhead = 0;
//TEMPe
more:
	if (!r)
		goto again;
	return r;
}

int kvm_inject_irq(kvm_vcpu_context_t vcpu, unsigned irq)
{
	struct kvm_interrupt intr;

	intr.irq = irq;
	return ioctl(vcpu->fd, KVM_INTERRUPT, &intr);
}

#ifdef KVM_CAP_SET_GUEST_DEBUG
int kvm_set_guest_debug(kvm_vcpu_context_t vcpu, struct kvm_guest_debug *dbg)
{
	return ioctl(vcpu->fd, KVM_SET_GUEST_DEBUG, dbg);
}
#endif

int kvm_set_signal_mask(kvm_vcpu_context_t vcpu, const sigset_t *sigset)
{
	struct kvm_signal_mask *sigmask;
	int r;

	if (!sigset) {
		r = ioctl(vcpu->fd, KVM_SET_SIGNAL_MASK, NULL);
		if (r == -1)
			r = -errno;
		return r;
	}
	sigmask = qemu_malloc(sizeof(*sigmask) + sizeof(*sigset));

	sigmask->len = 8;
	memcpy(sigmask->sigset, sigset, sizeof(*sigset));
	r = ioctl(vcpu->fd, KVM_SET_SIGNAL_MASK, sigmask);
	if (r == -1)
		r = -errno;
	free(sigmask);
	return r;
}

int kvm_irqchip_in_kernel(kvm_context_t kvm)
{
	return kvm->irqchip_in_kernel;
}

int kvm_pit_in_kernel(kvm_context_t kvm)
{
	return kvm->pit_in_kernel;
}

int kvm_has_sync_mmu(void)
{
        int r = 0;
#ifdef KVM_CAP_SYNC_MMU
        r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_SYNC_MMU);
#endif
        return r;
}

int kvm_inject_nmi(kvm_vcpu_context_t vcpu)
{
#ifdef KVM_CAP_USER_NMI
	return ioctl(vcpu->fd, KVM_NMI);
#else
	return -ENOSYS;
#endif
}

int kvm_init_coalesced_mmio(kvm_context_t kvm)
{
	int r = 0;
	kvm->coalesced_mmio = 0;
#ifdef KVM_CAP_COALESCED_MMIO
	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_COALESCED_MMIO);
	if (r > 0) {
		kvm->coalesced_mmio = r;
		return 0;
	}
#endif
	return r;
}

int kvm_coalesce_mmio_region(target_phys_addr_t addr, ram_addr_t size)
{
#ifdef KVM_CAP_COALESCED_MMIO
	kvm_context_t kvm = kvm_context;
	struct kvm_coalesced_mmio_zone zone;
	int r;

	if (kvm->coalesced_mmio) {

		zone.addr = addr;
		zone.size = size;

		r = kvm_vm_ioctl(kvm_state, KVM_REGISTER_COALESCED_MMIO, &zone);
		if (r < 0) {
			perror("kvm_register_coalesced_mmio_zone");
			return r;
		}
		return 0;
	}
#endif
	return -ENOSYS;
}

int kvm_uncoalesce_mmio_region(target_phys_addr_t addr, ram_addr_t size)
{
#ifdef KVM_CAP_COALESCED_MMIO
	kvm_context_t kvm = kvm_context;
	struct kvm_coalesced_mmio_zone zone;
	int r;

	if (kvm->coalesced_mmio) {

		zone.addr = addr;
		zone.size = size;

		r = kvm_vm_ioctl(kvm_state, KVM_UNREGISTER_COALESCED_MMIO, &zone);
		if (r < 0) {
			perror("kvm_unregister_coalesced_mmio_zone");
			return r;
		}
		DPRINTF("Unregistered coalesced mmio region for %llx (%lx)\n", addr, size);
		return 0;
	}
#endif
	return -ENOSYS;
}

#ifdef KVM_CAP_DEVICE_ASSIGNMENT
int kvm_assign_pci_device(kvm_context_t kvm,
			  struct kvm_assigned_pci_dev *assigned_dev)
{
	return kvm_vm_ioctl(kvm_state, KVM_ASSIGN_PCI_DEVICE, assigned_dev);
}

static int kvm_old_assign_irq(kvm_context_t kvm,
		   struct kvm_assigned_irq *assigned_irq)
{
	return kvm_vm_ioctl(kvm_state, KVM_ASSIGN_IRQ, assigned_irq);
}

#ifdef KVM_CAP_ASSIGN_DEV_IRQ
int kvm_assign_irq(kvm_context_t kvm,
		   struct kvm_assigned_irq *assigned_irq)
{
	int ret;

	ret = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_ASSIGN_DEV_IRQ);
	if (ret > 0) {
		return kvm_vm_ioctl(kvm_state, KVM_ASSIGN_DEV_IRQ, assigned_irq);
	}

	return kvm_old_assign_irq(kvm, assigned_irq);
}

int kvm_deassign_irq(kvm_context_t kvm,
		     struct kvm_assigned_irq *assigned_irq)
{
	return kvm_vm_ioctl(kvm_state, KVM_DEASSIGN_DEV_IRQ, assigned_irq);
}
#else
int kvm_assign_irq(kvm_context_t kvm,
		   struct kvm_assigned_irq *assigned_irq)
{
	return kvm_old_assign_irq(kvm, assigned_irq);
}
#endif
#endif

#ifdef KVM_CAP_DEVICE_DEASSIGNMENT
int kvm_deassign_pci_device(kvm_context_t kvm,
			    struct kvm_assigned_pci_dev *assigned_dev)
{
	return kvm_vm_ioctl(kvm_state, KVM_DEASSIGN_PCI_DEVICE, assigned_dev);
}
#endif

int kvm_destroy_memory_region_works(kvm_context_t kvm)
{
	int ret = 0;

#ifdef KVM_CAP_DESTROY_MEMORY_REGION_WORKS
	ret = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION,
		    KVM_CAP_DESTROY_MEMORY_REGION_WORKS);
	if (ret <= 0)
		ret = 0;
#endif
	return ret;
}

int kvm_reinject_control(kvm_context_t kvm, int pit_reinject)
{
#ifdef KVM_CAP_REINJECT_CONTROL
	int r;
	struct kvm_reinject_control control;

	control.pit_reinject = pit_reinject;

	r = kvm_ioctl(kvm_state, KVM_CHECK_EXTENSION, KVM_CAP_REINJECT_CONTROL);
	if (r > 0) {
		return  kvm_vm_ioctl(kvm_state, KVM_REINJECT_CONTROL, &control);
	}
#endif
	return -ENOSYS;
}

int kvm_has_gsi_routing(kvm_context_t kvm)
{
    int r = 0;

#ifdef KVM_CAP_IRQ_ROUTING
    r = kvm_check_extension(kvm_state, KVM_CAP_IRQ_ROUTING);
#endif
    return r;
}

int kvm_get_gsi_count(kvm_context_t kvm)
{
#ifdef KVM_CAP_IRQ_ROUTING
	return kvm_check_extension(kvm_state, KVM_CAP_IRQ_ROUTING);
#else
	return -EINVAL;
#endif
}

int kvm_clear_gsi_routes(kvm_context_t kvm)
{
#ifdef KVM_CAP_IRQ_ROUTING
	kvm->irq_routes->nr = 0;
	return 0;
#else
	return -EINVAL;
#endif
}

int kvm_add_routing_entry(kvm_context_t kvm,
		          struct kvm_irq_routing_entry* entry)
{
#ifdef KVM_CAP_IRQ_ROUTING
	struct kvm_irq_routing *z;
	struct kvm_irq_routing_entry *new;
	int n, size;

	if (kvm->irq_routes->nr == kvm->nr_allocated_irq_routes) {
		n = kvm->nr_allocated_irq_routes * 2;
		if (n < 64)
			n = 64;
		size = sizeof(struct kvm_irq_routing);
		size += n * sizeof(*new);
		z = realloc(kvm->irq_routes, size);
		if (!z)
			return -ENOMEM;
		kvm->nr_allocated_irq_routes = n;
		kvm->irq_routes = z;
	}
	n = kvm->irq_routes->nr++;
	new = &kvm->irq_routes->entries[n];
	memset(new, 0, sizeof(*new));
	new->gsi = entry->gsi;
	new->type = entry->type;
	new->flags = entry->flags;
	new->u = entry->u;

	set_gsi(kvm, entry->gsi);

	return 0;
#else
	return -ENOSYS;
#endif
}

int kvm_add_irq_route(kvm_context_t kvm, int gsi, int irqchip, int pin)
{
#ifdef KVM_CAP_IRQ_ROUTING
	struct kvm_irq_routing_entry e;

	e.gsi = gsi;
	e.type = KVM_IRQ_ROUTING_IRQCHIP;
	e.flags = 0;
	e.u.irqchip.irqchip = irqchip;
	e.u.irqchip.pin = pin;
	return kvm_add_routing_entry(kvm, &e);
#else
	return -ENOSYS;
#endif
}

int kvm_del_routing_entry(kvm_context_t kvm,
	                  struct kvm_irq_routing_entry* entry)
{
#ifdef KVM_CAP_IRQ_ROUTING
	struct kvm_irq_routing_entry *e, *p;
	int i, gsi, found = 0;

	gsi = entry->gsi;

	for (i = 0; i < kvm->irq_routes->nr; ++i) {
		e = &kvm->irq_routes->entries[i];
		if (e->type == entry->type
		    && e->gsi == gsi) {
			switch (e->type)
			{
			case KVM_IRQ_ROUTING_IRQCHIP: {
				if (e->u.irqchip.irqchip ==
				    entry->u.irqchip.irqchip
				    && e->u.irqchip.pin ==
				    entry->u.irqchip.pin) {
					p = &kvm->irq_routes->
					    entries[--kvm->irq_routes->nr];
					*e = *p;
					found = 1;
				}
				break;
			}
			case KVM_IRQ_ROUTING_MSI: {
				if (e->u.msi.address_lo ==
				    entry->u.msi.address_lo
				    && e->u.msi.address_hi ==
				    entry->u.msi.address_hi
				    && e->u.msi.data == entry->u.msi.data) {
					p = &kvm->irq_routes->
					    entries[--kvm->irq_routes->nr];
					*e = *p;
					found = 1;
				}
				break;
			}
			default:
				break;
			}
			if (found) {
				/* If there are no other users of this GSI
				 * mark it available in the bitmap */
				for (i = 0; i < kvm->irq_routes->nr; i++) {
					e = &kvm->irq_routes->entries[i];
					if (e->gsi == gsi)
						break;
				}
				if (i == kvm->irq_routes->nr)
					clear_gsi(kvm, gsi);

				return 0;
			}
		}
	}
	return -ESRCH;
#else
	return -ENOSYS;
#endif
}

int kvm_update_routing_entry(kvm_context_t kvm,
	                  struct kvm_irq_routing_entry* entry,
                          struct kvm_irq_routing_entry* newentry)
{
#ifdef KVM_CAP_IRQ_ROUTING
    struct kvm_irq_routing_entry *e;
    int i;

    if (entry->gsi != newentry->gsi ||
        entry->type != newentry->type) {
        return -EINVAL;
    }

    for (i = 0; i < kvm->irq_routes->nr; ++i) {
        e = &kvm->irq_routes->entries[i];
        if (e->type != entry->type || e->gsi != entry->gsi) {
            continue;
        }
        switch (e->type) {
        case KVM_IRQ_ROUTING_IRQCHIP:
            if (e->u.irqchip.irqchip == entry->u.irqchip.irqchip &&
                e->u.irqchip.pin == entry->u.irqchip.pin) {
                memcpy(&e->u.irqchip, &newentry->u.irqchip, sizeof e->u.irqchip);
                return 0;
            }
            break;
        case KVM_IRQ_ROUTING_MSI:
            if (e->u.msi.address_lo == entry->u.msi.address_lo &&
                e->u.msi.address_hi == entry->u.msi.address_hi &&
                e->u.msi.data == entry->u.msi.data) {
                memcpy(&e->u.msi, &newentry->u.msi, sizeof e->u.msi);
                return 0;
            }
            break;
        default:
            break;
        }
    }
    return -ESRCH;
#else
    return -ENOSYS;
#endif
}

int kvm_del_irq_route(kvm_context_t kvm, int gsi, int irqchip, int pin)
{
#ifdef KVM_CAP_IRQ_ROUTING
	struct kvm_irq_routing_entry e;

	e.gsi = gsi;
	e.type = KVM_IRQ_ROUTING_IRQCHIP;
	e.flags = 0;
	e.u.irqchip.irqchip = irqchip;
	e.u.irqchip.pin = pin;
	return kvm_del_routing_entry(kvm, &e);
#else
	return -ENOSYS;
#endif
}

int kvm_commit_irq_routes(kvm_context_t kvm)
{
#ifdef KVM_CAP_IRQ_ROUTING
	kvm->irq_routes->flags = 0;
	return kvm_vm_ioctl(kvm_state, KVM_SET_GSI_ROUTING, kvm->irq_routes);
#else
	return -ENOSYS;
#endif
}

int kvm_get_irq_route_gsi(kvm_context_t kvm)
{
	int i, bit;
	uint32_t *buf = kvm->used_gsi_bitmap;

	/* Return the lowest unused GSI in the bitmap */
	for (i = 0; i < kvm->max_gsi / 32; i++) {
		bit = ffs(~buf[i]);
		if (!bit)
			continue;

		return bit - 1 + i * 32;
	}

	return -ENOSPC;
}

#ifdef KVM_CAP_DEVICE_MSIX
int kvm_assign_set_msix_nr(kvm_context_t kvm,
                           struct kvm_assigned_msix_nr *msix_nr)
{
    return kvm_vm_ioctl(kvm_state, KVM_ASSIGN_SET_MSIX_NR, msix_nr);
}

int kvm_assign_set_msix_entry(kvm_context_t kvm,
                              struct kvm_assigned_msix_entry *entry)
{
    return kvm_vm_ioctl(kvm_state, KVM_ASSIGN_SET_MSIX_ENTRY, entry);
}
#endif

#if defined(KVM_CAP_IRQFD) && defined(CONFIG_eventfd)

#include <sys/eventfd.h>

static int _kvm_irqfd(kvm_context_t kvm, int fd, int gsi, int flags)
{
	struct kvm_irqfd data = {
		.fd    = fd,
		.gsi   = gsi,
		.flags = flags,
	};

	return kvm_vm_ioctl(kvm_state, KVM_IRQFD, &data);
}

int kvm_irqfd(kvm_context_t kvm, int gsi, int flags)
{
	int r;
	int fd;

	if (!kvm_check_extension(kvm_state, KVM_CAP_IRQFD))
		return -ENOENT;

	fd = eventfd(0, 0);
	if (fd < 0)
		return -errno;

	r = _kvm_irqfd(kvm, fd, gsi, 0);
	if (r < 0) {
		close(fd);
		return -errno;
	}

	return fd;
}

#else /* KVM_CAP_IRQFD */

int kvm_irqfd(kvm_context_t kvm, int gsi, int flags)
{
	return -ENOSYS;
}

#endif /* KVM_CAP_IRQFD */
static inline unsigned long kvm_get_thread_id(void)
{
    return syscall(SYS_gettid);
}

static void qemu_cond_wait(pthread_cond_t *cond)
{
    CPUState *env = cpu_single_env;
    static const struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = 100000,
    };

    pthread_cond_timedwait(cond, &qemu_mutex, &ts);
    cpu_single_env = env;
}

static void sig_ipi_handler(int n)
{
}

static void on_vcpu(CPUState *env, void (*func)(void *data), void *data)
{
    struct qemu_work_item wi;

    if (env == current_env) {
        func(data);
        return;
    }

    wi.func = func;
    wi.data = data;
    if (!env->kvm_cpu_state.queued_work_first)
        env->kvm_cpu_state.queued_work_first = &wi;
    else
        env->kvm_cpu_state.queued_work_last->next = &wi;
    env->kvm_cpu_state.queued_work_last = &wi;
    wi.next = NULL;
    wi.done = false;

    pthread_kill(env->kvm_cpu_state.thread, SIG_IPI);
    while (!wi.done)
        qemu_cond_wait(&qemu_work_cond);
}

static void inject_interrupt(void *data)
{
    cpu_interrupt(current_env, (long)data);
}

void kvm_inject_interrupt(CPUState *env, int mask)
{
    on_vcpu(env, inject_interrupt, (void *)(long)mask);
}

void kvm_update_interrupt_request(CPUState *env)
{
    int signal = 0;

    if (env) {
        if (!current_env || !current_env->created)
            signal = 1;
        /*
         * Testing for created here is really redundant
         */
        if (current_env && current_env->created &&
            env != current_env && !env->kvm_cpu_state.signalled)
            signal = 1;

        if (signal) {
            env->kvm_cpu_state.signalled = 1;
            if (env->kvm_cpu_state.thread)
                pthread_kill(env->kvm_cpu_state.thread, SIG_IPI);
        }
    }
}

static void kvm_do_load_registers(void *_env)
{
    CPUState *env = _env;

    kvm_arch_load_regs(env);
}

void kvm_load_registers(CPUState *env)
{
    if (kvm_enabled() && qemu_system_ready)
        on_vcpu(env, kvm_do_load_registers, env);
}

static void kvm_do_save_registers(void *_env)
{
    CPUState *env = _env;

    kvm_arch_save_regs(env);
}

void kvm_save_registers(CPUState *env)
{
    if (kvm_enabled())
        on_vcpu(env, kvm_do_save_registers, env);
}

static void kvm_do_load_mpstate(void *_env)
{
    CPUState *env = _env;

    kvm_arch_load_mpstate(env);
}

void kvm_load_mpstate(CPUState *env)
{
    if (kvm_enabled() && qemu_system_ready)
        on_vcpu(env, kvm_do_load_mpstate, env);
}

static void kvm_do_save_mpstate(void *_env)
{
    CPUState *env = _env;

    kvm_arch_save_mpstate(env);
    env->halted = (env->mp_state == KVM_MP_STATE_HALTED);
}

void kvm_save_mpstate(CPUState *env)
{
    if (kvm_enabled())
        on_vcpu(env, kvm_do_save_mpstate, env);
}

int kvm_cpu_exec(CPUState *env)
{
    int r;

    r = kvm_run(env->kvm_cpu_state.vcpu_ctx, env);
    if (r < 0) {
        printf("kvm_run returned %d\n", r);
        vm_stop(0);
    }

    return 0;
}

static int is_cpu_stopped(CPUState *env)
{
    return !vm_running || env->stopped;
}

static void flush_queued_work(CPUState *env)
{
    struct qemu_work_item *wi;

    if (!env->kvm_cpu_state.queued_work_first)
        return;

    while ((wi = env->kvm_cpu_state.queued_work_first)) {
        env->kvm_cpu_state.queued_work_first = wi->next;
        wi->func(wi->data);
        wi->done = true;
    }
    env->kvm_cpu_state.queued_work_last = NULL;
    pthread_cond_broadcast(&qemu_work_cond);
}

static void kvm_main_loop_wait(CPUState *env, int timeout)
{
    struct timespec ts;
    int r, e;
    siginfo_t siginfo;
    sigset_t waitset;

    pthread_mutex_unlock(&qemu_mutex);

    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;
    sigemptyset(&waitset);
    sigaddset(&waitset, SIG_IPI);

    r = sigtimedwait(&waitset, &siginfo, &ts);
    e = errno;

    pthread_mutex_lock(&qemu_mutex);

    if (r == -1 && !(e == EAGAIN || e == EINTR)) {
	printf("sigtimedwait: %s\n", strerror(e));
	exit(1);
    }

    cpu_single_env = env;
    flush_queued_work(env);

    if (env->stop) {
	env->stop = 0;
	env->stopped = 1;
	pthread_cond_signal(&qemu_pause_cond);
    }

    env->kvm_cpu_state.signalled = 0;
}

static int all_threads_paused(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        if (penv->stop)
            return 0;
        penv = (CPUState *)penv->next_cpu;
    }

    return 1;
}

static void pause_all_threads(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        if (penv != cpu_single_env) {
            penv->stop = 1;
            pthread_kill(penv->kvm_cpu_state.thread, SIG_IPI);
        } else {
            penv->stop = 0;
            penv->stopped = 1;
            cpu_exit(penv);
        }
        penv = (CPUState *)penv->next_cpu;
    }

    while (!all_threads_paused())
	qemu_cond_wait(&qemu_pause_cond);
}

static void resume_all_threads(void)
{
    CPUState *penv = first_cpu;

    assert(!cpu_single_env);

    while (penv) {
        penv->stop = 0;
        penv->stopped = 0;
        pthread_kill(penv->kvm_cpu_state.thread, SIG_IPI);
        penv = (CPUState *)penv->next_cpu;
    }
}

static void kvm_vm_state_change_handler(void *context, int running, int reason)
{
    if (running)
	resume_all_threads();
    else
	pause_all_threads();
}

static void setup_kernel_sigmask(CPUState *env)
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);

    sigprocmask(SIG_BLOCK, NULL, &set);
    sigdelset(&set, SIG_IPI);
    
    kvm_set_signal_mask(env->kvm_cpu_state.vcpu_ctx, &set);
}

static void qemu_kvm_system_reset(void)
{
    CPUState *penv = first_cpu;

    pause_all_threads();

    qemu_system_reset();

    while (penv) {
        kvm_arch_cpu_reset(penv);
        penv = (CPUState *)penv->next_cpu;
    }

    resume_all_threads();
}

static void process_irqchip_events(CPUState *env)
{
    kvm_arch_process_irqchip_events(env);
    if (kvm_arch_has_work(env))
        env->halted = 0;
}

static int kvm_main_loop_cpu(CPUState *env)
{
    setup_kernel_sigmask(env);

    pthread_mutex_lock(&qemu_mutex);

    kvm_qemu_init_env(env);
#ifdef TARGET_I386
    kvm_tpr_vcpu_start(env);
#endif

    cpu_single_env = env;
    kvm_arch_load_regs(env);

    while (1) {
        int run_cpu = !is_cpu_stopped(env);
        if (run_cpu && !kvm_irqchip_in_kernel(kvm_context)) {
            process_irqchip_events(env);
            run_cpu = !env->halted;
        }
        if (run_cpu) {
            kvm_main_loop_wait(env, 0);
            kvm_cpu_exec(env);
	} else {
            kvm_main_loop_wait(env, 1000);
	}
    }
    pthread_mutex_unlock(&qemu_mutex);
    return 0;
}

static void *ap_main_loop(void *_env)
{
    CPUState *env = _env;
    sigset_t signals;
    struct ioperm_data *data = NULL;

    current_env = env;
    env->thread_id = kvm_get_thread_id();
    sigfillset(&signals);
    sigprocmask(SIG_BLOCK, &signals, NULL);
    env->kvm_cpu_state.vcpu_ctx = kvm_create_vcpu(env, env->cpu_index);

#ifdef USE_KVM_DEVICE_ASSIGNMENT
    /* do ioperm for io ports of assigned devices */
    LIST_FOREACH(data, &ioperm_head, entries)
	on_vcpu(env, kvm_arch_do_ioperm, data);
#endif

    /* signal VCPU creation */
    pthread_mutex_lock(&qemu_mutex);
    current_env->created = 1;
    pthread_cond_signal(&qemu_vcpu_cond);

    /* and wait for machine initialization */
    while (!qemu_system_ready)
	qemu_cond_wait(&qemu_system_cond);
    pthread_mutex_unlock(&qemu_mutex);

    kvm_main_loop_cpu(env);
    return NULL;
}

void kvm_init_vcpu(CPUState *env)
{
    pthread_create(&env->kvm_cpu_state.thread, NULL, ap_main_loop, env);

    while (env->created == 0)
	qemu_cond_wait(&qemu_vcpu_cond);
}

int kvm_vcpu_inited(CPUState *env)
{
    return env->created;
}

#ifdef TARGET_I386
void kvm_hpet_disable_kpit(void)
{
    struct kvm_pit_state2 ps2;

    kvm_get_pit2(kvm_context, &ps2);
    ps2.flags |= KVM_PIT_FLAGS_HPET_LEGACY;
    kvm_set_pit2(kvm_context, &ps2);
}

void kvm_hpet_enable_kpit(void)
{
    struct kvm_pit_state2 ps2;

    kvm_get_pit2(kvm_context, &ps2);
    ps2.flags &= ~KVM_PIT_FLAGS_HPET_LEGACY;
    kvm_set_pit2(kvm_context, &ps2);
}
#endif

int kvm_init_ap(void)
{
#ifdef TARGET_I386
    kvm_tpr_opt_setup();
#endif
    qemu_add_vm_change_state_handler(kvm_vm_state_change_handler, NULL);

    signal(SIG_IPI, sig_ipi_handler);
    return 0;
}

void qemu_kvm_notify_work(void)
{
    uint64_t value = 1;
    char buffer[8];
    size_t offset = 0;

    if (io_thread_fd == -1)
	return;

    memcpy(buffer, &value, sizeof(value));

    while (offset < 8) {
	ssize_t len;

	len = write(io_thread_fd, buffer + offset, 8 - offset);
	if (len == -1 && errno == EINTR)
	    continue;

        /* In case we have a pipe, there is not reason to insist writing
         * 8 bytes
         */
	if (len == -1 && errno == EAGAIN)
	    break;

        if (len <= 0)
            break;

	offset += len;
    }
}

/* If we have signalfd, we mask out the signals we want to handle and then
 * use signalfd to listen for them.  We rely on whatever the current signal
 * handler is to dispatch the signals when we receive them.
 */

static void sigfd_handler(void *opaque)
{
    int fd = (unsigned long)opaque;
    struct qemu_signalfd_siginfo info;
    struct sigaction action;
    ssize_t len;

    while (1) {
	do {
	    len = read(fd, &info, sizeof(info));
	} while (len == -1 && errno == EINTR);

	if (len == -1 && errno == EAGAIN)
	    break;

	if (len != sizeof(info)) {
	    printf("read from sigfd returned %zd: %m\n", len);
	    return;
	}

	sigaction(info.ssi_signo, NULL, &action);
	if (action.sa_handler)
	    action.sa_handler(info.ssi_signo);

    }
}

/* Used to break IO thread out of select */
static void io_thread_wakeup(void *opaque)
{
    int fd = (unsigned long)opaque;
    char buffer[4096];

    /* Drain the pipe/(eventfd) */
    while (1) {
	ssize_t len;

	len = read(fd, buffer, sizeof(buffer));
	if (len == -1 && errno == EINTR)
	    continue;

	if (len <= 0)
	    break;
    }
}

int kvm_main_loop(void)
{
    int fds[2];
    sigset_t mask;
    int sigfd;

    io_thread = pthread_self();
    qemu_system_ready = 1;

    if (qemu_eventfd(fds) == -1) {
	fprintf(stderr, "failed to create eventfd\n");
	return -errno;
    }

    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    qemu_set_fd_handler2(fds[0], NULL, io_thread_wakeup, NULL,
			 (void *)(unsigned long)fds[0]);

    io_thread_fd = fds[1];

    sigemptyset(&mask);
    sigaddset(&mask, SIGIO);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    sigfd = qemu_signalfd(&mask);
    if (sigfd == -1) {
	fprintf(stderr, "failed to create signalfd\n");
	return -errno;
    }

    fcntl(sigfd, F_SETFL, O_NONBLOCK);

    qemu_set_fd_handler2(sigfd, NULL, sigfd_handler, NULL,
			 (void *)(unsigned long)sigfd);

    pthread_cond_broadcast(&qemu_system_cond);

    io_thread_sigfd = sigfd;
    cpu_single_env = NULL;

    while (1) {
        main_loop_wait(1000);
        if (qemu_shutdown_requested()) {
            if (qemu_no_shutdown()) {
                vm_stop(0);
            } else
                break;
	} else if (qemu_powerdown_requested())
            qemu_system_powerdown();
        else if (qemu_reset_requested())
	    qemu_kvm_system_reset();
	else if (kvm_debug_cpu_requested) {
	    gdb_set_stop_cpu(kvm_debug_cpu_requested);
	    vm_stop(EXCP_DEBUG);
	    kvm_debug_cpu_requested = NULL;
	}
    }

    pause_all_threads();
    pthread_mutex_unlock(&qemu_mutex);

    return 0;
}

#ifdef TARGET_I386
static int destroy_region_works = 0;
#endif


#if !defined(TARGET_I386)
int kvm_arch_init_irq_routing(void)
{
    return 0;
}
#endif

extern int no_hpet;

static int kvm_create_context()
{
    int r;

    if (!kvm_irqchip) {
        kvm_disable_irqchip_creation(kvm_context);
    }
    if (!kvm_pit) {
        kvm_disable_pit_creation(kvm_context);
    }
    if (kvm_create(kvm_context, 0, NULL) < 0) {
       kvm_finalize(kvm_state);
       return -1;
    }
    r = kvm_arch_qemu_create_context();
    if(r <0)
       kvm_finalize(kvm_state);
    if (kvm_pit && !kvm_pit_reinject) {
        if (kvm_reinject_control(kvm_context, 0)) {
            fprintf(stderr, "failure to disable in-kernel PIT reinjection\n");
            return -1;
        }
    }
#ifdef TARGET_I386
    destroy_region_works = kvm_destroy_memory_region_works(kvm_context);
#endif

    r = kvm_arch_init_irq_routing();
    if (r < 0) {
        return r;
    }

    kvm_init_ap();
    if (kvm_irqchip) {
        if (!qemu_kvm_has_gsi_routing()) {
            irq0override = 0;
#ifdef TARGET_I386
            /* if kernel can't do irq routing, interrupt source
             * override 0->2 can not be set up as required by hpet,
             * so disable hpet.
             */
            no_hpet=1;
        } else  if (!qemu_kvm_has_pit_state2()) {
            no_hpet=1;
        }
#else
        }
#endif
    }

    return 0;
}

#ifdef TARGET_I386
static int must_use_aliases_source(target_phys_addr_t addr)
{
    if (destroy_region_works)
        return false;
    if (addr == 0xa0000 || addr == 0xa8000)
        return true;
    return false;
}

static int must_use_aliases_target(target_phys_addr_t addr)
{
    if (destroy_region_works)
        return false;
    if (addr >= 0xe0000000 && addr < 0x100000000ull)
        return true;
    return false;
}

static struct mapping {
    target_phys_addr_t phys;
    ram_addr_t ram;
    ram_addr_t len;
} mappings[50];
static int nr_mappings;

static struct mapping *find_ram_mapping(ram_addr_t ram_addr)
{
    struct mapping *p;

    for (p = mappings; p < mappings + nr_mappings; ++p) {
        if (p->ram <= ram_addr && ram_addr < p->ram + p->len) {
            return p;
        }
    }
    return NULL;
}

static struct mapping *find_mapping(target_phys_addr_t start_addr)
{
    struct mapping *p;

    for (p = mappings; p < mappings + nr_mappings; ++p) {
        if (p->phys <= start_addr && start_addr < p->phys + p->len) {
            return p;
        }
    }
    return NULL;
}

static void drop_mapping(target_phys_addr_t start_addr)
{
    struct mapping *p = find_mapping(start_addr);

    if (p)
        *p = mappings[--nr_mappings];
}
#endif

void kvm_set_phys_mem(target_phys_addr_t start_addr, ram_addr_t size,
                      ram_addr_t phys_offset)
{
    int r = 0;
    unsigned long area_flags;
#ifdef TARGET_I386
    struct mapping *p;
#endif

    if (start_addr + size > phys_ram_size) {
        phys_ram_size = start_addr + size;
    }

    phys_offset &= ~IO_MEM_ROM;
    area_flags = phys_offset & ~TARGET_PAGE_MASK;

    if (area_flags != IO_MEM_RAM) {
#ifdef TARGET_I386
        if (must_use_aliases_source(start_addr)) {
            kvm_destroy_memory_alias(kvm_context, start_addr);
            return;
        }
        if (must_use_aliases_target(start_addr))
            return;
#endif
        while (size > 0) {
            p = find_mapping(start_addr);
            if (p) {
                kvm_unregister_memory_area(kvm_context, p->phys, p->len);
                drop_mapping(p->phys);
            }
            start_addr += TARGET_PAGE_SIZE;
            if (size > TARGET_PAGE_SIZE) {
                size -= TARGET_PAGE_SIZE;
            } else {
                size = 0;
            }
        }
        return;
    }

    r = kvm_is_containing_region(kvm_context, start_addr, size);
    if (r)
        return;

    if (area_flags >= TLB_MMIO)
        return;

#ifdef TARGET_I386
    if (must_use_aliases_source(start_addr)) {
        p = find_ram_mapping(phys_offset);
        if (p) {
            kvm_create_memory_alias(kvm_context, start_addr, size,
                                    p->phys + (phys_offset - p->ram));
        }
        return;
    }
#endif

    r = kvm_register_phys_mem(kvm_context, start_addr,
                              qemu_get_ram_ptr(phys_offset),
                              size, 0);
    if (r < 0) {
        printf("kvm_cpu_register_physical_memory: failed\n");
        exit(1);
    }

#ifdef TARGET_I386
    drop_mapping(start_addr);
    p = &mappings[nr_mappings++];
    p->phys = start_addr;
    p->ram = phys_offset;
    p->len = size;
#endif

    return;
}

int kvm_setup_guest_memory(void *area, unsigned long size)
{
    int ret = 0;

#ifdef MADV_DONTFORK
    if (kvm_enabled() && !kvm_has_sync_mmu())
        ret = madvise(area, size, MADV_DONTFORK);
#endif

    if (ret)
        perror ("madvise");

    return ret;
}

int kvm_qemu_check_extension(int ext)
{
    return kvm_check_extension(kvm_state, ext);
}

int kvm_qemu_init_env(CPUState *cenv)
{
    return kvm_arch_qemu_init_env(cenv);
}

#ifdef KVM_CAP_SET_GUEST_DEBUG

struct kvm_set_guest_debug_data {
    struct kvm_guest_debug dbg;
    int err;
};

static void kvm_invoke_set_guest_debug(void *data)
{
    struct kvm_set_guest_debug_data *dbg_data = data;

    dbg_data->err = kvm_set_guest_debug(cpu_single_env->kvm_cpu_state.vcpu_ctx,
		    &dbg_data->dbg);
}

int kvm_update_guest_debug(CPUState *env, unsigned long reinject_trap)
{
    struct kvm_set_guest_debug_data data;

    data.dbg.control = 0;
    if (env->singlestep_enabled)
	data.dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;

    kvm_arch_update_guest_debug(env, &data.dbg);
    data.dbg.control |= reinject_trap;

    on_vcpu(env, kvm_invoke_set_guest_debug, &data);
    return data.err;
}

#endif

/*
 * dirty pages logging
 */
/* FIXME: use unsigned long pointer instead of unsigned char */
unsigned char *kvm_dirty_bitmap = NULL;
int kvm_physical_memory_set_dirty_tracking(int enable)
{
    int r = 0;

    if (!kvm_enabled())
        return 0;

    if (enable) {
        if (!kvm_dirty_bitmap) {
            unsigned bitmap_size = BITMAP_SIZE(phys_ram_size);
            kvm_dirty_bitmap = qemu_malloc(bitmap_size);
            if (kvm_dirty_bitmap == NULL) {
                perror("Failed to allocate dirty pages bitmap");
                r=-1;
            }
            else {
                r = kvm_dirty_pages_log_enable_all(kvm_context);
            }
        }
    }
    else {
        if (kvm_dirty_bitmap) {
            r = kvm_dirty_pages_log_reset(kvm_context);
            qemu_free(kvm_dirty_bitmap);
            kvm_dirty_bitmap = NULL;
        }
    }
    return r;
}

/* get kvm's dirty pages bitmap and update qemu's */
static int kvm_get_dirty_pages_log_range(unsigned long start_addr,
                                         unsigned char *bitmap,
                                         unsigned long offset,
                                         unsigned long mem_size)
{
    unsigned int i, j, n=0;
    unsigned char c;
    unsigned long page_number, addr, addr1;
    ram_addr_t ram_addr;
    unsigned int len = ((mem_size/TARGET_PAGE_SIZE) + 7) / 8;

    /* 
     * bitmap-traveling is faster than memory-traveling (for addr...) 
     * especially when most of the memory is not dirty.
     */
    for (i=0; i<len; i++) {
        c = bitmap[i];
        while (c>0) {
            j = ffsl(c) - 1;
            c &= ~(1u<<j);
            page_number = i * 8 + j;
            addr1 = page_number * TARGET_PAGE_SIZE;
            addr  = offset + addr1;
            ram_addr = cpu_get_physical_page_desc(addr);
            cpu_physical_memory_set_dirty(ram_addr);
            n++;
        }
    }
    return 0;
}
static int kvm_get_dirty_bitmap_cb(unsigned long start, unsigned long len,
                                   void *bitmap, void *opaque)
{
    return kvm_get_dirty_pages_log_range(start, bitmap, start, len);
}

/* 
 * get kvm's dirty pages bitmap and update qemu's
 * we only care about physical ram, which resides in slots 0 and 3
 */
int kvm_update_dirty_pages_log(void)
{
    int r = 0;


    r = kvm_get_dirty_pages_range(kvm_context, 0, -1UL,
                                  NULL,
                                  kvm_get_dirty_bitmap_cb);
    return r;
}

void kvm_qemu_log_memory(target_phys_addr_t start, target_phys_addr_t size,
                         int log)
{
    if (log)
	kvm_dirty_pages_log_enable_slot(kvm_context, start, size);
    else {
#ifdef TARGET_I386
        if (must_use_aliases_target(start))
            return;
#endif
	kvm_dirty_pages_log_disable_slot(kvm_context, start, size);
    }
}

int kvm_get_phys_ram_page_bitmap(unsigned char *bitmap)
{
    unsigned int bsize  = BITMAP_SIZE(phys_ram_size);
    unsigned int brsize = BITMAP_SIZE(ram_size);
    unsigned int extra_pages = (phys_ram_size - ram_size) / TARGET_PAGE_SIZE;
    unsigned int extra_bytes = (extra_pages +7)/8;
    unsigned int hole_start = BITMAP_SIZE(0xa0000);
    unsigned int hole_end   = BITMAP_SIZE(0xc0000);

    memset(bitmap, 0xFF, brsize + extra_bytes);
    memset(bitmap + hole_start, 0, hole_end - hole_start);
    memset(bitmap + brsize + extra_bytes, 0, bsize - brsize - extra_bytes);

    return 0;
}

#ifdef KVM_CAP_IRQCHIP

int kvm_set_irq(int irq, int level, int *status)
{
    return kvm_set_irq_level(kvm_context, irq, level, status);
}

#endif

int qemu_kvm_get_dirty_pages(unsigned long phys_addr, void *buf)
{
    return kvm_get_dirty_pages(kvm_context, phys_addr, buf);
}

void kvm_mutex_unlock(void)
{
    assert(!cpu_single_env);
    pthread_mutex_unlock(&qemu_mutex);
}

void kvm_mutex_lock(void)
{
    pthread_mutex_lock(&qemu_mutex);
    cpu_single_env = NULL;
}

#ifdef USE_KVM_DEVICE_ASSIGNMENT
void kvm_add_ioperm_data(struct ioperm_data *data)
{
    LIST_INSERT_HEAD(&ioperm_head, data, entries);
}

void kvm_remove_ioperm_data(unsigned long start_port, unsigned long num)
{
    struct ioperm_data *data;

    data = LIST_FIRST(&ioperm_head);
    while (data) {
        struct ioperm_data *next = LIST_NEXT(data, entries);

        if (data->start_port == start_port && data->num == num) {
            LIST_REMOVE(data, entries);
            qemu_free(data);
        }

        data = next;
    }
}

void kvm_ioperm(CPUState *env, void *data)
{
    if (kvm_enabled() && qemu_system_ready)
	on_vcpu(env, kvm_arch_do_ioperm, data);
}

#endif

int kvm_physical_sync_dirty_bitmap(target_phys_addr_t start_addr, target_phys_addr_t end_addr)
{
#ifndef TARGET_IA64

#ifdef TARGET_I386
    if (must_use_aliases_source(start_addr))
        return 0;
#endif

    kvm_get_dirty_pages_range(kvm_context, start_addr, end_addr - start_addr,
			      NULL, kvm_get_dirty_bitmap_cb);
#endif
    return 0;
}

int kvm_log_start(target_phys_addr_t phys_addr, target_phys_addr_t len)
{
#ifdef TARGET_I386
    if (must_use_aliases_source(phys_addr))
        return 0;
#endif

#ifndef TARGET_IA64
    kvm_qemu_log_memory(phys_addr, len, 1);
#endif
    return 0;
}

int kvm_log_stop(target_phys_addr_t phys_addr, target_phys_addr_t len)
{
#ifdef TARGET_I386
    if (must_use_aliases_source(phys_addr))
        return 0;
#endif

#ifndef TARGET_IA64
    kvm_qemu_log_memory(phys_addr, len, 0);
#endif
    return 0;
}

int kvm_set_boot_cpu_id(uint32_t id)
{
	return kvm_set_boot_vcpu_id(kvm_context, id);
}

#ifdef TARGET_I386
#ifdef KVM_CAP_MCE
struct kvm_x86_mce_data
{
    CPUState *env;
    struct kvm_x86_mce *mce;
};

static void kvm_do_inject_x86_mce(void *_data)
{
    struct kvm_x86_mce_data *data = _data;
    int r;

    r = kvm_set_mce(data->env->kvm_cpu_state.vcpu_ctx, data->mce);
    if (r < 0)
        perror("kvm_set_mce FAILED");
}
#endif

void kvm_inject_x86_mce(CPUState *cenv, int bank, uint64_t status,
                        uint64_t mcg_status, uint64_t addr, uint64_t misc)
{
#ifdef KVM_CAP_MCE
    struct kvm_x86_mce mce = {
        .bank = bank,
        .status = status,
        .mcg_status = mcg_status,
        .addr = addr,
        .misc = misc,
    };
    struct kvm_x86_mce_data data = {
            .env = cenv,
            .mce = &mce,
    };

    on_vcpu(cenv, kvm_do_inject_x86_mce, &data);
#endif
}
#endif
