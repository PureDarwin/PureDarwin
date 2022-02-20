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
 * The literal_data which is set into a merged_section's literal_data field for
 * S_LITERAL_POINTERS sections.  The external functions declared at the end of
 * this file operate on this data and are used for the other fields of a
 * merged_section for literals (literal_merge, literal_write, and literal_free).
 */
struct literal_pointer_data {
    struct literal_pointer_bucket **hashtable;	/* the hash table */
    struct literal_pointer_block		/* the literal pointers */
	*literal_pointer_blocks;
    struct literal_pointer_load_order_data/* the load order info needed to */
	*literal_pointer_load_order_data; /*  re-merge when using -dead_strip */
#ifdef DEBUG
    unsigned long nfiles;	/* number of files with this section */
    unsigned long nliterals;	/* total number of literal pointers in the */
				/*  input files merged into this section  */
    unsigned long nprobes;	/* number of hash probes */
#endif /* DEBUG */
};

/* the number of entries in the hash table */
#define LITERAL_POINTER_HASHSIZE 1000

/* the hash bucket entries in the hash table points to; allocated as needed */
struct literal_pointer_bucket {
    struct literal_pointer
	*literal_pointer;	/* pointer to the literal pointer */
    unsigned long output_offset;/* offset to this pointer in the output file */
    struct literal_pointer_bucket
	*next;			/* next in the hash chain */
};

/* The structure to hold a literal pointer.   This can be one of two things,
 * if the symbol is undefined then merged_symbol is not NULL and points to
 * a merged symbol and with offset defines the merged literal, second the
 * literal being pointed to is in the merged section literal_ms and is at
 * merged_section_offset in that section and offset is added.
 */
struct literal_pointer {
    struct merged_section *literal_ms;	
    unsigned long merged_section_offset;
    unsigned long offset;
    struct merged_symbol *merged_symbol;
};

/* the number of entries each literal pointer block*/
#define LITERAL_POINTER_BLOCK_SIZE 1000
/* the blocks that store the literals; allocated as needed */
struct literal_pointer_block {
    unsigned long used;			/* the number of literals used in */
    struct literal_pointer		/*  this block */
	literal_pointers
	[LITERAL_POINTER_BLOCK_SIZE];	/* the literal pointers */
    struct literal_pointer_block *next;	/* the next block */
};

/* the load order info needed to re-merge when using -dead_strip */
struct literal_pointer_load_order_data {
    char *order_line_buffer;
    unsigned long nliteral_pointer_order_lines;
    struct literal_pointer_order_line *literal_pointer_order_lines;
};
/* the load order info for a single literal pointer order line */
struct literal_pointer_order_line {
    unsigned character_index;
    unsigned long line_number;
    unsigned long output_offset;
};

__private_extern__ void literal_pointer_merge(
    struct literal_pointer_data *data, 
    struct merged_section *ms,
    struct section *s, 
    struct section_map *section_map,
    enum bool redo_live);

__private_extern__ void literal_pointer_order(
    struct literal_pointer_data *data, 
    struct merged_section *ms);

__private_extern__ void literal_pointer_reset_live(
    struct literal_pointer_data *data, 
    struct merged_section *ms);

__private_extern__ void literal_pointer_output(
    struct literal_pointer_data *data, 
    struct merged_section *ms);

__private_extern__ void literal_pointer_free(
    struct literal_pointer_data *data);

#ifdef DEBUG
__private_extern__ void print_literal_pointer_data(
    struct literal_pointer_data *data, 
    char *ident);
__private_extern__ void literal_pointer_data_stats(
    struct literal_pointer_data *data,
    struct merged_section *ms);
#endif /* DEBUG */
