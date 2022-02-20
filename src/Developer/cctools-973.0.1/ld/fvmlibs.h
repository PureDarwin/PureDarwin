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

#ifndef RLD
/*
 * Global types, variables and routines declared in the file fvmlibs.c.
 *
 * The following include file need to be included before this file:
 * #include <sys/loader.h> 
 * #include "ld.h"
 */

struct merged_fvmlib {
    char *fvmlib_name;		/* The name of this fixed VM shared library. */
    struct fvmlib_command *fl;	/* The LC_LOADFVMLIB load command for this */
				/*  fixed VM shared library. */
    struct object_file		/* Pointer to the object file the load */
	*definition_object;	/*  command was found in */
    enum bool multiple;		/* Flag to indicate if this was already */
				/*  loaded from more than one object */
    struct merged_fvmlib *next;	/* The next in the list, NULL otherwise */
};

/* the pointer to the head of the load fixed VM shared library commamds */
__private_extern__ struct merged_fvmlib *merged_fvmlibs;

/* the pointer to the head of the fixed VM shared library segments */
__private_extern__ struct merged_segment *fvmlib_segments;

__private_extern__ void merge_fvmlibs(
    void);

#ifdef DEBUG
__private_extern__ void print_load_fvmlibs_list(
    void);
__private_extern__ void print_fvmlib_segments(
    void);
#endif /* DEBUG */
#endif /* !defined(RLD) */
