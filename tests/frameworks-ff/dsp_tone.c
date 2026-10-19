// writes 2 s of a 880 Hz tone straight to /dev/dsp0, timing the writes, to check the device path alone
#include <fcntl.h>
#include <math.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/dsp0", O_WRONLY);
    if (fd < 0) {
        perror("dsp0");
        return 1;
    }
    static int16_t buf[48000 * 2 * 2];
    for (int i = 0; i < 96000; i++)
        buf[2 * i] = buf[2 * i + 1] = (int16_t)(12000 * sin(2 * M_PI * 880 * i / 48000.0));
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    uint64_t t0 = mach_absolute_time();
    ssize_t total = 0;
    for (size_t off = 0; off < sizeof(buf); off += 4096) {
        ssize_t w = write(fd, (char *)buf + off, 4096);
        if (w <= 0)
            break;
        total += w;
    }
    double secs = (mach_absolute_time() - t0) * (double)tb.numer / tb.denom / 1e9;
    printf("info dsp_tone wrote %zd bytes (2 s of audio) in %.2f s\n", total, secs);
    close(fd);
    return 0;
}
