// AudioQueue.c - output queues on the HAL's output device: an IOProc converts queued buffers to the device format
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// buffers play in order and go back to the client from a callback thread, so a slow callback never stalls the IOProc

#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct entry {
    AudioQueueBufferRef buffer;
    struct entry *next;
};

struct listener {
    AudioQueuePropertyID id;
    AudioQueuePropertyListenerProc proc;
    void *data;
    struct listener *next;
};

struct OpaqueAudioQueue {
    AudioStreamBasicDescription format, device;
    AudioQueueOutputCallback callback;
    void *userData;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    // queued buffers play from head, played ones wait in done for the callback thread
    struct entry *head, *tail, *done, *doneTail;
    UInt32 headOffset;
    AudioConverterRef converter;
    AudioDeviceIOProcID ioProc;
    pthread_t callbackThread;
    Boolean running, stopping, disposing;
    Float32 volume;
    Float64 sampleTime;
    struct listener *listeners;
    AudioQueueBufferRef *buffers;
    UInt32 bufferCount;
};

static Boolean formatSupported(const AudioStreamBasicDescription *f)
{
    if (f->mFormatID != kAudioFormatLinearPCM || f->mSampleRate <= 0 || f->mChannelsPerFrame < 1 || f->mBytesPerFrame == 0)
        return false;
    return (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) == 0;
}

static void notifyRunning(AudioQueueRef q)
{
    struct listener copies[8];
    int n = 0;

    pthread_mutex_lock(&q->lock);
    for (struct listener *l = q->listeners; l && n < 8; l = l->next) {
        if (l->id == kAudioQueueProperty_IsRunning)
            copies[n++] = *l;
    }
    pthread_mutex_unlock(&q->lock);
    for (int i = 0; i < n; i++)
        copies[i].proc(copies[i].data, q, kAudioQueueProperty_IsRunning);
}

// moves the head buffer to the done list, with the lock held
static void retireHead(AudioQueueRef q)
{
    struct entry *e = q->head;

    q->head = e->next;
    if (!q->head)
        q->tail = NULL;
    q->headOffset = 0;
    e->next = NULL;
    if (q->doneTail)
        q->doneTail->next = e;
    else
        q->done = e;
    q->doneTail = e;
    pthread_cond_broadcast(&q->changed);
}

// the converter's input: what is left of the head buffer, none when the queue has run dry
static OSStatus queueInput(AudioConverterRef converter, UInt32 *packets, AudioBufferList *ioData,
                           AudioStreamPacketDescription **descriptions, void *userData)
{
    AudioQueueRef q = userData;
    UInt32 bpf = q->format.mBytesPerFrame;

    (void)converter;
    (void)descriptions;
    pthread_mutex_lock(&q->lock);
    while (q->head && q->headOffset >= q->head->buffer->mAudioDataByteSize)
        retireHead(q);
    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        *packets = 0;
        return kAudioQueueErr_BufferEmpty;
    }
    AudioQueueBufferRef b = q->head->buffer;
    UInt32 frames = (b->mAudioDataByteSize - q->headOffset) / bpf;
    if (frames > *packets)
        frames = *packets;
    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0] = (AudioBuffer){ q->format.mChannelsPerFrame, frames * bpf, (uint8_t *)b->mAudioData + q->headOffset };
    q->headOffset += frames * bpf;
    if (frames == 0)
        retireHead(q);
    pthread_mutex_unlock(&q->lock);
    *packets = frames;
    return noErr;
}

