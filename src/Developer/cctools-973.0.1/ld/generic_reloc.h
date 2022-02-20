/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

/*
 * Global types, variables and routines declared in the file generic_reloc.c.
 *
 * The following include file need to be included before this file:
 * #include <reloc.h>
 * #include "section.h"
 */
__private_extern__ void generic_reloc(
    char *contents,
    struct relocation_info *relocs,
    struct section_map *map,
    long pcrel_at_end_of_disp,
    struct live_refs *refs,
    unsigned long reloc_index);

__private_extern__ int undef_bsearch(
    const unsigned long *index,
    const struct undefined_map *undefined_map);

/*
 * These routines are used to get/set values that might not be aligned correctly
 * which are being relocated.
 */
static
inline
long
get_long(
void *addr)
{
    long l;

	memcpy(&l, addr, sizeof(long));
	if(cur_obj->swapped)
	    return(SWAP_LONG(l));
	else
	    return(l);
}

static
inline
short
get_short(
void *addr)
{
    short s;

	memcpy(&s, addr, sizeof(short));
	if(cur_obj->swapped)
	    return(SWAP_SHORT(s));
	else
	    return(s);
}

static
inline
char
get_byte(
char *addr)
{
	return(*addr);
}

static
inline
void
set_long(
void *addr,
long value)
{
	if(cur_obj->swapped)
	    value = SWAP_LONG(value);
	memcpy(addr, &value, sizeof(long));
}

static
inline
void
set_short(
void *addr,
short value)
{
    if(cur_obj->swapped)
	value = SWAP_SHORT(value);
    memcpy(addr, &value, sizeof(short));
}

static
inline
void
set_byte(
char *addr,
char value)
{
	*addr = value;
}
