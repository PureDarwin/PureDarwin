// signals.c - Linux signals for an elftrans guest: dispositions, masks, frames and rt_sigreturn
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <pthread.h>
#include <signal.h>
#include <libproc.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include "elftrans.h"

#define NSIG_LINUX 65
#define SA_LINUX_ONSTACK 0x08000000UL
#define SA_LINUX_RESTART 0x10000000UL
#define SA_LINUX_NODEFER 0x40000000UL
#define SA_LINUX_RESETHAND 0x80000000UL
#define SS_LINUX_ONSTACK 1
#define SS_LINUX_DISABLE 2
#define BIT(sig) (1ULL << ((sig) - 1))

typedef struct {
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
    uint16_t cs, gs, fs, ss;
    uint64_t err, trapno, oldmask, cr2, fpstate, reserved[8];
} LinuxSigcontext;

typedef struct {
    uintptr_t sp;
    int32_t flags, pad;
    uint64_t size;
} LinuxStack;

typedef struct {
    uint64_t flags;
    uintptr_t link;
    LinuxStack stack;
    LinuxSigcontext mcontext;
    uint64_t sigmask;
} LinuxUcontext;

typedef struct {
    int32_t signo, error, code, pad;
    union {
        struct {
            int32_t pid;
            uint32_t uid;
        } kill;
        uintptr_t addr;
        uint8_t raw[112];
    };
} LinuxSiginfo;

// what the handler sees: its return address (the restorer), then the context and the signal's details
typedef struct {
    uintptr_t pretcode;
    LinuxUcontext uc;
    LinuxSiginfo info;
} LinuxSigframe;

_Static_assert(sizeof(LinuxSigcontext) == 256 && sizeof(LinuxUcontext) == 304 && sizeof(LinuxSiginfo) == 128,
    "Linux x86-64 signal layout");

typedef struct {
    int tid;
    pthread_t thread;
    _Atomic uint64_t pending;
    LinuxStack altstack;
    LinuxSiginfo info[NSIG_LINUX];
} GuestThread;

__thread int guest_in_host = 1;
static __thread GuestThread *self;
static __thread uint64_t guest_mask;
static GuestThread *threads[4096];
static pthread_mutex_t threads_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t action_lock = PTHREAD_MUTEX_INITIALIZER;
static LinuxSigaction actions[NSIG_LINUX];
static uintptr_t host_text_lo, host_text_hi;

// Linux numbers to Darwin ones, 0 where Darwin has none (16 SIGSTKFLT, 30 SIGPWR, the real-time signals)
static const int native_of[32] = {
    0, SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS,
    SIGFPE, SIGKILL, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM,
    SIGTERM, 0, SIGCHLD, SIGCONT, SIGSTOP, SIGTSTP, SIGTTIN,
    SIGTTOU, SIGURG, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF,
    SIGWINCH, SIGIO, 0, SIGSYS
};
static int linux_of[32];

// a signal for another guest thread with no Darwin counterpart rides on SIGEMT, which Linux has no number for
#define CARRIER SIGEMT

static sigset_t
native_mask(uint64_t mask)
{
    sigset_t set;

    sigemptyset(&set);
    for (int sig = 1; sig < 32; ++sig)
        if (native_of[sig] && sig != 5 && (mask & BIT(sig)))
            sigaddset(&set, native_of[sig]);
    return set;
}

// SIGKILL and SIGSTOP never blocked, the native mask follows the guest's
static void
set_mask(uint64_t mask)
{
    guest_mask = mask & ~(BIT(9) | BIT(19));
    sigset_t set = native_mask(guest_mask);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
}

uint64_t
signals_mask(void)
{
    return guest_mask;
}

