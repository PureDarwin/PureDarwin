/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/*
 * LZVN buffer decode, over the vendored lzfse decoder (BSD-3-Clause, see
 * third_party/lzfse/). lzfse_internal.h declares lzvn_decode_buffer() but
 * upstream never defines it, so drive the state-based lzvn_decode() instead.
 */

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
