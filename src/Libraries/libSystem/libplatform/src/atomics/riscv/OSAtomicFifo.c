#include <TargetConditionals.h>

#if TARGET_OS_OSX || TARGET_OS_DRIVERKIT

#include <stddef.h>
#include <os/lock.h>

#include "libkern/OSAtomic.h"

// riscv has no preemption free zone in the commpage text, so the head's spare
// int is an os_unfair_lock that serialises the two ends of the list
typedef struct {
	void *head;
	void *tail;
	os_unfair_lock lock;
} _OSFifoQueueHead;

_Static_assert(sizeof(_OSFifoQueueHead) <= sizeof(OSFifoQueueHead),
		"fifo head layout");

#define _OSFIFO_NEXT(elem, offset) (*(void **)((char *)(elem) + (offset)))

void
OSAtomicFifoEnqueue(OSFifoQueueHead *__list, void *__new, size_t __offset)
{
	_OSFifoQueueHead *list = (_OSFifoQueueHead *)__list;

	_OSFIFO_NEXT(__new, __offset) = NULL;
	os_unfair_lock_lock(&list->lock);
	if (list->tail) {
		_OSFIFO_NEXT(list->tail, __offset) = __new;
	} else {
		list->head = __new;
	}
	list->tail = __new;
	os_unfair_lock_unlock(&list->lock);
}

void *
OSAtomicFifoDequeue(OSFifoQueueHead *__list, size_t __offset)
{
	_OSFifoQueueHead *list = (_OSFifoQueueHead *)__list;
	void *elem;

	os_unfair_lock_lock(&list->lock);
	elem = list->head;
	if (elem) {
		list->head = _OSFIFO_NEXT(elem, __offset);
		if (!list->head) {
			list->tail = NULL;
		}
	}
	os_unfair_lock_unlock(&list->lock);
	return elem;
}

#endif // TARGET_OS_OSX || TARGET_OS_DRIVERKIT