void
signals_thread_start(int tid, uint64_t mask)
{
    GuestThread *t = calloc(1, sizeof(*t));

    if (!t) _exit(126);
    t->tid = tid, t->thread = pthread_self();
    self = t;
    pthread_mutex_lock(&threads_lock);
    for (unsigned i = 0; i < sizeof(threads) / sizeof(threads[0]); ++i)
        if (!threads[i]) {
            threads[i] = t;
            break;
        }
    pthread_mutex_unlock(&threads_lock);
    set_mask(mask);
}

void
signals_thread_exit(void)
{
    pthread_mutex_lock(&threads_lock);
    for (unsigned i = 0; i < sizeof(threads) / sizeof(threads[0]); ++i)
        if (threads[i] == self)
            threads[i] = NULL;
    pthread_mutex_unlock(&threads_lock);
}

static GuestThread *
thread_of(int tid)
{
    GuestThread *found = NULL;

    pthread_mutex_lock(&threads_lock);
    for (unsigned i = 0; i < sizeof(threads) / sizeof(threads[0]) && !found; ++i)
        if (threads[i] && threads[i]->tid == tid)
            found = threads[i];
    pthread_mutex_unlock(&threads_lock);
    return found;
}

static void
post(GuestThread *t, int sig, const LinuxSiginfo *info)
{
    t->info[sig] = *info;
    atomic_fetch_or(&t->pending, BIT(sig));
}

int
signals_pending(void)
{
    return self && (atomic_load(&self->pending) & ~guest_mask) != 0;
}

// default actions: ignore for these, terminate (Darwin's own default for the mapped ones) for the rest
static int
ignored_by_default(int sig)
{
    return sig == 17 || sig == 18 || sig == 23 || sig == 28 || sig == 20 || sig == 21 || sig == 22;
}

static _Noreturn void
die(int sig)
{
    int native = sig < 32 ? native_of[sig] : 0;

    if (native) {
        sigset_t set;
        signal(native, SIG_DFL);
        sigemptyset(&set);
        sigaddset(&set, native);
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
        pthread_kill(pthread_self(), native);
    }
    _exit(128 + sig);
}

static int
on_altstack(uintptr_t sp)
{
    return self->altstack.sp && sp > self->altstack.sp && sp - self->altstack.sp <= self->altstack.size;
}

// the frame a Linux kernel would build: on the alternate stack when asked, else below the red zone
static void
build_frame(GuestRegs *r, int sig, const LinuxSigaction *action)
{
    uintptr_t sp = r->rsp;
    int altstack = 0;

    if ((action->flags & SA_LINUX_ONSTACK) && self->altstack.sp && !(self->altstack.flags & SS_LINUX_DISABLE) &&
        !on_altstack(sp))
        sp = self->altstack.sp + self->altstack.size, altstack = 1;
    else
        sp -= 128;
    uintptr_t fp = (sp - xsave_size) & ~(uintptr_t)63;
    memcpy((void *)fp, (void *)r->xsave, xsave_size);
    LinuxSigframe *f = (LinuxSigframe *)((((fp - sizeof(LinuxSigframe)) & ~(uintptr_t)15)) - 8);
    memset(f, 0, sizeof(*f));
    f->pretcode = action->restorer;
    f->uc.stack = self->altstack;
    f->uc.stack.flags = altstack || on_altstack(r->rsp) ? SS_LINUX_ONSTACK : self->altstack.sp ? 0 : SS_LINUX_DISABLE;
    LinuxSigcontext *m = &f->uc.mcontext;
    m->r8 = r->r8, m->r9 = r->r9, m->r10 = r->r10, m->r11 = r->r11;
    m->r12 = r->r12, m->r13 = r->r13, m->r14 = r->r14, m->r15 = r->r15;
    m->rdi = r->rdi, m->rsi = r->rsi, m->rbp = r->rbp, m->rbx = r->rbx;
    m->rdx = r->rdx, m->rax = r->rax, m->rcx = r->rcx, m->rsp = r->rsp, m->rip = r->rip, m->eflags = r->rflags;
    m->cs = 0x33, m->ss = 0x2b, m->fpstate = fp, m->oldmask = guest_mask;
    if (sig == 11 || sig == 7)
        m->cr2 = self->info[sig].addr;
    f->uc.sigmask = guest_mask;
    f->info = self->info[sig];

    uint64_t mask = guest_mask | action->mask;
    if (!(action->flags & SA_LINUX_NODEFER))
        mask |= BIT(sig);
    set_mask(mask);
    r->rsp = (uintptr_t)f, r->rip = action->handler;
    r->rdi = (uint64_t)sig, r->rsi = (uintptr_t)&f->info, r->rdx = (uintptr_t)&f->uc, r->rax = 0;
    r->rflags &= ~(uint64_t)0x500;
}

