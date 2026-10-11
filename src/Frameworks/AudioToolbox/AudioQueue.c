// AudioQueue output over /dev/dsp0 (48 kHz, s16, stereo). Buffers play in order on a worker thread, then go back
// to the client's callback. Without a device they are paced in real time, so clients see the same timing
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef int32_t OSStatus;
typedef unsigned char Boolean;

typedef struct AudioStreamBasicDescription {
    double mSampleRate;
    uint32_t mFormatID;
    uint32_t mFormatFlags;
    uint32_t mBytesPerPacket;
    uint32_t mFramesPerPacket;
    uint32_t mBytesPerFrame;
    uint32_t mChannelsPerFrame;
    uint32_t mBitsPerChannel;
    uint32_t mReserved;
} AudioStreamBasicDescription;

typedef struct AudioStreamPacketDescription {
    int64_t mStartOffset;
    uint32_t mVariableFramesInPacket;
    uint32_t mDataByteSize;
} AudioStreamPacketDescription;

typedef struct AudioQueueBuffer {
    uint32_t mAudioDataBytesCapacity;
    void *mAudioData;
    uint32_t mAudioDataByteSize;
    void *mUserData;
    uint32_t mPacketDescriptionCapacity;
    AudioStreamPacketDescription *mPacketDescriptions;
    uint32_t mPacketDescriptionCount;
} AudioQueueBuffer, *AudioQueueBufferRef;

typedef struct OpaqueAudioQueue *AudioQueueRef;
typedef void (*AudioQueueOutputCallback)(void *userData, AudioQueueRef queue, AudioQueueBufferRef buffer);

enum {
    kAudioFormatLinearPCM = 0x6c70636d, // 'lpcm'
    kAudioFormatFlagIsFloat = 1u << 0,
    kAudioFormatFlagIsBigEndian = 1u << 1,
    kAudioFormatFlagIsSignedInteger = 1u << 2,
    kAudioFormatFlagIsNonInterleaved = 1u << 5,
};

enum {
    kAudioQueueErr_InvalidBuffer = -66687,
    kAudioQueueErr_InvalidQueueType = -66682,
    kAudioQueueErr_InvalidParameter = -66671,
    kAudioFormatUnsupportedDataFormatError = 0x666d743f, // 'fmt?'
};

#define DEVICE_RATE 48000

struct entry {
    AudioQueueBufferRef buffer;
    struct entry *next;
};

struct OpaqueAudioQueue {
    AudioStreamBasicDescription format;
    AudioQueueOutputCallback callback;
    void *userData;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct entry *head, *tail;
    pthread_t thread;
    int running;
    int fd;
    double phase; // source frames consumed, for resampling to the device rate
    AudioQueueBufferRef *buffers; // owned, freed with the queue
    size_t bufferCount;
};

static int formatSupported(const AudioStreamBasicDescription *f) {
    if (f->mFormatID != kAudioFormatLinearPCM || f->mSampleRate <= 0)
        return 0;
    if (f->mChannelsPerFrame < 1 || f->mChannelsPerFrame > 2)
        return 0;
    if (f->mFormatFlags & (kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsNonInterleaved))
        return 0;
    if (f->mFormatFlags & kAudioFormatFlagIsFloat)
        return f->mBitsPerChannel == 32;
    return f->mBitsPerChannel == 16;
}

static float sampleAt(const AudioQueueRef q, const uint8_t *data, uint32_t frame, uint32_t channel) {
    uint32_t c = channel < q->format.mChannelsPerFrame ? channel : 0;

    if (q->format.mFormatFlags & kAudioFormatFlagIsFloat)
        return ((const float *)data)[frame * q->format.mChannelsPerFrame + c];
    return ((const int16_t *)data)[frame * q->format.mChannelsPerFrame + c] / 32768.0f;
}

static void play(AudioQueueRef q, AudioQueueBufferRef b) {
    uint32_t frames = q->format.mBytesPerFrame ? b->mAudioDataByteSize / q->format.mBytesPerFrame : 0;

    if (frames == 0)
        return;
    if (q->fd < 0) {
        double seconds = frames / q->format.mSampleRate;
        struct timespec ts = { (time_t)seconds, (long)((seconds - (time_t)seconds) * 1e9) };
        nanosleep(&ts, NULL);
        return;
    }

    double step = q->format.mSampleRate / DEVICE_RATE;
    size_t outFrames = (size_t)((frames - q->phase) / step) + 1;
    int16_t *out = malloc(outFrames * 2 * sizeof(int16_t));
    size_t n = 0;

    if (out == NULL)
        return;
    for (; q->phase < frames; q->phase += step, n++) {
        for (uint32_t c = 0; c < 2; c++) {
            float s = sampleAt(q, b->mAudioData, (uint32_t)q->phase, c);
            s = s > 1.0f ? 1.0f : s < -1.0f ? -1.0f : s;
            out[n * 2 + c] = (int16_t)(s * 32767.0f);
        }
    }
    q->phase -= frames;

    // a blocking write paces playback at the device rate
    size_t bytes = n * 2 * sizeof(int16_t), done = 0;
    while (done < bytes) {
        ssize_t w = write(q->fd, (uint8_t *)out + done, bytes - done);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0)
            break;
        done += (size_t)w;
    }
    free(out);
}

