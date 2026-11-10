// hello-syscalls.c - files, directories, clocks, ioctls, prctl and uname as a Linux program sees them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

int
main(void)
{
    char cwd[256], name[16] = {0};
    struct utsname u;
    struct timespec res;
    uint64_t mask = 0;
    int fds[2], on = 1;

    if (!getcwd(cwd, sizeof(cwd)) || cwd[0] != '/') return 1;
    if (uname(&u) || strcmp(u.sysname, "Linux") || strcmp(u.machine, "x86_64")) return 2;
    if (time(NULL) < 1700000000 || clock_getres(CLOCK_MONOTONIC, &res)) return 3;
    if (syscall(SYS_sched_getaffinity, 0, sizeof(mask), &mask) != 8 || !mask) return 4;
    if (syscall(SYS_prctl, 15, "elftrans-test", 0, 0, 0) || syscall(SYS_prctl, 16, name, 0, 0, 0) ||
        strcmp(name, "elftrans-test")) return 5;

    if (mkdir("/tmp/elftrans-st", 0755) && access("/tmp/elftrans-st", F_OK)) return 6;
    int fd = open("/tmp/elftrans-st/f", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || pwrite(fd, "hello world", 11, 0) != 11) return 7;
    if (lseek(fd, 6, SEEK_SET) != 6) return 8;
    char a[3] = {0}, b[3] = {0};
    struct iovec iov[2] = { { a, 2 }, { b, 3 } };
    if (readv(fd, iov, 2) != 5 || memcmp(a, "wo", 2) || memcmp(b, "rld", 3)) return 9;
    unsigned char sx[256];
    if (syscall(SYS_statx, AT_FDCWD, "/tmp/elftrans-st/f", 0, 0x7ff, sx)) return 10;
    if (*(uint64_t *)(sx + 40) != 11 || (*(uint16_t *)(sx + 28) & 0170000) != 0100000) return 11;
    struct timespec times[2] = { { 1000000000, 0 }, { 1000000000, 0 } };
    struct stat st;
    if (utimensat(AT_FDCWD, "/tmp/elftrans-st/f", times, 0) || stat("/tmp/elftrans-st/f", &st) ||
        st.st_mtime != 1000000000) return 12;
    if (truncate("/tmp/elftrans-st/f", 4) || fstat(fd, &st) || st.st_size != 4) return 13;
    if (fchmod(fd, 0600) || fstat(fd, &st) || (st.st_mode & 0777) != 0600) return 14;
    close(fd);
    if (unlinkat(AT_FDCWD, "/tmp/elftrans-st/f", 0) || rmdir("/tmp/elftrans-st")) return 15;

    if (pipe(fds) || ioctl(fds[0], FIONBIO, &on) || !(fcntl(fds[0], F_GETFL) & O_NONBLOCK)) return 16;
    int pending = -1;
    if (write(fds[1], "abc", 3) != 3 || ioctl(fds[0], FIONREAD, &pending) || pending != 3) return 17;
    if (isatty(fds[0])) return 18;

    puts("syscalls on XNU");
    return 0;
}
