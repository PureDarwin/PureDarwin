// SPDX-License-Identifier: MIT
// FreeBSD's pctrie(9) as a small hash
#pragma once
#include <sys/types.h>
#include <sys/errno.h>

// pctrie(9) as a small hash of nodes keyed by a uint64 field of the stored type
#define PD_PCTRIE_BUCKETS 64

struct pd_pctrie_node {
	struct pd_pctrie_node *next;
	void *val;
};

struct pctrie {
	struct pd_pctrie_node *buckets[PD_PCTRIE_BUCKETS];
};

static inline void
pctrie_init(struct pctrie *ptree)
{
	__builtin_memset(ptree, 0, sizeof(*ptree));
}

static inline int
pctrie_is_empty(struct pctrie *ptree)
{
	int i;

	for (i = 0; i < PD_PCTRIE_BUCKETS; i++) {
		if (ptree->buckets[i] != NULL)
			return 0;
	}
	return 1;
}

static inline size_t
pctrie_node_size(void)
{
	return sizeof(struct pd_pctrie_node);
}

static inline int
pctrie_zone_init(void *mem, int size, int flags)
{
	return 0;
}

static inline unsigned
pd_pctrie_bucket(uint64_t key)
{
	return (unsigned)((key >> 6) ^ (key >> 12)) % PD_PCTRIE_BUCKETS;
}

#define PCTRIE_DEFINE(name, type, field, allocfn, freefn)			\
static inline int								\
name##_PCTRIE_INSERT(struct pctrie *ptree, struct type *obj)			\
{										\
	struct pd_pctrie_node *node = (struct pd_pctrie_node *)allocfn(ptree);	\
	unsigned b = pd_pctrie_bucket((uint64_t)obj->field);			\
	if (node == NULL)							\
		return ENOMEM;							\
	node->val = obj;							\
	node->next = ptree->buckets[b];						\
	ptree->buckets[b] = node;						\
	return 0;								\
}										\
static inline struct type *							\
name##_PCTRIE_LOOKUP(struct pctrie *ptree, uint64_t key)			\
{										\
	struct pd_pctrie_node *node;						\
	for (node = ptree->buckets[pd_pctrie_bucket(key)]; node != NULL;	\
	    node = node->next) {						\
		if ((uint64_t)((struct type *)node->val)->field == key)		\
			return (struct type *)node->val;			\
	}									\
	return NULL;								\
}										\
static inline void								\
name##_PCTRIE_REMOVE(struct pctrie *ptree, uint64_t key)			\
{										\
	struct pd_pctrie_node **pp = &ptree->buckets[pd_pctrie_bucket(key)];	\
	while (*pp != NULL) {							\
		struct pd_pctrie_node *node = *pp;				\
		if ((uint64_t)((struct type *)node->val)->field == key) {	\
			*pp = node->next;					\
			freefn(ptree, node);					\
			return;							\
		}								\
		pp = &node->next;						\
	}									\
}										\
static inline void								\
name##_PCTRIE_RECLAIM(struct pctrie *ptree)					\
{										\
	int i;									\
	for (i = 0; i < PD_PCTRIE_BUCKETS; i++) {				\
		while (ptree->buckets[i] != NULL) {				\
			struct pd_pctrie_node *node = ptree->buckets[i];	\
			ptree->buckets[i] = node->next;				\
			freefn(ptree, node);					\
		}								\
	}									\
}
