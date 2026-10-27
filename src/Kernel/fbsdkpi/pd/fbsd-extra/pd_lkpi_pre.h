// SPDX-License-Identifier: MIT
// forced first include for linuxkpi, net80211 and the drivers built on them
#pragma once
// forced first include for linuxkpi, net80211 and the drivers built on them
#define PD_LINUXKPI 1
// linux code needs small negative errno values (IS_ERR, "r < 0"), haiku's are INT_MIN based
#include "pd_linux_errno.h"
#define __FreeBSD__ 16

typedef __SIZE_TYPE__ __size_t;
typedef long long quad_t;
#define strtoq(p, e, b) ((quad_t)strtoll(p, e, b))

struct task_struct;
struct proc;
struct ucred;
struct vnet;

// the fields of FreeBSD's struct thread that linuxkpi reads
struct thread {
	struct task_struct *td_lkpi_task;
	struct proc *td_proc;
	struct ucred *td_ucred;
	struct vnet *td_vnet;
	const char *td_vnet_lpush;
	void *td_fpop;
	void *td_sa;
	int td_pflags;
	int td_critnest;
	int td_intr_nesting_level;
	int td_tid;
	int td_priority;
	int td_user_pri;
	int td_pinned;
	int td_owepreempt;
	int td_pd_wait_state;
	long long td_pd_timeout_us;
	char td_name[32];
};
#define TDP_NOFAULTING 0x00000080

// asm/cpufeature.h tests these straight from cpuid leaf 1, amdgpu's
// passthrough detection needs the hypervisor bit
static inline unsigned int
pd_cpuid1_reg(int ecx)
{
#if defined(__x86_64__)
	unsigned int a, b, c, d;
	__asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
	return ecx ? c : d;
#else
	return 0;
#endif
}
#define cpu_feature pd_cpuid1_reg(0)
#define cpu_feature2 pd_cpuid1_reg(1)
#define CPUID_CLFSH 0x00080000
#define CPUID2_SSE41 0x00080000
#define CPUID_PAT 0x00010000
#define CPUID2_HV 0x80000000

// FreeBSD base types the copied sys headers expect from machine/_types.h
typedef __UINTPTR_TYPE__ __uintptr_t;
typedef __INTPTR_TYPE__ __intptr_t;
typedef unsigned char __uint8_t;
typedef unsigned short __uint16_t;
typedef unsigned int __uint32_t;
typedef unsigned long __uint64_t;
typedef signed char __int8_t;
typedef short __int16_t;
typedef int __int32_t;
typedef long __int64_t;
typedef int __lwpid_t;
typedef int lwpid_t;
typedef int cpusetid_t;
typedef int cpuwhich_t;
typedef int cpulevel_t;
#define __result_use_check __attribute__((__warn_unused_result__))
#define __result_use_or_ignore_check
#define cpu_stdext_feature 0
#define CPUID_STDEXT_CLFLUSHOPT 0x00800000

// FreeBSD's __CONCAT expands its arguments first, linuxkpi's cleanup.h needs that
#define __CONCAT1(x, y) x ## y
#define __CONCAT(x, y) __CONCAT1(x, y)
#define __diagused
#define __witness_used
#define kdb_active 0
#define MAXCOMLEN 19

typedef unsigned int fixpt_t;
typedef long long sbintime_t;
typedef unsigned long vm_size_t;
typedef struct vm_object *vm_object_t;
typedef struct vm_map *vm_map_t;
#define mp_ncores mp_ncpus
#define physmem pd_lkpi_physmem()
unsigned long pd_lkpi_physmem(void);
#define PRI_FIFO 4
#define PRI_MIN_KERN 48
#define PWAIT 120
#define TDA_SCHED 0
#define M_USE_RESERVE 0x0400
#define M_NODUMP 0x0800
#define M_NORECLAIM 0x0080
#define M_NEVERFREED 0x00
#define LOG_DEBUG 7
#define MTX_NOWITNESS 0x00000008
#define MTX_NEW 0x00000020
#define PMAP_HAS_DMAP 0
#define __maybe_unused __attribute__((__unused__))
#define ENOIOCTL (-3)
#define C_HARDCLOCK 0x0100
#define C_ABSOLUTE 0x0200
#define C_DIRECT_EXEC 0x0001
#define ksize_t size_t

// linux/types.h needs these before any vm header is seen
#include <SupportDefs.h>
typedef phys_addr_t vm_paddr_t;
typedef addr_t vm_offset_t;

struct thread *pd_curthread(void);
#define curthread pd_curthread()
struct malloc_type;
int vasprintf(char **ret, struct malloc_type *mtp, const char *format, __builtin_va_list ap);

