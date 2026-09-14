// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2022 Corellium LLC
 *
 * Author: Ernesto A. Fernández <ernesto@corellium.com>
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libzbitmap.h"

#define MIN(x, y)   ((x) > (y) ? (y) : (x))

#define ZBM_MAGIC       "ZBM\x09"
#define ZBM_MAGIC_SZ    4

#define ZBM_MAX_DECMP_CHUNK_SIZE        0x8000
#define ZBM_MAX_DECMP_CHUNK_SIZE_BITS   15

struct uint24 {
    uint8_t     low;
    uint8_t     mid;
    uint8_t     hig;
};

/* This header is shared by both compressed and decompressed chunks */
struct zbm_chunk_hdr {
    struct uint24   len;        /* Length of the chunk */
    struct uint24   decmp_len;  /* Length of the chunk after decompression */
};

/* The full header for compressed chunks */
struct zbm_cmp_chunk_hdr {
    /* Shared with decompressed chunks */
    struct zbm_chunk_hdr hdr;

    /* Offset for each of the three metadata areas */
    struct uint24   meta_off_1;
    struct uint24   meta_off_2;
    struct uint24   meta_off_3;
};

/* Pointer to a half-byte */
struct nybl_ptr {
    uint8_t         *addr;  /* Address of the byte */
    int             nibble; /* Which of the two nibbles? */
};

/* 0-2 and 0xf are not real bitmap indexes */
#define ZBM_BITMAP_COUNT        (16 - 1 - 3)
#define ZBM_BITMAP_BASE         3
#define ZBM_BITMAP_BYTECNT      17
#define ZBM_MAX_PERIOD_BYTECNT  2

struct zbm_bmap {
    uint8_t     bitmap;         /* The bitmap */
    uint8_t     period_bytecnt; /* Read this many bytes to get the new period */
};

struct zbm_state {
    /* Updated during a chunk read */
    uint8_t         *dest;      /* Write the next byte here */
    size_t          dest_left;  /* Room left in destination buffer */
    uint32_t        written;    /* Bytes written so far for current chunk */
    uint16_t        period;     /* Repetition period for decompression, in bytes */

    /* Updated right before a chunk read */
    const uint8_t   *src_end;   /* End of current chunk */
    uint32_t        len;        /* Length of the chunk */
    uint32_t        decmp_len;  /* Expected chunk length after decompression */

    /* Updated after a chunk read */
    const uint8_t   *src;       /* Start of buffer, or current chunk if any */
    size_t          src_left;   /* Room left in the source buffer */
    size_t          prewritten; /* Bytes written for previous chunks */

    /* Current position in data and metadata areas for this chunk */
    const uint8_t   *data;
    const uint8_t   *meta_1;
    const uint8_t   *meta_2;
    struct nybl_ptr meta_3;

    /* Array of bitmaps for the current chunk */
    struct zbm_bmap bitmaps[ZBM_BITMAP_COUNT];
};

static int zbm_check_magic(struct zbm_state *state)
{
    if(state->src_left < ZBM_MAGIC_SZ)
        return ZBM_INVAL;

    if(memcmp(state->src, ZBM_MAGIC, ZBM_MAGIC_SZ))
        return ZBM_INVAL;

    state->src += ZBM_MAGIC_SZ;
    state->src_left -= ZBM_MAGIC_SZ;
    return 0;
}

static uint32_t zbm_u24_to_u32(struct uint24 n)
{
    uint32_t res;

    res = n.hig;
    res <<= 8;
    res += n.mid;
    res <<= 8;
    res += n.low;
    return res;
}

/* Some chunks just have regular uncompressed data, but with a header */
static int zbm_chunk_is_uncompressed(struct zbm_state *state)
{
    return state->len == state->decmp_len + sizeof(struct zbm_chunk_hdr);
}

static int zbm_handle_uncompressed_chunk(struct zbm_state *state)
{
    state->meta_1 = state->meta_2 = NULL;
    state->meta_3.addr = NULL;
    state->meta_3.nibble = 0;
    state->data = state->src + sizeof(struct zbm_chunk_hdr);
    memcpy(state->dest, state->data, state->decmp_len);

    state->dest += state->decmp_len;
    state->dest_left -= state->decmp_len;
    state->written = state->decmp_len;
    return 0;
}

static int zbm_read_nibble(struct nybl_ptr *nybl, const uint8_t *limit, uint8_t *result)
{
    if(nybl->addr >= limit)
        return ZBM_INVAL;

    if(nybl->nibble == 0) {
        *result = *nybl->addr & 0xf;
        nybl->nibble = 1;
    } else {
        *result = (*nybl->addr >> 4) & 0xf;
        nybl->nibble = 0;
        ++nybl->addr;
    }
    return 0;
}

