/*
 * This header is for functions & variables that will ONLY be
 * used inside libkvm for powerpc.
 * THESE ARE NOT EXPOSED TO THE USER AND ARE ONLY FOR USE
 * WITHIN LIBKVM.
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *	Avi Kivity   <avi@qumranet.com>
 *	Yaniv Kamay  <yaniv@qumranet.com>
 *
 * Copyright 2007 IBM Corporation.
 * Added by: Jerone Young <jyoung5@us.ibm.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#ifndef KVM_POWERPC_H
#define KVM_POWERPC_H

#include "kvm-common.h"

extern int kvm_page_size;

#define PAGE_SIZE kvm_page_size
#define PAGE_MASK (~(PAGE_SIZE - 1))

static inline void eieio(void)
{
	asm volatile("eieio" : : : "memory");
}

#define smp_wmb()	eieio()

#endif
