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
 * Global types, variables and routines declared in the file 8byte_literals.c.
 *
 * The following include files need to be included before this file:
 * #include "ld.h"
 * #include "objects.h"
 */

/*
 * The literal_data which is set into a merged_section's literal_data field for
 * S_8BYTE_LITERALS sections.  The external functions declared at the end of
 * this file operate on this data and are used for the other fields of a
 * merged_section for literals (literal_merge and literal_write).
 */
struct literal8_data {
    struct literal8_block *literal8_blocks;	/* the literal8's */
    struct literal8_load_order_data	 /* the load order info needed to */
	*literal8_load_order_data;	 /*  re-merge when using -dead_strip */
#ifdef DEBUG
    unsigned long nfiles;	/* number of files with this section */
    unsigned long nliterals;	/* total number of literals in the input files*/
				/*  merged into this section  */
#endif /* DEBUG */
};

/* the number of entries in the hash table */
#define LITERAL8_BLOCK_SIZE 60

/* The structure to hold an 8 byte literal */
struct literal8 {
    unsigned long long0;
    unsigned long long1;
};

/* the blocks that store the literals; allocated as needed */
struct literal8_block {
    unsigned long used;			/* the number of literals used in */
    struct literal8			/*  this block */
	literal8s[LITERAL8_BLOCK_SIZE];	/* the literals */
    struct literal8_block *next;	/* the next block */
};

/* the load order info needed to re-merge when using -dead_strip */
struct literal8_load_order_data {
    unsigned long nliteral8_order_lines;
    struct literal8_order_line *literal8_order_lines;
};
/* the load order info for a single literal8 order line */
struct literal8_order_line {
    struct literal8 literal8;
    unsigned long line_number;
    unsigned long output_offset;
};

__private_extern__ void literal8_merge(
    struct literal8_data *data,
    struct merged_section *ms,
    struct section *s,
    struct section_map *section_map,
    enum bool redo_live);

__private_extern__ void literal8_order(
    struct literal8_data *data,
    struct merged_section *ms);

__private_extern__ void literal8_reset_live(
    struct literal8_data *data,
    struct merged_section *ms);

__private_extern__ enum bool get_hex_from_sectorder(
    struct merged_section *ms,
    unsigned long *index,
    unsigned long *value,
    unsigned long line_number);

__private_extern__ unsigned long lookup_literal8(
    struct literal8 literal8,
    struct literal8_data *data,
    struct merged_section *ms);

__private_extern__ void literal8_output(
    struct literal8_data *data,
    struct merged_section *ms);

__private_extern__ void literal8_free(
    struct literal8_data *data);

#ifdef DEBUG
__private_extern__ void print_literal8_data(
    struct literal8_data *data,
    char *indent);

__private_extern__ void literal8_data_stats(
    struct literal8_data *data,
    struct merged_section *ms);
#endif /* DEBUG */
