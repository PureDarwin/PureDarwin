/*
 * Copyright 2003-2007, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H


#include <SupportDefs.h>


/* This header defines a doubly-linked list. It differentiates between a link
 * and an item.
 * A link is what is put into and removed from a list, an item is the whole
 * object that contains the link. The item doesn't have to be begin with a
 * link; the offset to the link structure is given to init_list_etc(), so that
 * list_get_next/prev_item() will work correctly.
 * Note, the offset value is only needed for the *_item() functions. If the
 * offset is 0, list_link and item are identical - if you use init_list(),
 * you don't have to care about the difference between a link and an item.
 */


// The use of offsetof() on non-PODs is invalid. Since many structs use
// templated members (i.e. DoublyLinkedList) which makes them non-PODs we
// can't use offsetof() anymore. This macro does the same, but requires an
// instance of the object in question.
#define offset_of_member(OBJECT, MEMBER) \
	((size_t)((char*)&OBJECT.MEMBER - (char*)&OBJECT))


typedef struct list_link list_link;

/* The object that is put into the list must begin with these
 * fields, but it doesn't have to be this structure.
 */

struct list_link {
	list_link	*next;
	list_link	*prev;
};

struct list {
	list_link	link;
	int32		offset;
};


#ifdef __cplusplus
extern "C" {
#endif

extern void list_init(struct list *list);
extern void list_init_etc(struct list *list, int32 offset);
extern void list_add_link_to_head(struct list *list, void *_link);
extern void list_add_link_to_tail(struct list *list, void *_link);
extern void list_remove_link(struct list_link *_link);
extern void *list_get_next_item(struct list *list, void *item);
extern void *list_get_prev_item(struct list *list, void *item);
extern void *list_get_last_item(struct list *list);
extern void list_add_item(struct list *list, void *item);
extern void list_remove_item(struct list *list, void *item);
extern void list_insert_item_before(struct list *list, void *before, void *item);
extern void *list_remove_head_item(struct list *list);
extern void *list_remove_tail_item(struct list *list);
extern void list_move_to_list(struct list *sourceList, struct list *targetList);

static inline bool
list_is_empty(struct list *list)
{
	return list->link.next == (list_link *)list;
}

static inline void *
list_get_first_item(struct list *list)
{
	return list_get_next_item(list, NULL);
}

#ifdef __cplusplus
}
#endif

#endif	/* KERNEL_LIST_H */
