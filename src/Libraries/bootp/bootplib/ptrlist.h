/*
 * Copyright (c) 2003 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _S_PTRLIST_H
#define _S_PTRLIST_H

#include <mach/boolean.h>

/* the initial number of elements in the list */
#define PTRLIST_NUMBER		16

typedef struct {
    void * *	array;	/* malloc'd array of pointers */
    int		size;	/* number of elements in array */
    int		count;	/* number of occupied elements */
} ptrlist_t;

void		ptrlist_init(ptrlist_t * list);
void		ptrlist_init_size(ptrlist_t * list, int size);
boolean_t	ptrlist_add(ptrlist_t * list, void * element);
boolean_t	ptrlist_insert(ptrlist_t * list, void * element, int i);
void		ptrlist_free(ptrlist_t * list);
boolean_t	ptrlist_dup(ptrlist_t * dest, ptrlist_t * source);
boolean_t	ptrlist_concat(ptrlist_t * list, ptrlist_t * extra);
int		ptrlist_count(ptrlist_t * list);
void *		ptrlist_element(ptrlist_t * list, int i);
boolean_t	ptrlist_remove(ptrlist_t * list, int i, void * * ret);
int		ptrlist_index(ptrlist_t * list, void * element);

#endif /* _S_PTRLIST_H */
