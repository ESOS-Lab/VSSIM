/*
 * This file contains the s390 specific implementation for the
 * architecture dependent functions defined in kvm-common.h and
 * libkvm.h
 *
 * Copyright (C) 2006 Qumranet
 * Copyright IBM Corp. 2008
 *
 * Authors:
 *	Carsten Otte <cotte@de.ibm.com>
 *	Christian Borntraeger <borntraeger@de.ibm.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#include <sys/ioctl.h>
#include <asm/ptrace.h>

#include "libkvm.h"
#include "kvm-common.h"
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

void kvm_show_code(kvm_context_t kvm, int vcpu)
{
	fprintf(stderr, "%s: Operation not supported\n", __FUNCTION__);
}

void kvm_show_regs(kvm_context_t kvm, int vcpu)
{
	struct kvm_regs regs;
	struct kvm_sregs sregs;
	int i;

	if (kvm_get_regs(kvm, vcpu, &regs))
		return;

	if (kvm_get_sregs(kvm, vcpu, &sregs))
		return;

	fprintf(stderr, "guest vcpu #%d\n", vcpu);
	fprintf(stderr, "PSW:\t%16.16lx %16.16lx\n",
					kvm->run[vcpu]->s390_sieic.mask,
					kvm->run[vcpu]->s390_sieic.addr);
	fprintf(stderr,"GPRS:");
	for (i=0; i<15; i+=4)
		fprintf(stderr, "\t%16.16lx %16.16lx %16.16lx %16.16lx\n",
							regs.gprs[i],
							regs.gprs[i+1],
							regs.gprs[i+2],
							regs.gprs[i+3]);
	fprintf(stderr,"ACRS:");
	for (i=0; i<15; i+=4)
		fprintf(stderr, "\t%8.8x %8.8x %8.8x %8.8x\n",
							sregs.acrs[i],
							sregs.acrs[i+1],
							sregs.acrs[i+2],
							sregs.acrs[i+3]);

	fprintf(stderr,"CRS:");
	for (i=0; i<15; i+=4)
		fprintf(stderr, "\t%16.16lx %16.16lx %16.16lx %16.16lx\n",
							sregs.crs[i],
							sregs.crs[i+1],
							sregs.crs[i+2],
							sregs.crs[i+3]);
}

int kvm_arch_create(kvm_context_t kvm, unsigned long phys_mem_bytes,
			 void **vm_mem)
{
	return 0;
}

int kvm_arch_run(struct kvm_run *run, kvm_context_t kvm, int vcpu)
{
	int ret = 0;

	switch (run->exit_reason){
	default:
		ret = 1;
		break;
	}
	return ret;
}

int kvm_s390_initial_reset(kvm_context_t kvm, int slot)
{
	return ioctl(kvm->vcpu_fd[slot], KVM_S390_INITIAL_RESET, NULL);
}

int kvm_s390_interrupt(kvm_context_t kvm, int slot,
	struct kvm_s390_interrupt *kvmint)
{
	if (slot>=0)
		return ioctl(kvm->vcpu_fd[slot], KVM_S390_INTERRUPT, kvmint);
	else
		return ioctl(kvm->vm_fd, KVM_S390_INTERRUPT, kvmint);
}

int kvm_s390_set_initial_psw(kvm_context_t kvm, int slot, psw_t psw)
{
	return ioctl(kvm->vcpu_fd[slot], KVM_S390_SET_INITIAL_PSW, &psw);
}

int kvm_s390_store_status(kvm_context_t kvm, int slot, unsigned long addr)
{
	return ioctl(kvm->vcpu_fd[slot], KVM_S390_STORE_STATUS, addr);
}