static void zbm_rewind_nibble(struct nybl_ptr *nybl)
{
    if(nybl->nibble == 0) {
        nybl->nibble = 1;
        --nybl->addr;
    } else {
        nybl->nibble = 0;
    }
}

static int zbm_apply_bitmap(struct zbm_state *state, struct zbm_bmap *bitmap)
{
    int i;

    /* The periods are stored in the first metadata area */
    if(bitmap->period_bytecnt) {
        state->period = 0;
        for(i = 0; i < bitmap->period_bytecnt; ++i) {
            if(state->meta_1 >= state->src_end)
                return ZBM_INVAL;
            state->period |= *state->meta_1 << i * 8;
            ++state->meta_1;
        }
    }
    if(state->period == 0)
        return ZBM_INVAL;

    for(i = 0; i < 8; ++i) {
        if(state->written == state->decmp_len)
            break;
        if(bitmap->bitmap & 1 << i) {
            if(state->data >= state->src_end)
                return ZBM_INVAL;
            *state->dest = *state->data;
            ++state->data;
        } else {
            if(state->prewritten + state->written < state->period)
                return ZBM_INVAL;
            *state->dest = *(state->dest - state->period);
        }
        ++state->dest;
        --state->dest_left;
        ++state->written;
    }

    return 0;
}

static int zbm_apply_bitmap_number(struct zbm_state *state, uint8_t bmp_num)
{
    struct zbm_bmap next = {0};

    /* Not a valid bitmap number (it signals a repetition) */
    if(bmp_num == 0xf)
        return ZBM_INVAL;

    /* An actual index in the bitmap array */
    if(bmp_num > ZBM_MAX_PERIOD_BYTECNT)
        return zbm_apply_bitmap(state, &state->bitmaps[bmp_num - ZBM_BITMAP_BASE]);

    /* For < 2, use the next bitmap in the second metadata area */
    if(state->meta_2 >= state->src_end)
        return ZBM_INVAL;
    next.bitmap = *state->meta_2;
    next.period_bytecnt = bmp_num;
    ++state->meta_2;
    return zbm_apply_bitmap(state, &next);
}

/* Find out how many times we need to repeat the current bitmap operation */
static int zbm_read_repetition_count(struct zbm_state *state, uint16_t *repeat)
{
    uint8_t nibble;
    uint16_t total;
    int err;

    /* Don't confuse the trailing bitmaps with a repetition count */
    if(state->decmp_len - state->written <= 8) {
        *repeat = 1;
        return 0;
    }

    err = zbm_read_nibble(&state->meta_3, state->src_end, &nibble);
    if(err)
        return err;

    if(nibble != 0xf) {
        /* No repetition count: the previous bitmap number gets applied once */
        zbm_rewind_nibble(&state->meta_3);
        *repeat = 1;
        return 0;
    }

    /*
     * Under this scheme, repeating a bitmap number 3 times wouldn't save any
     * space, so the repetition count starts from 4.
     */
    total = 4;
    while(nibble == 0xf) {
        err = zbm_read_nibble(&state->meta_3, state->src_end, &nibble);
        if(err)
            return err;
        total += nibble;
        if(total < nibble)
            return ZBM_INVAL;
    }

    *repeat = total;
    return 0;
}

static int zbm_decompress_single_bitmap(struct zbm_state *state)
{
    uint8_t bmp_num;
    uint16_t repeat;
    int i;
    int err;

    /* The current nibble is the offset of the next bitmap to apply */
    err = zbm_read_nibble(&state->meta_3, state->src_end, &bmp_num);
    if(err)
        return err;

    err = zbm_read_repetition_count(state, &repeat);
    if(err)
        return err;

    for(i = 0; i < repeat; ++i) {
        err = zbm_apply_bitmap_number(state, bmp_num);
        if(err)
            return err;
    }
    return 0;
}

/* Pointer to a bit */
struct bit_ptr {
    uint8_t         *addr;  /* Address of the byte */
    int             offset; /* Bit number */
};

/* This function does not perform boundary checks, the caller must do it */
static int zbm_read_single_bit(struct bit_ptr *bit)
{
    int res = *bit->addr >> bit->offset & 1;

    ++bit->offset;
    if(bit->offset != 8)
        return res;
    bit->offset = 0;
    ++bit->addr;
    return res;
}

