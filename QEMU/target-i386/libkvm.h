/*
 * This header is for functions & variables that will ONLY be
 * used inside libkvm for x86.
 * THESE ARE NOT EXPOSED TO THE USER AND ARE ONLY FOR USE
 * WITHIN LIBKVM.
 *
 * derived from libkvm.c
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *	Avi Kivity   <avi@qumranet.com>
 *	Yaniv Kamay  <yaniv@qumranet.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#ifndef KVM_X86_H
#define KVM_X86_H

#define PAGE_SIZE 4096ul
#define PAGE_MASK (~(PAGE_SIZE - 1))

int kvm_set_tss_addr(kvm_context_t kvm, unsigned long addr);

#define smp_wmb()   asm volatile("" ::: "memory")

#endif