// on the way back to the guest: one deliverable signal gets its handler, an interrupted syscall restarts or fails
void
signals_deliver(GuestRegs *r)
{
    uint64_t ready;
    int restart = r->kind == FRAME_SYSCALL && (long)r->rax == LINUX_ERESTARTSYS;

    while (self && (ready = atomic_load(&self->pending) & ~guest_mask)) {
        int sig = __builtin_ctzll(ready) + 1;
        atomic_fetch_and(&self->pending, ~BIT(sig));
        pthread_mutex_lock(&action_lock);
        LinuxSigaction action = actions[sig];
        if (action.handler > 1 && (action.flags & SA_LINUX_RESETHAND))
            actions[sig].handler = 0;
        pthread_mutex_unlock(&action_lock);
        if (action.handler == 1 || (action.handler == 0 && ignored_by_default(sig)))
            continue;
        if (action.handler == 0)
            die(sig);
        if (restart) {
            if (action.flags & SA_LINUX_RESTART)
                r->rax = r->orig_rax, r->rip -= 2;
            else
                r->rax = (uint64_t)-4;
            restart = 0;
        }
        build_frame(r, sig, &action);
        return;
    }
    // interrupted with nothing to handle: the syscall runs again
    if (restart)
        r->rax = r->orig_rax, r->rip -= 2;
}

long
sys_rt_sigreturn(GuestRegs *r)
{
    LinuxUcontext *uc = (LinuxUcontext *)(uintptr_t)r->rsp;
    LinuxSigcontext *m = &uc->mcontext;

    r->r8 = m->r8, r->r9 = m->r9, r->r10 = m->r10, r->r11 = m->r11;
    r->r12 = m->r12, r->r13 = m->r13, r->r14 = m->r14, r->r15 = m->r15;
    r->rdi = m->rdi, r->rsi = m->rsi, r->rbp = m->rbp, r->rbx = m->rbx;
    r->rdx = m->rdx, r->rax = m->rax, r->rcx = m->rcx, r->rsp = m->rsp, r->rip = m->rip;
    r->rflags = (r->rflags & ~(uint64_t)0x40dd5) | (m->eflags & 0x40dd5);
    if (m->fpstate)
        memcpy((void *)r->xsave, (void *)(uintptr_t)m->fpstate, xsave_size);
    r->kind = FRAME_SIGNAL;
    set_mask(uc->sigmask);
    return SYSCALL_RESTORED;
}

// ELFTRANS_CRASHDUMP: a fault in guest code prints its registers and the top of its stack, each value with
// the file mapped there, so a return address names the caller
static void
crash_name(uint64_t v, char *out, size_t size)
{
    char path[PATH_MAX];

    out[0] = 0;
    if (v < 0x1000 || proc_regionfilename(getpid(), v, path, sizeof(path)) <= 0)
        return;
    const char *base = strrchr(path, '/');
    snprintf(out, size, " %s", base ? base + 1 : path);
}