static OSStatus ioProc(AudioObjectID device, const AudioTimeStamp *now, const AudioBufferList *input, const AudioTimeStamp *inputTime,
                       AudioBufferList *output, const AudioTimeStamp *outputTime, void *data)
{
    AudioQueueRef q = data;
    AudioBuffer *out = &output->mBuffers[0];
    UInt32 frames = out->mDataByteSize / q->device.mBytesPerFrame;
    UInt32 n = frames;

    (void)device;
    (void)now;
    (void)input;
    (void)inputTime;
    (void)outputTime;
    OSStatus err = AudioConverterFillComplexBuffer(q->converter, queueInput, q, &n, output, NULL);
    if (err != noErr && err != kAudioQueueErr_BufferEmpty)
        n = 0;
    out->mDataByteSize = frames * q->device.mBytesPerFrame;

    float *samples = out->mData;
    UInt32 ch = q->device.mChannelsPerFrame;
    if (q->volume != 1.0f) {
        for (UInt32 i = 0; i < n * ch; i++)
            samples[i] *= q->volume;
    }
    memset(samples + n * ch, 0, (size_t)(frames - n) * ch * sizeof(float));
    q->sampleTime += n;

    // a stop that is not immediate ends once everything queued has played
    pthread_mutex_lock(&q->lock);
    if (q->stopping && !q->head) {
        q->stopping = false;
        q->running = false;
        pthread_cond_broadcast(&q->changed);
    }
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

static void *callbackThread(void *arg)
{
    AudioQueueRef q = arg;
    Boolean wasRunning = false;

    pthread_mutex_lock(&q->lock);
    for (;;) {
        while (!q->done && !q->disposing && wasRunning == q->running)
            pthread_cond_wait(&q->changed, &q->lock);
        if (q->done) {
            struct entry *e = q->done;

            q->done = e->next;
            if (!q->done)
                q->doneTail = NULL;
            pthread_mutex_unlock(&q->lock);
            q->callback(q->userData, q, e->buffer);
            free(e);
            pthread_mutex_lock(&q->lock);
            continue;
        }
        if (q->disposing)
            break;
        if (wasRunning != q->running) {
            wasRunning = q->running;
            pthread_mutex_unlock(&q->lock);
            // an ended stop leaves the device to whoever else plays, the IOProc takes the queue lock
            if (!wasRunning)
                AudioDeviceStop(kPDAudioOutputDevice, q->ioProc);
            notifyRunning(q);
            pthread_mutex_lock(&q->lock);
        }
    }
    pthread_mutex_unlock(&q->lock);
    return NULL;
}

OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *format, AudioQueueOutputCallback callback, void *userData,
                             CFRunLoopRef runLoop, CFStringRef runLoopMode, UInt32 flags, AudioQueueRef *outQ)
{
    (void)runLoop;
    (void)runLoopMode;
    (void)flags;
    if (format == NULL || callback == NULL || outQ == NULL)
        return kAudio_ParamError;
    if (!formatSupported(format))
        return kAudioFormatUnsupportedDataFormatError;
    if (!PDAudioOutputDeviceIsPresent())
        return kAudioQueueErr_InvalidDevice;

    AudioQueueRef q = calloc(1, sizeof(*q));
    if (q == NULL)
        return kAudio_MemFullError;
    UInt32 size = sizeof(q->device);
    AudioObjectPropertyAddress a = { kAudioDevicePropertyStreamFormat, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
    AudioObjectGetPropertyData(kPDAudioOutputDevice, &a, 0, NULL, &size, &q->device);
    q->format = *format;
    q->callback = callback;
    q->userData = userData;
    q->volume = 1.0f;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->changed, NULL);

    OSStatus err = AudioConverterNew(&q->format, &q->device, &q->converter);
    if (err == noErr)
        err = AudioDeviceCreateIOProcID(kPDAudioOutputDevice, ioProc, q, &q->ioProc);
    if (err == noErr && pthread_create(&q->callbackThread, NULL, callbackThread, q) != 0)
        err = kAudio_MemFullError;
    if (err != noErr) {
        if (q->ioProc)
            AudioDeviceDestroyIOProcID(kPDAudioOutputDevice, q->ioProc);
        if (q->converter)
            AudioConverterDispose(q->converter);
        free(q);
        return err;
    }
    *outQ = q;
    return noErr;
}

