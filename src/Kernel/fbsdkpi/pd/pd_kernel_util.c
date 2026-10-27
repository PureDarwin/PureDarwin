// SPDX-License-Identifier: MIT
// the small haiku kernel and libroot helpers the compat layer links against

#include <OS.h>
#include <KernelExport.h>
#include <image.h>
#include <util/list.h>
#include <errno.h>
#include <string.h>

#include "pd_fbsd_os.h"

// doubly linked lists with the link at an offset inside each item

#define LINK(list, item)	((list_link *)((uint8 *)(item) + (list)->offset))
#define ITEM(list, link)	((void *)((uint8 *)(link) - (list)->offset))

void
list_init(struct list *list)
{
	list_init_etc(list, 0);
}

void
list_init_etc(struct list *list, int32 offset)
{
	list->link.next = list->link.prev = &list->link;
	list->offset = offset;
}

void
list_add_link_to_head(struct list *list, void *_link)
{
	list_link *link = _link;
	link->next = list->link.next;
	link->prev = &list->link;
	list->link.next->prev = link;
	list->link.next = link;
}

void
list_add_link_to_tail(struct list *list, void *_link)
{
	list_link *link = _link;
	link->next = &list->link;
	link->prev = list->link.prev;
	list->link.prev->next = link;
	list->link.prev = link;
}

void
list_remove_link(struct list_link *link)
{
	link->next->prev = link->prev;
	link->prev->next = link->next;
}

void *
list_get_next_item(struct list *list, void *item)
{
	list_link *link = item == NULL ? list->link.next : LINK(list, item)->next;
	return link == &list->link ? NULL : ITEM(list, link);
}

void *
list_get_prev_item(struct list *list, void *item)
{
	list_link *link = item == NULL ? list->link.prev : LINK(list, item)->prev;
	return link == &list->link ? NULL : ITEM(list, link);
}

void *
list_get_last_item(struct list *list)
{
	return list_get_prev_item(list, NULL);
}

void
list_add_item(struct list *list, void *item)
{
	list_add_link_to_tail(list, LINK(list, item));
}

void
list_remove_item(struct list *list, void *item)
{
	list_remove_link(LINK(list, item));
}

void
list_insert_item_before(struct list *list, void *before, void *item)
{
	if (before == NULL) {
		list_add_item(list, item);
		return;
	}
	list_link *link = LINK(list, item);
	list_link *at = LINK(list, before);
	link->next = at;
	link->prev = at->prev;
	at->prev->next = link;
	at->prev = link;
}

void *
list_remove_head_item(struct list *list)
{
	void *item = list_get_next_item(list, NULL);
	if (item != NULL)
		list_remove_item(list, item);
	return item;
}

void *
list_remove_tail_item(struct list *list)
{
	void *item = list_get_prev_item(list, NULL);
	if (item != NULL)
		list_remove_item(list, item);
	return item;
}

void
list_move_to_list(struct list *source, struct list *target)
{
	if (source->link.next == &source->link) {
		list_init_etc(target, source->offset);
		return;
	}
	target->link = source->link;
	target->offset = source->offset;
	target->link.next->prev = &target->link;
	target->link.prev->next = &target->link;
	list_init_etc(source, source->offset);
}

// errno has no meaning in the kernel, one shared word is enough

static int sErrno;

int *
_errnop(void)
{
	return &sErrno;
}

int
ffsl(long mask)
{
	return mask == 0 ? 0 : __builtin_ctzl(mask) + 1;
}

// the kernel exports only ffs, linuxkpi's __ffs64 needs this one
int
ffsll(long long mask)
{
	return mask == 0 ? 0 : __builtin_ctzll(mask) + 1;
}

// mb, rmb and wmb order device and write-combined memory too, so they are the fence or dsb
// instructions, a compiler fence leaves cpu writes in the wc buffers when the gpu reads
void
memory_full_barrier(void)
{
#if defined(__x86_64__)
	__asm__ volatile("mfence" ::: "memory");
#elif defined(__aarch64__)
	__asm__ volatile("dsb sy" ::: "memory");
#else
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

void
memory_read_barrier(void)
{
#if defined(__x86_64__)
	__asm__ volatile("lfence" ::: "memory");
#elif defined(__aarch64__)
	__asm__ volatile("dsb ld" ::: "memory");
#else
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
}

void
memory_write_barrier(void)
{
#if defined(__x86_64__)
	__asm__ volatile("sfence" ::: "memory");
#elif defined(__aarch64__)
	__asm__ volatile("dsb st" ::: "memory");
#else
	__atomic_thread_fence(__ATOMIC_RELEASE);
#endif
}

unsigned long
real_time_clock(void)
{
	return pdos_uptime_us() / 1000000;
}

// there is no image registry or team model, these answer as if empty

status_t
_get_next_image_info(team_id team, int32 *cookie, image_info *info, size_t size)
{
	return B_ENTRY_NOT_FOUND;
}

status_t
get_image_symbol(image_id image, const char *name, int32 symbolType,
	void **_symbolLocation)
{
	return B_ENTRY_NOT_FOUND;
}

uid_t
geteuid(void)
{
	return 0;
}

status_t
kill_thread(thread_id thread)
{
	return B_NOT_ALLOWED;
}

// the highest physical address bus_dma may hand out
phys_addr_t
vm_page_max_address(void)
{
	return 0x0000ffffffffffffULL;
}
