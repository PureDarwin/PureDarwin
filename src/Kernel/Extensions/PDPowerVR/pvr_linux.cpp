#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <linux/pd_kernel.h>

// the linux kernel services behind pd_kernel.h: heap, mutexes, the handle table and user copies

struct pd_kmalloc_hdr {
	size_t size;
	size_t pad;
};

extern "C" void *
pd_kmalloc(size_t size, gfp_t flags)
{
	struct pd_kmalloc_hdr *h;

	h = (struct pd_kmalloc_hdr *)((flags & __GFP_ZERO) ? IOMallocZero(size + sizeof(*h)) :
	    IOMalloc(size + sizeof(*h)));
	if (h == NULL)
		return NULL;
	h->size = size;
	return h + 1;
}

extern "C" void
pd_kfree(const void *ptr)
{
	struct pd_kmalloc_hdr *h;

	if (ptr == NULL || IS_ERR(ptr))
		return;
	h = (struct pd_kmalloc_hdr *)ptr - 1;
	IOFree(h, h->size + sizeof(*h));
}

extern "C" void
pd_mutex_init(struct mutex *m)
{
	if (m->l == NULL)
		m->l = IOLockAlloc();
}

extern "C" void
pd_mutex_destroy(struct mutex *m)
{
	if (m->l != NULL)
		IOLockFree((IOLock *)m->l);
	m->l = NULL;
}

extern "C" void
pd_mutex_lock(struct mutex *m)
{
	// a lock first taken before anyone initialised it, as zeroed linux structures allow
	if (m->l == NULL)
		pd_mutex_init(m);
	IOLockLock((IOLock *)m->l);
}

extern "C" void
pd_mutex_unlock(struct mutex *m)
{
	IOLockUnlock((IOLock *)m->l);
}

extern "C" bool
pd_mutex_trylock(struct mutex *m)
{
	if (m->l == NULL)
		pd_mutex_init(m);
	return IOLockTryLock((IOLock *)m->l);
}

extern "C" int
pd_current_pid(void)
{
	return proc_selfpid();
}

extern "C" void
pd_msleep(u32 ms)
{
	IOSleep(ms);
}

// the kernel's own self test passes kernel buffers, only a kernel thread may
static bool
pd_from_kernel(void)
{
	return proc_selfpid() == 0;
}

extern "C" unsigned long
pd_copy_from_user(void *to, const void *from, unsigned long n)
{
	if (pd_from_kernel()) {
		memcpy(to, from, n);
		return 0;
	}
	return copyin((user_addr_t)(uintptr_t)from, to, n) == 0 ? 0 : n;
}

extern "C" unsigned long
pd_copy_to_user(void *to, const void *from, unsigned long n)
{
	if (pd_from_kernel()) {
		memcpy(to, from, n);
		return 0;
	}
	return copyout(from, (user_addr_t)(uintptr_t)to, n) == 0 ? 0 : n;
}

extern "C" void *
pd_memdup_user(const void *uptr, size_t n)
{
	void *p = pd_kmalloc(n, GFP_KERNEL);

	if (p == NULL)
		return ERR_PTR(-ENOMEM);
	if (pd_copy_from_user(p, uptr, n) != 0) {
		pd_kfree(p);
		return ERR_PTR(-EFAULT);
	}
	return p;
}

extern "C" void
xa_init_flags(struct xarray *xa, unsigned int flags)
{
	bzero(xa, sizeof(*xa));
	pd_mutex_init(&xa->lock);
	xa->alloc1 = (flags & XA_FLAGS_ALLOC1) == XA_FLAGS_ALLOC1;
}

extern "C" void
xa_destroy(struct xarray *xa)
{
	pd_kfree(xa->slots);
	xa->slots = NULL;
	xa->nslots = 0;
	pd_mutex_destroy(&xa->lock);
}

static bool
xa_grow(struct xarray *xa, u32 need)
{
	u32 n = xa->nslots ? xa->nslots : 16;
	void **slots;

	while (n <= need) {
		n *= 2;
	}
	slots = (void **)pd_kmalloc(n * sizeof(void *), GFP_KERNEL | __GFP_ZERO);
	if (slots == NULL)
		return false;
	if (xa->slots != NULL) {
		memcpy(slots, xa->slots, xa->nslots * sizeof(void *));
		pd_kfree(xa->slots);
	}
	xa->slots = slots;
	xa->nslots = n;
	return true;
}

