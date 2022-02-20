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

#ifdef RLD
/*
 * The set structure that holds the information for a set of dynamicly loaded
 * object files.
 */
struct set {
    char *output_addr;		/* the output memory for this set */
    unsigned long output_size;	/* the size of the output memory for this set */
    struct object_file		/* the structures for the common symbols of */
       *link_edit_common_object;/*  this set that are allocated by rld() */
    struct section_map
       *link_edit_section_maps;
    struct section
       *link_edit_common_section;
    unsigned long narchives;	/* the number of archives loaded in this set */
    struct archive *archives;	/* addresses and sizes of where they are */
};
struct archive {
    char *file_name;		/* name of the archive that is mapped */
    char *file_addr;		/* address the archive is mapped at */
    unsigned long file_size;	/* size that is mapped */
};
/*
 * Pointer to the array of set structures.
 */
__private_extern__ struct set *sets;
/*
 * Index into the array of set structures for the current set.
 */
__private_extern__ long cur_set;

__private_extern__ void new_set(
    void);
__private_extern__ void new_archive_or_fat(
    char *file_name,
    char *file_addr,
    unsigned long file_size);
__private_extern__ void remove_set(
    void);
__private_extern__ void free_sets(
    void);
__private_extern__ void clean_archives_and_fats(
    void);

#endif /* RLD */
