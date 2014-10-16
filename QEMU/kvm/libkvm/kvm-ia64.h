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

#ifndef KVM_IA64_H
#define KVM_IA64_H

#include "kvm-common.h"

extern int kvm_page_size;

#define PAGE_SIZE kvm_page_size
#define PAGE_MASK (~(kvm_page_size - 1))

#define ia64_mf()	asm volatile ("mf" ::: "memory")
#define smp_wmb()	ia64_mf()

#endif
