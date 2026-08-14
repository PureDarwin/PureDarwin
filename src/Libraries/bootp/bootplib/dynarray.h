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

#ifndef _S_DYNARRAY_H
#define _S_DYNARRAY_H

/*
 * dynarray.h
 * - simple array "object" that grows when needed, and has caller-supplied
 *   functions to free/duplicate elements
 */

/* 
 * Modification History
 *
 * December 6, 1999	Dieter Siegmund (dieter@apple)
 * - initial revision
 */

#include <mach/boolean.h>
#include "ptrlist.h"

typedef void dynarray_free_func_t(void *);
typedef void * dynarray_copy_func_t(void * source);

typedef struct dynarray_s {
    ptrlist_t			list;
    dynarray_free_func_t *	free_func;
    dynarray_copy_func_t *	copy_func;
} dynarray_t;

void		dynarray_init(dynarray_t * list, 
			      dynarray_free_func_t * free_func,
			      dynarray_copy_func_t * copy_func);
boolean_t	dynarray_add(dynarray_t * list, void * element);
boolean_t	dynarray_remove(dynarray_t * list, int i, void * * element_p);
boolean_t	dynarray_insert(dynarray_t * list, void * element, int i);
void		dynarray_free(dynarray_t * list);
boolean_t	dynarray_free_element(dynarray_t * list, int i);
boolean_t	dynarray_dup(dynarray_t * source, dynarray_t * copy);
int		dynarray_count(dynarray_t * list);
void *		dynarray_element(dynarray_t * list, int i);
int		dynarray_index(dynarray_t * list, void * element);

#endif /* _S_DYNARRAY_H */
