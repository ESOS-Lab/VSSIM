/*
 *  IA64 execution defines
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *  Copyright (c) 2007 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __IA64_H__
#define __IA64_H__

//#include "dyngen-exec.h"
#include "config.h"

#include "dyngen-exec.h"

#include "cpu.h"
#include "exec-all.h"

#define tcg_qemu_tb_exec(tb_ptr) 0

register struct CPUIA64State *env asm(AREG0);

static inline void env_to_regs(void)
{
}

static inline void regs_to_env(void)
{
}

void do_interrupt (CPUState *env);

void cpu_lock(void);
void cpu_unlock(void);

static inline int cpu_halted(CPUState *env) {
    /* handle exit of HALTED state */
    if (!(env->hflags & HF_HALTED_MASK))
        return 0;
    return EXCP_HALTED;
}

static inline int cpu_has_work(CPUState *env)
{
    return (env->interrupt_request & (CPU_INTERRUPT_HARD));
}

#endif
