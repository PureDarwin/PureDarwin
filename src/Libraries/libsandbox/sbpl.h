/*
 * sbpl.h - Sandbox Profile Language compiler.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#ifndef PUREDARWIN_SBPL_H
#define PUREDARWIN_SBPL_H

#include <stddef.h>

/*
 * Compiles an SBPL profile into the kernel's format. parameters is a
 * NULL-terminated list of name/value pairs for (param "name"). On failure
 * returns -1 and sets *error to a malloc'd message.
 */
int sbpl_compile(const char *source, const char *const parameters[],
    void **profile, size_t *size, char **error);

#endif /* PUREDARWIN_SBPL_H */
