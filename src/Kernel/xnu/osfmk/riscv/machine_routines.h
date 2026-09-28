/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

#ifndef _RISCV_MACHINE_ROUTINES_H_
#define _RISCV_MACHINE_ROUTINES_H_

#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/boolean.h>
#include <kern/kern_types.h>
#include <pexpert/pexpert.h>

#include <sys/cdefs.h>
#include <sys/appleapiopts.h>

#include <stdarg.h>
#include <stdint.h>

#ifdef XNU_KERNEL_PRIVATE
#include <kern/sched_hygiene.h>
#include <kern/startup.h>
#endif /* XNU_KERNEL_PRIVATE */

__BEGIN_DECLS
#ifdef XNU_KERNEL_PRIVATE
typedef bool (*expected_fault_handler_t)(riscv_saved_state_t *);
#endif /* XNU_KERNEL_PRIVATE */

/* Interrupt handling */

void ml_cpu_signal(unsigned int cpu_id);
void ml_cpu_signal_deferred_adjust_timer(uint64_t nanosecs);
uint64_t ml_cpu_signal_deferred_get_timer(void);
void ml_cpu_signal_deferred(unsigned int cpu_id);
void ml_cpu_signal_retract(unsigned int cpu_id);

#ifdef XNU_KERNEL_PRIVATE
extern void ml_wait_for_cpu_signal_to_enable(void);
extern void assert_ml_cpu_signal_is_enabled(bool enabled);
#endif /* XNU_KERNEL_PRIVATE */

/* Initialize Interrupts */
void    ml_init_interrupt(void);

/* Get Interrupts Enabled */
boolean_t ml_get_interrupts_enabled(void);

/* Set Interrupts Enabled */
#if __has_feature(ptrauth_calls)
uint64_t ml_pac_safe_interrupts_disable(void);
void ml_pac_safe_interrupts_restore(uint64_t);
#endif /* __has_feature(ptrauth_calls) */
boolean_t ml_set_interrupts_enabled_with_debug(boolean_t enable, boolean_t debug);
boolean_t ml_set_interrupts_enabled(boolean_t enable);
boolean_t ml_early_set_interrupts_enabled(boolean_t enable);

// Functions for disabling measurements for AppleCLPC only.
boolean_t sched_perfcontrol_ml_set_interrupts_without_measurement(boolean_t enable);
void sched_perfcontrol_abandon_preemption_disable_measurement(void);

/* Check if running at interrupt context */
boolean_t ml_at_interrupt_context(void);


/* Generate a fake interrupt */
void ml_cause_interrupt(void);

void siq_init(void);
void siq_cpu_init(void);

#ifdef XNU_KERNEL_PRIVATE

char ml_get_current_core_type(void);

/* did this interrupt context interrupt userspace? */
bool ml_did_interrupt_userspace(void);

#if SCHED_HYGIENE_DEBUG
void _ml_interrupt_masked_debug_start(uintptr_t handler_addr, int type);
void _ml_interrupt_masked_debug_end(void);
#endif /* SCHED_HYGIENE_DEBUG */

static inline void
ml_interrupt_masked_debug_start(void *handler_addr, int type)
{
#if SCHED_HYGIENE_DEBUG
	if (static_if(sched_debug_interrupt_disable)) {
		_ml_interrupt_masked_debug_start((uintptr_t)handler_addr, type);
	}
#else /* !SCHED_HYGIENE_DEBUG */
#pragma unused(handler_addr, type)
#endif /* SCHED_HYGIENE_DEBUG */
}

static inline void
ml_interrupt_masked_debug_end(void)
{
#if SCHED_HYGIENE_DEBUG
	if (static_if(sched_debug_interrupt_disable)) {
		_ml_interrupt_masked_debug_end();
	}
#endif /* SCHED_HYGIENE_DEBUG */
}

#if SCHED_HYGIENE_DEBUG
void ml_irq_debug_start(uintptr_t handler, uintptr_t vector);
void ml_irq_debug_end(void);
void ml_irq_debug_abandon(void);

void ml_spin_debug_reset(thread_t thread);
void ml_spin_debug_clear(thread_t thread);
void ml_spin_debug_clear_self(void);
void ml_handle_interrupts_disabled_duration(thread_t thread);
void ml_handle_stackshot_interrupt_disabled_duration(thread_t thread);
void ml_handle_interrupt_handler_duration(thread_t thread);
#endif /* SCHED_HYGIENE_DEBUG */

extern bool ml_snoop_thread_is_on_core(thread_t thread);
extern boolean_t ml_is_quiescing(void);
extern void ml_set_is_quiescing(boolean_t);
extern uint64_t ml_get_booter_memory_size(void);

#endif /* XNU_KERNEL_PRIVATE */

/* Type for the Time Base Enable function */
typedef void (*time_base_enable_t)(cpu_id_t cpu_id, boolean_t enable);

#if defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE)
/* Type for the Processor Cache Dispatch function */
typedef void (*cache_dispatch_t)(cpu_id_t cpu_id, unsigned int select, unsigned int param0, unsigned int param1);

typedef uint32_t (*get_decrementer_t)(void);
typedef void (*set_decrementer_t)(uint32_t);
typedef void (*fiq_handler_t)(void);

#endif

