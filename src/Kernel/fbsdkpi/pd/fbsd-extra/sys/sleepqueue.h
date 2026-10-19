// SPDX-License-Identifier: MIT
// FreeBSD's sys/sleepqueue.h
#pragma once
// sleepqueue(9) over the fbsdkpi wait seam, implemented in pd_lkpi_kern.c
#include <sys/types.h>

struct lock_object;

#define SLEEPQ_TYPE 0x0ff
#define SLEEPQ_SLEEP 0x00
#define SLEEPQ_CONDVAR 0x01
#define SLEEPQ_PAUSE 0x02
#define SLEEPQ_SX 0x03
#define SLEEPQ_LK 0x04
#define SLEEPQ_INTERRUPTIBLE 0x100
#define SLEEPQ_UNFAIR 0x200
#define SLEEPQ_DROP 0x400

void sleepq_lock(const void *wchan);
void sleepq_release(const void *wchan);
void sleepq_add(const void *wchan, struct lock_object *lock, const char *wmesg, int flags, int queue);
int sleepq_signal(const void *wchan, int flags, int pri, int queue);
int sleepq_broadcast(const void *wchan, int flags, int pri, int queue);
void sleepq_set_timeout(const void *wchan, int timo);
void sleepq_set_timeout_sbt(const void *wchan, sbintime_t sbt, sbintime_t pr, int flags);
void sleepq_wait(const void *wchan, int pri);
int sleepq_wait_sig(const void *wchan, int pri);
int sleepq_timedwait(const void *wchan, int pri);
int sleepq_timedwait_sig(const void *wchan, int pri);
u_int sleepq_sleepcnt(const void *wchan, int queue);
