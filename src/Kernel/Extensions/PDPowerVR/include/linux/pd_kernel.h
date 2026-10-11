#ifndef _PD_LINUX_KERNEL_H
#define _PD_LINUX_KERNEL_H

// the linux kernel interfaces the ported imagination driver files use, over iokit (pvr_linux.cpp)
#include "pd_linux_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(6, 6, 0)

typedef u64 phys_addr_t;
typedef long ssize_t_pd;

// the arm64 virt kernel runs 16k pages, iokit's own definitions win where they are seen first
#ifndef PAGE_SHIFT
#define PAGE_SHIFT	14
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#undef PAGE_MASK
#define PAGE_MASK	(~((u64)PAGE_SIZE - 1))

#define U8_MAX		((u8)~0U)
#define U16_MAX		((u16)~0U)
#define U32_MAX		((u32)~0U)
#define U64_MAX		((u64)~0ULL)
#define S32_MAX		((s32)(U32_MAX >> 1))
#define U64_C(x)	x##ULL
#define _AC(x, y)	x##y
#define SZ_1T		0x10000000000ULL
#define SZ_1G		0x40000000
#define SZ_8K		0x00002000
#define SZ_16K		0x00004000
#define SZ_32K		0x00008000
#define SZ_128K		0x00020000
#define SZ_256K		0x00040000
#define SZ_512K		0x00080000
#define SZ_4M		0x00400000
#define SZ_8M		0x00800000
#define SZ_32M		0x02000000
#define SZ_64M		0x04000000
#define SZ_128M		0x08000000
#define SZ_512M		0x20000000
#define SZ_2G		0x80000000ULL
#define SZ_8G		0x200000000ULL
#define SZ_32G		0x800000000ULL
#define SZ_64G		0x1000000000ULL
#define SZ_128G		0x2000000000ULL
#define SZ_256G		0x4000000000ULL
#define SZ_512G		0x8000000000ULL

#ifndef _BITULL
#define _BITULL(x)	(1ULL << (x))
#endif
#define GENMASK(h, l)	((~0UL << (l)) & (~0UL >> (63 - (h))))
#ifndef GENMASK_ULL
#define GENMASK_ULL(h, l) ((~0ULL << (l)) & (~0ULL >> (63 - (h))))
#endif
#ifndef FIELD_GET
#define FIELD_GET(mask, val) (((val) & (mask)) >> __builtin_ctzll(mask))
#endif
#define FIELD_PREP(mask, val) (((u64)(val) << __builtin_ctzll(mask)) & (mask))
#define __ffs(x)	((unsigned long)__builtin_ctzll(x))
#define __fls(x)	((unsigned long)(63 - __builtin_clzll(x)))
#define fls(x)		((x) ? 32 - __builtin_clz(x) : 0)
#define ilog2(x)	(63 - __builtin_clzll((u64)(x)))
#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define round_up(x, y)	((((x) - 1) | ((__typeof__(x))((y) - 1))) + 1)
#define round_down(x, y) ((x) & ~((__typeof__(x))((y) - 1)))
#define IS_ALIGNED(x, a) (((x) & ((__typeof__(x))(a) - 1)) == 0)
#define PAGE_ALIGN(x)	ALIGN(x, PAGE_SIZE)
#define lower_32_bits(n) ((u32)((n) & 0xffffffff))
#define upper_32_bits(n) ((u32)(((u64)(n)) >> 32))

#undef min
#undef max
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))
#define min_t(t, a, b)	((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define max_t(t, a, b)	((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define clamp(v, lo, hi) min(max(v, lo), hi)
#define typeof_member(T, m) __typeof__(((T *)0)->m)
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - __builtin_offsetof(type, member)))
#define sizeof_field(T, m) sizeof(((T *)0)->m)
#define check_add_overflow(a, b, d) __builtin_add_overflow(a, b, d)
#define check_mul_overflow(a, b, d) __builtin_mul_overflow(a, b, d)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)
#define fallthrough	__attribute__((fallthrough))
#ifndef __always_inline
#define __always_inline	inline __attribute__((always_inline))
#endif
#define __must_check
#define __maybe_unused	__attribute__((unused))
#define __always_unused	__attribute__((unused))
#define __user
#define __iomem
#define notrace
#define BUILD_BUG_ON(x)	static_assert(!(x), #x)

