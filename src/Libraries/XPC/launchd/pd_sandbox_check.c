/*
 * PureDarwin: sandbox_check() would ask Sandbox.kext whether an operation is
 * allowed, and that query interface does not exist yet. Returning 0 (allowed)
 * is the documented answer for a process with no profile. sandbox_init() itself
 * is real and lives in src/Libraries/libsandbox.
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

/*
 * Per-audit-token variant, used by notifyd's pathwatch.c and by
 * diskarbitrationd's DAServer.c to ask whether a client may mount at a path.
 * Same reasoning as sandbox_check() above: with no Sandbox.kext policy to
 * consult, "allowed" is the documented no-profile behaviour.
 */
int
sandbox_check_by_audit_token(audit_token_t audit, const char *operation,
                             sandbox_filter_type_t type, ...)
{
    (void)audit;
    (void)operation;
    (void)type;
    return 0;
}