// one logical cpu as far as per-cpu data goes, matching Haiku's smp.h
#define CPU_FOREACH(i) for ((i) = 0; (i) < 1; (i)++)
#define CPU_ABSENT(i) ((i) != 0)
#define DPCPU_DEFINE(t, n) t pd_dpcpu_##n[1]
#define DPCPU_DEFINE_STATIC(t, n) static t pd_dpcpu_##n[1]
#define DPCPU_DECLARE(t, n) extern t pd_dpcpu_##n[1]
#define DPCPU_PTR(n) (&pd_dpcpu_##n[0])
#define DPCPU_GET(n) (pd_dpcpu_##n[0])
#define DPCPU_SET(n, v) (pd_dpcpu_##n[0] = (v))
#define DPCPU_ID_PTR(i, n) (&pd_dpcpu_##n[0])
#define DPCPU_ID_GET(i, n) (pd_dpcpu_##n[0])
#define PCPU_GET(n) PD_PCPU_GET_##n
#define PD_PCPU_GET_cpuid 0
// ktr tracing compiles away
#define KTR_DRM 0
#define CTR(m, ...) do { } while (0)
// mutexes take no lock-order flags here
#define mtx_lock_flags(m, f) mtx_lock(m)
#define WARN_GIANTOK 0x01
#define WARN_SLEEPOK 0x02
#define WARN_PANIC 0x04
#define WITNESS_WARN(flags, lock, fmt, ...) do { } while (0)
#define _LKPI_MTX_NOWITNESS MTX_NOWITNESS
#define SBT_1S ((sbintime_t)1 << 32)
#define SBT_1MS (SBT_1S / 1000)
#define SBT_1US (SBT_1S / 1000000)
#define SBT_1NS (SBT_1S / 1000000000)
#define TDP_ITHREAD 0x00000400
#define C_CATCH 0x0400
#define mstosbt(ms) ((sbintime_t)(ms) * SBT_1MS)
#define ustosbt(us) ((sbintime_t)(us) * SBT_1US)
#define nstosbt(ns) ((sbintime_t)(ns) * SBT_1NS)
int pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t pr, int flags);
#define OSRELEASELEN 32
extern char osrelease[];
extern int (*lkpi_alloc_current)(struct thread *, int);
int linux_alloc_current_noop(struct thread *, int);
#define PRIV_NET80211_VAP_GETKEY 440
#define PRIV_NET80211_VAP_MANAGE 441
#define PRIV_NET80211_VAP_SETMAC 442
#define PRIV_NET80211_CREATE_VAP 443

