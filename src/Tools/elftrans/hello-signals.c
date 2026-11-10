// hello-signals.c - handlers, masks, the alternate stack, an interrupted read, and SSE state across a syscall
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t got, on_alt;
static char altbuf[65536];

static void
on_usr1(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    if (sig == SIGUSR1 && si->si_signo == SIGUSR1) ++got;
}

static void
on_usr2(int sig)
{
    char here;

    (void)sig;
    on_alt = &here >= altbuf && &here < altbuf + sizeof(altbuf);
}

static void *
poke(void *target)
{
    usleep(200000);
    pthread_kill(*(pthread_t *)target, SIGUSR1);
    return NULL;
}

int
main(void)
{
    struct sigaction sa = {0};
    sa.sa_sigaction = on_usr1;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &sa, NULL)) return 1;
    raise(SIGUSR1);
    if (got != 1) return 2;

    // blocked, then delivered when unblocked
    sigset_t usr1;
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    sigprocmask(SIG_BLOCK, &usr1, NULL);
    raise(SIGUSR1);
    if (got != 1) return 3;
    sigprocmask(SIG_UNBLOCK, &usr1, NULL);
    if (got != 2) return 4;

    stack_t st = { .ss_sp = altbuf, .ss_size = sizeof(altbuf) };
    struct sigaction sb = {0};
    sb.sa_handler = on_usr2;
    sb.sa_flags = SA_ONSTACK;
    if (sigaltstack(&st, NULL) || sigaction(SIGUSR2, &sb, NULL)) return 5;
    raise(SIGUSR2);
    if (!on_alt) return 6;

    // no SA_RESTART: a signal from another thread ends a blocking read with EINTR
    int fds[2];
    pthread_t me = pthread_self(), other;
    char c;
    if (pipe(fds) || pthread_create(&other, NULL, poke, &me)) return 7;
    ssize_t n = read(fds[0], &c, 1);
    if (n != -1 || errno != EINTR || got != 3) return 8;
    pthread_join(other, NULL);

    // a syscall keeps the vector registers
    double in = 2.5, out = 0;
    __asm__ volatile("movsd %1, %%xmm7\n\tmov $39, %%eax\n\tsyscall\n\tmovsd %%xmm7, %0"
        : "=m"(out) : "m"(in) : "rax", "rcx", "r11", "xmm7", "memory");
    if (out != in) return 9;

    puts("signals on XNU");
    return 0;
}
