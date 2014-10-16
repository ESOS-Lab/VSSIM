/*
 * This file contains the powerpc specific implementation for the
 * architecture dependent functions defined in kvm-common.h and
 * libkvm.h
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *      Avi Kivity   <avi@qumranet.com>
 *      Yaniv Kamay  <yaniv@qumranet.com>
 *
 * Copyright IBM Corp. 2007,2008
 * Authors:
 * 	Jerone Young <jyoung5@us.ibm.com>
 * 	Christian Ehrhardt <ehrhardt@linux.vnet.ibm.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#include "libkvm.h"
#include "kvm-powerpc.h"
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

int handle_dcr(struct kvm_run *run,  kvm_context_t kvm, int vcpu)
{
	int ret = 0;

	if (run->dcr.is_write)
		ret = kvm->callbacks->powerpc_dcr_write(vcpu,
							run->dcr.dcrn,
							run->dcr.data);
	else
		ret = kvm->callbacks->powerpc_dcr_read(vcpu,
							run->dcr.dcrn,
							&(run->dcr.data));

	return ret;
}

void kvm_show_code(kvm_context_t kvm, int vcpu)
{
	fprintf(stderr, "%s: Operation not supported\n", __FUNCTION__);
}

void kvm_show_regs(kvm_context_t kvm, int vcpu)
{
	struct kvm_regs regs;
	int i;

	if (kvm_get_regs(kvm, vcpu, &regs))
		return;

	fprintf(stderr,"guest vcpu #%d\n", vcpu);
	fprintf(stderr,"pc:   %016"PRIx64" msr:  %016"PRIx64"\n",
	        regs.pc, regs.msr);
	fprintf(stderr,"lr:   %016"PRIx64" ctr:  %016"PRIx64"\n",
	        regs.lr, regs.ctr);
	fprintf(stderr,"srr0: %016"PRIx64" srr1: %016"PRIx64"\n",
	        regs.srr0, regs.srr1);
	for (i=0; i<32; i+=4)
	{
		fprintf(stderr, "gpr%02d: %016"PRIx64" %016"PRIx64" %016"PRIx64
		        " %016"PRIx64"\n", i,
			regs.gpr[i],
			regs.gpr[i+1],
			regs.gpr[i+2],
			regs.gpr[i+3]);
	}

	fflush(stdout);
}

int kvm_arch_create(kvm_context_t kvm, unsigned long phys_mem_bytes,
			 void **vm_mem)
{
	int r;

	r = kvm_init_coalesced_mmio(kvm);
	if (r < 0)
		return r;

	return 0;
}

int kvm_arch_run(struct kvm_run *run, kvm_context_t kvm, int vcpu)
{
	int ret = 0;

	switch (run->exit_reason){
	case KVM_EXIT_DCR:
		ret = handle_dcr(run, kvm, vcpu);
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}
