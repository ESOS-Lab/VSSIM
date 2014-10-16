/*
 * Kernel-based Virtual Machine test driver
 *
 * This test driver provides a simple way of testing kvm, without a full
 * device model.
 *
 * Copyright (C) 2006 Qumranet
 *
 * Authors:
 *
 *  Avi Kivity <avi@qumranet.com>
 *  Yaniv Kamay <yaniv@qumranet.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#define _GNU_SOURCE

#include <libkvm.h>
#include "test/lib/x86/fake-apic.h"
#include "test/x86/ioram.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <getopt.h>
#include <stdbool.h>

#include "iotable.h"

static uint8_t ioram[IORAM_LEN];

static int gettid(void)
{
	return syscall(__NR_gettid);
}

static int tkill(int pid, int sig)
{
	return syscall(__NR_tkill, pid, sig);
}

kvm_context_t kvm;

#define MAX_VCPUS 4

#define IPI_SIGNAL (SIGRTMIN + 4)

static int ncpus = 1;
static sem_t init_sem;
static __thread int vcpu;
static int apic_ipi_vector = 0xff;
static sigset_t kernel_sigmask;
static sigset_t ipi_sigmask;
static uint64_t memory_size = 128 * 1024 * 1024;

static struct io_table pio_table;

struct vcpu_info {
	int id;
	pid_t tid;
	sem_t sipi_sem;
};

struct vcpu_info *vcpus;

static uint32_t apic_sipi_addr;

static void apic_send_sipi(int vcpu)
{
	sem_post(&vcpus[vcpu].sipi_sem);
}

static void apic_send_ipi(int vcpu)
{
	struct vcpu_info *v;

	if (vcpu < 0 || vcpu >= ncpus)
		return;
	v = &vcpus[vcpu];
	tkill(v->tid, IPI_SIGNAL);
}

static int apic_io(void *opaque, int size, int is_write,
		   uint64_t addr, uint64_t *value)
{
	if (!is_write)
		*value = -1u;

	switch (addr - APIC_BASE) {
	case APIC_REG_NCPU:
		if (!is_write)
			*value = ncpus;
		break;
	case APIC_REG_ID:
		if (!is_write)
			*value = vcpu;
		break;
	case APIC_REG_SIPI_ADDR:
		if (!is_write)
			*value = apic_sipi_addr;
		else
			apic_sipi_addr = *value;
		break;
	case APIC_REG_SEND_SIPI:
		if (is_write)
			apic_send_sipi(*value);
		break;
	case APIC_REG_IPI_VECTOR:
		if (!is_write)
			*value = apic_ipi_vector;
		else
			apic_ipi_vector = *value;
		break;
	case APIC_REG_SEND_IPI:
		if (is_write)
			apic_send_ipi(*value);
		break;
	}

	return 0;
}

static int apic_init(void)
{
	return io_table_register(&pio_table, APIC_BASE,
				 APIC_SIZE, apic_io, NULL);
}

static int misc_io(void *opaque, int size, int is_write,
		   uint64_t addr, uint64_t *value)
{
	static int newline = 1;

	if (!is_write)
		*value = -1;

	switch (addr) {
	case 0xff: // irq injector
		if (is_write) {
			printf("injecting interrupt 0x%x\n", (uint8_t)*value);
			kvm_inject_irq(kvm, 0, *value);
		}
		break;
	case 0xf1: // serial
		if (is_write) {
			if (newline)
				fputs("GUEST: ", stdout);
			putchar(*value);
			newline = *value == '\n';
		}
		break;
	case 0xd1:
		if (!is_write)
			*value = memory_size;
		break;
	case 0xf4: // exit
		if (is_write)
			exit(*value);
		break;
	}

	return 0;
}

static int misc_init(void)
{
	int err;

	err = io_table_register(&pio_table, 0xff, 1, misc_io, NULL);
	if (err < 0)
		return err;

	err = io_table_register(&pio_table, 0xf1, 1, misc_io, NULL);
	if (err < 0)
		return err;

	err = io_table_register(&pio_table, 0xf4, 1, misc_io, NULL);
	if (err < 0)
		return err;

	return io_table_register(&pio_table, 0xd1, 1, misc_io, NULL);
}

#define IRQCHIP_IO_BASE 0x2000

static int irqchip_io(void *opaque, int size, int is_write,
		      uint64_t addr, uint64_t *value)
{
	addr -= IRQCHIP_IO_BASE;

	if (is_write) {
		kvm_set_irq_level(kvm, addr, *value, NULL);
	}
	return 0;
}

static int test_inb(void *opaque, uint16_t addr, uint8_t *value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val;
		entry->handler(entry->opaque, 1, 0, addr, &val);
		*value = val;
	} else {
		*value = -1;
		printf("inb 0x%x\n", addr);
	}

	return 0;
}

static int test_inw(void *opaque, uint16_t addr, uint16_t *value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val;
		entry->handler(entry->opaque, 2, 0, addr, &val);
		*value = val;
	} else {
		*value = -1;
		printf("inw 0x%x\n", addr);
	}

	return 0;
}

static int test_inl(void *opaque, uint16_t addr, uint32_t *value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val;
		entry->handler(entry->opaque, 4, 0, addr, &val);
		*value = val;
	} else {
		*value = -1;
		printf("inl 0x%x\n", addr);
	}

	return 0;
}

static int test_outb(void *opaque, uint16_t addr, uint8_t value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val = value;
		entry->handler(entry->opaque, 1, 1, addr, &val);
	} else
		printf("outb $0x%x, 0x%x\n", value, addr);

	return 0;
}

static int test_outw(void *opaque, uint16_t addr, uint16_t value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val = value;
		entry->handler(entry->opaque, 2, 1, addr, &val);
	} else
		printf("outw $0x%x, 0x%x\n", value, addr);

	return 0;
}

static int test_outl(void *opaque, uint16_t addr, uint32_t value)
{
	struct io_table_entry *entry;

	entry = io_table_lookup(&pio_table, addr);
	if (entry) {
		uint64_t val = value;
		entry->handler(entry->opaque, 4, 1, addr, &val);
	} else
		printf("outl $0x%x, 0x%x\n", value, addr);

	return 0;
}

#ifdef KVM_CAP_SET_GUEST_DEBUG
static int test_debug(void *opaque, void *vcpu,
		      struct kvm_debug_exit_arch *arch_info)
{
	printf("test_debug\n");
	return 0;
}
#endif

static int test_halt(void *opaque, int vcpu)
{
	int n;

	sigwait(&ipi_sigmask, &n);
	kvm_inject_irq(kvm, vcpus[vcpu].id, apic_ipi_vector);
	return 0;
}

static int test_io_window(void *opaque)
{
	return 0;
}

static int test_try_push_interrupts(void *opaque)
{
	return 0;
}

#ifdef KVM_CAP_USER_NMI
static void test_push_nmi(void *opaque)
{
}
#endif

static void test_post_kvm_run(void *opaque, void *vcpu)
{
}

static int test_pre_kvm_run(void *opaque, void *vcpu)
{
	return 0;
}

static int test_mem_read(void *opaque, uint64_t addr, uint8_t *data, int len)
{
	if (addr < IORAM_BASE_PHYS || addr + len > IORAM_BASE_PHYS + IORAM_LEN)
		return 1;
	memcpy(data, ioram + addr - IORAM_BASE_PHYS, len);
	return 0;
}

static int test_mem_write(void *opaque, uint64_t addr, uint8_t *data, int len)
{
	if (addr < IORAM_BASE_PHYS || addr + len > IORAM_BASE_PHYS + IORAM_LEN)
		return 1;
	memcpy(ioram + addr - IORAM_BASE_PHYS, data, len);
	return 0;
}

static int test_shutdown(void *opaque, void *env)
{
	printf("shutdown\n");
	kvm_show_regs(kvm, 0);
	exit(1);
	return 1;
}

static struct kvm_callbacks test_callbacks = {
	.inb         = test_inb,
	.inw         = test_inw,
	.inl         = test_inl,
	.outb        = test_outb,
	.outw        = test_outw,
	.outl        = test_outl,
	.mmio_read   = test_mem_read,
	.mmio_write  = test_mem_write,
#ifdef KVM_CAP_SET_GUEST_DEBUG
	.debug       = test_debug,
#endif
	.halt        = test_halt,
	.io_window = test_io_window,
	.try_push_interrupts = test_try_push_interrupts,
#ifdef KVM_CAP_USER_NMI
	.push_nmi = test_push_nmi,
#endif
	.post_kvm_run = test_post_kvm_run,
	.pre_kvm_run = test_pre_kvm_run,
	.shutdown = test_shutdown,
};

static void load_file(void *mem, const char *fname)
{
	int r;
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}
	while ((r = read(fd, mem, 4096)) != -1 && r != 0)
		mem += r;
	if (r == -1) {
		perror("read");
		exit(1);
	}
}

static void enter_32(kvm_context_t kvm)
{
	struct kvm_regs regs = {
		.rsp = 0x80000,  /* 512KB */
		.rip = 0x100000, /* 1MB */
		.rflags = 2,
	};
	struct kvm_sregs sregs = {
		.cs = { 0, -1u,  8, 11, 1, 0, 1, 1, 0, 1, 0, 0 },
		.ds = { 0, -1u, 16,  3, 1, 0, 1, 1, 0, 1, 0, 0 },
		.es = { 0, -1u, 16,  3, 1, 0, 1, 1, 0, 1, 0, 0 },
		.fs = { 0, -1u, 16,  3, 1, 0, 1, 1, 0, 1, 0, 0 },
		.gs = { 0, -1u, 16,  3, 1, 0, 1, 1, 0, 1, 0, 0 },
		.ss = { 0, -1u, 16,  3, 1, 0, 1, 1, 0, 1, 0, 0 },

		.tr = { 0, 10000, 24, 11, 1, 0, 0, 0, 0, 0, 0, 0 },
		.ldt = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		.gdt = { 0, 0 },
		.idt = { 0, 0 },
		.cr0 = 0x37,
		.cr3 = 0,
		.cr4 = 0,
		.efer = 0,
		.apic_base = 0,
		.interrupt_bitmap = { 0 },
	};

	kvm_set_regs(kvm, 0, &regs);
	kvm_set_sregs(kvm, 0, &sregs);
}