OSStatus AudioQueueAllocateBufferWithPacketDescriptions(AudioQueueRef q, UInt32 size, UInt32 descriptions, AudioQueueBufferRef *outBuffer)
{
    if (q == NULL || outBuffer == NULL || size == 0)
        return kAudio_ParamError;
    // the header, its packet descriptions and the data in one block
    size_t header = (sizeof(AudioQueueBuffer) + 15) & ~(size_t)15;
    size_t packets = ((size_t)descriptions * sizeof(AudioStreamPacketDescription) + 15) & ~(size_t)15;
    uint8_t *block = calloc(1, header + packets + size);
    AudioQueueBufferRef *list = realloc(q->buffers, (q->bufferCount + 1) * sizeof(*list));
    if (block == NULL || list == NULL) {
        free(block);
        if (list)
            q->buffers = list;
        return kAudio_MemFullError;
    }
    AudioQueueBuffer init = { size, block + header + packets, 0, NULL, descriptions,
                              descriptions ? (AudioStreamPacketDescription *)(block + header) : NULL, 0 };
    memcpy(block, &init, sizeof(init));
    q->buffers = list;
    q->buffers[q->bufferCount++] = (AudioQueueBufferRef)block;
    *outBuffer = (AudioQueueBufferRef)block;
    return noErr;
}

OSStatus AudioQueueAllocateBuffer(AudioQueueRef q, UInt32 size, AudioQueueBufferRef *outBuffer)
{
    return AudioQueueAllocateBufferWithPacketDescriptions(q, size, 0, outBuffer);
}

static Boolean isQueued(AudioQueueRef q, AudioQueueBufferRef b)
{
    for (struct entry *e = q->head; e; e = e->next) {
        if (e->buffer == b)
            return true;
    }
    return false;
}

