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
 * Global types, variables and routines declared in the file cstring_literals.c.
 *
 * The following include files need to be included before this file:
 * #include "ld.h"
 * #include "objects.h"
 */

/*
 * The literal_data which is set into a merged_section's literal_data field for
 * S_CSTRING_LITERALS sections.  The external functions declared at the end of
 * this file operate on this data and are used for the other fields of a
 * merged_section for literals (literal_merge and literal_write).
 */
struct cstring_data {
    struct cstring_bucket **hashtable;		/* the hash table */
    struct cstring_block *cstring_blocks;	/* the cstrings */
    struct cstring_load_order_data	 /* the load order info needed to */
	*cstring_load_order_data;	 /*  re-merge when using -dead_strip */
#ifdef DEBUG
    unsigned long nfiles;	/* number of files with this section */
    unsigned long nbytes;	/* total number of bytes in the input files*/
				/*  merged into this section  */
    unsigned long ninput_strings;/* number of strings in the input file */
    unsigned long noutput_strings;/* number of strings in the output file */
    unsigned long nprobes;	/* number of hash probes */
#endif /* DEBUG */
};

/* the number of entries in the hash table */
#define CSTRING_HASHSIZE 1022

/* the hash bucket entries in the hash table points to; allocated as needed */
struct cstring_bucket {
    char *cstring;		/* pointer to the string */
    unsigned long offset;	/* offset of this string in the output file */
    struct cstring_bucket *next;/* next in the hash chain */
};

/* the blocks that store the strings; allocated as needed */
struct cstring_block {
    unsigned long size;		/* the number of bytes in this block */
    unsigned long used;		/* the number of bytes used in this block */
    enum bool full;		/* no more strings are to come from this block*/
    char *cstrings;		/* the strings */
    struct cstring_block *next;	/* the next block */
};

/* the load order info needed to re-merge when using -dead_strip */
struct cstring_load_order_data {
    char *order_line_buffer;
    unsigned long ncstring_order_lines;
    struct cstring_order_line *cstring_order_lines;
};
/* the load order info for a single cstring order line */
struct cstring_order_line {
    unsigned character_index;
    unsigned long output_offset;
};

__private_extern__ void cstring_merge(
    struct cstring_data *data,
    struct merged_section *ms,
    struct section *s,
    struct section_map *section_map,
    enum bool redo_live);

__private_extern__ void cstring_order(
    struct cstring_data *data,
    struct merged_section *ms);

__private_extern__ void cstring_reset_live(
    struct cstring_data *data,
    struct merged_section *ms);

__private_extern__ void get_cstring_from_sectorder(
    struct merged_section *ms,
    unsigned long *index,
    char *buffer,
    unsigned long line_number,
    unsigned long char_pos);

__private_extern__ unsigned long lookup_cstring(
    char *cstring,
    struct cstring_data *data,
    struct merged_section *ms);

__private_extern__ void cstring_output(
    struct cstring_data *data,
    struct merged_section *ms);

__private_extern__ void cstring_free(
    struct cstring_data *data);

#ifdef DEBUG
__private_extern__ void print_cstring_data(
    struct cstring_data *data,
    char *indent);

__private_extern__ void cstring_data_stats(
    struct cstring_data *data,
    struct merged_section *ms);
#endif /* DEBUG */
