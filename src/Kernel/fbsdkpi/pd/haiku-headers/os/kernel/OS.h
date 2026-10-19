/*
 * Copyright 2004-2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _OS_H
#define _OS_H


#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>

#include <SupportDefs.h>
#include <StorageDefs.h>


#ifdef __cplusplus
extern "C" {
#endif

/* System constants */

#define B_OS_NAME_LENGTH	32
#define B_INFINITE_TIMEOUT	(0x7FFFFFFFFFFFFFFFLL)

#define B_PAGE_SIZE			PAGESIZE

enum {
	B_TIMEOUT						= 0x8,	/* relative timeout */
	B_RELATIVE_TIMEOUT				= 0x8,	/* fails after a relative timeout
												with B_TIMED_OUT */
	B_ABSOLUTE_TIMEOUT				= 0x10,	/* fails after an absolute timeout
												with B_TIMED_OUT */

	/* experimental Haiku only API */
	B_TIMEOUT_REAL_TIME_BASE		= 0x40,
	B_ABSOLUTE_REAL_TIME_TIMEOUT	= B_ABSOLUTE_TIMEOUT
										| B_TIMEOUT_REAL_TIME_BASE
								/* fails after an absolute timeout
												with B_TIMED_OUT based on the
												real time clock */
};


/* Types */

typedef int32 area_id;
typedef int32 port_id;
typedef int32 sem_id;
typedef int32 team_id;
typedef int32 thread_id;


/* Areas */

typedef struct area_info {
	area_id		area;
	char		name[B_OS_NAME_LENGTH];
	size_t		size;
	uint32		lock;
	uint32		protection;
	team_id		team;
	uint32		ram_size;
	uint32		copy_count;
	uint32		in_count;
	uint32		out_count;
	void		*address;
} area_info;

/* area locking */
#define B_NO_LOCK				0
#define B_LAZY_LOCK				1
#define B_FULL_LOCK				2
#define B_CONTIGUOUS			3
#define	B_LOMEM					4	/* B_CONTIGUOUS, < 16 MB physical address */
#define	B_32_BIT_FULL_LOCK		5	/* B_FULL_LOCK, < 4 GB physical addresses */
#define	B_32_BIT_CONTIGUOUS		6	/* B_CONTIGUOUS, < 4 GB physical address */

/* address spec for create_area(), and clone_area() */
#define B_ANY_ADDRESS				0
#define B_EXACT_ADDRESS				1
#define B_BASE_ADDRESS				2
#define B_CLONE_ADDRESS				3
#define	B_ANY_KERNEL_ADDRESS		4
/* B_ANY_KERNEL_BLOCK_ADDRESS		5 */
#define B_RANDOMIZED_ANY_ADDRESS	6
#define B_RANDOMIZED_BASE_ADDRESS	7

/* area protection */
#define B_READ_AREA				(1 << 0)
#define B_WRITE_AREA			(1 << 1)
#define B_EXECUTE_AREA			(1 << 2)
#define B_STACK_AREA			(1 << 3)
	/* "stack" protection is not available on most platforms - it's used
	   to only commit memory as needed, and have guard pages at the
	   bottom of the stack. */
#define B_CLONEABLE_AREA		(1 << 8)

extern area_id		create_area(const char *name, void **startAddress,
						uint32 addressSpec, size_t size, uint32 lock,
						uint32 protection);
extern area_id		clone_area(const char *name, void **destAddress,
						uint32 addressSpec, uint32 protection, area_id source);
extern area_id		find_area(const char *name);
extern area_id		area_for(void *address);
extern status_t		delete_area(area_id id);
extern status_t		resize_area(area_id id, size_t newSize);
extern status_t		set_area_protection(area_id id, uint32 newProtection);

/* system private, use macros instead */
extern status_t		_get_area_info(area_id id, area_info *areaInfo, size_t size);
extern status_t		_get_next_area_info(team_id team, ssize_t *cookie,
						area_info *areaInfo, size_t size);

