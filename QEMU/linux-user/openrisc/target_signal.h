#ifndef OPENRISC_TARGET_SIGNAL_H
#define OPENRISC_TARGET_SIGNAL_H

#include "cpu.h"

/* this struct defines a stack used during syscall handling */

typedef struct target_sigaltstack {
    abi_long ss_sp;
    abi_ulong ss_size;
    abi_long ss_flags;
} target_stack_t;

/* sigaltstack controls  */
#define TARGET_SS_ONSTACK     1
#define TARGET_SS_DISABLE     2

#define TARGET_MINSIGSTKSZ    2048
#define TARGET_SIGSTKSZ       8192

static inline abi_ulong get_sp_from_cpustate(CPUOpenRISCState *state)
{
    return state->gpr[1];
}


#endif /* OPENRISC_TARGET_SIGNAL_H */