static int zbm_read_single_bitmap(struct bit_ptr *bit, const uint8_t *limit, struct zbm_bmap *result)
{
    int i;

    result->bitmap = 0;
    result->period_bytecnt = 0;

    /* The bitmap itself */
    for(i = 0; i < 8; ++i) {
        if(bit->addr >= limit)
            return ZBM_INVAL;
        result->bitmap |= zbm_read_single_bit(bit) << i;
    }

    /*
     * The two trailing bits tell us how many bytes to read for the next
     * repetition period
     */
    for(i = 0; i < 2; ++i) {
        if(bit->addr >= limit)
            return ZBM_INVAL;
        result->period_bytecnt |= zbm_read_single_bit(bit) << i;
    }

    return 0;
}

static int zbm_read_bitmaps(struct zbm_state *state)
{
    struct bit_ptr bmap = {0};
    int err, i;

    if(state->len < ZBM_BITMAP_BYTECNT)
        return ZBM_INVAL;

    bmap.addr = (uint8_t *)state->src_end - ZBM_BITMAP_BYTECNT;
    bmap.offset = 0;

    for(i = 0; i < ZBM_BITMAP_COUNT; ++i) {
        err = zbm_read_single_bitmap(&bmap, state->src_end, &state->bitmaps[i]);
        if(err)
            return err;
        if(state->bitmaps[i].period_bytecnt > ZBM_MAX_PERIOD_BYTECNT)
            return ZBM_INVAL;
    }
    return 0;
}

static int zbm_handle_compressed_chunk(struct zbm_state *state)
{
    const struct zbm_cmp_chunk_hdr *hdr = NULL;
    uint32_t meta_off_1, meta_off_2, meta_off_3;
    int err;

    state->written = 0;
    state->period = 8;

    if(state->len < sizeof(*hdr))
        return ZBM_INVAL;
    hdr = (struct zbm_cmp_chunk_hdr *)state->src;
    state->data = state->src + sizeof(*hdr);

    meta_off_1 = zbm_u24_to_u32(hdr->meta_off_1);
    meta_off_2 = zbm_u24_to_u32(hdr->meta_off_2);
    meta_off_3 = zbm_u24_to_u32(hdr->meta_off_3);
    if(meta_off_1 >= state->len || meta_off_2 >= state->len || meta_off_3 >= state->len)
        return ZBM_INVAL;
    state->meta_1 = state->src + meta_off_1;
    state->meta_2 = state->src + meta_off_2;
    state->meta_3.addr = (uint8_t *)state->src + meta_off_3;
    state->meta_3.nibble = 0;

    err = zbm_read_bitmaps(state);
    if(err)
        return err;

    while(state->written < state->decmp_len) {
        err = zbm_decompress_single_bitmap(state);
        if(err)
            return err;
    }

    return 0;
}

static int zbm_handle_chunk(struct zbm_state *state)
{
    const struct zbm_chunk_hdr *decmp_hdr = NULL;

    if(state->src_left < sizeof(*decmp_hdr))
        return ZBM_INVAL;
    decmp_hdr = (struct zbm_chunk_hdr *)state->src;

    state->len = zbm_u24_to_u32(decmp_hdr->len);
    if(state->len > state->src_left)
        return ZBM_INVAL;
    state->src_end = state->src + state->len;

    state->decmp_len = zbm_u24_to_u32(decmp_hdr->decmp_len);
    if(state->decmp_len > ZBM_MAX_DECMP_CHUNK_SIZE)
        return ZBM_INVAL;
    if(!state->dest) /* We just wanted the length, so we are done */
        return 0;
    if(state->decmp_len > state->dest_left)
        return ZBM_RANGE;

    if(zbm_chunk_is_uncompressed(state))
        return zbm_handle_uncompressed_chunk(state);

    return zbm_handle_compressed_chunk(state);
}

int zbm_decompress(void *dest, size_t dest_size, const void *src, size_t src_size, size_t *out_len)
{
    struct zbm_state state = {0};
    int err;

    state.src = src;
    state.src_left = src_size;
    state.dest = dest;
    state.dest_left = dest_size;
    state.prewritten = 0;

    err = zbm_check_magic(&state);
    if(err)
        return err;

    /* The final chunk has zero decompressed length */
    do {
        err = zbm_handle_chunk(&state);
        if(err)
            return err;
        state.src += state.len;
        state.src_left -= state.len;
        state.prewritten += state.decmp_len;
    } while(state.decmp_len != 0);

    *out_len = state.prewritten;
    return 0;
}

#define ZBM_MAX_BITMAP_COUNT    (ZBM_MAX_DECMP_CHUNK_SIZE >> 3)
#define ZBM_POSSIBLE_BMPROTS    (1 << 10)

struct zbm_compress_state {
    /* Updated during a chunk write */
    const uint8_t   *src;       /* Next byte to read */
    uint32_t        read;       /* Bytes processed so far for current chunk */
    uint32_t        written;    /* Bytes written so far for current chunk */
    uint8_t         *dest;      /* Write the next byte here */
    uint16_t        period;     /* Repetition period for compression, in bytes */

