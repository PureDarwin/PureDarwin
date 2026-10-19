// SPDX-License-Identifier: MIT
// FreeBSD's sys/sched.h scheduler hooks
#pragma once
// the scheduler hooks linuxkpi calls, xnu schedules its own threads
#define sched_pin() do { } while (0)
#define sched_unpin() do { } while (0)
#define sched_bind(td, cpu) do { } while (0)
#define sched_unbind(td) do { } while (0)
#define sched_is_bound(td) 0
#define sched_relinquish(td) pd_lkpi_yield()
#define SRQ_BORING 0
void pd_lkpi_yield(void);