extern "C" void *
xa_load(struct xarray *xa, unsigned long index)
{
	return index < xa->nslots ? xa->slots[index] : NULL;
}

extern "C" void *
xa_erase(struct xarray *xa, unsigned long index)
{
	void *old = NULL;

	pd_mutex_lock(&xa->lock);
	if (index < xa->nslots) {
		old = xa->slots[index];
		xa->slots[index] = NULL;
	}
	pd_mutex_unlock(&xa->lock);
	return old;
}

extern "C" void *
xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t)
{
	void *old = NULL;

	pd_mutex_lock(&xa->lock);
	if (index >= xa->nslots && !xa_grow(xa, (u32)index)) {
		pd_mutex_unlock(&xa->lock);
		return ERR_PTR(-ENOMEM);
	}
	old = xa->slots[index];
	xa->slots[index] = entry;
	pd_mutex_unlock(&xa->lock);
	return old;
}

// handles count up and are not reused soon, like the xarray's cyclic allocation
extern "C" int
xa_alloc(struct xarray *xa, u32 *id, void *entry, struct xa_limit limit, gfp_t)
{
	u32 lo = limit.min;
	int err = -EBUSY;

	if (xa->alloc1 && lo == 0)
		lo = 1;
	pd_mutex_lock(&xa->lock);
	if (xa->next < lo)
		xa->next = lo;
	for (u32 tries = 0; tries < 2; tries++) {
		for (u32 i = xa->next; i <= limit.max && i < 0x100000; i++) {
			if (i >= xa->nslots && !xa_grow(xa, i)) {
				err = -ENOMEM;
				goto out;
			}
			if (xa->slots[i] == NULL) {
				xa->slots[i] = entry;
				*id = i;
				xa->next = i + 1;
				err = 0;
				goto out;
			}
		}
		xa->next = lo;
	}
out:
	pd_mutex_unlock(&xa->lock);
	return err;
}

extern "C" bool
xa_empty(struct xarray *xa)
{
	for (u32 i = 0; i < xa->nslots; i++) {
		if (xa->slots[i] != NULL)
			return false;
	}
	return true;
}

extern "C" void *
pd_xa_find(struct xarray *xa, unsigned long *index)
{
	for (unsigned long i = *index; i < xa->nslots; i++) {
		if (xa->slots[i] != NULL) {
			*index = i;
			return xa->slots[i];
		}
	}
	return NULL;
}

extern "C" unsigned long
pd_clear_user(void *to, unsigned long n)
{
	static const uint8_t zero[256] = { 0 };

	if (pd_from_kernel()) {
		bzero(to, n);
		return 0;
	}
	while (n > 0) {
		unsigned long c = n < sizeof(zero) ? n : sizeof(zero);

		if (copyout(zero, (user_addr_t)(uintptr_t)to, c) != 0)
			return n;
		to = (uint8_t *)to + c;
		n -= c;
	}
	return 0;
}

// linux's rule: a shorter user struct is zero-extended, a longer one must be zero past ours
extern "C" int
pd_copy_struct_from_user(void *dst, size_t ksize, const void *src, size_t usize)
{
	size_t n = usize < ksize ? usize : ksize;

	if (pd_from_kernel()) {
		memcpy(dst, src, n);
		if (usize < ksize)
			bzero((uint8_t *)dst + usize, ksize - usize);
		return 0;
	}
	if (copyin((user_addr_t)(uintptr_t)src, dst, n) != 0)
		return -EFAULT;
	if (usize < ksize)
		bzero((uint8_t *)dst + usize, ksize - usize);
	for (size_t off = ksize; off < usize; off++) {
		uint8_t b;

		if (copyin((user_addr_t)(uintptr_t)src + off, &b, 1) != 0)
			return -EFAULT;
		if (b != 0)
			return -E2BIG;
	}
	return 0;
}
