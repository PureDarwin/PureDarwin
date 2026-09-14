/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2022 Corellium LLC
 *
 * Author: Ernesto A. Fernández <ernesto@corellium.com>
 */

#ifndef _LIBZBITMAP_H
#define _LIBZBITMAP_H

#include <stddef.h>

#define ZBM_NOMEM       (-1)    /* Failed to allocate memory */
#define ZBM_INVAL       (-2)    /* Compressed input is invalid */
#define ZBM_RANGE       (-3)    /* The destination buffer is too small */
#define ZBM_OVERFLOW    (-4)    /* Integer overflow from size or index */

/**
 * zbm_decompress - Decompress an LZBITMAP buffer
 * @dest:       destination buffer (may be NULL)
 * @dest_size:  size of the destination buffer
 * @src:        source buffer
 * @src_size:   size of the source buffer
 * @out_len:    on return, the length of the decompressed output
 *
 * May be called with a NULL destination buffer to retrieve the expected length
 * of the decompressed data. Returns 0 on success, or a negative error code in
 * case of failure.
 */
int zbm_decompress(void *dest, size_t dest_size, const void *src, size_t src_size, size_t *out_len);

/**
 * zbm_compress - Compress a whole buffer with LZBITMAP
 * @dest:       destination buffer (may be NULL)
 * @dest_size:  size of the destination buffer
 * @src:        source buffer
 * @src_size:   size of the source buffer
 * @out_len:    on return, the length of the compressed output
 *
 * May be called with a NULL destination buffer to retrieve the maximum possible
 * length of the compressed output. Returns 0 on success, or a negative error
 * code in case of failure.
 *
 * This function just compresses the whole buffer sequentially, using a single
 * thread. For speed, the caller should consider using zbm_compress_chunk()
 * instead.
 */
int zbm_compress(void *dest, size_t dest_size, const void *src, size_t src_size, size_t *out_len);

#define ZBM_MAX_CHUNK_SIZE      0x800A
#define ZBM_LAST_CHUNK_SIZE     6

/*
 * zbm_compress_chunk - Compress a single chunk in a buffer with LZBITMAP
 * @dest:       destination buffer
 * @dest_size:  size of the destination buffer
 * @src:        source buffer
 * @src_size:   size of the source buffer
 * @index:      which chunk from @src to compress
 * @out_len:    on return, the length of the compressed output
 *
 * Returns 0 on success, or a negative error code in case of failure.
 *
 * To compress a whole buffer, this function must be called repeatedly with the
 * same value of @src and a sequentially increasing value of @index. The process
 * concludes when @out_len returns ZBM_LAST_CHUNK_SIZE. The size of @dest must
 * always be ZBM_MAX_CHUNK_SIZE or more.
 *
 * The advantage of this over zbm_compress() is that the caller can compress
 * multiple chunks in parallel. For an example using pthreads, check the test
 * code in test/test.c.
 */
int zbm_compress_chunk(void *dest, size_t dest_size, const void *src, size_t src_size, size_t index, size_t *out_len);

#endif /* _LIBZBITMAP_H */