#define CacheConfig                     0x00000000UL
#define CacheControl                    0x00000001UL
#define CacheClean                      0x00000002UL
#define CacheCleanRegion                0x00000003UL
#define CacheCleanFlush                 0x00000004UL
#define CacheCleanFlushRegion           0x00000005UL
#define CacheShutdown                   0x00000006UL

#define CacheControlEnable              0x00000000UL

#define CacheConfigCCSIDR               0x00000001UL
#define CacheConfigSize                 0x00000100UL

/* Type for the Processor Idle function */
typedef void (*processor_idle_t)(cpu_id_t cpu_id, boolean_t enter, uint64_t *new_timeout_ticks);

/* Type for the Idle Tickle function */
typedef void (*idle_tickle_t)(void);

/* Type for the Idle Timer function */
typedef void (*idle_timer_t)(void *refcon, uint64_t *new_timeout_ticks);

/* Type for the IPI Hander */
typedef void (*ipi_handler_t)(void);

/* Type for the Lockdown Hander */
typedef void (*lockdown_handler_t)(void *);

/* Type for the Platform specific Error Handler */
typedef void (*platform_error_handler_t)(void *refcon, vm_offset_t fault_addr);

// The ex_cb module is obsolete, these SDK-exported definitions are kept for historical reasons.

/* Unused.  Left for historical reasons. */
typedef enum{
	EXCB_CLASS_ILLEGAL_INSTR_SET,
#ifdef CONFIG_XNUPOST
	EXCB_CLASS_TEST1,
	EXCB_CLASS_TEST2,
	EXCB_CLASS_TEST3,
#endif
	EXCB_CLASS_MAX
}
ex_cb_class_t;

/* Unused.  Left for historical reasons. */
typedef enum{
	EXCB_ACTION_RERUN,
	EXCB_ACTION_NONE,
#ifdef CONFIG_XNUPOST
	EXCB_ACTION_TEST_FAIL,
#endif
}
ex_cb_action_t;

/* Unused.  Left for historical reasons. */
typedef struct{
	vm_offset_t far;
}
ex_cb_state_t;

/* Unused.  Left for historical reasons. */
typedef ex_cb_action_t (*ex_cb_t) (
	ex_cb_class_t           cb_class,
	void                            *refcon,
	const ex_cb_state_t     *state
	);

// Unimplemented, kept for historical reasons.
kern_return_t ex_cb_register(
	ex_cb_class_t   cb_class,
	ex_cb_t                 cb,
	void                    *refcon );

// Unimplemented, kept for historical reasons.
ex_cb_action_t ex_cb_invoke(
	ex_cb_class_t   cb_class,
	vm_offset_t         far);

typedef enum {
	CLUSTER_TYPE_INVALID = -1,
	CLUSTER_TYPE_SMP     = 0,
	CLUSTER_TYPE_E       = 1,
	CLUSTER_TYPE_P       = 2,
	CLUSTER_TYPE_M       = 3,
	MAX_CPU_TYPES,
} cluster_type_t;

#ifdef XNU_KERNEL_PRIVATE
void ml_parse_cpu_topology(void);
#endif /* XNU_KERNEL_PRIVATE */

unsigned int ml_get_cpu_count(void);

unsigned int ml_get_cpu_number_type(cluster_type_t cluster_type, bool logical, bool available);

unsigned int ml_get_cluster_number_type(cluster_type_t cluster_type);

unsigned int ml_cpu_cache_sharing(unsigned int level, cluster_type_t cluster_type, bool include_all_cpu_types);

/* Size in bytes of the cache at the given level, 0 = memory, from the device tree cache nodes */
uint64_t ml_cpu_cache_size(unsigned int level);

unsigned int ml_get_cpu_types(void);

int ml_get_boot_cpu_number(void);

int ml_get_cpu_number(uint32_t phys_id);

unsigned int ml_get_cpu_number_local(void);

int ml_get_cluster_number(uint32_t phys_id);

int ml_get_max_cpu_number(void);

int ml_get_max_cluster_number(void);

// Return the id of a cluster's first cpu.
unsigned int ml_get_first_cpu_id(unsigned int cluster_id);

// Return the die id of a cluster.
unsigned int ml_get_die_id(unsigned int cluster_id);

// Return the index of a cluster in its die.
unsigned int ml_get_die_cluster_id(unsigned int cluster_id);

// Return the highest die id of the system.
unsigned int ml_get_max_die_id(void);

int ml_get_cluster_number_local(void);

/* Struct for ml_cpu_get_info */
struct ml_cpu_info {
	unsigned long           vector_unit;
	unsigned long           cache_line_size;
	unsigned long           l1_icache_size;
	unsigned long           l1_dcache_size;
	unsigned long           l2_settings;
	unsigned long           l2_cache_size;
	unsigned long           l3_settings;
	unsigned long           l3_cache_size;
};
typedef struct ml_cpu_info ml_cpu_info_t;

cluster_type_t ml_get_boot_cluster_type(void);

#ifdef KERNEL_PRIVATE
#include "cpu_topology.h"
#endif /* KERNEL_PRIVATE */

// Maps per-CPU and per-cluster PIO registers from EDT. Must run after riscv_vm_init(),
// so it can't be part of ml_parse_cpu_topology().
void ml_map_cpu_pio(void);

