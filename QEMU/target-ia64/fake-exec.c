/*
 * fake-exec.c for ia64.
 *
 * This is a file for stub functions so that compilation is possible
 * when TCG CPU emulation is disabled during compilation.
 *
 * Copyright 2007 IBM Corporation.
 * Added by & Authors:
 * 	Jerone Young <jyoung5@us.ibm.com>
 *
 * Copyright 2008 Intel Corporation.
 * Added by Xiantao Zhang <xiantao.zhang@intel.com>
 *
 * This work is licensed under the GNU GPL licence version 2 or later.
 *
 */
#include <stdio.h>

#include "cpu.h"
#include "exec-all.h"

int code_copy_enabled = 0;

void cpu_gen_init(void)
{
}

unsigned long code_gen_max_block_size(void)
{
    return 32;
}

int cpu_ia64_gen_code(CPUState *env, TranslationBlock *tb, int *gen_code_size_ptr)
{
    return 0;
}

void tcg_dump_info(FILE *f,
                   int (*cpu_fprintf)(FILE *f, const char *fmt, ...))
{
    return;
}

int cpu_restore_state(TranslationBlock *tb,
                      CPUState *env, unsigned long searched_pc,
                      void *puc)

{
    return 0;
}
