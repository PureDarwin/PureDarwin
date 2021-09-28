#include <libkern/OSAtomic.h>
#include <System/i386/cpu_capabilities.h>

#define OS_UNFAIR_LOCK_INLINE 1
#include "os/lock_private.h"

typedef	volatile struct {
	void	*first;
	void	*last;
	os_unfair_lock	 lock;
} __attribute__ ((aligned (16))) UnfairFifoQueueHead;

#define set_next(element, offset, new) \
	*((void**)(((uintptr_t)element) + offset)) = new;
#define get_next(element, offset) \
	*((void**)(((uintptr_t)element) + offset));

// This is a naive implementation using unfair locks to support translated
// x86_64 apps only. Native x86_64 and arm64 apps will use the
// PFZ implementations
void OSAtomicFifoEnqueue$VARIANT$UnfairLock(UnfairFifoQueueHead *list, void *new, size_t offset) {
	set_next(new, offset, NULL);

	os_unfair_lock_lock_inline(&list->lock);
	if (list->last == NULL) {
		list->first = new;
	} else {
		set_next(list->last, offset, new);
	}
	list->last = new;
	os_unfair_lock_unlock_inline(&list->lock);
}

void* OSAtomicFifoDequeue$VARIANT$UnfairLock(UnfairFifoQueueHead *list, size_t offset) {
	os_unfair_lock_lock_inline(&list->lock);
	void *element = list->first;
	if (element != NULL) {
		void *next = get_next(element, offset);
		if (next == NULL) {
			list->last = NULL;
		}
		list->first = next;
	}
	os_unfair_lock_unlock_inline(&list->lock);

	return element;
}

#define MakeResolver(name)													\
	void * name ## Resolver(void) __asm__("_" #name);						\
	void * name ## Resolver(void) {											\
		__asm__(".symbol_resolver _" #name);								\
		uint64_t capabilities = *(uint64_t*)_COMM_PAGE_CPU_CAPABILITIES64;	\
		if (capabilities & kIsTranslated) {									\
			return name ## $VARIANT$UnfairLock; 							\
		} else {															\
			return name ## $VARIANT$PFZ;    								\
		}                                                          			\
	}

void OSAtomicFifoEnqueue$VARIANT$PFZ(OSFifoQueueHead *, void *, size_t);
void* OSAtomicFifoDequeue$VARIANT$PFZ(OSFifoQueueHead *, size_t);

MakeResolver(OSAtomicFifoEnqueue)
MakeResolver(OSAtomicFifoDequeue)