static void init_vcpu(int n)
{
	sigemptyset(&ipi_sigmask);
	sigaddset(&ipi_sigmask, IPI_SIGNAL);
	sigprocmask(SIG_UNBLOCK, &ipi_sigmask, NULL);
	sigprocmask(SIG_BLOCK, &ipi_sigmask, &kernel_sigmask);
	vcpus[n].id = n;
	vcpus[n].tid = gettid();
	vcpu = n;
	kvm_set_signal_mask(kvm, n, &kernel_sigmask);
	sem_post(&init_sem);
}

static void *do_create_vcpu(void *_n)
{
	int n = (long)_n;
	struct kvm_regs regs;

	kvm_create_vcpu(kvm, n);
	init_vcpu(n);
	sem_wait(&vcpus[n].sipi_sem);
	kvm_get_regs(kvm, n, &regs);
	regs.rip = apic_sipi_addr;
	kvm_set_regs(kvm, n, &regs);
	kvm_run(kvm, n, &vcpus[n]);
	return NULL;
}

static void start_vcpu(int n)
{
	pthread_t thread;

	sem_init(&vcpus[n].sipi_sem, 0, 0);
	pthread_create(&thread, NULL, do_create_vcpu, (void *)(long)n);
}

static void usage(const char *progname)
{
	fprintf(stderr,
"Usage: %s [OPTIONS] [bootstrap] flatfile\n"
"KVM test harness.\n"
"\n"
"  -s, --smp=NUM          create a VM with NUM virtual CPUs\n"
"  -p, --protected-mode   start VM in protected mode\n"
"  -m, --memory=NUM[GMKB] allocate NUM memory for virtual machine.  A suffix\n"
"                         can be used to change the unit (default: `M')\n"
"  -h, --help             display this help screen and exit\n"
"\n"
"Report bugs to <kvm@vger.kernel.org>.\n"
		, progname);
}

