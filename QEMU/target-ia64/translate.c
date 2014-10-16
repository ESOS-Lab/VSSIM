/*
 *  translation.c : IA64 translation code.
 *  Just put it as blank now, and implement it later.
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static uint16_t *gen_opc_ptr;

#include "cpu.h"
#include "exec-all.h"
#include "disas.h"
#include "gen-op.h"

int gen_intermediate_code(CPUState *env, TranslationBlock *tb)
{
    return 0;
}
int gen_intermediate_code_pc(CPUState *env, TranslationBlock *tb)
{
    return 0;
}
