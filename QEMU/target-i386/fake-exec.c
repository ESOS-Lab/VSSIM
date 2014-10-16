/*
 * fake-exec.c
 *
 * This is a file for stub functions so that compilation is possible
 * when TCG CPU emulation is disabled during compilation.
 *
 * Copyright 2007 IBM Corporation.
 * Added by & Authors:
 * 	Jerone Young <jyoung5@us.ibm.com>
 * This work is licensed under the GNU GPL licence version 2 or later.
 *
 */
#include "exec.h"
#include "cpu.h"

int code_copy_enabled = 0;

CCTable cc_table[CC_OP_NB];

void cpu_dump_statistics (CPUState *env, FILE*f,
                          int (*cpu_fprintf)(FILE *f, const char *fmt, ...),
                          int flags)
{
}

unsigned long code_gen_max_block_size(void)
{
    return 32;
}

void cpu_gen_init(void)
{
}

int cpu_restore_state(TranslationBlock *tb,
                      CPUState *env, unsigned long searched_pc,
                      void *puc)

{
    return 0;
}

int cpu_x86_gen_code(CPUState *env, TranslationBlock *tb, int *gen_code_size_ptr)
{
    return 0;
}

void flush_icache_range(unsigned long start, unsigned long stop)
{
}

void optimize_flags_init(void)
{
}
