/*
 * PureDarwin: real os/assumes.c (os_assumes/os_assert failure logging)
 * composes its crash-report message through the real os_log firehose
 * private ABI (os_log_pack_t, firehose tracepoint plumbing) - a genuinely
 * separate subsystem this tree doesn't back with a running logd/
 * ReportCrash, the same category as the already-documented os_log_create
 * stub in pd_libSystem_compat.c. Rather than pull in that whole private
 * ABI for a message that's discarded with no daemon to receive it, this
 * gives os_assumes()/os_assert() real fprintf(stderr, ...)-based failure
 * reporting (the actual fallback real Apple's own os_assumes() takes on
 * the CrashReporterClient-message path when composing failed) and calls
 * the real CRSetCrashLogMessage()/abort() for the fatal path - genuine
 * behavior, just without the firehose tracepoint machinery.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <CrashReporterClient.h>

void
_os_assumes_log(uint64_t code)
{
    fprintf(stderr, "PureDarwin: os_assumes failure (code 0x%llx)\n", (unsigned long long)code);
}

void
_os_assumes_log_ctx(void *callout, void *ctx, uint64_t code)
{
    (void)callout;
    (void)ctx;
    fprintf(stderr, "PureDarwin: os_assumes failure with context (code 0x%llx)\n", (unsigned long long)code);
}

char *
_os_assert_log(uint64_t code)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "os_assert failure (code 0x%llx)", (unsigned long long)code);
    fprintf(stderr, "PureDarwin: %s\n", buf);
    return buf;
}

void
_os_assert_log_ctx(void *callout, void *ctx, uint64_t code)
{
    (void)callout;
    (void)ctx;
    fprintf(stderr, "PureDarwin: os_assert failure with context (code 0x%llx)\n", (unsigned long long)code);
}

void
_os_crash(const char *message)
{
    fprintf(stderr, "PureDarwin: fatal: %s\n", message);
    CRSetCrashLogMessage(message);
    abort();
}
