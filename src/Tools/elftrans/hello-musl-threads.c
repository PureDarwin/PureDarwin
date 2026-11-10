// hello-musl-threads.c - exercise multiple joins and futex results
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

static int values[4];

static void *
worker(void *argument)
{
    int *value = argument;
    uint64_t mask = 0;
    if (syscall(14, 2, NULL, &mask, 8) ||
        !(mask & (1ULL << (10 - 1)))) return NULL;
    *value = 42;
    return argument;
}

int
main(void)
{
    int word = 1;
    struct timespec short_wait = {0, 1000000};
    if (syscall(202, &word, 128, 0, NULL, NULL, 0) != -1 ||
        errno != EAGAIN) return 1;
    if (syscall(202, &word, 129, 1, NULL, NULL, 0) != 0) return 2;
    if (syscall(202, &word, 128, 1, &short_wait, NULL, 0) != -1 ||
        errno != ETIMEDOUT) return 3;
    if (syscall(324, 0, 0) < 0) return 6;

    uint64_t usr1 = 1ULL << (10 - 1);
    uint64_t old = ~0ULL;
    if (syscall(14, 0, &usr1, &old, 8) || old) return 7;
    uint64_t current = 0;
    if (syscall(14, 99, NULL, &current, 8) || current != usr1)
        return 8;
    if (syscall(14, 0, &usr1, NULL, 4) != -1 || errno != EINVAL)
        return 9;

    pthread_t threads[4];
    for (int i = 0; i < 4; ++i)
        if (pthread_create(&threads[i], NULL, worker, &values[i])) return 4;
    for (int i = 0; i < 4; ++i) {
        void *answer = NULL;
        if (pthread_join(threads[i], &answer) || answer != &values[i] ||
            values[i] != 42) return 5;
    }
    if (syscall(14, 1, &usr1, &old, 8) || old != usr1) return 10;
    if (syscall(14, 2, NULL, &current, 8) || current) return 11;
    puts("musl threads and futex on XNU");
    return 0;
}
