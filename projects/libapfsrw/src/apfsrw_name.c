/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

// Names as APFS hashes and compares them on case- and normalization-insensitive volumes
// (spec p.78-79): canonical decomposition (NFD), full case folding when the volume folds case

#include "apfsrw/apfsrw.h"
// kernel shims before utf8proc: its own malloc/free must land on the kext allocator
#include "apfsrw/apfsrw_port.h"

// vendored as-is: none of our warnings apply to it
#define UTF8PROC_STATIC
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "utf8proc.c"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

// a 255 byte name decomposes to a few code points per byte at most
#define APFSRW_FOLD_MAX 1024

long apfsrw_name_fold(const char *name, size_t len, int fold, uint32_t *out, long max)
{
    utf8proc_ssize_t n;

    n = utf8proc_decompose((const utf8proc_uint8_t *)name, (utf8proc_ssize_t)len,
        (utf8proc_int32_t *)out, (utf8proc_ssize_t)max,
        (utf8proc_option_t)(UTF8PROC_DECOMPOSE | (fold ? UTF8PROC_CASEFOLD : 0)));
    if (n < 0 || n > max)
        return -1;
    return (long)n;
}

static int is_ascii(const char *s, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if ((unsigned char)s[i] >= 0x80)
            return 0;
    }

    return 1;
}

static int ascii_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int apfsrw_name_equal(const char *a, size_t alen, const char *b, size_t blen, int fold)
{
    uint32_t *fa, *fb;
    long na, nb;
    int eq;

    if (alen == blen && memcmp(a, b, alen) == 0)
        return 1;

    if (is_ascii(a, alen) && is_ascii(b, blen)) {
        size_t i;

        if (!fold || alen != blen)
            return 0;

        for (i = 0; i < alen; i++) {
            if (ascii_lower((unsigned char)a[i]) != ascii_lower((unsigned char)b[i]))
                return 0;
        }
        return 1;
    }

    fa = (uint32_t *)malloc(2 * APFSRW_FOLD_MAX * sizeof(uint32_t));
    if (fa == NULL)
        return 0;

    fb = fa + APFSRW_FOLD_MAX;
    na = apfsrw_name_fold(a, alen, fold, fa, APFSRW_FOLD_MAX);
    nb = apfsrw_name_fold(b, blen, fold, fb, APFSRW_FOLD_MAX);
    eq = na >= 0 && na == nb && memcmp(fa, fb, (size_t)na * sizeof(uint32_t)) == 0;
    free(fa);
    return eq;
}