#define get_area_info(id, areaInfo) \
	_get_area_info((id), (areaInfo),sizeof(*(areaInfo)))
#define get_next_area_info(team, cookie, areaInfo) \
	_get_next_area_info((team), (cookie), (areaInfo), sizeof(*(areaInfo)))


/* Ports */

typedef struct port_info {
	port_id		port;
	team_id		team;
	char		name[B_OS_NAME_LENGTH];
	int32		capacity;		/* queue depth */
	int32		queue_count;	/* # msgs waiting to be read */
	int32		total_count;	/* total # msgs read so far */
} port_info;

extern port_id		create_port(int32 capacity, const char *name);
extern port_id		find_port(const char *name);
extern ssize_t		read_port(port_id port, int32 *code, void *buffer,
						size_t bufferSize);
extern ssize_t		read_port_etc(port_id port, int32 *code, void *buffer,
						size_t bufferSize, uint32 flags, bigtime_t timeout);
extern status_t		write_port(port_id port, int32 code, const void *buffer,
						size_t bufferSize);
extern status_t		write_port_etc(port_id port, int32 code, const void *buffer,
						size_t bufferSize, uint32 flags, bigtime_t timeout);
extern status_t		close_port(port_id port);
extern status_t		delete_port(port_id port);

extern ssize_t		port_buffer_size(port_id port);
extern ssize_t		port_buffer_size_etc(port_id port, uint32 flags,
						bigtime_t timeout);
extern ssize_t		port_count(port_id port);
extern status_t		set_port_owner(port_id port, team_id team);

/* system private, use the macros instead */
extern status_t		_get_port_info(port_id port, port_info *portInfo,
						size_t portInfoSize);
extern status_t		_get_next_port_info(team_id team, int32 *cookie,
						port_info *portInfo, size_t portInfoSize);

#define get_port_info(port, info) \
	_get_port_info((port), (info), sizeof(*(info)))
#define get_next_port_info(team, cookie, info) \
	_get_next_port_info((team), (cookie), (info), sizeof(*(info)))


/* WARNING: The following is Haiku experimental API. It might be removed or
   changed in the future. */

typedef struct port_message_info {
	size_t		size;
	uid_t		sender;
	gid_t		sender_group;
	team_id		sender_team;
} port_message_info;

/* similar to port_buffer_size_etc(), but returns (more) info */
extern status_t		_get_port_message_info_etc(port_id port,
						port_message_info *info, size_t infoSize, uint32 flags,
						bigtime_t timeout);

#define get_port_message_info_etc(port, info, flags, timeout) \
	_get_port_message_info_etc((port), (info), sizeof(*(info)), flags, timeout)


/* Semaphores */

typedef struct sem_info {
	sem_id		sem;
	team_id		team;
	char		name[B_OS_NAME_LENGTH];
	int32		count;
	thread_id	latest_holder;
} sem_info;

/* semaphore flags */
enum {
	B_CAN_INTERRUPT				= 0x01,	/* acquisition of the semaphore can be
										   interrupted (system use only) */
	B_CHECK_PERMISSION			= 0x04,	/* ownership will be checked (system use
										   only) */
	B_KILL_CAN_INTERRUPT		= 0x20,	/* acquisition of the semaphore can be
										   interrupted by SIGKILL[THR], even
										   if not B_CAN_INTERRUPT (system use
										   only) */

	/* release_sem_etc() only flags */
	B_DO_NOT_RESCHEDULE			= 0x02,	/* thread is not rescheduled */
	B_RELEASE_ALL				= 0x08,	/* all waiting threads will be woken up,
										   count will be zeroed */
	B_RELEASE_IF_WAITING_ONLY	= 0x10	/* release count only if there are any
										   threads waiting */
};

extern sem_id		create_sem(int32 count, const char *name);
extern status_t		delete_sem(sem_id id);
extern status_t		acquire_sem(sem_id id);
extern status_t		acquire_sem_etc(sem_id id, int32 count, uint32 flags,
						bigtime_t timeout);