    /* Updated right before a chunk write */
    const uint8_t   *dest_end;  /* Maximum limit for the current chunk */
    const uint8_t   *src_end;   /* End of current decompressed chunk */
    uint32_t        decmp_len;  /* Decompressed length of the chunk */

    /* Updated after a chunk write */
    size_t          dest_left;  /* Room left in destination buffer */
    size_t          preread;    /* Bytes processed for previous chunks */
    size_t          prewritten; /* Bytes written for previous chunks */
    size_t          src_left;   /* Room left in the source buffer */

    /* Array of all bitmaps applied for the current chunk */
    struct zbm_bmap bitmaps[ZBM_MAX_BITMAP_COUNT];
    uint16_t        periods[ZBM_MAX_BITMAP_COUNT];
    int             bmp_cnt;

    /* How many times was each bitmap-rotation combination applied? */
    uint64_t        usecnts[ZBM_POSSIBLE_BMPROTS];

    /* Array of bitmaps applied most often */
    struct zbm_bmap top_bitmaps[ZBM_BITMAP_COUNT];
};

/* Internal error used when a compression attempt was ineffective */
#define ZBM_CANT_COMPRESS   (-1024)

static int zbm_bmprot(struct zbm_bmap *bmap)
{
    return (int)bmap->bitmap << 2 | (int)bmap->period_bytecnt;
}

static int zbm_write_magic(struct zbm_compress_state *state)
{
    if(state->dest_left < ZBM_MAGIC_SZ)
        return ZBM_INVAL;

    memcpy(state->dest, ZBM_MAGIC, ZBM_MAGIC_SZ);

    state->dest += ZBM_MAGIC_SZ;
    state->dest_left -= ZBM_MAGIC_SZ;
    state->prewritten += ZBM_MAGIC_SZ;
    return 0;
}

static int zbm_build_uncompressed_chunk(struct zbm_compress_state *state)
{
    /* Undo the previous compression attempt */
    state->src -= state->read;
    state->read = 0;
    state->dest -= state->written - sizeof(struct zbm_chunk_hdr);
    state->written = sizeof(struct zbm_chunk_hdr);

    memcpy(state->dest, state->src, state->decmp_len);

    state->src += state->decmp_len;
    state->read += state->decmp_len;
    state->written += state->decmp_len;
    state->dest += state->decmp_len;
    return 0;
}

static uint8_t zbm_compare_bytes(uint64_t bytes1, uint64_t bytes2)
{
    static const uint64_t mask0 = 0xff;
    static const uint64_t mask1 = 0xff00;
    static const uint64_t mask2 = 0xff0000;
    static const uint64_t mask3 = 0xff000000;
    static const uint64_t mask4 = 0xff00000000;
    static const uint64_t mask5 = 0xff0000000000;
    static const uint64_t mask6 = 0xff000000000000;
    static const uint64_t mask7 = 0xff00000000000000;
    uint8_t diff = 0;
    uint64_t xor;

    xor = bytes1 ^ bytes2;
    diff += (xor & mask0) != 0;
    diff += (xor & mask1) != 0;
    diff += (xor & mask2) != 0;
    diff += (xor & mask3) != 0;
    diff += (xor & mask4) != 0;
    diff += (xor & mask5) != 0;
    diff += (xor & mask6) != 0;
    diff += (xor & mask7) != 0;
    return diff;
}

static void zbm_append_new_bitmap(struct zbm_compress_state *state, uint8_t bitmap, uint16_t period)
{
    struct zbm_bmap *new_bmp = NULL;

    new_bmp = &state->bitmaps[state->bmp_cnt];
    new_bmp->bitmap = bitmap;
    state->periods[state->bmp_cnt] = period;
    if(period == state->period)
        new_bmp->period_bytecnt = 0;
    else if(period <= 0xff)
        new_bmp->period_bytecnt = 1;
    else
        new_bmp->period_bytecnt = 2;
    ++state->bmp_cnt;

    ++state->usecnts[zbm_bmprot(new_bmp)];

    state->period = period;
}

static uint8_t zbm_calculate_bitmap(const uint8_t *bytes1, const uint8_t *bytes2, uint8_t size)
{
    uint8_t bitmap;
    uint8_t i;

    bitmap = 0;
    for(i = 0; i < size; ++i) {
        if(bytes1[i] != bytes2[i])
            bitmap |= 1 << i;
    }
    return bitmap;
}