#define READ_ONCE(x)	(*(volatile __typeof__(x) *)&(x))
#define WRITE_ONCE(x, v) (*(volatile __typeof__(x) *)&(x) = (v))
#define mb()		__asm__ volatile ("dsb sy" ::: "memory")
#define wmb()		__asm__ volatile ("dsb st" ::: "memory")
#define rmb()		__asm__ volatile ("dsb ld" ::: "memory")
#define dma_wmb()	__asm__ volatile ("dmb oshst" ::: "memory")
#define smp_mb()	__asm__ volatile ("dmb ish" ::: "memory")

#ifndef EPERM
#define EPERM		1
#endif
#ifndef ENOENT
#define ENOENT		2
#endif
#ifndef EINTR
#define EINTR		4
#endif
#ifndef ENXIO
#define ENXIO		6
#endif
#ifndef E2BIG
#define E2BIG		7
#endif
#ifndef EAGAIN
#define EAGAIN		35
#endif
#ifndef EFAULT
#define EFAULT		14
#endif
#ifndef EEXIST
#define EEXIST		17
#endif
#ifndef ENODEV
#define ENODEV		19
#endif
#ifndef ENOTTY
#define ENOTTY		25
#endif
#ifndef ERANGE
#define ERANGE		34
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP	45
#endif
#ifndef ECANCELED
#define ECANCELED	89
#endif
#ifndef ENOTSUPP
#define ENOTSUPP	524
#endif
#ifndef EOVERFLOW
#define EOVERFLOW	84
#endif

#define MAX_ERRNO	4095
#define IS_ERR_VALUE(x)	((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)
static inline void *ERR_PTR(long error) { return (void *)error; }
static inline long PTR_ERR(const void *ptr) { return (long)ptr; }
static inline bool IS_ERR(const void *ptr) { return IS_ERR_VALUE((unsigned long)ptr); }
static inline bool IS_ERR_OR_NULL(const void *ptr) { return !ptr || IS_ERR(ptr); }
static inline int PTR_ERR_OR_ZERO(const void *ptr) { return IS_ERR(ptr) ? (int)PTR_ERR(ptr) : 0; }
static inline void *ERR_CAST(const void *ptr) { return (void *)ptr; }