OSStatus AudioQueueFreeBuffer(AudioQueueRef q, AudioQueueBufferRef b)
{
    if (q == NULL || b == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    if (isQueued(q, b)) {
        pthread_mutex_unlock(&q->lock);
        return kAudioQueueErr_InvalidBuffer;
    }
    for (UInt32 i = 0; i < q->bufferCount; i++) {
        if (q->buffers[i] == b) {
            q->buffers[i] = q->buffers[--q->bufferCount];
            pthread_mutex_unlock(&q->lock);
            free(b);
            return noErr;
        }
    }
    pthread_mutex_unlock(&q->lock);
    return kAudioQueueErr_InvalidBuffer;
}

OSStatus AudioQueueEnqueueBuffer(AudioQueueRef q, AudioQueueBufferRef b, UInt32 numPacketDescs, const AudioStreamPacketDescription *descs)
{
    (void)numPacketDescs;
    (void)descs;
    if (q == NULL || b == NULL)
        return kAudio_ParamError;
    if (b->mAudioDataByteSize > b->mAudioDataBytesCapacity)
        return kAudioQueueErr_InvalidBuffer;
    struct entry *e = calloc(1, sizeof(*e));
    if (e == NULL)
        return kAudio_MemFullError;
    e->buffer = b;
    pthread_mutex_lock(&q->lock);
    if (q->tail)
        q->tail->next = e;
    else
        q->head = e;
    q->tail = e;
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

OSStatus AudioQueueStart(AudioQueueRef q, const AudioTimeStamp *startTime)
{
    (void)startTime;
    if (q == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    q->stopping = false;
    if (q->running) {
        pthread_mutex_unlock(&q->lock);
        return noErr;
    }
    q->running = true;
    pthread_cond_broadcast(&q->changed);
    pthread_mutex_unlock(&q->lock);
    return AudioDeviceStart(kPDAudioOutputDevice, q->ioProc);
}

// conversion happens in the IOProc, so there is nothing to prepare ahead
OSStatus AudioQueuePrime(AudioQueueRef q, UInt32 frames, UInt32 *outPrepared)
{
    if (q == NULL)
        return kAudio_ParamError;
    if (outPrepared)
        *outPrepared = frames;
    return noErr;
}

// everything still queued goes back to the client, as on macOS
static void returnQueued(AudioQueueRef q)
{
    pthread_mutex_lock(&q->lock);
    while (q->head)
        retireHead(q);
    pthread_mutex_unlock(&q->lock);
    AudioConverterReset(q->converter);
}

OSStatus AudioQueueStop(AudioQueueRef q, Boolean immediate)
{
    if (q == NULL)
        return kAudio_ParamError;
    if (!immediate) {
        pthread_mutex_lock(&q->lock);
        if (q->running)
            q->stopping = true;
        pthread_mutex_unlock(&q->lock);
        return noErr;
    }
    AudioDeviceStop(kPDAudioOutputDevice, q->ioProc);
    pthread_mutex_lock(&q->lock);
    q->running = false;
    q->stopping = false;
    pthread_cond_broadcast(&q->changed);
    pthread_mutex_unlock(&q->lock);
    returnQueued(q);
    return noErr;
}

OSStatus AudioQueuePause(AudioQueueRef q)
{
    if (q == NULL)
        return kAudio_ParamError;
    AudioDeviceStop(kPDAudioOutputDevice, q->ioProc);
    pthread_mutex_lock(&q->lock);
    q->running = false;
    pthread_cond_broadcast(&q->changed);
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

// what is queued already plays out, the converter keeps no partial frames
OSStatus AudioQueueFlush(AudioQueueRef q)
{
    return q ? noErr : kAudio_ParamError;
}

OSStatus AudioQueueReset(AudioQueueRef q)
{
    if (q == NULL)
        return kAudio_ParamError;
    AudioDeviceStop(kPDAudioOutputDevice, q->ioProc);
    returnQueued(q);
    pthread_mutex_lock(&q->lock);
    Boolean running = q->running;
    pthread_mutex_unlock(&q->lock);
    return running ? AudioDeviceStart(kPDAudioOutputDevice, q->ioProc) : noErr;
}

OSStatus AudioQueueDispose(AudioQueueRef q, Boolean immediate)
{
    (void)immediate;
    if (q == NULL)
        return kAudio_ParamError;
    AudioDeviceStop(kPDAudioOutputDevice, q->ioProc);
    AudioDeviceDestroyIOProcID(kPDAudioOutputDevice, q->ioProc);
    returnQueued(q);
    pthread_mutex_lock(&q->lock);
    q->running = false;
    q->disposing = true;
    pthread_cond_broadcast(&q->changed);
    pthread_mutex_unlock(&q->lock);
    if (!pthread_equal(pthread_self(), q->callbackThread))
        pthread_join(q->callbackThread, NULL);
    else
        pthread_detach(q->callbackThread);
    AudioConverterDispose(q->converter);
    for (UInt32 i = 0; i < q->bufferCount; i++)
        free(q->buffers[i]);
    free(q->buffers);
    for (struct listener *l = q->listeners, *next; l; l = next) {
        next = l->next;
        free(l);
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->changed);
    free(q);
    return noErr;
}

OSStatus AudioQueueGetParameter(AudioQueueRef q, AudioQueueParameterID id, AudioQueueParameterValue *outValue)
{
    if (q == NULL || outValue == NULL)
        return kAudio_ParamError;
    if (id != kAudioQueueParam_Volume)
        return kAudioQueueErr_InvalidParameter;
    *outValue = q->volume;
    return noErr;
}

OSStatus AudioQueueSetParameter(AudioQueueRef q, AudioQueueParameterID id, AudioQueueParameterValue value)
{
    if (q == NULL)
        return kAudio_ParamError;
    if (id != kAudioQueueParam_Volume)
        return kAudioQueueErr_InvalidParameter;
    q->volume = value < 0 ? 0 : value > 1 ? 1 : value;
    return noErr;
}

static OSStatus property(AudioQueueRef q, AudioQueuePropertyID id, void *out, UInt32 *size)
{
    switch (id) {
    case kAudioQueueProperty_IsRunning:
        *size = sizeof(UInt32);
        if (out)
            *(UInt32 *)out = q->running;
        return noErr;
    case kAudioQueueDeviceProperty_SampleRate:
        *size = sizeof(Float64);
        if (out)
            *(Float64 *)out = q->device.mSampleRate;
        return noErr;
    case kAudioQueueDeviceProperty_NumberChannels:
        *size = sizeof(UInt32);
        if (out)
            *(UInt32 *)out = q->device.mChannelsPerFrame;
        return noErr;
    case kAudioQueueProperty_StreamDescription:
        *size = sizeof(AudioStreamBasicDescription);
        if (out)
            *(AudioStreamBasicDescription *)out = q->format;
        return noErr;
    case kAudioQueueProperty_CurrentDevice:
        *size = sizeof(CFStringRef);
        if (out)
            *(CFStringRef *)out = NULL;
        return noErr;
    }
    return kAudioQueueErr_InvalidProperty;
}

OSStatus AudioQueueGetPropertySize(AudioQueueRef q, AudioQueuePropertyID id, UInt32 *outSize)
{
    if (q == NULL || outSize == NULL)
        return kAudio_ParamError;
    return property(q, id, NULL, outSize);
}

OSStatus AudioQueueGetProperty(AudioQueueRef q, AudioQueuePropertyID id, void *out, UInt32 *ioSize)
{
    UInt32 size = 0;

    if (q == NULL || out == NULL || ioSize == NULL)
        return kAudio_ParamError;
    OSStatus err = property(q, id, NULL, &size);
    if (err != noErr)
        return err;
    if (*ioSize < size)
        return kAudioQueueErr_InvalidPropertySize;
    *ioSize = size;
    return property(q, id, out, &size);
}

// the queue converts to the device's own layout, so a channel layout or device is accepted without changing anything
OSStatus AudioQueueSetProperty(AudioQueueRef q, AudioQueuePropertyID id, const void *data, UInt32 size)
{
    if (q == NULL || (data == NULL && size != 0))
        return kAudio_ParamError;
    switch (id) {
    case kAudioQueueProperty_ChannelLayout:
    case kAudioQueueProperty_CurrentDevice:
        return noErr;
    }
    return kAudioQueueErr_InvalidProperty;
}

OSStatus AudioQueueNewInput(const AudioStreamBasicDescription *format, AudioQueueInputCallback callback, void *userData,
                            CFRunLoopRef runLoop, CFStringRef runLoopMode, UInt32 flags, AudioQueueRef *outQ)
{
    if (format == NULL || callback == NULL || outQ == NULL)
        return kAudio_ParamError;
    *outQ = NULL;
    return kAudioQueueErr_InvalidDevice;
}

OSStatus AudioQueueAddPropertyListener(AudioQueueRef q, AudioQueuePropertyID id, AudioQueuePropertyListenerProc proc, void *data)
{
    if (q == NULL || proc == NULL)
        return kAudio_ParamError;
    struct listener *l = calloc(1, sizeof(*l));
    if (l == NULL)
        return kAudio_MemFullError;
    *l = (struct listener){ id, proc, data, NULL };
    pthread_mutex_lock(&q->lock);
    l->next = q->listeners;
    q->listeners = l;
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

OSStatus AudioQueueRemovePropertyListener(AudioQueueRef q, AudioQueuePropertyID id, AudioQueuePropertyListenerProc proc, void *data)
{
    OSStatus result = kAudio_ParamError;

    if (q == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    for (struct listener **p = &q->listeners; *p; p = &(*p)->next) {
        if ((*p)->id == id && (*p)->proc == proc && (*p)->data == data) {
            struct listener *l = *p;
            *p = l->next;
            free(l);
            result = noErr;
            break;
        }
    }
    pthread_mutex_unlock(&q->lock);
    return result;
}

OSStatus AudioQueueGetCurrentTime(AudioQueueRef q, void *timeline, AudioTimeStamp *outTime, Boolean *outDiscontinuity)
{
    (void)timeline;
    if (q == NULL || outTime == NULL)
        return kAudio_ParamError;
    memset(outTime, 0, sizeof(*outTime));
    outTime->mSampleTime = q->sampleTime;
    outTime->mHostTime = AudioGetCurrentHostTime();
    outTime->mFlags = kAudioTimeStampSampleHostTimeValid;
    if (outDiscontinuity)
        *outDiscontinuity = false;
    return noErr;
}
