/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#ifndef APFSRW_LZVN_H
#define APFSRW_LZVN_H

#include <stddef.h>

/* Returns the number of bytes written to dst, or 0 on failure. */
size_t apfsrw_lzvn_decode(void *dst, size_t dst_size, const void *src,
    size_t src_size);

#endif /* APFSRW_LZVN_H */