// libkern and systm pieces newer than Haiku's compat copies
#define SCHEDULER_STOPPED() 0
#define MPASS(ex) do { } while (0)
#define ilog2(n) ((int)(sizeof(unsigned long long) * 8 - 1 - __builtin_clzll((unsigned long long)(n))))
#define order_base_2(n) ((n) <= 1 ? 0 : ilog2((n) - 1) + 1)
#define roundup_pow_of_two(n) ((__typeof(n))1 << order_base_2(n))
#define bitcount(x) __builtin_popcount((unsigned)(x))
#define bitcount16(x) __builtin_popcount((unsigned short)(x))
#define bitcount32(x) __builtin_popcount((unsigned)(x))
#define bitcountl(x) __builtin_popcountl((unsigned long)(x))
#define ulmin(a, b) ((unsigned long)(a) < (unsigned long)(b) ? (unsigned long)(a) : (unsigned long)(b))
static inline int WOULD_OVERFLOW(__SIZE_TYPE__ n, __SIZE_TYPE__ s) { __SIZE_TYPE__ r; return __builtin_mul_overflow(n, s, &r); }
#define DROP_GIANT() do {
#define PICKUP_GIANT() } while (0)
#define THREAD_NO_SLEEPING() do { } while (0)
#define THREAD_SLEEPING_OK() do { } while (0)
#define td_ast_pending(td, tda) 0
#define PI_SWI(x) 0
#define PROC_LOCK(p) do { } while (0)
#define PROC_UNLOCK(p) do { } while (0)
#define major(d) ((int)(((d) >> 32) & 0xffffffff))
#define minor(d) ((int)((d) & 0xffffffff))
#define makedev(M, m) ((((unsigned long long)(M)) << 32) | (unsigned)(m))
#define atomic_clear_rel_long atomic_clear_long
#define atomic_cmpset_acq_long atomic_cmpset_long
unsigned long strtoul(const char *, char **, int);
long strtol(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
#define strtouq(p, e, b) strtoull(p, e, b)
char *strchrnul(const char *, int);
void explicit_bzero(void *, __SIZE_TYPE__);
unsigned int arc4random_uniform(unsigned int);
void qsort(void *, __SIZE_TYPE__, __SIZE_TYPE__, int (*)(const void *, const void *));
void *bsearch(const void *, const void *, __SIZE_TYPE__, __SIZE_TYPE__,
    int (*)(const void *, const void *));
unsigned long long pd_lkpi_tsc_freq(void);
#define tsc_freq pd_lkpi_tsc_freq()
#define OFF_TO_IDX(off) ((vm_pindex_t)(((vm_ooffset_t)(off)) >> PAGE_SHIFT))
// copies from user space may fault and sleep here, there is nothing to switch off
#define vm_fault_disable_pagefaults() 0
#define vm_fault_enable_pagefaults(save) ((void)(save))
void qsort_r(void *, __SIZE_TYPE__, __SIZE_TYPE__, int (*)(const void *, const void *, void *), void *);
struct timeval;
struct timespec;
void microtime(struct timeval *);
void getmicrouptime(struct timeval *);
void nanotime(struct timespec *);
void nanouptime(struct timespec *);
void getnanouptime(struct timespec *);
#define timespecsub(a, b, r) do { (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; (r)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; if ((r)->tv_nsec < 0) { (r)->tv_sec--; (r)->tv_nsec += 1000000000L; } } while (0)
void *pd_lkpi_phys_to_dmap(unsigned long long pa);
#define PHYS_TO_DMAP(x) pd_lkpi_phys_to_dmap(x)
#define PHYS_IN_DMAP(pa) 0
// the cpu cache line size from cpuid leaf 1, what clflush loops step by
static inline unsigned int
pd_clflush_line_size(void)
{
#if defined(__x86_64__)
	unsigned int a, b, c, d;
	__asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
	return ((b >> 8) & 0xff) * 8;
#else
	return 64;
#endif
}
// without an llc the gpu reads memory, so cpu writes it must see are flushed out of the cache
static inline void
pd_flush_cache_range(unsigned long start, unsigned long end)
{
#if defined(__x86_64__)
	unsigned long line = pd_clflush_line_size();

	__asm__ volatile("mfence" ::: "memory");
	for (start &= ~(line - 1); start < end; start += line)
		__asm__ volatile("clflush (%0)" :: "r"(start) : "memory");
	__asm__ volatile("mfence" ::: "memory");
#endif
}
#if defined(__x86_64__)
#define pmap_invalidate_cache() __asm__ volatile("wbinvd" ::: "memory")
#else
#define pmap_invalidate_cache() do { } while (0)
#endif
#define pmap_force_invalidate_cache_range(s, e) pd_flush_cache_range((unsigned long)(s), (unsigned long)(e))
int copyin_nofault(const void *, void *, __SIZE_TYPE__);
int copyout_nofault(const void *, void *, __SIZE_TYPE__);
__SIZE_TYPE__ malloc_size(__SIZE_TYPE__);
void bus_topo_lock(void);
void bus_topo_unlock(void);
int isspace(int);
struct ifnet;
void rt_ieee80211msg(struct ifnet *, int, void *, __SIZE_TYPE__);
void rt_ifmsg(struct ifnet *, int);
void tdsignal(struct thread *, int);
void max_linkhdr_grow(int);
unsigned short in_cksum_hdr(const void *ip);
void *kmem_alloc_contig(unsigned long size, int flags, unsigned long low, unsigned long high,
    unsigned long alignment, unsigned long boundary, int memattr);
void kmem_free(void *addr, unsigned long size);
void *contigmalloc_domainset(unsigned long size, struct malloc_type *type, void *ds, int flags,
    unsigned long low, unsigned long high, unsigned long alignment, unsigned long boundary);
#define vm_wait(obj) pause_sbt("vmwait", SBT_1MS, 0, 0)
#ifndef ALIGN
#define ALIGN(p) (((unsigned long)(p) + 7) & ~7UL)
#endif
#define vm_page_xunbusy(m) do { } while (0)
// extra holders of a page (get_page), the last put_page frees it
#define vm_page_wire(m) __atomic_add_fetch(&(m)->ref_count, 1, __ATOMIC_ACQ_REL)
#define vm_page_tryxbusy(m) 1
#define vm_page_reference(m) do { } while (0)
#define vm_page_dirty(m) do { } while (0)
#define VM_OBJECT_WLOCK(o) do { } while (0)
#define VM_OBJECT_WUNLOCK(o) do { } while (0)
struct file;
struct filecaps;
#define fdrop(fp, td) 0
#define fget_unlocked(td, fd, rights, fpp) EBADF
#define falloc(td, fpp, fdp, flags) ENFILE
#define finit(fp, flag, type, data, ops) do { } while (0)
#define fdclose(td, fp, fd) do { } while (0)
#define backlight_register(name, dev) ((void *)0)
#define backlight_destroy(bd) 0
#define SYSCTL_ADD_ROOT_NODE(ctx, nbr, name, access, handler, descr) ((struct sysctl_oid *)0)
#define SYSCTL_OUT_STR(r, s) 0
// jiffies count milliseconds, matching haiku's hz of 1000
unsigned long pd_lkpi_jiffies(void);
#define jiffies pd_lkpi_jiffies()
void _intr_drain(int irq);
void hexdump(const void *ptr, int length, const char *hdr, int flags);
