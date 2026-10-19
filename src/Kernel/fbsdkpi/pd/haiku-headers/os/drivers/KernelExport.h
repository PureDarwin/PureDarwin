/*
 * Copyright 2005-2008, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_EXPORT_H
#define _KERNEL_EXPORT_H


#include <SupportDefs.h>
#include <OS.h>


/* interrupts and spinlocks */

typedef ulong cpu_status;

// WARNING: For Haiku debugging only! This changes the spinlock type in a
// binary incompatible way!
//#define B_DEBUG_SPINLOCK_CONTENTION	1

#if B_DEBUG_SPINLOCK_CONTENTION
	typedef struct {
		int32	lock;
		int32		failed_try_acquire;
		bigtime_t	total_wait;
		bigtime_t	total_held;
		bigtime_t	last_acquired;
	} spinlock;

#	define B_SPINLOCK_INITIALIZER { 0, 0, 0, 0, 0 }
#	define B_INITIALIZE_SPINLOCK(spinlock)	do {	\
			(spinlock)->lock = 0;					\
			(spinlock)->failed_try_acquire = 0;		\
			(spinlock)->total_wait = 0;				\
			(spinlock)->total_held = 0;				\
			(spinlock)->last_acquired = 0;			\
		} while (false)
#else
	typedef struct {
		int32	lock;
	} spinlock;

#	define B_SPINLOCK_INITIALIZER { 0 }
#	define B_INITIALIZE_SPINLOCK(spinlock)	do {	\
			(spinlock)->lock = 0;					\
		} while (false)
#endif

#define B_SPINLOCK_IS_LOCKED(spinlock)	(atomic_get(&(spinlock)->lock) > 0)

typedef struct {
	int32		lock;
} rw_spinlock;

#define B_RW_SPINLOCK_INITIALIZER	{ 0 }
#define B_INITIALIZE_RW_SPINLOCK(rw_spinlock)	do {	\
		(rw_spinlock)->lock = 0;						\
	} while (false)

typedef struct {
	spinlock	lock;
	uint32		count;
} seqlock;

#define B_SEQLOCK_INITIALIZER	{ B_SPINLOCK_INITIALIZER, 0 }
#define B_INITIALIZE_SEQLOCK(seqlock)	do {		\
		B_INITIALIZE_SPINLOCK(&(seqlock)->lock);	\
		(seqlock)->count = 0;						\
	} while (false)

/* interrupt handling support for device drivers */

#ifdef PD_LKPI_DRM
// amdgpu's dc has its own interrupt_handler type
#define interrupt_handler haiku_interrupt_handler
#endif
typedef int32 (*interrupt_handler)(void *data);

/* Values returned by interrupt handlers */
#define B_UNHANDLED_INTERRUPT	0	/* pass to next handler */
#define B_HANDLED_INTERRUPT		1	/* don't pass on */
#define B_INVOKE_SCHEDULER		2	/* don't pass on; invoke the scheduler */

/* Flags that can be passed to install_io_interrupt_handler() */
#define B_NO_ENABLE_COUNTER		1


/* timer interrupts services */

typedef struct timer timer;
typedef	int32 (*timer_hook)(timer *);

struct timer {
	struct timer *next;
	int64		schedule_time;
	void		*user_data;
	uint16		flags;
	uint16		cpu;
	timer_hook	hook;
	bigtime_t	period;
};

#define B_ONE_SHOT_ABSOLUTE_TIMER	1
#define	B_ONE_SHOT_RELATIVE_TIMER	2
#define	B_PERIODIC_TIMER			3


/* virtual memory buffer functions */

#define B_DMA_IO		0x00000001
#define B_READ_DEVICE	0x00000002

typedef struct {
	phys_addr_t	address;	/* address in physical memory */
	phys_size_t	size;		/* size of block */
} physical_entry;

/* address specifications for mapping physical memory */
#define	B_ANY_KERNEL_BLOCK_ADDRESS	(B_ANY_KERNEL_ADDRESS + 1)

/* memory types for physical memory */
#define B_UNCACHED_MEMORY			(1 << 28)
#define B_WRITE_COMBINING_MEMORY	(2 << 28)
#define B_WRITE_THROUGH_MEMORY		(3 << 28)
#define B_WRITE_PROTECTED_MEMORY	(4 << 28)
#define B_WRITE_BACK_MEMORY			(5 << 28)
#define B_MEMORY_TYPE_MASK			(0xf0000000)

/* area protection flags for the kernel */
#define B_KERNEL_READ_AREA			(1 << 4)
#define B_KERNEL_WRITE_AREA			(1 << 5)
#define B_KERNEL_EXECUTE_AREA		(1 << 6)
#define B_KERNEL_STACK_AREA			(1 << 7)