static void *worker(void *arg) {
    AudioQueueRef q = arg;

    pthread_mutex_lock(&q->lock);
    while (q->running) {
        if (q->head == NULL) {
            pthread_cond_wait(&q->cond, &q->lock);
            continue;
        }
        struct entry *e = q->head;
        q->head = e->next;
        if (q->head == NULL)
            q->tail = NULL;
        pthread_mutex_unlock(&q->lock);

        AudioQueueBufferRef b = e->buffer;
        free(e);
        play(q, b);
        q->callback(q->userData, q, b);

        pthread_mutex_lock(&q->lock);
    }
    pthread_mutex_unlock(&q->lock);
    return NULL;
}

// the callback runs on the queue's own thread even when a run loop is given
OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *format, AudioQueueOutputCallback callback,
                             void *userData, void *callbackRunLoop, void *callbackRunLoopMode, uint32_t flags,
                             AudioQueueRef *outQueue) {
    (void)callbackRunLoop;
    (void)callbackRunLoopMode;
    (void)flags;
    if (format == NULL || callback == NULL || outQueue == NULL)
        return kAudioQueueErr_InvalidParameter;
    if (!formatSupported(format))
        return kAudioFormatUnsupportedDataFormatError;

    AudioQueueRef q = calloc(1, sizeof(*q));
    if (q == NULL)
        return kAudioQueueErr_InvalidParameter;
    q->format = *format;
    if (q->format.mBytesPerFrame == 0)
        q->format.mBytesPerFrame = q->format.mChannelsPerFrame * q->format.mBitsPerChannel / 8;
    q->callback = callback;
    q->userData = userData;
    q->fd = -1;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    *outQueue = q;
    return 0;
}

OSStatus AudioQueueAllocateBuffer(AudioQueueRef q, uint32_t size, AudioQueueBufferRef *outBuffer) {
    if (q == NULL || outBuffer == NULL)
        return kAudioQueueErr_InvalidParameter;

    AudioQueueBufferRef b = calloc(1, sizeof(*b) + size);
    if (b == NULL)
        return kAudioQueueErr_InvalidParameter;
    AudioQueueBufferRef *list = realloc(q->buffers, (q->bufferCount + 1) * sizeof(*list));
    if (list == NULL) {
        free(b);
        return kAudioQueueErr_InvalidParameter;
    }
    q->buffers = list;
    q->buffers[q->bufferCount++] = b;
    b->mAudioDataBytesCapacity = size;
    b->mAudioData = b + 1;
    *outBuffer = b;
    return 0;
}

OSStatus AudioQueueFreeBuffer(AudioQueueRef q, AudioQueueBufferRef b) {
    if (q == NULL || b == NULL)
        return kAudioQueueErr_InvalidBuffer;
    for (size_t i = 0; i < q->bufferCount; i++) {
        if (q->buffers[i] == b) {
            q->buffers[i] = q->buffers[--q->bufferCount];
            free(b);
            return 0;
        }
    }
    return kAudioQueueErr_InvalidBuffer;
}

OSStatus AudioQueueEnqueueBuffer(AudioQueueRef q, AudioQueueBufferRef b, uint32_t numPacketDescs,
                                 const AudioStreamPacketDescription *packetDescs) {
    (void)numPacketDescs;
    (void)packetDescs;
    if (q == NULL || b == NULL)
        return kAudioQueueErr_InvalidBuffer;

    struct entry *e = calloc(1, sizeof(*e));
    if (e == NULL)
        return kAudioQueueErr_InvalidBuffer;
    e->buffer = b;
    pthread_mutex_lock(&q->lock);
    if (q->tail)
        q->tail->next = e;
    else
        q->head = e;
    q->tail = e;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

OSStatus AudioQueueStart(AudioQueueRef q, const void *startTime) {
    (void)startTime;
    if (q == NULL)
        return kAudioQueueErr_InvalidParameter;

    pthread_mutex_lock(&q->lock);
    if (q->running) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    q->running = 1;
    pthread_mutex_unlock(&q->lock);
    if (q->fd < 0)
        q->fd = open("/dev/dsp0", O_WRONLY | O_CLOEXEC);
    if (pthread_create(&q->thread, NULL, worker, q) != 0) {
        q->running = 0;
        return kAudioQueueErr_InvalidParameter;
    }
    return 0;
}

// immediate or not, buffers still queued are dropped without being played out
OSStatus AudioQueueStop(AudioQueueRef q, Boolean immediate) {
    (void)immediate;
    if (q == NULL)
        return kAudioQueueErr_InvalidParameter;

    pthread_mutex_lock(&q->lock);
    int wasRunning = q->running;
    q->running = 0;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->lock);
    if (wasRunning && !pthread_equal(pthread_self(), q->thread))
        pthread_join(q->thread, NULL);
    if (q->fd >= 0) {
        close(q->fd);
        q->fd = -1;
    }
    return 0;
}

OSStatus AudioQueueDispose(AudioQueueRef q, Boolean immediate) {
    if (q == NULL)
        return kAudioQueueErr_InvalidParameter;

    AudioQueueStop(q, immediate);
    for (struct entry *e = q->head, *next; e; e = next) {
        next = e->next;
        free(e);
    }
    for (size_t i = 0; i < q->bufferCount; i++)
        free(q->buffers[i]);
    free(q->buffers);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
    return 0;
}