void
crash_dump(int code, void *address, const x86_thread_state64_t *s)
{
    static const char *const names[] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "r8", "r9",
        "r10", "r11", "r12", "r13", "r14", "r15", "rip" };
    const uint64_t regs[] = { s->__rax, s->__rbx, s->__rcx, s->__rdx, s->__rsi, s->__rdi, s->__rbp, s->__rsp,
        s->__r8, s->__r9, s->__r10, s->__r11, s->__r12, s->__r13, s->__r14, s->__r15, s->__rip };
    char line[PATH_MAX + 96], name[PATH_MAX + 8];
    int n;

    n = snprintf(line, sizeof(line), "elftrans: crash, %d at %p\n", code, address);
    write(2, line, (size_t)n);
    for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        crash_name(regs[i], name, sizeof(name));
        n = snprintf(line, sizeof(line), "elftrans:   %-3s %016llx%s\n", names[i], (unsigned long long)regs[i], name);
        write(2, line, (size_t)n);
    }
    // on the faulting thread (the signal path, code is the signal) its TLS: the FS base set for it and the TCB
    // there, whose first word glibc points back at itself, with the tid at 0x2d0 in glibc's struct pthread
    if (code < 32) {
        extern uint64_t elftrans_guest_fs_base(void);
        uint64_t fs = elftrans_guest_fs_base();

        n = snprintf(line, sizeof(line), "elftrans:   fs base %016llx%s\n", (unsigned long long)fs,
            fs ? "" : " (none)");
        write(2, line, (size_t)n);
        if (fs) {
            const uint64_t *tcb = (const uint64_t *)(uintptr_t)fs;

            crash_name(tcb[0], name, sizeof(name));
            n = snprintf(line, sizeof(line), "elftrans:   fs:0 %016llx%s, fs:0x10 %016llx, tid %d\n",
                (unsigned long long)tcb[0], name, (unsigned long long)tcb[2], *(const int *)(uintptr_t)(fs + 0x2d0));
            write(2, line, (size_t)n);
        }
    }
    const uint64_t *sp = (const uint64_t *)(uintptr_t)s->__rsp;
    for (int i = 0; i < 32; i++) {
        crash_name(sp[i], name, sizeof(name));
        if (!name[0])
            continue;
        n = snprintf(line, sizeof(line), "elftrans:   [rsp+%#x] %016llx%s\n", i * 8, (unsigned long long)sp[i], name);
        write(2, line, (size_t)n);
    }
}

// in guest code the thread goes in through elftrans_entry and gets the signal there, in elftrans or libSystem
// code it waits as pending while the host call returns EINTR
static void
host_signal(int native, siginfo_t *si, void *context)
{
    ucontext_t *uc = context;
    x86_thread_state64_t *s = &uc->uc_mcontext->__ss;
    int sig = native < 32 ? linux_of[native] : 0;
    int fault = native == SIGSEGV || native == SIGBUS || native == SIGILL || native == SIGFPE;
    uintptr_t rip = (uintptr_t)s->__rip;
    int host = !self || guest_in_host || (rip >= host_text_lo && rip < host_text_hi);

    if (fault && host) {
        char line[160];
        int n = snprintf(line, sizeof(line), "elftrans: signal %d in elftrans at %#llx address %p\n", native,
            (unsigned long long)rip, si->si_addr);
        if (n > 0) write(2, line, (size_t)(n < (int)sizeof(line) ? n : (int)sizeof(line) - 1));
        _exit(128 + native);
    }
    if (fault && !host && getenv("ELFTRANS_CRASHDUMP"))
        crash_dump(native, si->si_addr, s);
    if (!self)
        return;
    if (sig) {
        LinuxSiginfo info = {0};
        info.signo = sig, info.code = si->si_code;
        if (fault)
            info.addr = (uintptr_t)si->si_addr;
        else
            info.kill.pid = si->si_pid, info.kill.uid = si->si_uid;
        post(self, sig, &info);
    }
    if (host)
        return;

    GuestRegs g;
    g.rax = s->__rax, g.rbx = s->__rbx, g.rcx = s->__rcx, g.rdx = s->__rdx;
    g.rsi = s->__rsi, g.rdi = s->__rdi, g.rbp = s->__rbp, g.rsp = s->__rsp;
    g.r8 = s->__r8, g.r9 = s->__r9, g.r10 = s->__r10, g.r11 = s->__r11;
    g.r12 = s->__r12, g.r13 = s->__r13, g.r14 = s->__r14, g.r15 = s->__r15;
    g.rip = s->__rip, g.rflags = s->__rflags;
    s->__rbx = push_frame(&g, FRAME_SIGNAL);
    s->__rsp = g.xsave;
    s->__rip = (uint64_t)(uintptr_t)elftrans_entry;
}