/* kernel daemon service */

typedef void (*daemon_hook)(void *arg, int iteration);


/* kernel debugging facilities */

/* special return codes for kernel debugger */
#define B_KDEBUG_CONT	2
#define B_KDEBUG_QUIT	3

typedef int (*debugger_command_hook)(int argc, char **argv);


#ifdef __cplusplus
extern "C" {
#endif

/* interrupts, spinlock, and timers */
extern cpu_status	disable_interrupts(void);
extern void			restore_interrupts(cpu_status status);

extern void			acquire_spinlock(spinlock *lock);
extern void			release_spinlock(spinlock *lock);

extern bool			try_acquire_write_spinlock(rw_spinlock* lock);
extern void			acquire_write_spinlock(rw_spinlock* lock);
extern void			release_write_spinlock(rw_spinlock* lock);
extern bool			try_acquire_read_spinlock(rw_spinlock* lock);
extern void			acquire_read_spinlock(rw_spinlock* lock);
extern void			release_read_spinlock(rw_spinlock* lock);

extern bool			try_acquire_write_seqlock(seqlock* lock);
extern void			acquire_write_seqlock(seqlock* lock);
extern void			release_write_seqlock(seqlock* lock);
extern uint32		acquire_read_seqlock(seqlock* lock);
extern bool			release_read_seqlock(seqlock* lock, uint32 count);

extern status_t		install_io_interrupt_handler(int32 interrupt_number,
						interrupt_handler handler, void *data, uint32 flags);
extern status_t		remove_io_interrupt_handler(int32 interrupt_number,
						interrupt_handler handler, void	*data);
#ifdef PD_LKPI_DRM
#undef interrupt_handler
#endif

#ifndef PD_LINUXKPI
extern status_t		add_timer(timer *t, timer_hook hook, bigtime_t period,
						int32 flags);
extern bool			cancel_timer(timer *t);
#endif

/* kernel threads */
extern thread_id	spawn_kernel_thread(thread_func function,
						const char *name, int32 priority, void *arg);

/* signal functions */
extern int			send_signal_etc(pid_t thread, uint signal, uint32 flags);

/* virtual memory */
extern status_t		lock_memory_etc(team_id team, void *buffer, size_t numBytes,
						uint32 flags);
extern status_t		lock_memory(void *buffer, size_t numBytes, uint32 flags);
extern status_t		unlock_memory_etc(team_id team, void *address,
						size_t numBytes, uint32 flags);
extern status_t		unlock_memory(void *buffer, size_t numBytes, uint32 flags);
extern status_t		get_memory_map_etc(team_id team, const void *address,
						size_t numBytes, physical_entry *table,
						uint32* _numEntries);
extern int32		get_memory_map(const void *buffer, size_t size,
						physical_entry *table, int32 numEntries);
extern area_id		map_physical_memory(const char *areaName,
						phys_addr_t physicalAddress, size_t size, uint32 flags,
						uint32 protection, void **_mappedAddress);

/* kernel debugging facilities */
#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
extern void			dprintf(const char *format, ...) _PRINTFLIKE(1, 2);
#endif
extern void			dvprintf(const char *format, va_list args);
extern void			kprintf(const char *fmt, ...) _PRINTFLIKE(1, 2);

extern void			dump_block(const char *buffer, int size, const char *prefix);
						/* TODO: temporary API: hexdumps given buffer */

extern bool			set_dprintf_enabled(bool new_state);

extern void			panic(const char *format, ...) _PRINTFLIKE(1, 2);

extern void			kernel_debugger(const char *message);
extern uint64		parse_expression(const char *string);

extern int			add_debugger_command(const char *name,
						debugger_command_hook hook, const char *help);
extern int			remove_debugger_command(const char *name,
						debugger_command_hook hook);

/* Miscellaneous */
extern void			spin(bigtime_t microseconds);

extern status_t		register_kernel_daemon(daemon_hook hook, void *arg,
						int frequency);
extern status_t		unregister_kernel_daemon(daemon_hook hook, void *arg);

extern void			call_all_cpus(void (*func)(void *, int), void *cookie);
extern void			call_all_cpus_sync(void (*func)(void *, int), void *cookie);
extern void			memory_read_barrier(void);
extern void			memory_write_barrier(void);

/* safe methods to access user memory without having to lock it */
extern status_t		user_memcpy(void *to, const void *from, size_t size);
extern ssize_t		user_strlcpy(char *to, const char *from, size_t size);
extern status_t		user_memset(void *start, char c, size_t count);

#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL_EXPORT_H */