/* Struct for ml_processor_register */
struct ml_processor_info {
	cpu_id_t                        cpu_id;
	vm_offset_t                     start_paddr;
	boolean_t                       supports_nap;
	void                            *platform_cache_dispatch;
	time_base_enable_t              time_base_enable;
	processor_idle_t                processor_idle;
	idle_tickle_t                   *idle_tickle;
	idle_timer_t                    idle_timer;
	void                            *idle_timer_refcon;
	vm_offset_t                     powergate_stub_addr;
	uint32_t                        powergate_stub_length;
	uint32_t                        powergate_latency;
	platform_error_handler_t        platform_error_handler;
	uint64_t                        regmap_paddr;
	uint32_t                        phys_id;
	uint32_t                        log_id;
	uint32_t                        l2_access_penalty; /* unused */
	uint32_t                        cluster_id;

	cluster_type_t                  cluster_type;

	uint32_t                        l2_cache_id;
	uint32_t                        l2_cache_size;
	uint32_t                        l3_cache_id;
	uint32_t                        l3_cache_size;
};
typedef struct ml_processor_info ml_processor_info_t;

#if defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE)
/* Struct for ml_init_timebase */
struct  tbd_ops {
	fiq_handler_t     tbd_fiq_handler;
	get_decrementer_t tbd_get_decrementer;
	set_decrementer_t tbd_set_decrementer;
};
typedef struct tbd_ops        *tbd_ops_t;
typedef struct tbd_ops        tbd_ops_data_t;
#endif /* defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE) */


// Called by the platform kext on the CPU being registered. Returns an opaque processor handle
// and the IPI and PMI handlers to register, KERN_SUCCESS or an error.
kern_return_t ml_processor_register(ml_processor_info_t *ml_processor_info,
    processor_t *processor, ipi_handler_t *ipi_handler,
    perfmon_interrupt_handler_func *pmi_handler);

/* Register a lockdown handler */
kern_return_t ml_lockdown_handler_register(lockdown_handler_t, void *);

/* Register a M$ flushing  */
typedef kern_return_t (*mcache_flush_function)(void *service);
kern_return_t ml_mcache_flush_callback_register(mcache_flush_function func, void *service);
kern_return_t ml_mcache_flush(void);

#if XNU_KERNEL_PRIVATE

void ml_lockdown_init(void);

/* Machine layer routine for intercepting panics */
__printflike(1, 0)
void ml_panic_trap_to_debugger(const char *panic_format_str,
    va_list *panic_args,
    unsigned int reason,
    void *ctx,
    uint64_t panic_options_mask,
    unsigned long panic_caller,
    const char *panic_initiator);

/* Initialize Interrupts */
void ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon);

#endif /* XNU_KERNEL_PRIVATE */

vm_offset_t
    ml_static_vtop(
	vm_offset_t);

kern_return_t
ml_static_verify_page_protections(
	uint64_t base, uint64_t size, vm_prot_t prot);

vm_offset_t
    ml_static_ptovirt(
	vm_offset_t);

/* Offset required to obtain absolute time value from tick counter */
uint64_t ml_get_abstime_offset(void);

/* Offset required to obtain continuous time value from tick counter */
uint64_t ml_get_conttime_offset(void);

#ifdef __APPLE_API_UNSTABLE
/* PCI config cycle probing */
boolean_t ml_probe_read(
	vm_offset_t paddr,
	unsigned int *val);
boolean_t ml_probe_read_64(
	addr64_t paddr,
	unsigned int *val);

/* Read physical address byte */
unsigned int ml_phys_read_byte(
	vm_offset_t paddr);
unsigned int ml_phys_read_byte_64(
	addr64_t paddr);

/* Read physical address half word */
unsigned int ml_phys_read_half(
	vm_offset_t paddr);
unsigned int ml_phys_read_half_64(
	addr64_t paddr);

/* Read physical address word*/
unsigned int ml_phys_read(
	vm_offset_t paddr);
unsigned int ml_phys_read_64(
	addr64_t paddr);
unsigned int ml_phys_read_word(
	vm_offset_t paddr);
unsigned int ml_phys_read_word_64(
	addr64_t paddr);

/* Read physical address double word */
unsigned long long ml_phys_read_double(
	vm_offset_t paddr);
unsigned long long ml_phys_read_double_64(
	addr64_t paddr);

