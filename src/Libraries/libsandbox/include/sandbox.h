/*
 * sandbox.h - sandbox_init(3).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#ifndef _SANDBOX_H_
#define _SANDBOX_H_

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

/* The profile argument names a built-in profile rather than holding SBPL. */
#define SANDBOX_NAMED 0x0001

extern const char kSBXProfileNoInternet[];
extern const char kSBXProfileNoNetwork[];
extern const char kSBXProfileNoWrite[];
extern const char kSBXProfileNoWriteExceptTemporary[];
extern const char kSBXProfilePureComputation[];

int sandbox_init(const char *profile, unsigned long flags, char **errorbuf);
int sandbox_init_with_parameters(const char *profile, uint64_t flags,
    const char *const parameters[], char **errorbuf);
void sandbox_free_error(char *errorbuf);

__END_DECLS

#endif /* _SANDBOX_H_ */
