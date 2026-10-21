/*
 * pcmplay: write raw 16-bit/48kHz/stereo PCM to /dev/dsp0 (RavynHDAudio).
 *
 * RavynHDAudio's writePCM() paces itself against hardware's real DMA read
 * position (SD_LPIB) and blocks the write() until there's genuine free
 * space in the ring buffer - so this tool just writes as fast as it can
 * and lets the kernel-side flow control do the pacing. (An earlier
 * version paced writes here too via a fixed usleep(); running both a
 * guessed-rate sleep AND real hardware-rate blocking at once produced
 * exactly the drift/stutter you'd expect from two independent clocks
 * fighting each other.)
 *
 * Usage: pcmplay file.pcm     (raw PCM, no header)
 *        cmd | pcmplay -      (stdin)
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BYTES_PER_SEC   192000
#define CHUNK_BYTES     8192

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file.pcm | ->\n", argv[0]);
        fprintf(stderr, "  raw signed 16-bit little-endian, 48000 Hz, stereo, no header\n");
        return 1;
    }

    int in = strcmp(argv[1], "-") == 0 ? STDIN_FILENO : open(argv[1], O_RDONLY);
    if (in < 0) {
        perror("open input");
        return 1;
    }

    int out = open("/dev/dsp0", O_WRONLY);
    if (out < 0) {
        perror("open /dev/dsp0");
        if (in != STDIN_FILENO) {
            close(in);
        }
        return 1;
    }

    unsigned char buf[CHUNK_BYTES];
    long total = 0;
    int ret = 0;
    for (;;) {
        ssize_t n = read(in, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            ret = 1;
            break;
        }
        if (n == 0) {
            break;
        }

        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, n - off);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("write /dev/dsp0");
                ret = 1;
                goto done;
            }
            off += w;
        }
        total += n;
    }

done:
    if (ret == 0) {
        fprintf(stderr, "pcmplay: wrote %ld bytes (%.1f sec)\n",
                total, (double)total / BYTES_PER_SEC);
    }

    if (in != STDIN_FILENO) {
        close(in);
    }
    close(out);
    return ret;
}
