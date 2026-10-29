// CommonBuffering.c - chunk buffer that feeds whole blocks to the libcn encoders
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <stdlib.h>
#include <string.h>
#include "CommonBufferingPriv.h"

CNBufferRef
CNBufferCreate(size_t chunksize)
{
    CNBufferRef bufRef;

    if (chunksize == 0) return NULL;

    bufRef = calloc(1, sizeof(CNBuffer));
    if (bufRef == NULL) return NULL;

    bufRef->buf = malloc(chunksize);
    if (bufRef->buf == NULL) {
        free(bufRef);
        return NULL;
    }

    bufRef->chunksize = chunksize;
    return bufRef;
}

CNStatus
CNBufferRelease(CNBufferRef *bufRef)
{
    if (bufRef == NULL || *bufRef == NULL) return kCNParamError;

    free((*bufRef)->buf);
    free(*bufRef);
    *bufRef = NULL;
    return kCNSuccess;
}

// runs whole chunks of the buffered bytes plus new input through pFunc in one call
// the remainder stays buffered for the next call or the flush
CNStatus
CNBufferProcessData(CNBufferRef bufRef,
                    void *ctx, const void *in, const size_t inLen, void *out, size_t *outLen,
                    cnProcessFunction pFunc, cnSizeFunction sizeFunc)
{
    const uint8_t *src = in;
    size_t total, blocks, fromInput, need;
    uint8_t *work;
    int rc;

    if (bufRef == NULL || outLen == NULL || pFunc == NULL || sizeFunc == NULL) return kCNParamError;
    if (in == NULL && inLen != 0) return kCNParamError;

    total = bufRef->bufferPos + inLen;
    if (total < bufRef->chunksize) {
        if (inLen) memcpy(bufRef->buf + bufRef->bufferPos, src, inLen);
        bufRef->bufferPos = total;
        *outLen = 0;
        return kCNSuccess;
    }

    blocks = total - total % bufRef->chunksize;
    need = sizeFunc(ctx, blocks);
    if (out == NULL || *outLen < need) {
        *outLen = need;
        return kCNBufferTooSmall;
    }

    work = malloc(blocks);
    if (work == NULL) return kCNMemoryFailure;

    fromInput = blocks - bufRef->bufferPos;
    memcpy(work, bufRef->buf, bufRef->bufferPos);
    memcpy(work + bufRef->bufferPos, src, fromInput);

    rc = pFunc(ctx, work, blocks, out, outLen);
    free(work);
    if (rc != 0) return kCNFailure;

    bufRef->bufferPos = inLen - fromInput;
    if (bufRef->bufferPos) memcpy(bufRef->buf, src + fromInput, bufRef->bufferPos);
    return kCNSuccess;
}

// processes the partial chunk left in the buffer and empties it
CNStatus
CNBufferFlushData(CNBufferRef bufRef,
                  void *ctx, void *out, size_t *outLen,
                  cnProcessFunction pFunc, cnSizeFunction sizeFunc)
{
    size_t need;
    int rc;

    if (bufRef == NULL || outLen == NULL || pFunc == NULL || sizeFunc == NULL) return kCNParamError;

    if (bufRef->bufferPos == 0) {
        *outLen = 0;
        return kCNSuccess;
    }

    need = sizeFunc(ctx, bufRef->bufferPos);
    if (out == NULL || *outLen < need) {
        *outLen = need;
        return kCNBufferTooSmall;
    }

    rc = pFunc(ctx, bufRef->buf, bufRef->bufferPos, out, outLen);
    if (rc != 0) return kCNFailure;

    bufRef->bufferPos = 0;
    return kCNSuccess;
}

bool
CNBufferEmpty(CNBufferRef bufRef)
{
    return bufRef == NULL || bufRef->bufferPos == 0;
}
