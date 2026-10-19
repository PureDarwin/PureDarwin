// SPDX-License-Identifier: MIT
// the maple tree calls nouveau's vm_bind regions use, as a sorted list since a client holds a handful
#ifndef _PD_LINUX_MAPLE_TREE_H_
#define _PD_LINUX_MAPLE_TREE_H_

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>

#define MT_FLAGS_LOCK_EXTERN	0x1

struct pd_mt_range {
	struct pd_mt_range *next;
	unsigned long index;
	unsigned long last;
	void *entry;
};

struct maple_tree {
	struct pd_mt_range *head;
	unsigned int flags;
};

struct ma_state {
	struct maple_tree *tree;
	unsigned long index;
	unsigned long last;
	struct pd_mt_range *pos;
};

#define MA_STATE(name, mt, first, end) \
	struct ma_state name = { .tree = (mt), .index = (first), .last = (end), .pos = NULL }

static inline void
mt_init_flags(struct maple_tree *mt, unsigned int flags)
{
	mt->head = NULL;
	mt->flags = flags;
}

// the caller locks the tree itself
#define mt_set_external_lock(mt, lock)	do { } while (0)

static inline bool
mtree_empty(const struct maple_tree *mt)
{
	return mt->head == NULL;
}

static inline void
__mt_destroy(struct maple_tree *mt)
{
	struct pd_mt_range *r = mt->head, *n;

	for (; r != NULL; r = n) {
		n = r->next;
		kfree(r);
	}
	mt->head = NULL;
}

// the entry covering mas->index, or NULL with index and last set to the free gap around it
static inline void *
mas_walk(struct ma_state *mas)
{
	unsigned long lo = 0, hi = ULONG_MAX;
	struct pd_mt_range *r;

	for (r = mas->tree->head; r != NULL; r = r->next) {
		if (mas->index >= r->index && mas->index <= r->last) {
			mas->index = r->index;
			mas->last = r->last;
			mas->pos = r;
			return r->entry;
		}
		if (r->last < mas->index)
			lo = r->last + 1;
		else if (r->index > mas->index) {
			hi = r->index - 1;
			break;
		}
	}
	mas->index = lo;
	mas->last = hi;
	mas->pos = NULL;
	return NULL;
}

// stores entry over [index, last], the caller has made sure the range is free
static inline int
mas_store_gfp(struct ma_state *mas, void *entry, gfp_t gfp)
{
	struct pd_mt_range **pp = &mas->tree->head, *r;

	r = kzalloc(sizeof(*r), gfp);
	if (r == NULL)
		return -ENOMEM;
	r->index = mas->index;
	r->last = mas->last;
	r->entry = entry;
	while (*pp != NULL && (*pp)->index < r->index)
		pp = &(*pp)->next;
	r->next = *pp;
	*pp = r;
	mas->pos = r;
	return 0;
}

// the first entry overlapping [index, max], moving the state past it
static inline void *
mas_find(struct ma_state *mas, unsigned long max)
{
	struct pd_mt_range *r = mas->pos ? mas->pos->next : mas->tree->head;

	if (mas->pos != NULL && mas->pos->last == ULONG_MAX)
		return NULL;
	for (; r != NULL; r = r->next) {
		if (r->index > max)
			break;
		if (r->last >= mas->index) {
			mas->index = r->index;
			mas->last = r->last;
			mas->pos = r;
			return r->entry;
		}
	}
	mas->pos = NULL;
	mas->index = max;
	return NULL;
}

#define mas_for_each(mas, entry, max) \
	while (((entry) = mas_find((mas), (max))) != NULL)

// removes the entry covering mas->index and leaves the state before its successor
static inline void *
mas_erase(struct ma_state *mas)
{
	struct pd_mt_range **pp = &mas->tree->head, *r;
	struct pd_mt_range *prev = NULL;
	void *entry;

	for (; *pp != NULL; prev = *pp, pp = &(*pp)->next) {
		r = *pp;
		if (mas->index >= r->index && mas->index <= r->last) {
			*pp = r->next;
			entry = r->entry;
			kfree(r);
			mas->pos = prev;
			return entry;
		}
	}
	return NULL;
}

#endif
