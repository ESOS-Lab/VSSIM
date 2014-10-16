
#include "config.h"
#include "config-host.h"

#include "exec.h"

#include "qemu-kvm.h"

void qemu_kvm_call_with_env(void (*func)(void *), void *data, CPUState *newenv)
{
    CPUState *oldenv;
#define DECLARE_HOST_REGS
#include "hostregs_helper.h"

    oldenv = newenv;

#define SAVE_HOST_REGS
#include "hostregs_helper.h"

    env = newenv;

    env_to_regs();
    func(data);
    regs_to_env();

    env = oldenv;

#include "hostregs_helper.h"
}

static void call_helper_cpuid(void *junk)
{
    helper_cpuid();
}

void qemu_kvm_cpuid_on_env(CPUState *env)
{
    qemu_kvm_call_with_env(call_helper_cpuid, NULL, env);
}

