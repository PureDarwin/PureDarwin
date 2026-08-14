/*
 * Copyright (c) 2003-2018 Apple Inc. All rights reserved.
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

/*
 * dynarray.c
 * - simple array "object" that grows when needed, and has caller-supplied
 *   functions to free/duplicate elements
 */

/* 
 * Modification History
 *
 * December 6, 1999	Dieter Siegmund (dieter@apple)
 * - initial revision
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <strings.h>

#include "dynarray.h"
#include "symbol_scope.h"

PRIVATE_EXTERN void
dynarray_init(dynarray_t * list, dynarray_free_func_t * free_func,
	      dynarray_copy_func_t * copy_func)
{
    ptrlist_init(&list->list);
    list->free_func = free_func;
    list->copy_func = copy_func;
    return;
}

PRIVATE_EXTERN void
dynarray_free(dynarray_t * list)
{
    void *	element;

    while (ptrlist_remove(&list->list, 0, &element)) {
	if (element && list->free_func) {
	    (list->free_func)(element);
	}
    }
    ptrlist_free(&list->list);
    return;
}

PRIVATE_EXTERN boolean_t
dynarray_add(dynarray_t * list, void * element)
{
    return (ptrlist_add(&list->list, element));
}

PRIVATE_EXTERN boolean_t
dynarray_insert(dynarray_t * list, void * element, int i)
{
    return (ptrlist_insert(&list->list, element, i));
}

PRIVATE_EXTERN boolean_t
dynarray_remove(dynarray_t * list, int i, void * * element_p)
{
    return (ptrlist_remove(&list->list, i , element_p));
}

PRIVATE_EXTERN boolean_t
dynarray_free_element(dynarray_t * list, int i)
{
    void * p;

    if (dynarray_remove(list, i, &p)) {
	if (p && list->free_func) {
	    (*list->free_func)(p);
	}
	return (TRUE);
    }
    return (FALSE);
}

PRIVATE_EXTERN boolean_t
dynarray_dup(dynarray_t * dest, dynarray_t * source)
{
    int i;

    dest->copy_func = source->copy_func;
    dest->free_func = source->free_func;
    ptrlist_init(&dest->list);

    for (i = 0; i < ptrlist_count(&source->list); i++) {
	void * element = ptrlist_element(&source->list, i);

	if (element && dest->copy_func) {
	    element = (*dest->copy_func)(element);
	}
	ptrlist_add(&dest->list, element);
    }
    return (TRUE);
}

#if 0
/* concatenates extra onto list */
boolean_t
dynarray_concat(dynarray_t * list, dynarray_t * extra)
{
    if (extra->count == 0)
	return (TRUE);
    if (list->el_size != extra->el_size)
	return (FALSE);

    if ((extra->count + list->count) > list->size) {
	list->size = extra->count + list->count;
	if (list->array == NULL)
	    list->array = malloc(list->el_size * list->size);
	else
	    list->array = realloc(list->array, 
				  list->el_size * list->size);
    }
    if (list->array == NULL)
	return (FALSE);
    bcopy(extra->array, list->array + list->count, 
	  extra->count * list->el_size);
    list->count += extra->count;
    return (TRUE);
}
#endif /* 0 */

PRIVATE_EXTERN int
dynarray_count(dynarray_t * list)
{
    return (ptrlist_count(&list->list));
}

PRIVATE_EXTERN void *
dynarray_element(dynarray_t * list, int i)
{
    return (ptrlist_element(&list->list, i));
}

PRIVATE_EXTERN int
dynarray_index(dynarray_t * list, void * element)
{
    return (ptrlist_index(&list->list, element));
}

