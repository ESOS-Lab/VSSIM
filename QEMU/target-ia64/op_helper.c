/*
 * op_helper.c:  IA64 emulation cpu micro-operations helpers for qemu.
 *
 * Copyright (c) 2007 Intel Corporation
 * Zhang Xiantao <xiantao.zhang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "exec-all.h"

#include "qemu-kvm.h"
#include "qemu-common.h"

void cpu_ia64_set_model(CPUIA64State *env, uint32_t id);
void cpu_ia64_close(CPUIA64State *env);
void switch_mode(CPUState *env, int mode);
void do_interrupt(CPUIA64State *env);
int cpu_ia64_handle_mmu_fault (CPUState *env, target_ulong address,
                               int access_type, int is_user, int is_softmmu);
CPUState *cpu_ia64_init(const char *cpu_model)
{
    CPUState *env;
    env = qemu_mallocz(sizeof(CPUState));
    if (!env)
        return NULL;
    cpu_exec_init(env);
    cpu_reset(env);
    if (kvm_enabled()) {
        kvm_qemu_init_env(env);
        kvm_init_vcpu(env);
    }
    return env;
}

void cpu_reset(CPUIA64State *env)
{
}

static inline void set_feature(CPUIA64State *env, int feature)
{
}

void cpu_ia64_set_model(CPUIA64State *env, uint32_t id)
{
}

void cpu_ia64_close(CPUIA64State *env)
{
    free(env);
}

extern int semihosting_enabled;

void switch_mode(CPUState *env, int mode)
{
}

/* Handle a CPU exception.  */
void do_interrupt(CPUIA64State *env)
{
    if (kvm_enabled()) {
        printf("%s: unexpect\n", __FUNCTION__);
        exit(-1);
    }
}

int cpu_ia64_handle_mmu_fault (CPUState *env, target_ulong address,
                               int access_type, int is_user, int is_softmmu)
{
    return 1;
}

target_ulong cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    return -1;
}

void cpu_dump_state(CPUState *env, FILE *f,
                    int (*cpu_fprintf)(FILE *f, const char *fmt, ...),
                    int flags)
{
    return;
}

void tlb_fill (target_ulong addr, int is_write, int is_user, void *retaddr)
{
    return;
}
