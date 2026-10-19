// AudioConverter.c - linear PCM to linear PCM: sample type, channel count, interleaving and rate
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// compressed formats need codecs this system does not have and are refused with 'fmt?'

#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHANNELS 16

struct OpaqueAudioConverter {
    AudioStreamBasicDescription in, out;
    double step;          // input frames per output frame
    double position;      // read position in pending, in input frames
    float *pending;       // converted-to-float input frames, interleaved with in.mChannelsPerFrame
    size_t pendingFrames, pendingCapacity;
    Boolean ended;
    UInt32 quality;
};

static UInt32 bytesPerSample(const AudioStreamBasicDescription *f)
{
    return f->mBitsPerChannel / 8;
}

static Boolean isNonInterleaved(const AudioStreamBasicDescription *f)
{
    return (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
}

Boolean PDAudioPCMFormatIsSupported(const AudioStreamBasicDescription *f)
{
    if (f == NULL || f->mFormatID != kAudioFormatLinearPCM || f->mSampleRate <= 0)
        return false;
    if (f->mChannelsPerFrame < 1 || f->mChannelsPerFrame > MAX_CHANNELS)
        return false;
    if (f->mFormatFlags & kAudioFormatFlagIsBigEndian)
        return false;
    if (f->mFramesPerPacket != 0 && f->mFramesPerPacket != 1)
        return false;
    if (f->mFormatFlags & kAudioFormatFlagIsFloat)
        return f->mBitsPerChannel == 32 || f->mBitsPerChannel == 64;
    if (!(f->mFormatFlags & kAudioFormatFlagIsSignedInteger))
        return f->mBitsPerChannel == 8;
    return f->mBitsPerChannel == 16 || f->mBitsPerChannel == 24 || f->mBitsPerChannel == 32;
}

// fill in the derived fields a caller may leave zero
static AudioStreamBasicDescription normalize(const AudioStreamBasicDescription *f)
{
    AudioStreamBasicDescription n = *f;
    UInt32 sample = bytesPerSample(f);

    n.mFramesPerPacket = 1;
    if (n.mBytesPerFrame == 0)
        n.mBytesPerFrame = isNonInterleaved(f) ? sample : sample * f->mChannelsPerFrame;
    if (n.mBytesPerPacket == 0)
        n.mBytesPerPacket = n.mBytesPerFrame;
    return n;
}

static float readSample(const AudioStreamBasicDescription *f, const uint8_t *p)
{
    if (f->mFormatFlags & kAudioFormatFlagIsFloat)
        return f->mBitsPerChannel == 32 ? *(const float *)p : (float)*(const double *)p;
    switch (f->mBitsPerChannel) {
    case 8:
        return (f->mFormatFlags & kAudioFormatFlagIsSignedInteger) ? *(const int8_t *)p / 128.0f : (*p - 128) / 128.0f;
    case 16:
        return *(const int16_t *)p / 32768.0f;
    case 24: {
        int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
        return v / 8388608.0f;
    }
    default:
        return *(const int32_t *)p / 2147483648.0f;
    }
}

static void writeSample(const AudioStreamBasicDescription *f, uint8_t *p, float s)
{
    if (f->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (f->mBitsPerChannel == 32)
            *(float *)p = s;
        else
            *(double *)p = s;
        return;
    }
    s = s > 1.0f ? 1.0f : s < -1.0f ? -1.0f : s;
    switch (f->mBitsPerChannel) {
    case 8:
        if (f->mFormatFlags & kAudioFormatFlagIsSignedInteger)
            *(int8_t *)p = (int8_t)(s * 127.0f);
        else
            *p = (uint8_t)(s * 127.0f + 128.0f);
        break;
    case 16:
        *(int16_t *)p = (int16_t)(s * 32767.0f);
        break;
    case 24: {
        int32_t v = (int32_t)(s * 8388607.0f);
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        break;
    }
    default:
        *(int32_t *)p = (int32_t)(s * 2147483647.0);
        break;
    }
}

// where sample (frame, channel) lives in a buffer list of format f
static uint8_t *sampleAddress(const AudioStreamBasicDescription *f, const AudioBufferList *l, UInt32 frame, UInt32 channel)
{
    if (isNonInterleaved(f))
        return channel < l->mNumberBuffers ? (uint8_t *)l->mBuffers[channel].mData + (size_t)frame * f->mBytesPerFrame : NULL;
    return (uint8_t *)l->mBuffers[0].mData + (size_t)frame * f->mBytesPerFrame + channel * bytesPerSample(f);
}

static size_t framesIn(const AudioStreamBasicDescription *f, const AudioBufferList *l)
{
    if (l->mNumberBuffers == 0 || f->mBytesPerFrame == 0)
        return 0;
    size_t frames = l->mBuffers[0].mDataByteSize / f->mBytesPerFrame;
    for (UInt32 i = 1; isNonInterleaved(f) && i < l->mNumberBuffers; i++) {
        size_t n = l->mBuffers[i].mDataByteSize / f->mBytesPerFrame;
        frames = n < frames ? n : frames;
    }
    return frames;
}

OSStatus AudioConverterNew(const AudioStreamBasicDescription *source, const AudioStreamBasicDescription *destination,
                           AudioConverterRef *outConverter)
{
    if (source == NULL || destination == NULL || outConverter == NULL)
        return kAudio_ParamError;
    *outConverter = NULL;
    if (!PDAudioPCMFormatIsSupported(source) || !PDAudioPCMFormatIsSupported(destination))
        return kAudioConverterErr_FormatNotSupported;

    AudioConverterRef c = calloc(1, sizeof(*c));
    if (c == NULL)
        return kAudio_MemFullError;
    c->in = normalize(source);
    c->out = normalize(destination);
    c->step = c->in.mSampleRate / c->out.mSampleRate;
    *outConverter = c;
    return noErr;
}

OSStatus AudioConverterDispose(AudioConverterRef c)
{
    if (c == NULL)
        return kAudio_ParamError;
    free(c->pending);
    free(c);
    return noErr;
}

OSStatus AudioConverterReset(AudioConverterRef c)
{
    if (c == NULL)
        return kAudio_ParamError;
    c->pendingFrames = 0;
    c->position = 0;
    c->ended = false;
    return noErr;
}

static OSStatus propertySize(AudioConverterRef c, AudioConverterPropertyID id, UInt32 *size, Boolean *writable)
{
    Boolean w = false;

    switch (id) {
    case kAudioConverterCurrentInputStreamDescription:
    case kAudioConverterCurrentOutputStreamDescription:
        *size = sizeof(AudioStreamBasicDescription);
        break;
    case kAudioConverterPropertyMaximumInputPacketSize:
    case kAudioConverterPropertyMaximumOutputPacketSize:
    case kAudioConverterPropertyMinimumInputBufferSize:
    case kAudioConverterPropertyMinimumOutputBufferSize:
        *size = sizeof(UInt32);
        break;
    case kAudioConverterSampleRateConverterQuality:
    case kAudioConverterSampleRateConverterComplexity:
    case kAudioConverterPrimeMethod:
        *size = sizeof(UInt32);
        w = true;
        break;
    default:
        return kAudioConverterErr_PropertyNotSupported;
    }
    if (writable)
        *writable = w;
    (void)c;
    return noErr;
}

OSStatus AudioConverterGetPropertyInfo(AudioConverterRef c, AudioConverterPropertyID id, UInt32 *outSize, Boolean *outWritable)
{
    UInt32 size;
    if (c == NULL)
        return kAudio_ParamError;
    OSStatus err = propertySize(c, id, &size, outWritable);
    if (err == noErr && outSize)
        *outSize = size;
    return err;
}

OSStatus AudioConverterGetProperty(AudioConverterRef c, AudioConverterPropertyID id, UInt32 *ioSize, void *out)
{
    UInt32 size, value = 0;

    if (c == NULL || ioSize == NULL || out == NULL)
        return kAudio_ParamError;
    OSStatus err = propertySize(c, id, &size, NULL);
    if (err != noErr)
        return err;
    if (*ioSize < size)
        return kAudioConverterErr_BadPropertySizeError;
    switch (id) {
    case kAudioConverterCurrentInputStreamDescription:
        memcpy(out, &c->in, size);
        break;
    case kAudioConverterCurrentOutputStreamDescription:
        memcpy(out, &c->out, size);
        break;
    case kAudioConverterPropertyMaximumInputPacketSize:
    case kAudioConverterPropertyMinimumInputBufferSize:
        value = c->in.mBytesPerPacket;
        break;
    case kAudioConverterPropertyMaximumOutputPacketSize:
    case kAudioConverterPropertyMinimumOutputBufferSize:
        value = c->out.mBytesPerPacket;
        break;
    case kAudioConverterSampleRateConverterQuality:
    case kAudioConverterSampleRateConverterComplexity:
        value = c->quality;
        break;
    case kAudioConverterPrimeMethod:
        value = 2;
        break;
    }
    if (size == sizeof(UInt32))
        memcpy(out, &value, sizeof(value));
    *ioSize = size;
    return noErr;
}

OSStatus AudioConverterSetProperty(AudioConverterRef c, AudioConverterPropertyID id, UInt32 inSize, const void *data)
{
    UInt32 size;
    Boolean writable = false;

    if (c == NULL || data == NULL)
        return kAudio_ParamError;
    // a PCM stream has no magic cookie, there is no decoder to give one to
    if (id == kAudioConverterDecompressionMagicCookie)
        return kAudioConverterErr_PropertyNotSupported;
    OSStatus err = propertySize(c, id, &size, &writable);
    if (err != noErr)
        return err;
    if (!writable)
        return kAudioConverterErr_PropertyNotSupported;
    if (inSize < size)
        return kAudioConverterErr_BadPropertySizeError;
    if (id != kAudioConverterPrimeMethod)
        c->quality = *(const UInt32 *)data;
    return noErr;
}

// pull more input through the client's proc into the float pending buffer
static OSStatus pull(AudioConverterRef c, AudioConverterComplexInputDataProc proc, void *userData, size_t wanted)
{
    UInt32 channels = c->in.mChannelsPerFrame;
    UInt32 buffers = isNonInterleaved(&c->in) ? channels : 1;
    // room for the buffer list header and one AudioBuffer per channel
    union {
        AudioBufferList list;
        uint8_t storage[offsetof(AudioBufferList, mBuffers) + MAX_CHANNELS * sizeof(AudioBuffer)];
    } u;
    AudioBufferList *l = &u.list;
    UInt32 packets = (UInt32)wanted;
    AudioStreamPacketDescription *descriptions = NULL;

    memset(&u, 0, sizeof(u));
    l->mNumberBuffers = buffers;
    for (UInt32 i = 0; i < buffers; i++)
        l->mBuffers[i].mNumberChannels = buffers == 1 ? channels : 1;
    OSStatus err = proc(c, &packets, l, &descriptions, userData);
    if (err != noErr)
        return err;
    size_t frames = framesIn(&c->in, l);
    frames = packets < frames ? packets : frames;
    if (frames == 0) {
        c->ended = true;
        return noErr;
    }
    if (c->pendingFrames + frames > c->pendingCapacity) {
        size_t capacity = (c->pendingFrames + frames) * 2;
        float *p = realloc(c->pending, capacity * channels * sizeof(float));
        if (p == NULL)
            return kAudio_MemFullError;
        c->pending = p;
        c->pendingCapacity = capacity;
    }
    float *dst = c->pending + c->pendingFrames * channels;
    for (size_t f = 0; f < frames; f++) {
        for (UInt32 ch = 0; ch < channels; ch++) {
            const uint8_t *p = sampleAddress(&c->in, l, (UInt32)f, ch);
            dst[f * channels + ch] = p ? readSample(&c->in, p) : 0.0f;
        }
    }
    c->pendingFrames += frames;
    return noErr;
}

// the source channel feeding an output channel: copy, spread mono, or fold down to mono
static float mapChannel(AudioConverterRef c, const float *frame, UInt32 outChannel)
{
    UInt32 inChannels = c->in.mChannelsPerFrame, outChannels = c->out.mChannelsPerFrame;

    if (inChannels == 1)
        return frame[0];
    if (outChannels == 1) {
        float sum = 0;
        for (UInt32 i = 0; i < inChannels; i++)
            sum += frame[i];
        return sum / inChannels;
    }
    return outChannel < inChannels ? frame[outChannel] : 0.0f;
}

OSStatus AudioConverterFillComplexBuffer(AudioConverterRef c, AudioConverterComplexInputDataProc proc, void *userData,
                                         UInt32 *ioOutputDataPacketSize, AudioBufferList *outData,
                                         AudioStreamPacketDescription *outPacketDescription)
{
    (void)outPacketDescription;
    if (c == NULL || proc == NULL || ioOutputDataPacketSize == NULL || outData == NULL)
        return kAudio_ParamError;

    UInt32 channels = c->in.mChannelsPerFrame;
    size_t room = framesIn(&c->out, outData);
    size_t wanted = *ioOutputDataPacketSize < room ? *ioOutputDataPacketSize : room;
    size_t produced = 0;
    OSStatus err = noErr;

    while (produced < wanted) {
        size_t index = (size_t)c->position;
        // linear interpolation needs the frame after the read position too, except at the end
        if (index + 1 >= c->pendingFrames && !c->ended) {
            size_t need = (size_t)((wanted - produced) * c->step) + 2;
            if ((err = pull(c, proc, userData, need)) != noErr)
                break;
            continue;
        }
        if (index >= c->pendingFrames)
            break;
        const float *a = c->pending + index * channels;
        const float *b = index + 1 < c->pendingFrames ? a + channels : a;
        float t = (float)(c->position - index);
        for (UInt32 ch = 0; ch < c->out.mChannelsPerFrame; ch++) {
            float s = mapChannel(c, a, ch) * (1.0f - t) + mapChannel(c, b, ch) * t;
            writeSample(&c->out, sampleAddress(&c->out, outData, (UInt32)produced, ch), s);
        }
        produced++;
        c->position += c->step;
    }

    // drop the input frames the read position has passed
    size_t consumed = (size_t)c->position;
    if (consumed > c->pendingFrames)
        consumed = c->pendingFrames;
    if (consumed) {
        memmove(c->pending, c->pending + consumed * channels, (c->pendingFrames - consumed) * channels * sizeof(float));
        c->pendingFrames -= consumed;
        c->position -= consumed;
    }
    for (UInt32 i = 0; i < outData->mNumberBuffers; i++)
        outData->mBuffers[i].mDataByteSize = (UInt32)(produced * c->out.mBytesPerFrame);
    *ioOutputDataPacketSize = (UInt32)produced;
    return produced ? noErr : err;
}

struct oneShot {
    const void *data;
    UInt32 size;
    const AudioStreamBasicDescription *format;
};

static OSStatus oneShotInput(AudioConverterRef c, UInt32 *packets, AudioBufferList *l, AudioStreamPacketDescription **d, void *userData)
{
    struct oneShot *s = userData;
    (void)c;
    (void)d;
    l->mBuffers[0].mData = (void *)s->data;
    l->mBuffers[0].mDataByteSize = s->size;
    *packets = s->format->mBytesPerFrame ? s->size / s->format->mBytesPerFrame : 0;
    s->size = 0;
    return noErr;
}

// whole buffers, interleaved and at the same rate
OSStatus AudioConverterConvertBuffer(AudioConverterRef c, UInt32 inSize, const void *in, UInt32 *ioOutSize, void *out)
{
    if (c == NULL || in == NULL || ioOutSize == NULL || out == NULL)
        return kAudio_ParamError;
    if (c->in.mSampleRate != c->out.mSampleRate || isNonInterleaved(&c->in) || isNonInterleaved(&c->out))
        return kAudioConverterErr_OperationNotSupported;
    struct oneShot s = { in, inSize, &c->in };
    AudioBufferList l = { 1, { { c->out.mChannelsPerFrame, *ioOutSize, out } } };
    UInt32 frames = *ioOutSize / c->out.mBytesPerFrame;
    AudioConverterReset(c);
    OSStatus err = AudioConverterFillComplexBuffer(c, oneShotInput, &s, &frames, &l, NULL);
    *ioOutSize = frames * c->out.mBytesPerFrame;
    AudioConverterReset(c);
    return err;
}