extern status_t		release_sem(sem_id id);
extern status_t		release_sem_etc(sem_id id, int32 count, uint32 flags);
/* TODO: the following two calls are not part of the BeOS API, and might be
   changed or even removed for the final release of Haiku R1 */
extern status_t		switch_sem(sem_id semToBeReleased, sem_id id);
extern status_t		switch_sem_etc(sem_id semToBeReleased, sem_id id,
						int32 count, uint32 flags, bigtime_t timeout);
extern status_t		get_sem_count(sem_id id, int32 *threadCount);
extern status_t		set_sem_owner(sem_id id, team_id team);

/* system private, use the macros instead */
extern status_t		_get_sem_info(sem_id id, struct sem_info *info,
						size_t infoSize);
extern status_t		_get_next_sem_info(team_id team, int32 *cookie,
						struct sem_info *info, size_t infoSize);

#define get_sem_info(sem, info) \
	_get_sem_info((sem), (info), sizeof(*(info)))

#define get_next_sem_info(team, cookie, info) \
	_get_next_sem_info((team), (cookie), (info), sizeof(*(info)))


/* Teams */

typedef struct {
	team_id			team;
	int32			thread_count;
	int32			image_count;
	int32			area_count;
	thread_id		debugger_nub_thread;
	port_id			debugger_nub_port;
	int32			argc;
	char			args[64];
	uid_t			uid;
	gid_t			gid;

	/* Haiku R1 extensions */
	uid_t			real_uid;
	gid_t			real_gid;
	pid_t			group_id;
	pid_t			session_id;
	team_id			parent;
	char			name[B_OS_NAME_LENGTH];
	bigtime_t		start_time;
} team_info;

#define B_CURRENT_TEAM	0
#define B_SYSTEM_TEAM	1

extern status_t		kill_team(team_id team);
	/* see also: send_signal() */

/* system private, use macros instead */
extern status_t		_get_team_info(team_id id, team_info *info, size_t size);
extern status_t		_get_next_team_info(int32 *cookie, team_info *info,
						size_t size);

#define get_team_info(id, info) \
	_get_team_info((id), (info), sizeof(*(info)))

#define get_next_team_info(cookie, info) \
	_get_next_team_info((cookie), (info), sizeof(*(info)))

/* team usage info */

typedef struct {
	bigtime_t		user_time;
	bigtime_t		kernel_time;
} team_usage_info;

enum {
	/* compatible to sys/resource.h RUSAGE_SELF and RUSAGE_CHILDREN */
	B_TEAM_USAGE_SELF		= 0,
	B_TEAM_USAGE_CHILDREN	= -1
};

/* system private, use macros instead */
extern status_t		_get_team_usage_info(team_id team, int32 who,
						team_usage_info *info, size_t size);

#define get_team_usage_info(team, who, info) \
	_get_team_usage_info((team), (who), (info), sizeof(*(info)))


/* Threads */

typedef enum {
	B_THREAD_RUNNING	= 1,
	B_THREAD_READY,
	B_THREAD_RECEIVING,
	B_THREAD_ASLEEP,
	B_THREAD_SUSPENDED,
	B_THREAD_WAITING
} thread_state;

typedef struct {
	thread_id		thread;
	team_id			team;
	char			name[B_OS_NAME_LENGTH];
	thread_state	state;
	int32			priority;
	sem_id			sem;
	bigtime_t		user_time;
	bigtime_t		kernel_time;
	void			*stack_base;
	void			*stack_end;
} thread_info;

#define B_IDLE_PRIORITY					0
#define B_LOWEST_ACTIVE_PRIORITY		1
#define B_LOW_PRIORITY					5
#define B_NORMAL_PRIORITY				10
#define B_DISPLAY_PRIORITY				15
#define	B_URGENT_DISPLAY_PRIORITY		20
#define	B_REAL_TIME_DISPLAY_PRIORITY	100
#define	B_URGENT_PRIORITY				110
#define B_REAL_TIME_PRIORITY			120

