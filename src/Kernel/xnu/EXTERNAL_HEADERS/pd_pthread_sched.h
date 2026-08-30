/*
 * Supply declarations omitted from the sanitized Kernel.framework headers
 * but referenced by sched_prim.h while building libpthread 539.
 */
#ifndef PD_PTHREAD_SCHED_H
#define PD_PTHREAD_SCHED_H

typedef void *timer_call_param_t;
typedef struct pset_node *pset_node_t;
typedef struct run_queue *run_queue_t;
typedef unsigned int ast_t;
typedef int sched_mode_t;
typedef int sched_bucket_t;
struct sched_statistics;

#ifndef XNU_KERNEL_PRIVATE
#define XNU_KERNEL_PRIVATE 1
#endif
#ifndef MACH_KERNEL_PRIVATE
#define MACH_KERNEL_PRIVATE 1
#endif

#ifndef PERCPU_DECL
#define PERCPU_DECL(type_t, name) extern type_t name
#endif

#endif
