/*
 * This header is for functions & variables that will ONLY be
 * used inside libkvm for s390.
 * THESE ARE NOT EXPOSED TO THE USER AND ARE ONLY FOR USE
 * WITHIN LIBKVM.
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *	Avi Kivity   <avi@qumranet.com>
 *	Yaniv Kamay  <yaniv@qumranet.com>
 *
 * Copyright 2008 IBM Corporation.
 * Authors:
 *	Carsten Otte <cotte@de.ibm.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#ifndef KVM_S390_H
#define KVM_S390_H

#include <asm/ptrace.h>
#include "kvm-common.h"

#define PAGE_SIZE 4096ul
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define smp_wmb()   asm volatile("" ::: "memory")

#endif