#define B_SYSTEM_TIMEBASE				0
	/* time base for snooze_*(), compatible with the clockid_t constants defined
	   in <time.h> */

#define B_FIRST_REAL_TIME_PRIORITY		B_REAL_TIME_DISPLAY_PRIORITY

typedef status_t (*thread_func)(void *);
#define thread_entry thread_func
	/* thread_entry is for backward compatibility only! Use thread_func */

extern thread_id	spawn_thread(thread_func, const char *name, int32 priority,
						void *data);
extern status_t		kill_thread(thread_id thread);
extern status_t		resume_thread(thread_id thread);
extern status_t		suspend_thread(thread_id thread);

extern status_t		rename_thread(thread_id thread, const char *newName);
extern status_t		set_thread_priority(thread_id thread, int32 newPriority);
extern void			exit_thread(status_t status);
extern status_t		wait_for_thread(thread_id thread, status_t *returnValue);
extern status_t		wait_for_thread_etc(thread_id id, uint32 flags, bigtime_t timeout,
						status_t *_returnCode);
extern status_t		on_exit_thread(void (*callback)(void *), void *data);

extern thread_id 	find_thread(const char *name);

extern status_t		send_data(thread_id thread, int32 code, const void *buffer,
						size_t bufferSize);
extern int32		receive_data(thread_id *sender, void *buffer,
						size_t bufferSize);
extern bool			has_data(thread_id thread);

extern status_t		snooze(bigtime_t amount);
extern status_t		snooze_etc(bigtime_t amount, int timeBase, uint32 flags);
extern status_t		snooze_until(bigtime_t time, int timeBase);

/* system private, use macros instead */
extern status_t		_get_thread_info(thread_id id, thread_info *info, size_t size);
extern status_t		_get_next_thread_info(team_id team, int32 *cookie,
						thread_info *info, size_t size);

#define get_thread_info(id, info) \
	_get_thread_info((id), (info), sizeof(*(info)))

#define get_next_thread_info(team, cookie, info) \
	_get_next_thread_info((team), (cookie), (info), sizeof(*(info)))

/* bridge to the pthread API */
extern thread_id	get_pthread_thread_id(pthread_t thread);
/* TODO: Would be nice to have, but we use TLS to associate a thread with its
   pthread object. So this is not trivial to implement.
extern status_t		convert_to_pthread(thread_id thread, pthread_t *_thread);
*/


/* Time */

extern unsigned long real_time_clock(void);
extern void			set_real_time_clock(unsigned long secsSinceJan1st1970);
extern bigtime_t	real_time_clock_usecs(void);
extern bigtime_t	system_time(void);
						/* time since booting in microseconds */
extern nanotime_t	system_time_nsecs(void);
						/* time since booting in nanoseconds */

/* Alarm */

enum {
	B_ONE_SHOT_ABSOLUTE_ALARM	= 1,
	B_ONE_SHOT_RELATIVE_ALARM,
	B_PERIODIC_ALARM			/* "when" specifies the period */
};

extern bigtime_t	set_alarm(bigtime_t when, uint32 flags);


/* Debugger */

extern void			debugger(const char *message);
extern int			disable_debugger(int state);

/* TODO: Remove. Temporary debug helper. */
extern int			debug_printf(const char *format, ...)
						__attribute__ ((format (__printf__, 1, 2)));
extern int			debug_vprintf(const char *format, va_list args);
extern void			ktrace_printf(const char *format, ...)
						__attribute__ ((format (__printf__, 1, 2)));
extern void			ktrace_vprintf(const char *format, va_list args);


/* System information */

typedef struct {
	bigtime_t	active_time;	/* usec of doing useful work since boot */
	bool		enabled;
	uint64		current_frequency;
} cpu_info;

