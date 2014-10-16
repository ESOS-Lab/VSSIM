/*
 * IA64 virtual CPU header
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 *  Copyright (c) 2007 Intel Corporation
 *  Zhang xiantao <xiantao.zhang@intel.com>
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
#ifndef CPU_IA64_H
#define CPU_IA64_H
#include "config.h"
#include "ia64intrin.h"

#include<string.h>

#define TARGET_LONG_BITS 64

#define TARGET_PAGE_BITS 16

#define ELF_MACHINE	EM_IA_64

#define NB_MMU_MODES 2
#define CPU_PAL_HALT 1
#define HF_HALTED_MASK       (1 << CPU_PAL_HALT)

#include "cpu-defs.h"

#include "softfloat.h"

#define CPUState struct CPUIA64State

typedef struct CPUIA64State {
    CPU_COMMON;
    uint32_t hflags;
    int mp_state;
} CPUIA64State;

#define cpu_gen_code cpu_ia64_gen_code
#define cpu_init cpu_ia64_init
#define cpu_signal_handler cpu_ia64_signal_handler

extern struct CPUIA64State *env;
int cpu_get_pic_interrupt(CPUIA64State *s);
int cpu_exec(CPUState *env1);
CPUState *cpu_ia64_init(const char * cpu_model);

static inline int cpu_mmu_index (CPUState *env)
{
    return 0;
}

#define CPU_PC_FROM_TB(env, tb) do{}while(0)

#include "cpu-all.h"

/*
 * These ones really should go to the appropriate tcg header file, if/when
 * tcg support is added for ia64.
 */
void tcg_dump_info(FILE *f,
                   int (*cpu_fprintf)(FILE *f, const char *fmt, ...));

static inline void cpu_get_tb_cpu_state(CPUState *env, target_ulong *pc,
                                        target_ulong *cs_base, int *flags)
{
    *pc = 0;
    *cs_base = 0;
    *flags = 0;
}

#endif