static int
install_native(int native, uintptr_t handler)
{
    struct sigaction sa = {0};

    sigfillset(&sa.sa_mask);
    if (handler > 1 || native == SIGSEGV || native == SIGBUS || native == SIGILL || native == SIGFPE ||
        native == CARRIER) {
        sa.sa_sigaction = host_signal;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    } else {
        sa.sa_handler = handler ? SIG_IGN : SIG_DFL;
    }
    return sigaction(native, &sa, NULL);
}

int
signals_install(void)
{
    unsigned long size = 0;
    const uint8_t *text = getsegmentdata(&_mh_execute_header, "__TEXT", &size);

    host_text_lo = (uintptr_t)text, host_text_hi = (uintptr_t)text + size;
    for (int sig = 1; sig < 32; ++sig)
        if (native_of[sig])
            linux_of[native_of[sig]] = sig;
    if (install_native(SIGSEGV, 0) || install_native(SIGBUS, 0) || install_native(SIGILL, 0) ||
        install_native(SIGFPE, 0) || install_native(CARRIER, 0))
        return -1;
    signals_thread_start(getpid(), 0);
    return 0;
}

// fork: no other thread may hold the signal tables while the process is copied
void
signals_fork_prepare(void)
{
    pthread_mutex_lock(&action_lock);
    pthread_mutex_lock(&threads_lock);
}

void
signals_fork_parent(void)
{
    pthread_mutex_unlock(&threads_lock);
    pthread_mutex_unlock(&action_lock);
}

// the child: the forking thread is the only one left, with nothing pending, as a Linux child starts
void
signals_fork_child(int tid)
{
    memset(threads, 0, sizeof(threads));
    if (self) {
        self->tid = tid;
        atomic_store(&self->pending, 0);
        threads[0] = self;
    }
    pthread_mutex_unlock(&threads_lock);
    pthread_mutex_unlock(&action_lock);
}

// the Linux signals the guest ignores: exec keeps them ignored, and the mask, as Linux does
uint64_t
signals_ignored(void)
{
    uint64_t mask = 0;

    pthread_mutex_lock(&action_lock);
    for (int sig = 1; sig < NSIG_LINUX; ++sig)
        if (actions[sig].handler == 1)
            mask |= BIT(sig);
    pthread_mutex_unlock(&action_lock);
    return mask;
}

void
signals_inherit(uint64_t mask, uint64_t ignored)
{
    pthread_mutex_lock(&action_lock);
    for (int sig = 1; sig < NSIG_LINUX; ++sig) {
        int native = sig < 32 ? native_of[sig] : 0;

        if (!(ignored & BIT(sig)) || sig == 9 || sig == 19)
            continue;
        actions[sig].handler = 1;
        if (native && sig != 5)
            install_native(native, 1);
    }
    pthread_mutex_unlock(&action_lock);
    set_mask(mask);
}

// a Darwin signal number as Linux numbers it, for wait statuses
int
signals_linux_number(int native)
{
    return native > 0 && native < 32 && linux_of[native] ? linux_of[native] : native;
}

