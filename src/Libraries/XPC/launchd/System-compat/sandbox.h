#ifndef _PD_SANDBOX_COMPAT_H_
#define _PD_SANDBOX_COMPAT_H_

#include <sys/types.h>

typedef int sandbox_filter_type_t;

#define SANDBOX_FILTER_NONE        ((sandbox_filter_type_t)0)
#define SANDBOX_FILTER_PATH        ((sandbox_filter_type_t)1)
#define SANDBOX_FILTER_GLOBAL_NAME ((sandbox_filter_type_t)2)
#define SANDBOX_FILTER_LOCAL_NAME  ((sandbox_filter_type_t)3)

int sandbox_check(pid_t pid, const char *operation, sandbox_filter_type_t type, ...);
int sandbox_init(const char *profile, uint64_t flags, char **errorbuf);

/* notifyd's pathwatch.c: real per-audit-token sandbox check variant. */
#include <mach/message.h>
#define SANDBOX_CHECK_NO_REPORT 0x0001
/*
 * diskarbitrationd passes this to sandbox_check_by_audit_token(). The numeric
 * value is inert here - the PureDarwin sandbox_check* report "allowed"
 * regardless of flags, because there is no Sandbox.kext policy to consult - so
 * this is a distinct bit rather than a verified copy of Apple's value.
 */
#define SANDBOX_CHECK_ALLOW_APPROVAL 0x0002
int sandbox_check_by_audit_token(audit_token_t audit, const char *operation, sandbox_filter_type_t type, ...);

/*
 * Chain to the SDK's sandbox.h when there is one. Targets built -nostdinc
 * (configd) have no later sandbox.h on the path, and the declarations above are
 * all they need.
 */
#if __has_include_next(<sandbox.h>)
#include_next <sandbox.h>
#endif

#endif /* _PD_SANDBOX_COMPAT_H_ */
