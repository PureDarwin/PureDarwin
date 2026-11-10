// hello-musl-file.c - exercise Linux libc file I/O and allocation
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    char *buffer = malloc(128);
    if (!buffer) return 1;

    int missing = open("/elftrans-no-such-file", O_RDONLY);
    if (missing >= 0 || errno != ENOENT) return 2;

    int fd = open("/etc/hosts", O_RDONLY);
    if (fd < 0) return 3;
    ssize_t count = read(fd, buffer, 128);
    if (close(fd) || count <= 0) return 4;

    puts("musl file I/O and malloc on XNU");
    free(buffer);
    return 0;
}
