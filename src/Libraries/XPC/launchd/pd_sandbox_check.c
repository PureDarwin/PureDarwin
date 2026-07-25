/*
 * PureDarwin: see System-compat/sandbox.h for why this exists - real
 * sandbox_check() needs a kernel Sandbox.kext MAC-framework policy PD
 * doesn't run. Returning 0 (allowed) unconditionally is the real,
 * documented sandbox_check() behavior for "no active sandbox profile",
 * which matches PD's actual current state.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "sandbox.h"

int
sandbox_check(pid_t pid, const char *operation, sandbox_filter_type_t type, ...)
{
    (void)pid;
    (void)operation;
    (void)type;
    return 0;
}

int
sandbox_init(const char *profile, uint64_t flags, char **errorbuf)
{
    (void)profile;
    (void)flags;
    if (errorbuf != NULL) {
        *errorbuf = NULL;
    }
    return 0;
}
