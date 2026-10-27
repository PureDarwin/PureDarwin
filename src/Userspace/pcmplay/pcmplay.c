// pcmplay: play raw s16le 48 kHz stereo PCM through an AudioQueue
// usage: pcmplay file.pcm, or cmd | pcmplay -
#include <AudioToolbox/AudioToolbox.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SAMPLE_RATE     48000
#define BYTES_PER_FRAME 4
#define BYTES_PER_SEC   (SAMPLE_RATE * BYTES_PER_FRAME)
#define NUM_BUFFERS     3
#define BUFFER_BYTES    16384

struct player {
    int in;
    int eof;
    int error;
    int outstanding;
    long total;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

static size_t
fill(struct player *p, unsigned char *dst, size_t cap)
{
    size_t got = 0;

    while (got < cap) {
        ssize_t n = read(p->in, dst + got, cap - got);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("read");
            p->error = 1;
            break;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }

    // keep whole frames so the queue never sees a split sample
    return got - got % BYTES_PER_FRAME;
}

// runs on the queue's own thread each time a buffer has played
static void
refill(void *user, AudioQueueRef q, AudioQueueBufferRef buf)
{
    struct player *p = user;
    size_t n = 0;

    pthread_mutex_lock(&p->lock);
    if (!p->eof)
        n = fill(p, buf->mAudioData, buf->mAudioDataBytesCapacity);
    if (n == 0) {
        p->eof = 1;
        p->outstanding--;
        pthread_cond_signal(&p->cond);
        pthread_mutex_unlock(&p->lock);
        return;
    }
    p->total += (long)n;
    pthread_mutex_unlock(&p->lock);

    buf->mAudioDataByteSize = (UInt32)n;
    if (AudioQueueEnqueueBuffer(q, buf, 0, NULL) != noErr) {
        pthread_mutex_lock(&p->lock);
        p->eof = 1;
        p->error = 1;
        p->outstanding--;
        pthread_cond_signal(&p->cond);
        pthread_mutex_unlock(&p->lock);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file.pcm | ->\n", argv[0]);
        fprintf(stderr, "  raw signed 16-bit little-endian, 48000 Hz, stereo, no header\n");
        return 1;
    }

    struct player p;
    memset(&p, 0, sizeof(p));
    pthread_mutex_init(&p.lock, NULL);
    pthread_cond_init(&p.cond, NULL);

    p.in = strcmp(argv[1], "-") == 0 ? STDIN_FILENO : open(argv[1], O_RDONLY);
    if (p.in < 0) {
        perror("open input");
        return 1;
    }

    AudioStreamBasicDescription fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.mSampleRate = SAMPLE_RATE;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    fmt.mBytesPerPacket = BYTES_PER_FRAME;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerFrame = BYTES_PER_FRAME;
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 16;

    AudioQueueRef q = NULL;
    OSStatus err = AudioQueueNewOutput(&fmt, refill, &p, NULL, NULL, 0, &q);
    if (err != noErr) {
        fprintf(stderr, "pcmplay: AudioQueueNewOutput failed (%d)\n", (int)err);
        return 1;
    }

    // prime every buffer before starting so playback begins without a gap
    for (int i = 0; i < NUM_BUFFERS; i++) {
        AudioQueueBufferRef buf = NULL;
        if (AudioQueueAllocateBuffer(q, BUFFER_BYTES, &buf) != noErr)
            break;
        pthread_mutex_lock(&p.lock);
        p.outstanding++;
        pthread_mutex_unlock(&p.lock);
        refill(&p, q, buf);
    }

    err = AudioQueueStart(q, NULL);
    if (err != noErr) {
        fprintf(stderr, "pcmplay: AudioQueueStart failed (%d)\n", (int)err);
        p.error = 1;
    } else {
        pthread_mutex_lock(&p.lock);
        while (p.outstanding > 0) {
            pthread_cond_wait(&p.cond, &p.lock);
        }
        pthread_mutex_unlock(&p.lock);
    }

    AudioQueueStop(q, true);
    AudioQueueDispose(q, true);

    if (!p.error)
        fprintf(stderr, "pcmplay: played %ld bytes (%.1f sec)\n", p.total, (double)p.total / BYTES_PER_SEC);

    if (p.in != STDIN_FILENO)
        close(p.in);
    return p.error;
}
