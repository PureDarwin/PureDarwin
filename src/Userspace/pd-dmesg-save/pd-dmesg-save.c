// a copy of the kernel log on disk every 2 s, so a tester can read it back after a hang or a power-off
#include <fcntl.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

#define kInterval 2

static int log_save_disabled(void)
{
    char args[1024] = "";
    size_t len = sizeof(args) - 1;

    if (access("/etc/no-dmesg-save", F_OK) == 0)
        return 1;
    if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
        return 0;
    args[len] = '\0';
    for (char *p = args; (p = strstr(p, "pdlogsave=0")) != NULL; p++) {
        if ((p == args || p[-1] == ' ') && (p[11] == '\0' || p[11] == ' '))
            return 1;
    }
    return 0;
}

// the log has nul padding where the ring wrapped, a file of plain text is easier to read back
static size_t squeeze(char *buf, size_t len)
{
    size_t out = 0;

    for (size_t i = 0; i < len; i++) {
        if (buf[i] != '\0')
            buf[out++] = buf[i];
    }
    return out;
}

static void save(const char *path, const char *buf, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0)
        return;
    for (size_t done = 0; done < len; ) {
        ssize_t n = write(fd, buf + done, len - done);
        if (n <= 0)
            break;
        done += (size_t)n;
    }
    // this file only, a global sync every tick kept the whole disk busy
    fsync(fd);
    close(fd);
}

int main(void)
{
    int size = 0;
    size_t sizelen = sizeof(size);
    char *buf, *last;
    size_t lastlen = 0;
    unsigned n = 0;

    if (log_save_disabled())
        return 0;
    if (sysctlbyname("kern.msgbuf", &size, &sizelen, NULL, 0) != 0 || size <= 0) {
        fprintf(stderr, "pd-dmesg-save: no kernel log size\n");
        return 1;
    }
    buf = malloc((size_t)size);
    last = malloc((size_t)size);
    if (buf == NULL || last == NULL)
        return 1;

    for (;;) {
        int got = proc_kmsgbuf(buf, (uint32_t)size);
        size_t len = got > 0 ? squeeze(buf, (size_t)got) : 0;

        if (len != 0 && (len != lastlen || memcmp(buf, last, len) != 0)) {
            // two copies taken in turn, a power-off in the middle of a write still leaves the other one whole
            save(n++ % 2 ? "/var/log/dmesg-boot.1.txt" : "/var/log/dmesg-boot.0.txt", buf, len);
            memcpy(last, buf, len);
            lastlen = len;
        }
        sleep(kInterval);
    }
}
