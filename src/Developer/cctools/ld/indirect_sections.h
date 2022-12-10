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
 * The number of indirect symbol table entries in the output file.
 */
__private_extern__ unsigned long nindirectsyms;

/*
 * If we are still attempting to prebind when indirect_section_merge() is
 * called save_lazy_symbol_pointer_relocs will get set in layout.c.
 * Between the time indirect_section_merge() gets called and the time
 * reloc_output_for_dyld() gets called prebinding may be disabled because of
 * various problems.  But the count of relocs can't change after layout so
 * we'll put them out anyway.
 */
__private_extern__ enum bool save_lazy_symbol_pointer_relocs;

/*
 * The literal_data which is set into a merged_section's literal_data field for
 * indirect sections.  The external functions declared at the end of this file
 * operate on this data and are used for the other fields of a merged_section
 * for literals (literal_merge, literal_write, and literal_free).
 */
struct indirect_section_data {
    struct indirect_item_bucket **hashtable;	/* the hash table */
    struct indirect_item_block			/* the items */
	*indirect_item_blocks;
#ifdef DEBUG
    unsigned long nfiles;	/* number of files with this section */
    unsigned long nitems;	/* total number of items in the input files */
				/*  merged into this section */
    unsigned long nprobes;	/* number of hash probes */
#endif /* DEBUG */
};

/* the number of entries in the hash table */
#define INDIRECT_SECTION_HASHSIZE 1000

/* the hash bucket entries in the hash table points to; allocated as needed */
struct indirect_item_bucket {
    struct indirect_item *indirect_item; /* pointer to the item */
    unsigned long output_offset; /* offset to this pointer in the output file */
    struct indirect_item_bucket *next;   /* next in the hash chain */
};

/*
 * The structure to hold a item's merge symbol pointer that is the indirect
 * symbol for this item.
 */
struct indirect_item {
    struct merged_symbol *merged_symbol;
    struct object_file *obj;
    unsigned long index;
};

/* the number of entries each item block*/
#define INDIRECT_SECTION_BLOCK_SIZE 1000
/* the blocks that store the item's symbol; allocated as needed */
struct indirect_item_block {
    unsigned long used;			/* the number of items used in */
    struct indirect_item		/*  this block */
	indirect_items
	[INDIRECT_SECTION_BLOCK_SIZE];	/* the item's symbol */
    struct indirect_item_block *next;	/* the next block */
};

__private_extern__ void indirect_section_merge(
    struct indirect_section_data *data, 
    struct merged_section *ms,
    struct section *s, 
    struct section_map *section_map,
    enum bool redo_live);

__private_extern__ void indirect_section_order(
    struct indirect_section_data *data, 
    struct merged_section *ms);

__private_extern__ void indirect_section_reset_live(
    struct indirect_section_data *data, 
    struct merged_section *ms);

__private_extern__ enum bool indirect_live_ref(
    struct fine_reloc *fine_reloc,
    struct section_map *map,
    struct object_file *obj,
    struct ref *ref);

__private_extern__ void indirect_section_free(
    struct indirect_section_data *data);

__private_extern__ enum bool legal_reference(
    struct section_map *from_map,
    unsigned long from_offset,
    struct section_map *to_map,
    unsigned long to_offset,
    unsigned long from_reloc_index,
    enum bool sectdiff_reloc);

__private_extern__ void output_indirect_symbols(
    void);
