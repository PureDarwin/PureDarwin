// SPDX-License-Identifier: MIT
// haiku's private thread api, the parts the compat layer calls
#ifndef _PD_HAIKU_THREAD_H
#define _PD_HAIKU_THREAD_H
#include <OS.h>
#include <KernelExport.h>
#include <smp.h>
#ifdef __cplusplus
struct Thread;
extern "C" {
#else
typedef struct Thread Thread;
#endif
struct Thread *thread_get_current_thread(void);
void thread_pin_to_current_cpu(struct Thread *thread);
void thread_unpin_from_current_cpu(struct Thread *thread);
void thread_exit(void);
status_t set_thread_priority(thread_id id, int32 priority);
#ifdef __cplusplus
}
#endif
#endif
