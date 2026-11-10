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

// where named profiles live and relative (import "x.sb") names resolve
#define SBPL_PROFILE_DIR "/System/Library/Sandbox/Profiles"

/*
 * Compiles an SBPL profile into the kernel's format. parameters is a
 * NULL-terminated list of name/value pairs for (param "name"). import_dir
 * overrides SBPL_PROFILE_DIR when not NULL. On failure returns -1 and sets
 * *error to a malloc'd message.
 */
int sbpl_compile(const char *source, const char *const parameters[], const char *import_dir,
    void **profile, size_t *size, char **error);

// reads a whole profile file into a malloc'd NUL-terminated string, NULL with errno on failure
char *sbpl_read_file(const char *path);

#endif /* PUREDARWIN_SBPL_H */
