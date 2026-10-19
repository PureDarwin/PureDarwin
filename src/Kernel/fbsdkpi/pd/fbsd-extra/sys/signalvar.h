// SPDX-License-Identifier: MIT
// FreeBSD's sys/signalvar.h, no signals for kernel threads
#pragma once
// no signals reach kernel threads here
#define SIGPENDING(td) 0
struct thread;
static inline int cursig(struct thread *td) { return 0; }