long
sys_rt_sigaction(GuestRegs *r)
{
    int sig = (int)r->rdi;
    const LinuxSigaction *action = (const LinuxSigaction *)(uintptr_t)r->rsi;
    LinuxSigaction *previous = (LinuxSigaction *)(uintptr_t)r->rdx;

    if (r->r10 != sizeof(uint64_t) || sig < 1 || sig >= NSIG_LINUX)
        return -22;
    if (action && (sig == 9 || sig == 19))
        return -22;
    pthread_mutex_lock(&action_lock);
    LinuxSigaction old = actions[sig];
    long result = 0;
    if (action) {
        int native = sig < 32 ? native_of[sig] : 0;
        // the int3 of converted sites is elftrans's own, a guest SIGTRAP disposition is kept but not installed
        if (native && sig != 5 && install_native(native, action->handler))
            result = -linux_error(errno);
        else
            actions[sig] = *action;
    }
    pthread_mutex_unlock(&action_lock);
    if (previous && !result)
        *previous = old;
    return result;
}

long
sys_rt_sigprocmask(GuestRegs *r)
{
    int how = (int)r->rdi;
    const uint64_t *set = (const uint64_t *)(uintptr_t)r->rsi;
    uint64_t *old = (uint64_t *)(uintptr_t)r->rdx;
    uint64_t previous = guest_mask;

    if (r->r10 != sizeof(uint64_t))
        return -22;
    if (set) {
        if (how == 0) set_mask(guest_mask | *set);
        else if (how == 1) set_mask(guest_mask & ~*set);
        else if (how == 2) set_mask(*set);
        else return -22;
    }
    if (old) *old = previous;
    return 0;
}

long
sys_sigaltstack(GuestRegs *r)
{
    const LinuxStack *ss = (const LinuxStack *)(uintptr_t)r->rdi;
    LinuxStack *old = (LinuxStack *)(uintptr_t)r->rsi;

    if (old) {
        *old = self->altstack;
        old->flags = !self->altstack.sp ? SS_LINUX_DISABLE : on_altstack(r->rsp) ? SS_LINUX_ONSTACK : 0;
    }
    if (ss) {
        if (on_altstack(r->rsp))
            return -1;
        if (ss->flags & SS_LINUX_DISABLE)
            memset(&self->altstack, 0, sizeof(self->altstack));
        else if (ss->size < 2048)
            return -12;
        else
            self->altstack = *ss, self->altstack.flags = 0;
    }
    return 0;
}

// a signal for one guest thread: this one gets it on the way out, another is woken by its Darwin signal
static long
send_to_thread(int tid, int sig)
{
    GuestThread *t = thread_of(tid);
    LinuxSiginfo info = {0};

    if (!t) return -3;
    if (sig < 0 || sig >= NSIG_LINUX) return -22;
    if (!sig) return 0;
    info.signo = sig, info.code = -6, info.kill.pid = getpid(), info.kill.uid = getuid();
    post(t, sig, &info);
    if (t != self)
        pthread_kill(t->thread, sig < 32 && native_of[sig] ? native_of[sig] : CARRIER);
    return 0;
}

long
sys_tkill(GuestRegs *r)
{
    return send_to_thread((int)r->rdi, (int)r->rsi);
}

long
sys_tgkill(GuestRegs *r)
{
    if ((int)r->rdi != getpid()) return -3;
    return send_to_thread((int)r->rsi, (int)r->rdx);
}

long
sys_kill(GuestRegs *r)
{
    int pid = (int)r->rdi, sig = (int)r->rsi;

    if (sig < 0 || sig >= NSIG_LINUX) return -22;
    // this process: a Darwin signal for the ones with a counterpart, this thread for the others
    if (pid == getpid() || pid == 0) {
        if (sig < 32 && native_of[sig])
            return kill(getpid(), native_of[sig]) ? -linux_error(errno) : 0;
        return send_to_thread(self->tid, sig);
    }
    if (sig >= 32 || (sig && !native_of[sig])) return -22;
    return kill(pid, sig ? native_of[sig] : 0) ? -linux_error(errno) : 0;
}
