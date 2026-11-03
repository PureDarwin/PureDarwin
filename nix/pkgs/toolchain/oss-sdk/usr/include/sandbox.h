/*
 * sandbox.h (PureDarwin stub) - libsandbox is source-available, not open
 * source, so neither it nor this header exists here. Used by dyld (via
 * sandbox/private.h, which includes it) and launchd.
 */
#ifndef PUREDARWIN_SANDBOX_STUB_H
#define PUREDARWIN_SANDBOX_STUB_H

#include <stdint.h>

__BEGIN_DECLS

#define SANDBOX_NAMED (1 << 0)

int  sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
void sandbox_free_error(char *errorbuf);

__END_DECLS

#endif /* PUREDARWIN_SANDBOX_STUB_H */
