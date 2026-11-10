// dirhelper_priv.h - libSystem's per-user directory helpers, see pd_dirhelper.c
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DIRHELPER_USER_LOCAL = 0,
    DIRHELPER_USER_LOCAL_TEMP,
    DIRHELPER_USER_LOCAL_CACHE,
} dirhelper_which_t;

bool _set_user_dir_suffix(const char *suffix);
char *_dirhelper(dirhelper_which_t which, char *path, size_t pathlen);