static void zbm_find_good_pattern(struct zbm_compress_state *state)
{
    uint64_t needle;
    const uint8_t *to_compare = NULL;
    const uint8_t *next = NULL;
    const uint8_t *best = NULL;
    const uint8_t *split = NULL;
    int bytecnt, diff, best_cost;
    uint8_t bitmap;
    int i;

    bytecnt = MIN(8, state->decmp_len - state->read);
    needle = 0;
    for(i = 0; i < bytecnt; ++i)
        needle |= (uint64_t)state->src[i] << i * 8;

    /*
     * We'll estimate the cost of a period as the number of digits needed to
     * store it plus the number of characters that get changed. This isn't
     * perfect of course, because bitmap reuse is also a good idea.
     */

    diff = zbm_compare_bytes(*(uint64_t *)(state->src - state->period), needle);
    best = state->src - state->period;
    best_cost = diff;
    if(best_cost <= 1)
        goto done;

    /* We first look for patterns that can be reached with a 1-byte period */
    to_compare = state->src - MIN(0xff, state->preread + state->read);
    split = to_compare;
    while(to_compare <= state->src - 8) {
        diff = zbm_compare_bytes(*(uint64_t *)to_compare, needle);
        if(diff + 1 < best_cost) {
            best = to_compare;
            best_cost = diff + 1;
            if(best_cost == 1)
                goto done;
        }
        ++to_compare;
    }
    if(best_cost == 2)
        goto done;

    /*
     * We now look for patterns that can be reached with a 2-byte period, but
     * we focus only on the ones that start with the same byte. Checking
     * everything would take too long.
     */
    to_compare = state->src - MIN(0xffff, state->preread + state->read);
    while(to_compare < split) {
        next = memchr(to_compare, state->src[0], split - to_compare);
        if(!next)
            break;
        diff = zbm_compare_bytes(*(uint64_t *)next, needle);
        if(diff + 2 < best_cost) {
            best = next;
            best_cost = diff + 2;
            if(best_cost == 2)
                goto done;
        }
        to_compare = next + 1;
    }

done:
    bitmap = zbm_calculate_bitmap(best, state->src, bytecnt);
    zbm_append_new_bitmap(state, bitmap, state->src - best);
}

static int zbm_write_new_data(struct zbm_compress_state *state)
{
    struct zbm_bmap *bmap = &state->bitmaps[state->bmp_cnt - 1];
    int i;

    for(i = 0; i < 8; ++i) {
        if(state->src == state->src_end)
            break;
        if(state->dest == state->dest_end)
            return ZBM_CANT_COMPRESS;
        if(bmap->bitmap & 1 << i) {
            *state->dest = *state->src;
            ++state->dest;
            ++state->written;
        }
        ++state->src;
        ++state->read;
    }
    return 0;
}

static int zbm_compress_eight_bytes(struct zbm_compress_state *state)
{
    zbm_find_good_pattern(state);
    return zbm_write_new_data(state);
}

static int zbm_build_initial_bitmap(struct zbm_compress_state *state)
{
    struct zbm_bmap *init = &state->bitmaps[0];

    init->bitmap = 0xff;
    init->period_bytecnt = 0;
    state->bmp_cnt = 1;
    state->usecnts[zbm_bmprot(init)] = 1;

    if(state->decmp_len - state->read < 8)
        return ZBM_CANT_COMPRESS;
    memcpy(state->dest, state->src, 8);

    state->dest += 8;
    state->src += 8;
    state->read += 8;
    state->written += 8;
    return 0;
}

static int zbm_write_metadata_1(struct zbm_compress_state *state)
{
    struct zbm_bmap *bmap = NULL;
    uint16_t period;
    int i;

    /* The first metadata area stores the periods, when they change */
    for(i = 0; i < state->bmp_cnt; ++i) {
        bmap = &state->bitmaps[i];
        period = state->periods[i];

        if(bmap->period_bytecnt == 0)
            continue;

        if(state->dest == state->dest_end)
            return ZBM_CANT_COMPRESS;
        *state->dest = period;
        ++state->dest;
        ++state->written;

        if(bmap->period_bytecnt == 1)
            continue;

        if(state->dest == state->dest_end)
            return ZBM_CANT_COMPRESS;
        *state->dest = period >> 8;
        ++state->dest;
        ++state->written;
    }
    return 0;
}

static int zbm_write_metadata_2(struct zbm_compress_state *state)
{
    struct zbm_bmap *bmap = NULL;
    int i;

    /* The second metadata area stores most of the bitmaps */
    for(i = 0; i < state->bmp_cnt; ++i) {
        bmap = &state->bitmaps[i];

        /* This is one of the top bitmaps that go in the end of the chunk */
        if(state->usecnts[zbm_bmprot(bmap)] == 0)
            continue;

        if(state->dest == state->dest_end)
            return ZBM_CANT_COMPRESS;
        *state->dest = bmap->bitmap;
        ++state->dest;
        ++state->written;
    }
    return 0;
}

