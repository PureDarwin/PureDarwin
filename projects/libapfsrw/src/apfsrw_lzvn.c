/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include "apfsrw_lzvn.h"

#include <string.h>

#include "lzvn_decode_base.h"

size_t apfsrw_lzvn_decode(void *dst, size_t dst_size, const void *src,
    size_t src_size)
{
    lzvn_decoder_state state;

    if (dst == NULL || src == NULL || dst_size == 0 || src_size == 0)
        return 0;

    memset(&state, 0, sizeof(state));
    state.src = (const unsigned char *)src;
    state.src_end = (const unsigned char *)src + src_size;
    state.dst = (unsigned char *)dst;
    state.dst_begin = (unsigned char *)dst;
    state.dst_end = (unsigned char *)dst + dst_size;
    state.dst_current = (unsigned char *)dst;

    lzvn_decode(&state);
    return (size_t)(state.dst - (unsigned char *)dst);
}
