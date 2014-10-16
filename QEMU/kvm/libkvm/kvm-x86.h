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

#include "kvm-common.h"

#define PAGE_SIZE 4096ul
#define PAGE_MASK (~(PAGE_SIZE - 1))

int kvm_set_tss_addr(kvm_context_t kvm, unsigned long addr);

#ifdef KVM_CAP_VAPIC

/*!
 * \brief Enable kernel tpr access reporting
 *
 * When tpr access reporting is enabled, the kernel will call the
 * ->tpr_access() callback every time the guest vcpu accesses the tpr.
 *
 * \param kvm Pointer to the current kvm_context
 * \param vcpu vcpu to enable tpr access reporting on
 */
int kvm_enable_tpr_access_reporting(kvm_context_t kvm, int vcpu);

/*!
 * \brief Disable kernel tpr access reporting
 *
 * Undoes the effect of kvm_enable_tpr_access_reporting().
 *
 * \param kvm Pointer to the current kvm_context
 * \param vcpu vcpu to disable tpr access reporting on
 */
int kvm_disable_tpr_access_reporting(kvm_context_t kvm, int vcpu);

#endif

#define smp_wmb()   asm volatile("" ::: "memory")

#endif