typedef struct {
	bigtime_t		boot_time;			/* time of boot (usecs since 1/1/1970) */

	uint32			cpu_count;			/* number of cpus */

	uint64			max_pages;			/* total # of accessible pages */
	uint64			used_pages;			/* # of accessible pages in use */
	uint64			cached_pages;
	uint64			block_cache_pages;
	uint64			ignored_pages;		/* # of ignored/inaccessible pages */

	uint64			needed_memory;
	uint64			free_memory;

	uint64			max_swap_pages;
	uint64			free_swap_pages;

	uint32			page_faults;		/* # of page faults */

	uint32			max_sems;
	uint32			used_sems;

	uint32			max_ports;
	uint32			used_ports;

	uint32			max_threads;
	uint32			used_threads;

	uint32			max_teams;
	uint32			used_teams;

	char			kernel_name[B_FILE_NAME_LENGTH];
	char			kernel_build_date[B_OS_NAME_LENGTH];
	char			kernel_build_time[B_OS_NAME_LENGTH];

	int64			kernel_version;
	uint32			abi;				/* the system API */
} system_info;

enum topology_level_type {
	B_TOPOLOGY_UNKNOWN,
	B_TOPOLOGY_ROOT,
	B_TOPOLOGY_SMT,
	B_TOPOLOGY_CORE,
	B_TOPOLOGY_PACKAGE
};

enum cpu_platform {
	B_CPU_UNKNOWN,
	B_CPU_x86,
	B_CPU_x86_64,
	B_CPU_PPC,
	B_CPU_PPC_64,
	B_CPU_M68K,
	B_CPU_ARM,
	B_CPU_ARM_64,
	B_CPU_ALPHA,
	B_CPU_MIPS,
	B_CPU_SH,
	B_CPU_SPARC,
	B_CPU_RISC_V
};

enum cpu_vendor {
	B_CPU_VENDOR_UNKNOWN,
	B_CPU_VENDOR_AMD,
	B_CPU_VENDOR_CYRIX,
	B_CPU_VENDOR_IDT,
	B_CPU_VENDOR_INTEL,
	B_CPU_VENDOR_NATIONAL_SEMICONDUCTOR,
	B_CPU_VENDOR_RISE,
	B_CPU_VENDOR_TRANSMETA,
	B_CPU_VENDOR_VIA,
	B_CPU_VENDOR_IBM,
	B_CPU_VENDOR_MOTOROLA,
	B_CPU_VENDOR_NEC,
	B_CPU_VENDOR_HYGON,
	B_CPU_VENDOR_SUN,
	B_CPU_VENDOR_FUJITSU,
	B_CPU_VENDOR_ARM,
	B_CPU_VENDOR_BROADCOM,
	B_CPU_VENDOR_CAVIUM,
	B_CPU_VENDOR_DEC,
	B_CPU_VENDOR_HISILICON,
	B_CPU_VENDOR_INFINEON,
	B_CPU_VENDOR_FREESCALE,
	B_CPU_VENDOR_NVIDIA,
	B_CPU_VENDOR_APM,
	B_CPU_VENDOR_QUALCOMM,
	B_CPU_VENDOR_MARVELL,
	B_CPU_VENDOR_APPLE,
	B_CPU_VENDOR_AMPERE,
	B_CPU_VENDOR_MICROSOFT
};

typedef struct {
	enum cpu_platform		platform;
} cpu_topology_root_info;

typedef struct {
	enum cpu_vendor			vendor;
	uint32					cache_line_size;
} cpu_topology_package_info;

typedef struct {
	uint32					model;
	uint64					default_frequency;
} cpu_topology_core_info;

typedef struct {
	uint32							id;
	enum topology_level_type		type;
	uint32							level;

	union {
		cpu_topology_root_info		root;
		cpu_topology_package_info	package;
		cpu_topology_core_info		core;
	} data;
} cpu_topology_node_info;


extern status_t		get_system_info(system_info* info);
extern status_t		_get_cpu_info_etc(uint32 firstCPU, uint32 cpuCount,
						cpu_info* info, size_t size);
