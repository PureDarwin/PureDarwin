// PureDarwin libcompression: the buffer and stream API, zlib's raw deflate so far
#ifndef __COMPRESSION_HEADER__
#define __COMPRESSION_HEADER__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COMPRESSION_LZ4 = 0x100,
    COMPRESSION_ZLIB = 0x205,
    COMPRESSION_LZMA = 0x306,
    COMPRESSION_LZ4_RAW = 0x101,
    COMPRESSION_BROTLI = 0xB02,
    COMPRESSION_LZFSE = 0x801,
    COMPRESSION_LZBITMAP = 0x702,
} compression_algorithm;

typedef struct {
    uint8_t *dst_ptr;
    size_t dst_size;
    const uint8_t *src_ptr;
    size_t src_size;
    void *state;
} compression_stream;

typedef enum {
    COMPRESSION_STREAM_ENCODE = 0,
    COMPRESSION_STREAM_DECODE = 1,
} compression_stream_operation;

typedef enum {
    COMPRESSION_STREAM_FINALIZE = 0x0001,
} compression_stream_flags;

typedef enum {
    COMPRESSION_STATUS_OK = 0,
    COMPRESSION_STATUS_ERROR = -1,
    COMPRESSION_STATUS_END = 1,
} compression_status;

size_t compression_encode_scratch_buffer_size(compression_algorithm algorithm);
size_t compression_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size,
    void *scratch_buffer, compression_algorithm algorithm);
size_t compression_decode_scratch_buffer_size(compression_algorithm algorithm);
size_t compression_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size,
    void *scratch_buffer, compression_algorithm algorithm);

compression_status compression_stream_init(compression_stream *stream, compression_stream_operation operation,
    compression_algorithm algorithm);
compression_status compression_stream_process(compression_stream *stream, int flags);
compression_status compression_stream_destroy(compression_stream *stream);

#ifdef __cplusplus
}
#endif

#endif