/* Write physical address byte */
void ml_phys_write_byte(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_byte_64(
	addr64_t paddr, unsigned int data);

/* Write physical address half word */
void ml_phys_write_half(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_half_64(
	addr64_t paddr, unsigned int data);

/* Write physical address word */
void ml_phys_write(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_64(
	addr64_t paddr, unsigned int data);
void ml_phys_write_word(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_word_64(
	addr64_t paddr, unsigned int data);

/* Write physical address double word */
void ml_phys_write_double(
	vm_offset_t paddr, unsigned long long data);
void ml_phys_write_double_64(
	addr64_t paddr, unsigned long long data);

#if defined(__SIZEOF_INT128__) && APPLE_ARM64_ARCH_FAMILY
// Not all dependent projects' toolchains support 128-bit integers.
#define BUILD_QUAD_WORD_FUNCS 1
#else
#define BUILD_QUAD_WORD_FUNCS 0
#endif /* defined(__SIZEOF_INT128__) && APPLE_ARM64_ARCH_FAMILY */

#if BUILD_QUAD_WORD_FUNCS
// Not all dependent projects typedef `uint128_t` before consuming `machine_routines.h`.
typedef unsigned __int128 uint128_t;

/* Read physical address quad word */
uint128_t ml_phys_read_quad(
	vm_offset_t paddr);
uint128_t ml_phys_read_quad_64(
	addr64_t paddr);

/* Write physical address quad word */
void ml_phys_write_quad(
	vm_offset_t paddr, uint128_t data);
void ml_phys_write_quad_64(
	addr64_t paddr, uint128_t data);
#endif /* BUILD_QUAD_WORD_FUNCS */

void ml_static_mfree(
	vm_offset_t,
	vm_size_t);

kern_return_t
ml_static_protect(
	vm_offset_t start,
	vm_size_t size,
	vm_prot_t new_prot);

/* virtual to physical on wired pages */
vm_offset_t ml_vtophys(
	vm_offset_t vaddr);

/* Get processor cache info */
void ml_cpu_get_info(ml_cpu_info_t *ml_cpu_info);
void ml_cpu_get_info_type(ml_cpu_info_t * ml_cpu_info, cluster_type_t cluster_type);

#endif /* __APPLE_API_UNSTABLE */

typedef int ml_page_protection_t;

/* Return the type of page protection supported */
ml_page_protection_t ml_page_protection_type(void);

#ifdef __APPLE_API_PRIVATE
#ifdef  XNU_KERNEL_PRIVATE
vm_size_t ml_nofault_copy(
	vm_offset_t virtsrc,
	vm_offset_t virtdst,
	vm_size_t size);
boolean_t ml_validate_nofault(
	vm_offset_t virtsrc, vm_size_t size);
#endif /* XNU_KERNEL_PRIVATE */
#if     defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE)
/* IO memory map services */

extern vm_offset_t io_map(
	vm_map_offset_t         phys_addr,
	vm_size_t               size,
	unsigned int            flags,
	vm_prot_t               prot,
	bool                    unmappable);

/* Map memory map IO space */
vm_offset_t ml_io_map(
	vm_offset_t phys_addr,
	vm_size_t size);

vm_offset_t ml_io_map_wcomb(
	vm_offset_t phys_addr,
	vm_size_t size);

vm_offset_t ml_io_map_unmappable(
	vm_offset_t phys_addr,
	vm_size_t size,
	uint32_t flags);

vm_offset_t ml_io_map_with_prot(
	vm_offset_t phys_addr,
	vm_size_t size,
	vm_prot_t prot);

void ml_io_unmap(
	vm_offset_t addr,
	vm_size_t sz);

void ml_get_bouncepool_info(
	vm_offset_t *phys_addr,
	vm_size_t   *size);

vm_map_address_t ml_map_high_window(
	vm_offset_t     phys_addr,
	vm_size_t       len);

void ml_init_timebase(
	void            *args,
	tbd_ops_t       tbd_funcs,
	vm_offset_t     int_address,
	vm_offset_t     int_value);

uint64_t ml_get_timebase(void);

#if MACH_KERNEL_PRIVATE
void ml_memory_to_timebase_fence(void);
void ml_timebase_to_memory_fence(void);
#endif /* MACH_KERNEL_PRIVATE */

uint64_t ml_get_speculative_timebase(void);

uint64_t ml_get_timebase_entropy(void);

boolean_t ml_delay_should_spin(uint64_t interval);

void ml_delay_on_yield(void);

uint32_t ml_get_decrementer(void);

#include <machine/config.h>

uint64_t ml_get_hwclock(void);

uint64_t ml_get_hwclock_speculative(void);

boolean_t ml_get_timer_pending(void);

void platform_syscall(
	struct riscv_saved_state *);

void ml_set_decrementer(
	uint32_t dec_value);

boolean_t is_user_contex(
	void);


#endif /* PEXPERT_KERNEL_PRIVATE || MACH_KERNEL_PRIVATE  */

/* Zero bytes starting at a physical address */
void bzero_phys(
	addr64_t phys_address,
	vm_size_t length);

void bzero_phys_nc(addr64_t src64, vm_size_t bytes);

void bzero_phys_with_options(addr64_t src, vm_size_t bytes, int options);

#if MACH_KERNEL_PRIVATE
bool cpu_interrupt_is_pending(void);
#endif /* MACH_KERNEL_PRIVATE */

void ml_thread_policy(
	thread_t thread,
	unsigned policy_id,
	unsigned policy_info);

#define MACHINE_GROUP                           0x00000001
#define MACHINE_NETWORK_GROUP                   0x10000000
#define MACHINE_NETWORK_WORKLOOP                0x00000001
#define MACHINE_NETWORK_NETISR                  0x00000002

/* Set the maximum number of CPUs */
void ml_set_max_cpus(
	unsigned int max_cpus);

/* Return the maximum number of CPUs set by ml_set_max_cpus(), waiting if necessary */
unsigned int ml_wait_max_cpus(
	void);

/* Return the maximum memory size */
unsigned int ml_get_machine_mem(void);

#ifdef XNU_KERNEL_PRIVATE
/* Return max offset */
vm_map_offset_t ml_get_max_offset(
	boolean_t       is64,
	unsigned int option);
#define MACHINE_MAX_OFFSET_DEFAULT      0x01
#define MACHINE_MAX_OFFSET_MIN          0x02
#define MACHINE_MAX_OFFSET_MAX          0x04
#define MACHINE_MAX_OFFSET_DEVICE       0x08
#endif

extern void     ml_cpu_init_completed(void);
extern void     ml_cpu_up(void);
extern void     ml_cpu_down(void);
extern int      ml_find_next_up_processor(void);

// CPU count updates are separate from ml_cpu_up()/ml_cpu_down() because temporary CLPC
// cluster powerdowns must stay transparent to the user.
extern void     ml_cpu_up_update_counts(int cpu_id);
extern void     ml_cpu_down_update_counts(int cpu_id);

extern uint64_t ml_get_wake_timebase(void);
extern uint64_t ml_get_conttime_wake_time(void);

/* Time since the system was reset (as part of boot/wake) */
uint64_t ml_get_time_since_reset(void);

// Called by ApplePMGR to set wake time in mach_continuous_time() units. No-op without
// HAS_CONTINUOUS_HWCLOCK. UINT64_MAX means unknown and makes ml_get_time_since_reset() return it.
void ml_set_reset_time(uint64_t wake_time);

#ifdef XNU_KERNEL_PRIVATE
extern kern_return_t ml_interrupt_prewarm(uint64_t deadline);
#define TCOAL_DEBUG(x, a, b, c, d, e) do { } while(0)
#endif /* XNU_KERNEL_PRIVATE */

/* Bytes available on current stack */
vm_offset_t ml_stack_remaining(void);

#ifdef MACH_KERNEL_PRIVATE
void            machine_conf(void);
void            machine_lockdown(void);







#endif /* MACH_KERNEL_PRIVATE */


extern int      set_be_bit(void);
extern int      clr_be_bit(void);
extern int      be_tracing(void);

// cpu_broadcast_xcall may sleep the caller and the callback must wake it once all CPUs ran it.
// cpu_broadcast_xcall_simple() behaves as the prototype suggests.
typedef void (*broadcastFunc) (void *);
unsigned int cpu_broadcast_xcall(uint32_t *, boolean_t, broadcastFunc, void *);
unsigned int cpu_broadcast_xcall_simple(boolean_t, broadcastFunc, void *);
__result_use_check kern_return_t cpu_xcall(int, broadcastFunc, void *);
__result_use_check kern_return_t cpu_immediate_xcall(int, broadcastFunc, void *);

#ifdef  KERNEL_PRIVATE

// Perf controller registers a callback (single-threaded) to receive processor
// performance QoS change notifications from the scheduler.

typedef void (*cpu_qos_update_t)(int throughput_qos, uint64_t qos_param1, uint64_t qos_param2);
void cpu_qos_update_register(cpu_qos_update_t);

struct going_on_core {
	uint64_t        thread_id;
	uint16_t        qos_class;
	uint16_t        urgency;        /* XCPM compatibility */
	uint32_t        is_32_bit : 1; /* uses 32-bit ISA/register state in userspace (which may differ from address space size) */
	uint32_t        is_kernel_thread : 1;
	uint64_t        thread_group_id;
	void            *thread_group_data;
	uint64_t        scheduling_latency;     /* absolute time between when thread was made runnable and this ctx switch */
	uint64_t        start_time;
	uint64_t        scheduling_latency_at_same_basepri;
	uint32_t        energy_estimate_nj;     /* return: In nanojoules */
	/* smaller of the time between last change to base priority and ctx switch and scheduling_latency */
};
typedef struct going_on_core *going_on_core_t;

struct going_off_core {
	uint64_t        thread_id;
	uint32_t        energy_estimate_nj;     /* return: In nanojoules */
	uint32_t        reserved;
	uint64_t        end_time;
	uint64_t        thread_group_id;
	void            *thread_group_data;
};
typedef struct going_off_core *going_off_core_t;

struct thread_group_data {
	uint64_t        thread_group_id;
	void            *thread_group_data;
	uint32_t        thread_group_size;
	uint32_t        thread_group_flags;
};
typedef struct thread_group_data *thread_group_data_t;

struct perfcontrol_max_runnable_latency {
	uint64_t        max_scheduling_latencies[4 /* THREAD_URGENCY_MAX */];
};
typedef struct perfcontrol_max_runnable_latency *perfcontrol_max_runnable_latency_t;

struct perfcontrol_work_interval {
	uint64_t        thread_id;
	uint16_t        qos_class;
	uint16_t        urgency;
	uint32_t        flags; // notify
	uint64_t        work_interval_id;
	uint64_t        start;
	uint64_t        finish;
	uint64_t        deadline;
	uint64_t        next_start;
	uint64_t        thread_group_id;
	void            *thread_group_data;
	uint32_t        create_flags;
};
typedef struct perfcontrol_work_interval *perfcontrol_work_interval_t;

typedef enum {
	WORK_INTERVAL_START,
	WORK_INTERVAL_UPDATE,
	WORK_INTERVAL_FINISH,
	WORK_INTERVAL_CREATE,
	WORK_INTERVAL_DEALLOCATE,
} work_interval_ctl_t;

struct perfcontrol_work_interval_instance {
	work_interval_ctl_t     ctl;
	uint32_t                create_flags;
	uint64_t                complexity;
	uint64_t                thread_id;
	uint64_t                work_interval_id;
	uint64_t                instance_id; /* out: start, in: update/finish */
	uint64_t                start;
	uint64_t                finish;
	uint64_t                deadline;
	uint64_t                thread_group_id;
	void                    *thread_group_data;
};
typedef struct perfcontrol_work_interval_instance *perfcontrol_work_interval_instance_t;

// Per-CPU fixed counters (instructions, cycles) exported in the CLPC callout.
// CLPC calls back into XNU for the configurable counters.
struct perfcontrol_cpu_counters {
	uint64_t        instructions;
	uint64_t        cycles;
};

__options_decl(perfcontrol_thread_flags_mask_t, uint64_t, {
	PERFCTL_THREAD_FLAGS_MASK_CLUSTER_SHARED_RSRC_RR = 1 << 0,
	        PERFCTL_THREAD_FLAGS_MASK_CLUSTER_SHARED_RSRC_NATIVE_FIRST = 1 << 1,
});


// Structure used to pass information about a thread to CLPC
struct perfcontrol_thread_data {
	// Energy estimate (return value), filled by CLPC to update the thread's estimate
	uint32_t            energy_estimate_nj;
	/* Perfcontrol class for thread */
	perfcontrol_class_t perfctl_class;
	/* Thread ID for the thread */
	uint64_t            thread_id;
	/* Thread Group ID */
	uint64_t            thread_group_id;
	// Scheduling latency at the same base priority, computed by the scheduler.
	// Only populated for the thread going on-core.
	uint64_t            scheduling_latency_at_same_basepri;
	/* Thread Group data pointer */
	void                *thread_group_data;
	/* perfctl state pointer */
	void                *perfctl_state;
	/* Bitmask to indicate which thread flags have been updated as part of the callout */
	perfcontrol_thread_flags_mask_t thread_flags_mask;
	/* Actual values for the flags that are getting updated in the callout */
	perfcontrol_thread_flags_mask_t thread_flags;
};

// All scheduler callouts run with interrupts disabled and may only use mach/libkern KPI like spinlocks
// and atomics. perfcontrol_state_t spinlocks must be taken with interrupts off to avoid deadlock.

// Called on thread switch for timestamps, MMIO reads or other state switching.
// No scheduler locks held, interrupts ENABLED.
typedef void (*sched_perfcontrol_context_switch_t)(perfcontrol_state_t, perfcontrol_state_t);

// Called after the switch with the old thread, whose scheduler lock is held.
// thread_terminating TRUE means this is the last callout for this thread_id.
typedef void (*sched_perfcontrol_offcore_t)(perfcontrol_state_t, going_off_core_t /* populated by callee */, boolean_t);

// Called with the new thread's scheduler lock held, and also on going_on_core_t changes
// (priority/QoS, quantum expiry), so it is not always paired with offcore.
typedef void (*sched_perfcontrol_oncore_t)(perfcontrol_state_t, going_on_core_t);

// Periodic (hundreds of ms) report of max latency of runnable threads per urgency class.
typedef void (*sched_perfcontrol_max_runnable_latency_t)(perfcontrol_max_runnable_latency_t);

// Passes userland work interval info. No locks held, the state object stays valid during the callout.
typedef void (*sched_perfcontrol_work_interval_notify_t)(perfcontrol_state_t, perfcontrol_work_interval_t);

// Start, update and finish work interval instance with optional complexity estimate.
typedef void (*sched_perfcontrol_work_interval_ctl_t)(perfcontrol_state_t, perfcontrol_work_interval_instance_t);

// Thread group add/remove/update callbacks. No blocking and no locks allowed,
// the kernel might hold thread/task locks.
typedef void (*sched_perfcontrol_thread_group_init_t)(thread_group_data_t);
typedef void (*sched_perfcontrol_thread_group_deinit_t)(thread_group_data_t);
typedef void (*sched_perfcontrol_thread_group_flags_update_t)(thread_group_data_t);

// Called some time after the deadline from sched_perfcontrol_update_callback_deadline passes.
// Runs in context-switch/quantum-interrupt context and must follow its safety rules.
typedef void (*sched_perfcontrol_deadline_passed_t)(uint64_t deadline);

// Context switch callout: event, cpu_id, timestamp, flags, offcore/oncore thread data,
// the switching CPU's cpu_counters and the per core timeout_ticks.
typedef void (*sched_perfcontrol_csw_t)(
	perfcontrol_event event, uint32_t cpu_id, uint64_t timestamp, uint32_t flags,
	struct perfcontrol_thread_data *offcore, struct perfcontrol_thread_data *oncore,
	struct perfcontrol_cpu_counters *cpu_counters, uint64_t *timeout_ticks);


// Thread state update callout: event, cpu_id, timestamp, flags, the updated thread's
// thr_data and the per core timeout_ticks.
typedef void (*sched_perfcontrol_state_update_t)(
	perfcontrol_event event, uint32_t cpu_id, uint64_t timestamp, uint32_t flags,
	struct perfcontrol_thread_data *thr_data, uint64_t *timeout_ticks);

// Thread group blocking callout: blocked_tg waits on blocking_tg, with flags and the
// blocked thread's perfcontrol state.
typedef void (*sched_perfcontrol_thread_group_blocked_t)(
	thread_group_data_t blocked_tg, thread_group_data_t blocking_tg, uint32_t flags, perfcontrol_state_t blocked_thr_state);

// Thread group unblocking callout: unblocking_tg unblocks unblocked_tg, with flags and the
// unblocked thread's perfcontrol state.
typedef void (*sched_perfcontrol_thread_group_unblocked_t)(
	thread_group_data_t unblocked_tg, thread_group_data_t unblocking_tg, uint32_t flags, perfcontrol_state_t unblocked_thr_state);

// Per core timer expired callout: now, flags, cpu_id and timeout_ticks.
typedef void (*sched_perfcontrol_running_timer_expire_t)(
	uint64_t now, uint32_t flags, uint32_t cpu_id, uint64_t *timeout_ticks);


// Always pass the CURRENT version. New callbacks go at the end, NULL for unimplemented ones,
// and NULL callbacks resets to in-kernel defaults.

#define SCHED_PERFCONTROL_CALLBACKS_VERSION_0   (0) /* up-to oncore */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_1   (1) /* up-to max_runnable_latency */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_2   (2) /* up-to work_interval_notify */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_3   (3) /* up-to thread_group_deinit */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_4   (4) /* up-to deadline_passed */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_5   (5) /* up-to state_update */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_6   (6) /* up-to thread_group_flags_update */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_7   (7) /* up-to work_interval_ctl */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_8   (8) /* up-to thread_group_unblocked */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_9   (9) /* allows CLPC to specify resource contention flags */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_10  (10) /* allows CLPC to register a per core timer callback */
#define SCHED_PERFCONTROL_CALLBACKS_VERSION_CURRENT SCHED_PERFCONTROL_CALLBACKS_VERSION_10

struct sched_perfcontrol_callbacks {
	unsigned long version; /* Use SCHED_PERFCONTROL_CALLBACKS_VERSION_CURRENT */
	sched_perfcontrol_offcore_t                   offcore;
	sched_perfcontrol_context_switch_t            context_switch;
	sched_perfcontrol_oncore_t                    oncore;
	sched_perfcontrol_max_runnable_latency_t      max_runnable_latency;
	sched_perfcontrol_work_interval_notify_t      work_interval_notify;
	sched_perfcontrol_thread_group_init_t         thread_group_init;
	sched_perfcontrol_thread_group_deinit_t       thread_group_deinit;
	sched_perfcontrol_deadline_passed_t           deadline_passed;
	sched_perfcontrol_csw_t                       csw;
	sched_perfcontrol_state_update_t              state_update;
	sched_perfcontrol_thread_group_flags_update_t thread_group_flags_update;
	sched_perfcontrol_work_interval_ctl_t         work_interval_ctl;
	sched_perfcontrol_thread_group_blocked_t      thread_group_blocked;
	sched_perfcontrol_thread_group_unblocked_t    thread_group_unblocked;
	sched_perfcontrol_running_timer_expire_t      running_timer_expire;
};
typedef struct sched_perfcontrol_callbacks *sched_perfcontrol_callbacks_t;

extern void sched_perfcontrol_register_callbacks(sched_perfcontrol_callbacks_t callbacks, unsigned long size_of_state);
extern void sched_perfcontrol_thread_group_recommend(void *data, cluster_type_t recommendation);
extern void sched_perfcontrol_inherit_recommendation_from_tg(perfcontrol_class_t perfctl_class, boolean_t inherit);
extern const char* sched_perfcontrol_thread_group_get_name(void *data);

// Edge scheduler: CLPC sets a preferred pset per TG plus per-perfctl-class overrides, mapped to
// sched_bucket preferences. The _clusters_set() variant is the legacy cluster-based interface.
/* Token to indicate a particular perfctl class is not overriden */
#define SCHED_PERFCONTROL_PREFERRED_PSET_OVERRIDE_NONE         PSET_ID_INVALID
// CLPC can request immediate rebalancing of the TG's threads on a preferred pset change.
#define SCHED_PERFCONTROL_PREFERRED_PSET_MIGRATE_RUNNING       0x1
#define SCHED_PERFCONTROL_PREFERRED_PSET_MIGRATE_RUNNABLE      0x2
typedef uint64_t sched_perfcontrol_preferred_pset_options_t;

extern void sched_perfcontrol_thread_group_preferred_psets_set(
	void *machine_data, pset_id_t tg_preferred_pset,
	pset_id_t overrides[PERFCONTROL_CLASS_MAX],
	sched_perfcontrol_preferred_pset_options_t options);

#define SCHED_PERFCONTROL_PREFERRED_CLUSTER_OVERRIDE_NONE      UINT32_MAX
#define SCHED_PERFCONTROL_PREFERRED_CLUSTER_MIGRATE_RUNNING    SCHED_PERFCONTROL_PREFERRED_PSET_MIGRATE_RUNNING
#define SCHED_PERFCONTROL_PREFERRED_CLUSTER_MIGRATE_RUNNABLE   SCHED_PERFCONTROL_PREFERRED_PSET_MIGRATE_RUNNABLE
typedef sched_perfcontrol_preferred_pset_options_t sched_perfcontrol_preferred_cluster_options_t;

extern void sched_perfcontrol_thread_group_preferred_clusters_set(
	void *machine_data, uint32_t tg_preferred_cluster,
	uint32_t overrides[PERFCONTROL_CLASS_MAX],
	sched_perfcontrol_preferred_cluster_options_t options);

// Per-QoS edge matrix (num_psets x num_psets x PERFCONTROL_CLASS_MAX) of edge weights and
// migrate/steal permissions. Classes without a QoS bucket: set is a no-op, get returns zeroes.

extern void sched_perfcontrol_edge_matrix_by_qos_get(sched_clutch_edge *edge_matrix, bool *edge_requested, uint64_t flags, uint64_t num_psets, uint64_t num_classes);
extern void sched_perfcontrol_edge_matrix_by_qos_set(sched_clutch_edge *edge_matrix, bool *edge_changed, uint64_t flags, uint64_t num_psets, uint64_t num_classes);

// Legacy edge matrix interface. set applies to all QoSes, get reads the highest QoS.
// Superseded by sched_perfcontrol_edge_matrix_by_qos_get()/_set().

extern void sched_perfcontrol_edge_matrix_get(sched_clutch_edge *edge_matrix, bool *edge_requested, uint64_t flags, uint64_t matrix_order);
extern void sched_perfcontrol_edge_matrix_set(sched_clutch_edge *edge_matrix, bool *edge_changed, uint64_t flags, uint64_t matrix_order);

// Per-pset core rotation bitmasks from CLPC: preferred_bitmask marks idle cores to prefer,
// migration_bitmask marks cores to move threads off when another non-avoided idle core exists.

extern void sched_perfcontrol_edge_cpu_rotation_bitmasks_set(uint32_t pset_id, uint64_t preferred_bitmask, uint64_t migration_bitmask);
extern void sched_perfcontrol_edge_cpu_rotation_bitmasks_get(uint32_t pset_id, uint64_t *preferred_bitmask, uint64_t *migration_bitmask);

// Arm the one global sched_perfcontrol_deadline_passed deadline, checked on non-idle context switches.
// Returns TRUE if it canceled a pending callback. Fired callbacks don't repeat unless rearmed.
extern boolean_t sched_perfcontrol_update_callback_deadline(uint64_t deadline);

// SFI configuration.
extern kern_return_t sched_perfcontrol_sfi_set_window(uint64_t window_usecs);
extern kern_return_t sched_perfcontrol_sfi_set_bg_offtime(uint64_t offtime_usecs);
extern kern_return_t sched_perfcontrol_sfi_set_utility_offtime(uint64_t offtime_usecs);

typedef enum perfcontrol_callout_type {
	PERFCONTROL_CALLOUT_ON_CORE,
	PERFCONTROL_CALLOUT_OFF_CORE,
	PERFCONTROL_CALLOUT_CONTEXT,
	PERFCONTROL_CALLOUT_STATE_UPDATE,
	/* Add other callout types here */
	PERFCONTROL_CALLOUT_MAX
} perfcontrol_callout_type_t;

typedef enum perfcontrol_callout_stat {
	PERFCONTROL_STAT_INSTRS,
	PERFCONTROL_STAT_CYCLES,
	/* Add other stat types here */
	PERFCONTROL_STAT_MAX
} perfcontrol_callout_stat_t;

uint64_t perfcontrol_callout_stat_avg(perfcontrol_callout_type_t type,
    perfcontrol_callout_stat_t stat);

// Perf controller hint to use WFE instead of WFI in the idle loop, falling back to WFI after
// the timeout. Callers serialize updates.
uint32_t ml_update_cluster_wfe_recommendation(uint32_t wfe_cluster_id, uint64_t wfe_timeout_abstime_interval, uint64_t wfe_hint_flags);


void ml_enable_monitor(void);
boolean_t ml_device_is_prod_fused(void);

#endif /* KERNEL_PRIVATE */

boolean_t machine_timeout_suspended(void);
void ml_get_power_state(boolean_t *, boolean_t *);

boolean_t user_cont_hwclock_allowed(void);
uint8_t user_timebase_type(void);
boolean_t ml_thread_is64bit(thread_t thread);

bool ml_feature_supported(uint64_t feature_bit);

void ml_timer_evaluate(void);
boolean_t ml_timer_forced_evaluation(void);
void ml_gpu_stat_update(uint64_t);
uint64_t ml_gpu_stat(thread_t);
#endif /* __APPLE_API_PRIVATE */





extern uint32_t phy_read_panic;
extern uint32_t phy_write_panic;
#if DEVELOPMENT || DEBUG
extern uint64_t simulate_stretched_io;
#endif

void ml_hibernate_active_pre(void);
void ml_hibernate_active_post(void);

void ml_report_minor_badness(uint32_t badness_id);
#define ML_MINOR_BADNESS_CONSOLE_BUFFER_FULL              0
#define ML_MINOR_BADNESS_MEMFAULT_REPORTING_NOT_ENABLED   1
#define ML_MINOR_BADNESS_PIO_WRITTEN_FROM_USERSPACE       2

#ifdef XNU_KERNEL_PRIVATE
// Returns the original PC of an interrupted CPU, which some systems move to another
// register before a backtracer inspects the state.
uint64_t ml_get_backtrace_pc(struct riscv_saved_state *state);

// Whether a secure hibernation flow (hashed, securely stored image) is supported.
// Hibernation itself may still work when this is false.
bool ml_is_secure_hib_supported(void);

// Returns whether the task should use 1 GHz timebase.
bool ml_task_uses_1ghz_timebase(const task_t task);
#endif /* XNU_KERNEL_PRIVATE */




__END_DECLS

#endif /* _RISCV_MACHINE_ROUTINES_H_ */