static void sig_ignore(int sig)
{
	write(1, "boo\n", 4);
}

int main(int argc, char **argv)
{
	void *vm_mem;
	int i;
	const char *sopts = "s:phm:";
	struct option lopts[] = {
		{ "smp", 1, 0, 's' },
		{ "protected-mode", 0, 0, 'p' },
		{ "memory", 1, 0, 'm' },
		{ "help", 0, 0, 'h' },
		{ 0 },
	};
	int opt_ind, ch;
	bool enter_protected_mode = false;
	int nb_args;
	char *endptr;

	while ((ch = getopt_long(argc, argv, sopts, lopts, &opt_ind)) != -1) {
		switch (ch) {
		case 's':
			ncpus = atoi(optarg);
			break;
		case 'p':
			enter_protected_mode = true;
			break;
		case 'm':
			memory_size = strtoull(optarg, &endptr, 0);
			switch (*endptr) {
			case 'G': case 'g':
				memory_size <<= 30;
				break;
			case '\0':
			case 'M': case 'm':
				memory_size <<= 20;
				break;
			case 'K': case 'k':
				memory_size <<= 10;
				break;
			default:
				fprintf(stderr,
					"Unrecongized memory suffix: %c\n",
					*endptr);
				exit(1);
			}
			if (memory_size == 0) {
				fprintf(stderr,
					"Invalid memory size: 0\n");
				exit(1);
			}
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		case '?':
		default:
			fprintf(stderr,
				"Try `%s --help' for more information.\n",
				argv[0]);
			exit(1);
		}
	}

	nb_args = argc - optind;
	if (nb_args < 1 || nb_args > 2) {
		fprintf(stderr,
			"Incorrect number of arguments.\n"
			"Try `%s --help' for more information.\n",
			argv[0]);
		exit(1);
	}

	signal(IPI_SIGNAL, sig_ignore);

	vcpus = calloc(ncpus, sizeof *vcpus);
	if (!vcpus) {
		fprintf(stderr, "calloc failed\n");
		return 1;
	}

	kvm = kvm_init(&test_callbacks, 0);
	if (!kvm) {
		fprintf(stderr, "kvm_init failed\n");
		return 1;
	}
	if (kvm_create(kvm, memory_size, &vm_mem) < 0) {
		kvm_finalize(kvm);
		fprintf(stderr, "kvm_create failed\n");
		return 1;
	}

	vm_mem = kvm_create_phys_mem(kvm, 0, memory_size, 0, 1);

	if (enter_protected_mode)
		enter_32(kvm);
	else
		load_file(vm_mem + 0xf0000, argv[optind]);

	if (nb_args > 1)
		load_file(vm_mem + 0x100000, argv[optind + 1]);

	apic_init();
	misc_init();

	io_table_register(&pio_table, IRQCHIP_IO_BASE, 0x20, irqchip_io, NULL);

	sem_init(&init_sem, 0, 0);
	for (i = 0; i < ncpus; ++i)
		start_vcpu(i);
	for (i = 0; i < ncpus; ++i)
		sem_wait(&init_sem);

	kvm_run(kvm, 0, &vcpus[0]);

	return 0;
}