void pd_pvr_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define WARN_ON(c) ({ bool __c = !!(c); if (__c) pd_pvr_log("WARN_ON(%s) at %s:%d\n", #c, __FILE__, __LINE__); __c; })
#define WARN_ON_ONCE(c) WARN_ON(c)
#define WARN(c, ...) ({ bool __c = !!(c); if (__c) pd_pvr_log(__VA_ARGS__); __c; })
#define BUG_ON(c) do { if (c) pd_pvr_log("BUG_ON(%s) at %s:%d\n", #c, __FILE__, __LINE__); } while (0)
#define pr_info(...)	pd_pvr_log(__VA_ARGS__)
#define pr_err(...)	pd_pvr_log(__VA_ARGS__)
#define pr_warn(...)	pd_pvr_log(__VA_ARGS__)
#define pr_debug(...)	do { } while (0)
#define drm_info(d, ...)	pd_pvr_log(__VA_ARGS__)
#define drm_err(d, ...)		pd_pvr_log(__VA_ARGS__)
#define drm_warn(d, ...)	pd_pvr_log(__VA_ARGS__)
#define drm_dbg(d, ...)		do { } while (0)
#define drm_err_ratelimited(d, ...) pd_pvr_log(__VA_ARGS__)

// memory: iokit heap with a size header, so kfree needs no size
#define GFP_KERNEL	0x1u
#define __GFP_ZERO	0x2u
#define __GFP_NOWARN	0x4u
#define GFP_NOWAIT	0x8u
#define __GFP_HIGHMEM	0
#define __GFP_DMA32	0
typedef unsigned int gfp_t;
void *pd_kmalloc(size_t size, gfp_t flags);
void pd_kfree(const void *ptr);
#define kmalloc(s, f)		pd_kmalloc(s, f)
#define kzalloc(s, f)		pd_kmalloc(s, (f) | __GFP_ZERO)
#define kcalloc(n, s, f)	pd_kmalloc((size_t)(n) * (s), (f) | __GFP_ZERO)
#define kmalloc_array(n, s, f)	pd_kmalloc((size_t)(n) * (s), f)
#define kvmalloc(s, f)		pd_kmalloc(s, f)
#define kvzalloc(s, f)		pd_kmalloc(s, (f) | __GFP_ZERO)
#define kvmalloc_array(n, s, f)	pd_kmalloc((size_t)(n) * (s), f)
#define kvcalloc(n, s, f)	pd_kmalloc((size_t)(n) * (s), (f) | __GFP_ZERO)
#define kfree(p)		pd_kfree(p)
#define kvfree(p)		pd_kfree(p)
#define memdup_user(p, n)	pd_memdup_user(p, n)
void *pd_memdup_user(const void *uptr, size_t n);

#define memcpy(d, s, n)		__builtin_memcpy(d, s, n)
#define memset(d, c, n)		__builtin_memset(d, c, n)
#define memcmp(a, b, n)		__builtin_memcmp(a, b, n)

// copies from the calling task, the user client points them at its own buffers
unsigned long pd_copy_from_user(void *to, const void *from, unsigned long n);
unsigned long pd_copy_to_user(void *to, const void *from, unsigned long n);
#define copy_from_user(t, f, n)	pd_copy_from_user(t, f, n)
#define copy_to_user(t, f, n)	pd_copy_to_user(t, f, n)
#define u64_to_user_ptr(x)	((void *)(uintptr_t)(x))
unsigned long pd_clear_user(void *to, unsigned long n);
int pd_copy_struct_from_user(void *dst, size_t ksize, const void *src, size_t usize);
#define clear_user(t, n)	pd_clear_user(t, n)
#define copy_struct_from_user(d, k, s, u) pd_copy_struct_from_user(d, k, s, u)
#define SIZE_MAX_PD	(~(size_t)0)

// locks: iokit mutexes, created on first use
struct mutex {
	void *l;
};
typedef struct mutex spinlock_t;
void pd_mutex_init(struct mutex *m);
void pd_mutex_destroy(struct mutex *m);
void pd_mutex_lock(struct mutex *m);
void pd_mutex_unlock(struct mutex *m);
bool pd_mutex_trylock(struct mutex *m);
#define mutex_init(m)		pd_mutex_init(m)
#define mutex_destroy(m)	pd_mutex_destroy(m)
#define mutex_lock(m)		pd_mutex_lock(m)
#define mutex_unlock(m)		pd_mutex_unlock(m)
#define mutex_trylock(m)	pd_mutex_trylock(m)
#define mutex_lock_interruptible(m) (pd_mutex_lock(m), 0)
#define spin_lock_init(m)	pd_mutex_init(m)
#define spin_lock(m)		pd_mutex_lock(m)
#define spin_unlock(m)		pd_mutex_unlock(m)
#define spin_lock_irqsave(m, f)	do { (void)(f); pd_mutex_lock(m); } while (0)
#define spin_unlock_irqrestore(m, f) do { (void)(f); pd_mutex_unlock(m); } while (0)
#define lockdep_assert_held(m)	do { } while (0)
#define might_sleep()		do { } while (0)

struct rw_semaphore {
	struct mutex m;
};
#define init_rwsem(s)	pd_mutex_init(&(s)->m)
#define down_read(s)	pd_mutex_lock(&(s)->m)
#define up_read(s)	pd_mutex_unlock(&(s)->m)
#define down_write(s)	pd_mutex_lock(&(s)->m)
#define up_write(s)	pd_mutex_unlock(&(s)->m)

typedef struct {
	volatile int counter;
} atomic_t;
typedef struct {
	volatile s64 counter;
} atomic64_t;
#define ATOMIC_INIT(i)		{ (i) }
#define atomic_read(v)		__atomic_load_n(&(v)->counter, __ATOMIC_SEQ_CST)
#define atomic_set(v, i)	__atomic_store_n(&(v)->counter, i, __ATOMIC_SEQ_CST)
#define atomic_inc(v)		((void)__atomic_add_fetch(&(v)->counter, 1, __ATOMIC_SEQ_CST))
#define atomic_dec(v)		((void)__atomic_sub_fetch(&(v)->counter, 1, __ATOMIC_SEQ_CST))
#define atomic_inc_return(v)	__atomic_add_fetch(&(v)->counter, 1, __ATOMIC_SEQ_CST)
#define atomic_dec_return(v)	__atomic_sub_fetch(&(v)->counter, 1, __ATOMIC_SEQ_CST)
#define atomic_add_return(i, v)	__atomic_add_fetch(&(v)->counter, i, __ATOMIC_SEQ_CST)
#define atomic_dec_and_test(v)	(__atomic_sub_fetch(&(v)->counter, 1, __ATOMIC_SEQ_CST) == 0)
#undef atomic_fetch_or
#define atomic_fetch_or(i, v)	__atomic_fetch_or(&(v)->counter, i, __ATOMIC_SEQ_CST)
#define atomic_xchg(v, i)	__atomic_exchange_n(&(v)->counter, i, __ATOMIC_SEQ_CST)
#define atomic_cmpxchg(v, o, n)	({ __typeof__((v)->counter) __o = (o); \
	__atomic_compare_exchange_n(&(v)->counter, &__o, n, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); __o; })
#define atomic64_read(v)	atomic_read(v)
#define atomic64_set(v, i)	atomic_set(v, i)
#define atomic64_inc_return(v)	atomic_inc_return(v)

struct kref {
	atomic_t refcount;
};
static inline void kref_init(struct kref *k) { atomic_set(&k->refcount, 1); }
static inline void kref_get(struct kref *k) { atomic_inc(&k->refcount); }
static inline unsigned int kref_read(const struct kref *k) { return (unsigned int)atomic_read(&k->refcount); }
static inline int kref_put(struct kref *k, void (*release)(struct kref *))
{
	if (atomic_dec_and_test(&k->refcount)) {
		release(k);
		return 1;
	}
	return 0;
}
static inline int kref_get_unless_zero(struct kref *k)
{
	int c = atomic_read(&k->refcount);

	while (c != 0) {
		if (__atomic_compare_exchange_n(&k->refcount.counter, &c, c + 1, false,
		    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
			return 1;
	}
	return 0;
}

// lists, as include/linux/list.h
struct list_head {
	struct list_head *next, *prev;
};
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#undef LIST_HEAD
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
static inline void INIT_LIST_HEAD(struct list_head *l) { l->next = l; l->prev = l; }
static inline void __list_add(struct list_head *n, struct list_head *prev, struct list_head *next)
{
	next->prev = n;
	n->next = next;
	n->prev = prev;
	prev->next = n;
}
static inline void list_add(struct list_head *n, struct list_head *h) { __list_add(n, h, h->next); }
static inline void list_add_tail(struct list_head *n, struct list_head *h) { __list_add(n, h->prev, h); }
static inline void list_del(struct list_head *e)
{
	e->next->prev = e->prev;
	e->prev->next = e->next;
	e->next = e;
	e->prev = e;
}
static inline void list_del_init(struct list_head *e) { list_del(e); }
static inline bool list_empty(const struct list_head *h) { return h->next == h; }
static inline void list_move_tail(struct list_head *e, struct list_head *h) { list_del(e); list_add_tail(e, h); }
static inline void list_move(struct list_head *e, struct list_head *h) { list_del(e); list_add(e, h); }
static inline void list_splice_init(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		struct list_head *first = list->next, *last = list->prev, *at = head->next;

		first->prev = head;
		head->next = first;
		last->next = at;
		at->prev = last;
		INIT_LIST_HEAD(list);
	}
}
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)
#define list_first_entry_or_null(ptr, type, member) \
	(list_empty(ptr) ? NULL : list_first_entry(ptr, type, member))
#define list_next_entry(pos, member) list_entry((pos)->member.next, __typeof__(*(pos)), member)
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)
#define list_entry_is_head(pos, head, member) (&pos->member == (head))
#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_entry(pos, head, member) \
	for (pos = list_first_entry(head, __typeof__(*pos), member); \
	     &pos->member != (head); pos = list_next_entry(pos, member))