static int zbm_equal_bmaps(struct zbm_bmap *bmap1, struct zbm_bmap *bmap2)
{
    if(!bmap1 || !bmap2)
        return 0;
    if(bmap1->bitmap != bmap2->bitmap)
        return 0;
    if(bmap1->period_bytecnt != bmap2->period_bytecnt)
        return 0;
    return 1;
}

static int zbm_bmap_num_for_index(struct zbm_compress_state *state, int idx)
{
    struct zbm_bmap *bmap = NULL;
    struct zbm_bmap *top_bmap = NULL;
    int i;

    bmap = &state->bitmaps[idx];

    /* This is one of the top bitmaps that go in the end of the chunk */
    if(state->usecnts[zbm_bmprot(bmap)] == 0) {
        for(i = 0; i < ZBM_BITMAP_COUNT; ++i) {
            top_bmap = &state->top_bitmaps[i];
            if(zbm_equal_bmaps(top_bmap, bmap))
                return i + 3;
        }
    }

    /* This is a regular bitmap from the second metadata area */
    return bmap->period_bytecnt;
}

static int zbm_bmap_num_and_repcount(struct zbm_compress_state *state, int idx, int *repeat)
{
    int bmap_num, next_num;
    int i;

    bmap_num = zbm_bmap_num_for_index(state, idx);
    *repeat = 1;

    for(i = idx + 1; i < state->bmp_cnt; ++i) {
        next_num = zbm_bmap_num_for_index(state, i);
        if(next_num != bmap_num)
            break;
        *repeat += 1;
    }
    return bmap_num;
}

static int zbm_write_nibble(struct nybl_ptr *nybl, const uint8_t *limit, uint8_t val)
{
    if(nybl->addr >= limit)
        return ZBM_CANT_COMPRESS;

    if(nybl->nibble == 0) {
        *nybl->addr = val;
        nybl->nibble = 1;
    } else {
        *nybl->addr |= val << 4;
        nybl->nibble = 0;
        ++nybl->addr;
    }
    return 0;
}

static int zbm_write_repetition_count(struct nybl_ptr *nybl, const uint8_t *limit, int repeat)
{
    int nibble = 0xf;
    int err;

    /* 0xf marks that this is a repetition */
    err = zbm_write_nibble(nybl, limit, nibble);
    if(err)
        return err;

    /* We count from the minimum possible repetition count */
    repeat -= 4;

    while(repeat > 0) {
        nibble = MIN(repeat, 0xf);
        err = zbm_write_nibble(nybl, limit, nibble);
        if(err)
            return err;
        repeat -= nibble;
    }

    /* The repetition must always end in a non-0xf nibble */
    if(nibble == 0xf) {
        err = zbm_write_nibble(nybl, limit, 0);
        if(err)
            return err;
    }
    return 0;
}

static int zbm_write_metadata_3(struct zbm_compress_state *state)
{
    struct nybl_ptr nybl = {0};
    int to_write, repeat;
    int i, j;
    int err;

    nybl.addr = state->dest;
    nybl.nibble = 0;

    for(i = 0; i < state->bmp_cnt; i += repeat) {
        to_write = zbm_bmap_num_and_repcount(state, i, &repeat);

        err = zbm_write_nibble(&nybl, state->dest_end, to_write);
        if(err)
            return err;

        /* Fewer than 3 repetitions are done trivially */
        if(repeat <= 3) {
            for(j = 1; j < repeat; ++j) {
                err = zbm_write_nibble(&nybl, state->dest_end, to_write);
                if(err)
                    return err;
            }
        } else {
            err = zbm_write_repetition_count(&nybl, state->dest_end, repeat);
            if(err)
                return err;
        }
    }

    /* Leave the trailing nibble alone and move on to the next byte */
    if(nybl.nibble == 1) {
        ++nybl.addr;
        nybl.nibble = 0;
    }
    state->written += nybl.addr - state->dest;
    state->dest = nybl.addr;
    return 0;
}

/* This function does not perform boundary checks, the caller must do it */
static void zbm_write_single_bit(struct bit_ptr *bit, int val)
{
    *bit->addr |= val << bit->offset;

    ++bit->offset;
    if(bit->offset != 8)
        return;
    bit->offset = 0;
    ++bit->addr;
    return;
}

