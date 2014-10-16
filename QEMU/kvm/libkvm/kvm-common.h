/*
 * This header is for functions & variables that will ONLY be
 * used inside libkvm.
 *
 * derived from libkvm.c
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *	Avi Kivity   <avi@qumranet.com>
 *	Yaniv Kamay  <yaniv@qumranet.com>
 *
 *   This work is licensed under the GNU LGPL license, version 2.
 */

#ifndef KVM_COMMON_H
#define KVM_COMMON_H

/* FIXME: share this number with kvm */
/* FIXME: or dynamically alloc/realloc regions */
#ifdef __s390__
#define KVM_MAX_NUM_MEM_REGIONS 1u
#define MAX_VCPUS 64
#define LIBKVM_S390_ORIGIN (0UL)
#elif defined(__ia64__)
#define KVM_MAX_NUM_MEM_REGIONS 32u
#define MAX_VCPUS 256
#else
#define KVM_MAX_NUM_MEM_REGIONS 32u
#define MAX_VCPUS 16
#endif


/* kvm abi verison variable */
extern int kvm_abi;

/**
 * \brief The KVM context
 *
 * The verbose KVM context
 */

struct kvm_context {
	/// Filedescriptor to /dev/kvm
	int fd;
	int vm_fd;
	int vcpu_fd[MAX_VCPUS];
	struct kvm_run *run[MAX_VCPUS];
	/// Callbacks that KVM uses to emulate various unvirtualizable functionality
	struct kvm_callbacks *callbacks;
	void *opaque;
	/// is dirty pages logging enabled for all regions or not
	int dirty_pages_log_all;
	/// do not create in-kernel irqchip if set
	int no_irqchip_creation;
	/// in-kernel irqchip status
	int irqchip_in_kernel;
	/// ioctl to use to inject interrupts
	int irqchip_inject_ioctl;
	/// do not create in-kernel pit if set
	int no_pit_creation;
	/// in-kernel pit status
	int pit_in_kernel;
	/// in-kernel coalesced mmio
	int coalesced_mmio;
#ifdef KVM_CAP_IRQ_ROUTING
	struct kvm_irq_routing *irq_routes;
	int nr_allocated_irq_routes;
#endif
	void *used_gsi_bitmap;
	int max_gsi;
};

int kvm_alloc_kernel_memory(kvm_context_t kvm, unsigned long memory,
								void **vm_mem);
int kvm_alloc_userspace_memory(kvm_context_t kvm, unsigned long memory,
								void **vm_mem);

int kvm_arch_create(kvm_context_t kvm, unsigned long phys_mem_bytes,
                        void **vm_mem);
int kvm_arch_run(struct kvm_run *run, kvm_context_t kvm, int vcpu);


void kvm_show_code(kvm_context_t kvm, int vcpu);

int handle_halt(kvm_context_t kvm, int vcpu);
int handle_shutdown(kvm_context_t kvm, void *env);
void post_kvm_run(kvm_context_t kvm, void *env);
int pre_kvm_run(kvm_context_t kvm, void *env);
int handle_io_window(kvm_context_t kvm);
int handle_debug(kvm_context_t kvm, int vcpu, void *env);
int try_push_interrupts(kvm_context_t kvm);

#endif