#define get_cpu_info(firstCPU, cpuCount, info) \
	_get_cpu_info_etc((firstCPU), (cpuCount), (info), sizeof(*(info)))

extern status_t		get_cpu_topology_info(cpu_topology_node_info* topologyInfos,
						uint32* topologyInfoCount);

#if defined(__i386__) || defined(__x86_64__)
typedef union {
	struct {
		uint32	max_eax;
		char	vendor_id[12];
	} eax_0;

	struct {
		uint32	stepping		: 4;
		uint32	model			: 4;
		uint32	family			: 4;
		uint32	type			: 2;
		uint32	reserved_0		: 2;
		uint32	extended_model	: 4;
		uint32	extended_family	: 8;
		uint32	reserved_1		: 4;

		uint32	brand_index		: 8;
		uint32	clflush			: 8;
		uint32	logical_cpus	: 8;
		uint32	apic_id			: 8;

		uint32	features;
		uint32	extended_features;
	} eax_1;

	struct {
		uint8	call_num;
		uint8	cache_descriptors[15];
	} eax_2;

	struct {
		uint32	reserved[2];
		uint32	serial_number_high;
		uint32	serial_number_low;
	} eax_3;

	char		as_chars[16];

	struct {
		uint32	eax;
		uint32	ebx;
		uint32	edx;
		uint32	ecx;
	} regs;
} cpuid_info;

extern status_t		get_cpuid(cpuid_info *info, uint32 eaxRegister,
						uint32 cpuNum);
#endif


extern int32		is_computer_on(void);
extern double		is_computer_on_fire(void);


/* signal related functions */
int		send_signal(thread_id threadID, unsigned int signal);
void	set_signal_stack(void* base, size_t size);


/* WARNING: Experimental API! */

enum {
	B_OBJECT_TYPE_FD		= 0,
	B_OBJECT_TYPE_SEMAPHORE	= 1,
	B_OBJECT_TYPE_PORT		= 2,
	B_OBJECT_TYPE_THREAD	= 3
};

enum {
	B_EVENT_READ				= 0x0001,	/* FD/port readable */
	B_EVENT_WRITE				= 0x0002,	/* FD/port writable */
	B_EVENT_ERROR				= 0x0004,	/* FD error */
	B_EVENT_PRIORITY_READ		= 0x0008,	/* FD priority readable */
	B_EVENT_PRIORITY_WRITE		= 0x0010,	/* FD priority writable */
	B_EVENT_HIGH_PRIORITY_READ	= 0x0020,	/* FD high priority readable */
	B_EVENT_HIGH_PRIORITY_WRITE	= 0x0040,	/* FD high priority writable */
	B_EVENT_DISCONNECTED		= 0x0080,	/* FD disconnected */

	B_EVENT_ACQUIRE_SEMAPHORE	= 0x0001,	/* semaphore can be acquired */

	B_EVENT_INVALID				= 0x1000,	/* FD/port/sem/thread ID not or
											   no longer valid (e.g. has been
											   close/deleted) */
};

typedef struct object_wait_info {
	int32		object;						/* ID of the object */
	uint16		type;						/* type of the object */
	uint16		events;						/* events mask */
} object_wait_info;

/* wait_for_objects[_etc]() waits until at least one of the specified events or,
   if given, the timeout occurred. When entering the function the
   object_wait_info::events field specifies the events for each object the
   caller is interested in. When the function returns the fields reflect the
   events that actually occurred. The events B_EVENT_INVALID, B_EVENT_ERROR,
   and B_EVENT_DISCONNECTED don't need to be specified. They will always be
   reported, when they occur. */

extern ssize_t		wait_for_objects(object_wait_info* infos, int numInfos);
extern ssize_t		wait_for_objects_etc(object_wait_info* infos, int numInfos,
						uint32 flags, bigtime_t timeout);


#ifdef __cplusplus
}
#endif

#endif /* _OS_H */