/* This function does not perform boundary checks, the caller must do it */
static void zbm_write_single_bitmap(struct bit_ptr *bit, struct zbm_bmap bmap)
{
    int i;

    /* The bitmap itself */
    for(i = 0; i < 8; ++i)
        zbm_write_single_bit(bit, bmap.bitmap >> i & 1);

    /* The trailing bits for the period bytecount */
    for(i = 0; i < 2; ++i)
        zbm_write_single_bit(bit, bmap.period_bytecnt >> i & 1);
}

static int zbm_write_trailing_bitmaps(struct zbm_compress_state *state)
{
    struct bit_ptr bmap = {0};
    int i;

    if(state->dest_end - state->dest < ZBM_BITMAP_BYTECNT)
        return ZBM_CANT_COMPRESS;
    memset(state->dest, 0, ZBM_BITMAP_BYTECNT);

    bmap.addr = state->dest;
    bmap.offset = 0;

    for(i = 0; i < ZBM_BITMAP_COUNT; ++i)
        zbm_write_single_bitmap(&bmap, state->top_bitmaps[i]);

    state->dest += ZBM_BITMAP_BYTECNT;
    state->written += ZBM_BITMAP_BYTECNT;
    return 0;
}

static struct uint24 zbm_u32_to_u24(uint32_t n)
{
    struct uint24 res;

    res.low = n;
    res.mid = n >> 8;
    res.hig = n >> 16;
    return res;
}

struct top_bmap {
    struct zbm_bmap *bmap;
    uint64_t        usecnt;
};

/* The array is sorted by use count, in descending order */
static void zbm_insert_in_top_bmaps(struct zbm_bmap *bmap, uint64_t usecnt, struct top_bmap tops[ZBM_BITMAP_COUNT])
{
    int i;

    for(i = 0; i < ZBM_BITMAP_COUNT; ++i) {
        if(zbm_equal_bmaps(tops[i].bmap, bmap)) /* Already in the array */
            return;
        if(tops[i].usecnt < usecnt)
            break;
    }

    if(i == ZBM_BITMAP_COUNT)
        return;

    memmove(&tops[i + 1], &tops[i], (ZBM_BITMAP_COUNT - i - 1) * sizeof(tops[i]));
    tops[i].bmap = bmap;
    tops[i].usecnt = usecnt;
}

static void zbm_find_most_common_bitmaps(struct zbm_compress_state *state)
{
    struct top_bmap tops[ZBM_BITMAP_COUNT] = {0};
    struct zbm_bmap *bmap = NULL;
    int i;

    for(i = 0; i < state->bmp_cnt; ++i) {
        bmap = &state->bitmaps[i];
        zbm_insert_in_top_bmaps(bmap, state->usecnts[zbm_bmprot(bmap)], tops);
        bmap = NULL;
    }

    for(i = 0; i < ZBM_BITMAP_COUNT; ++i) {
        if(tops[i].usecnt == 0) {
            /* Very few bitmaps, all are top */
            state->top_bitmaps[i].bitmap = 0;
            state->top_bitmaps[i].period_bytecnt = 0;
            continue;
        }
        bmap = tops[i].bmap;
        state->top_bitmaps[i] = *bmap;
        /* Mark this as unused for the second metadata area */
        state->usecnts[zbm_bmprot(bmap)] = 0;
    }
}

static int zbm_build_metadata(struct zbm_compress_state *state, struct zbm_cmp_chunk_hdr *hdr)
{
    int err;

    zbm_find_most_common_bitmaps(state);

    hdr->meta_off_1 = zbm_u32_to_u24(state->written);
    err = zbm_write_metadata_1(state);
    if(err)
        return err;

    hdr->meta_off_2 = zbm_u32_to_u24(state->written);
    err = zbm_write_metadata_2(state);
    if(err)
        return err;

    hdr->meta_off_3 = zbm_u32_to_u24(state->written);
    err = zbm_write_metadata_3(state);
    if(err)
        return err;

    return zbm_write_trailing_bitmaps(state);
}

static int zbm_build_compressed_chunk(struct zbm_compress_state *state)
{
    struct zbm_cmp_chunk_hdr *hdr = NULL;
    int err;

    if(sizeof(*hdr) > (size_t)(state->dest_end - state->dest))
        return ZBM_CANT_COMPRESS;
    state->dest -= sizeof(hdr->hdr);
    state->written -= sizeof(hdr->hdr);
    hdr = (struct zbm_cmp_chunk_hdr *)state->dest;
    state->dest += sizeof(*hdr);
    state->written += sizeof(*hdr);

    memset(state->bitmaps, 0, sizeof(state->bitmaps));
    memset(state->periods, 0, sizeof(state->bitmaps));
    state->bmp_cnt = 0;
    memset(state->usecnts, 0, sizeof(state->usecnts));
    memset(state->top_bitmaps, 0, sizeof(state->top_bitmaps));

    if(state->preread == 0) {
        err = zbm_build_initial_bitmap(state);
        if(err)
            return err;
    }
    state->period = 8;

    while(state->read < state->decmp_len) {
        err = zbm_compress_eight_bytes(state);
        if(err)
            return err;
    }

    return zbm_build_metadata(state, hdr);
}