#define list_for_each_entry_safe(pos, n, head, member) \
	for (pos = list_first_entry(head, __typeof__(*pos), member), n = list_next_entry(pos, member); \
	     &pos->member != (head); pos = n, n = list_next_entry(n, member))

// handle tables: a growable array standing in for the xarray
struct xarray {
	struct mutex lock;
	void **slots;
	u32 nslots;
	u32 next;
	bool alloc1;
};
#define XA_FLAGS_ALLOC	0x1u
#define XA_FLAGS_ALLOC1	0x3u
struct xa_limit {
	u32 min, max;
};
#define XA_LIMIT(lo, hi) ((struct xa_limit){ lo, hi })
#define xa_limit_32b XA_LIMIT(0, U32_MAX)
#define xa_limit_31b XA_LIMIT(0, S32_MAX)
void xa_init_flags(struct xarray *xa, unsigned int flags);
void xa_destroy(struct xarray *xa);
void *xa_load(struct xarray *xa, unsigned long index);
void *xa_erase(struct xarray *xa, unsigned long index);
void *xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp);
int xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xa_limit limit, gfp_t gfp);
bool xa_empty(struct xarray *xa);
void *pd_xa_find(struct xarray *xa, unsigned long *index);
#define xa_lock(xa)	pd_mutex_lock(&(xa)->lock)
#define xa_unlock(xa)	pd_mutex_unlock(&(xa)->lock)
#define __xa_erase(xa, i) xa_erase(xa, i)
#define xa_for_each(xa, index, entry) \
	for (index = 0; (entry = pd_xa_find(xa, &(index))) != NULL; index++)

