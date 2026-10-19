/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SEARCH_H_
#define _SEARCH_H_


#include <sys/types.h>


typedef enum {
	FIND,
	ENTER
} ACTION;

typedef struct entry {
	char *key;
	void *data;
} ENTRY;

typedef enum {
	preorder,
	postorder,
	endorder,
	leaf
} VISIT;

typedef void posix_tnode;


#ifdef __cplusplus
extern "C" {
#endif

extern int hcreate(size_t elementCount);
extern void hdestroy(void);
extern ENTRY *hsearch(ENTRY iteam, ACTION action);
extern void insque(void *element, void *insertAfter);
extern void *lfind(const void *key, const void *base, size_t *_elementCount,
	size_t width, int (*compareFunction)(const void *, const void *));
extern void  *lsearch(const void *key, void *base, size_t *_elementCount,
	size_t width, int (*compareFunction)(const void *, const void *));
extern void remque(void *element);
extern void *tdelete(const void *key, posix_tnode **_root,
	int (*compare)(const void *, const void *));
extern posix_tnode *tfind(const void *key, posix_tnode *const *root,
	int (*compare)(const void *, const void *));
extern posix_tnode *tsearch(const void *key, posix_tnode **_root,
	int (*compare)(const void *, const void *));
extern void twalk(const posix_tnode *root,
	void (*action)(const posix_tnode *, VISIT, int));

#ifdef _DEFAULT_SOURCE
extern void tdestroy(void *root, void (*free_key)(void *));
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SEARCH_H_ */