static int zbm_compress_single_chunk(struct zbm_compress_state *state)
{
    struct zbm_chunk_hdr *decmp_hdr = NULL;
    unsigned int max_chunk_sz;
    int err;

    state->read = 0;
    state->written = 0;

    state->decmp_len = MIN(state->src_left, ZBM_MAX_DECMP_CHUNK_SIZE);
    state->src_end = state->src + state->decmp_len;

    /* If a chunk gets bigger than this, we'll just store it uncompressed */
    max_chunk_sz = state->decmp_len + sizeof(*decmp_hdr);
    if(max_chunk_sz > state->dest_left)
        return ZBM_RANGE;
    state->dest_end = state->dest + max_chunk_sz;
    decmp_hdr = (struct zbm_chunk_hdr *)state->dest;

    state->dest += sizeof(*decmp_hdr);
    state->written += sizeof(*decmp_hdr);

    err = zbm_build_compressed_chunk(state);
    if(err == ZBM_CANT_COMPRESS)
        err = zbm_build_uncompressed_chunk(state);
    if(err)
        return err;

    decmp_hdr->len = zbm_u32_to_u24(state->written);
    decmp_hdr->decmp_len = zbm_u32_to_u24(state->decmp_len);
    return 0;
}

static int zmb_max_compressed_length(size_t src_size, size_t *out_len)
{
    size_t max_chunk_size = sizeof(struct zbm_chunk_hdr) + 0x8000;
    size_t chunk_count;

    chunk_count = (src_size + ZBM_MAX_DECMP_CHUNK_SIZE - 1) >> ZBM_MAX_DECMP_CHUNK_SIZE_BITS;
    if(chunk_count << ZBM_MAX_DECMP_CHUNK_SIZE_BITS < src_size)
        return ZBM_OVERFLOW;

    /* The magic, all the chunks, and the terminating empty chunk */
    *out_len = sizeof(uint32_t) + chunk_count * max_chunk_size + ZBM_LAST_CHUNK_SIZE;
    if(*out_len < src_size)
        return ZBM_OVERFLOW;

    return 0;
}

int zbm_compress(void *dest, size_t dest_size, const void *src, size_t src_size, size_t *out_len)
{
    struct zbm_compress_state *state = NULL;
    int err = 0;

    if(!dest)
        return zmb_max_compressed_length(src_size, out_len);

    state = calloc(1, sizeof(*state));
    if(!state)
        return ZBM_NOMEM;

    state->src = src;
    state->src_left = src_size;
    state->dest = dest;
    state->dest_left = dest_size;
    state->preread = 0;
    state->prewritten = 0;

    err = zbm_write_magic(state);
    if(err)
        goto fail;

    do {
        err = zbm_compress_single_chunk(state);
        if(err)
            goto fail;
        state->src_left -= state->read;
        state->dest_left -= state->written;
        state->preread += state->decmp_len;
        state->prewritten += state->written;
    } while(state->read != 0);

    *out_len = state->prewritten;
fail:
    free(state);
    return err;
}

int zbm_compress_chunk(void *dest, size_t dest_size, const void *src, size_t src_size, size_t index, size_t *out_len)
{
    struct zbm_compress_state *state = NULL;
    size_t offset;
    int err = 0;

    state = calloc(1, sizeof(*state));
    if(!state)
        return ZBM_NOMEM;

    state->src = src;
    state->src_left = src_size;
    state->dest = dest;
    state->dest_left = dest_size;
    state->preread = 0;
    state->prewritten = 0;

    if(index == 0) {
        err = zbm_write_magic(state);
        if(err)
            goto fail;
    }

    offset = index << ZBM_MAX_DECMP_CHUNK_SIZE_BITS;
    if(offset >> ZBM_MAX_DECMP_CHUNK_SIZE_BITS != index)
        return ZBM_OVERFLOW;
    if(state->src_left < offset) {
        state->preread = state->src_left;
        state->src += state->src_left;
        state->src_left = 0;
    } else {
        state->preread = offset;
        state->src += offset;
        state->src_left -= offset;
    }

    err = zbm_compress_single_chunk(state);
    if(err)
        goto fail;
    state->src_left -= state->read;
    state->dest_left -= state->written;
    state->preread += state->decmp_len;
    state->prewritten += state->written;

    *out_len = state->prewritten;
fail:
    free(state);
    return err;
}