// scatter lists: physically contiguous runs of a buffer, gathered from its iokit descriptor
struct scatterlist {
	dma_addr_t dma_address;
	u32 length;
};
struct sg_table {
	struct scatterlist *sgl;
	unsigned int nents;
	unsigned int orig_nents;
};
#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->length)
#define for_each_sgtable_dma_sg(sgt, sg, i) \
	for ((i) = 0, (sg) = (sgt)->sgl; (i) < (sgt)->nents; (i)++, (sg)++)

struct sg_dma_page_iter {
	struct sg_table *sgt;
	unsigned int i;
	u64 off;
};
static inline bool pd_sg_page_iter_valid(struct sg_dma_page_iter *it)
{
	return it->i < it->sgt->nents;
}
static inline void pd_sg_page_iter_next(struct sg_dma_page_iter *it)
{
	it->off += PAGE_SIZE;
	if (it->off >= it->sgt->sgl[it->i].length) {
		it->i++;
		it->off = 0;
	}
}
#define for_each_sgtable_dma_page(sgtab, it, pgoff) \
	for ((it)->sgt = (sgtab), (it)->i = 0, (it)->off = (u64)(pgoff) * PAGE_SIZE; \
	     pd_sg_page_iter_valid(it); pd_sg_page_iter_next(it))
#define sg_page_iter_dma_address(it) ((it)->sgt->sgl[(it)->i].dma_address + (it)->off)

// time
#define HZ		100
#define msecs_to_jiffies(m)	((m) / 10)
#define jiffies_to_msecs(j)	((j) * 10)
u64 pd_pvr_now_us(void);
void pd_pvr_udelay(u32 us);
#define jiffies			(pd_pvr_now_us() / 10000)
#define time_before(a, b)	((long)((a) - (b)) < 0)
#define time_after(a, b)	time_before(b, a)
#define udelay(u)		pd_pvr_udelay(u)
#define ndelay(n)		pd_pvr_udelay(1)
#define usleep_range(a, b)	pd_pvr_udelay(a)
void pd_msleep(u32 ms);
int pd_current_pid(void);
#define msleep(m)		pd_msleep(m)
typedef s64 ktime_t;
#define ktime_get()		((ktime_t)pd_pvr_now_us() * 1000)

#ifdef __cplusplus
}
#endif

#endif
