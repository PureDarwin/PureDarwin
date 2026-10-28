// COMPRESSION_ZLIB is raw deflate at level 5, as on macOS. The other algorithms report failure:
// buffer calls return 0 and stream_init returns an error, which callers already handle
#include "compression.h"
#include <stdlib.h>
#include <zlib.h>

struct state {
    z_stream z;
    int encode;
};

size_t compression_encode_scratch_buffer_size(compression_algorithm algorithm)
{
    return 0;
}

size_t compression_decode_scratch_buffer_size(compression_algorithm algorithm)
{
    return 0;
}

compression_status compression_stream_init(compression_stream *stream, compression_stream_operation operation,
    compression_algorithm algorithm)
{
    struct state *s;
    int ret;

    if (!stream || algorithm != COMPRESSION_ZLIB)
        return COMPRESSION_STATUS_ERROR;
    s = calloc(1, sizeof(*s));
    if (!s)
        return COMPRESSION_STATUS_ERROR;
    s->encode = operation == COMPRESSION_STREAM_ENCODE;
    ret = s->encode ? deflateInit2(&s->z, 5, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) : inflateInit2(&s->z, -15);
    if (ret != Z_OK) {
        free(s);
        return COMPRESSION_STATUS_ERROR;
    }
    stream->state = s;
    return COMPRESSION_STATUS_OK;
}

compression_status compression_stream_process(compression_stream *stream, int flags)
{
    struct state *s = stream ? stream->state : NULL;
    int ret;

    if (!s)
        return COMPRESSION_STATUS_ERROR;
    s->z.next_in = (Bytef *)stream->src_ptr;
    s->z.avail_in = (uInt)stream->src_size;
    s->z.next_out = stream->dst_ptr;
    s->z.avail_out = (uInt)stream->dst_size;
    if (s->encode)
        ret = deflate(&s->z, flags & COMPRESSION_STREAM_FINALIZE ? Z_FINISH : Z_NO_FLUSH);
    else
        ret = inflate(&s->z, Z_NO_FLUSH);
    stream->src_ptr = s->z.next_in;
    stream->src_size = s->z.avail_in;
    stream->dst_ptr = s->z.next_out;
    stream->dst_size = s->z.avail_out;
    if (ret == Z_STREAM_END)
        return COMPRESSION_STATUS_END;
    // no progress possible is not an error, the caller supplies more input or room
    if (ret == Z_OK || ret == Z_BUF_ERROR)
        return COMPRESSION_STATUS_OK;
    return COMPRESSION_STATUS_ERROR;
}

compression_status compression_stream_destroy(compression_stream *stream)
{
    struct state *s = stream ? stream->state : NULL;

    if (!s)
        return COMPRESSION_STATUS_ERROR;
    if (s->encode)
        deflateEnd(&s->z);
    else
        inflateEnd(&s->z);
    free(s);
    stream->state = NULL;
    return COMPRESSION_STATUS_OK;
}

static size_t process_buffer(uint8_t *dst, size_t dst_size, const uint8_t *src, size_t src_size,
    compression_algorithm algorithm, compression_stream_operation operation)
{
    compression_stream stream = { dst, dst_size, src, src_size, NULL };
    compression_status status;

    if (compression_stream_init(&stream, operation, algorithm) != COMPRESSION_STATUS_OK)
        return 0;
    status = compression_stream_process(&stream, COMPRESSION_STREAM_FINALIZE);
    compression_stream_destroy(&stream);
    // encoding must finish, decoding may stop when dst is full and returns what fit
    if (status == COMPRESSION_STATUS_ERROR || (operation == COMPRESSION_STREAM_ENCODE && status != COMPRESSION_STATUS_END))
        return 0;
    return dst_size - stream.dst_size;
}

size_t compression_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size,
    void *scratch_buffer, compression_algorithm algorithm)
{
    return process_buffer(dst_buffer, dst_size, src_buffer, src_size, algorithm, COMPRESSION_STREAM_ENCODE);
}

size_t compression_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size,
    void *scratch_buffer, compression_algorithm algorithm)
{
    return process_buffer(dst_buffer, dst_size, src_buffer, src_size, algorithm, COMPRESSION_STREAM_DECODE);
}
