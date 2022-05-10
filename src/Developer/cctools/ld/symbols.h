/*
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Global types, variables and routines declared in the file symbols.c.
 *
 * The following include file need to be included before this file:
 * #include <nlist.h>
 * #include "ld.h"
 */

/*
 * This structure holds an external symbol that has been merged and will be in
 * the output file.  The nlist feilds are used as follows:
 *      union {
 *	    char *n_name;  The name of the symbol (pointing into the merged
 * 			   string table).
 *	    long  n_strx;  Only set write before the symbol is written to the
 *			   output file and then the symbol is no longer used.
 *      } n_un;
 *      unsigned char n_type;	Same as in an object file.
 *      unsigned char n_sect;	"
 *      short	      n_desc;	"
 *      unsigned long n_value;	The value of the symbol as it came from the
 * 				object it was defined in for N_SECT and N_ABS
 *				type symbols.
 *				For common symbols the size of the largest
 *				common.
 *				For N_INDR symbols a pointer to the
 *				merged_symbol that it is an indirect for.
 *	
 */
struct merged_symbol {
    struct nlist nlist;		/* the nlist structure of this merged symbol */
    unsigned long name_len;	/* the size of the symbol name */
    struct object_file		/* pointer to the object file this symbol is */
	*definition_object;	/*  defined in */
    struct dynamic_library	/* pointer to the dynamic library this symbol */
	*definition_library;	/*  is defined in, if defined_in_dylib==TRUE */
    struct object_file		/* pointer to the object file this symbol is */
	*non_dylib_referenced_obj; /* first referenced in, */
				   /* if referenced_in_non_dylib == TRUE */
    unsigned long
	error_flagged_for_dylib:1, /* symbol reported as an error in dylib */
	defined_in_dylib:1,	   /* symbol defined in dylib */
	coalesced_defined_in_dylib:1, /* symbol defined in dylib that is a */
				      /*  coalesced symbol */
	weak_def_in_dylib:1,	   /* a weak definition in a dylib */
	referenced_in_non_dylib:1, /* symbol referenced in loaded objects and */
				   /*  will be in output file */
	flagged_read_only_reloc:1, /* symbol reported as an external reloc */
				   /*  in a read only section */
	twolevel_reference:1,	   /* set only for merged_symbol structs that */
				   /*  are not in the merged symbol table but */
				   /*  only in the undefined list as a two- */
				   /*  level namespace reference from a dylib.*/
	weak_reference_mismatch:1, /* seen both a weak and non-weak reference */
	seen_undef:1,		   /* seen an undefined reference from an */
				   /*  object file. So the N_WEAK_REF bit */
				   /*  does reflect the value for the output. */
	define_a_way:1,		   /* set if this symbol was defined as a */
				   /*  result of -undefined define_a_way */
	live:1,			   /* TRUE if the symbol is not to be dead */
				   /*  stripped. */
	unused:21;
    unsigned long output_index;	/* the symbol table index this symbol will */
				/*  have in the output file. */
    int undef_order;		/* if the symbol was undefined the order it */
				/*  was seen. */
    /*
     * For two-level namespace hints this is the index into the table of
     * contents for the definition symbol in the dylib it is defined in,
     */
    unsigned long itoc;

    /*
     * If the twolevel_reference bit above is set this is a pointer to
     * the dynamic_library struct the two-level reference is in.  Then the
     * library ordinal in the nlist struct can be used with the dependent_images
     * to cause the correct module to be loaded.
     */
    struct dynamic_library *referencing_library;

    /*
     * When doing dead code stripping this is set to the fine_reloc this symbol
     * is in if any.
     */
    struct fine_reloc *fine_reloc;
};

/*
 * The number of merged_symbol structrures in a merged_symbol_list.
 */
#ifndef RLD
#define NSYMBOLS 20001
#else
#define NSYMBOLS 201
#endif /* RLD */
/* The number of size of the hash table in a merged_symbol_list */
#define SYMBOL_LIST_HASH_SIZE	(NSYMBOLS * 2)

/* The number of buckets for conflicts */
#define SYMBOL_CHUNK_SIZE 10

/* The collection of buckets for conflicting hash values */
struct merged_symbol_chunk
{
    /* the buckets */
    struct merged_symbol symbols[SYMBOL_CHUNK_SIZE];

    /*
     * next chunk (if this is not null, it means all of the buckets of this
     * chunk are full)
     */
    struct merged_symbol_chunk *next;
};

/*
 * The block that has the hash table and a pointer to symbol list.
 */
struct merged_symbol_root {
    /* the hashed array of chunks */
    struct merged_symbol_chunk chunks[SYMBOL_LIST_HASH_SIZE];

    /* the list of used symbols */
    struct merged_symbol_list *list;
};

/*
 * The symbol list is the list of symbols that have been used. It's a compact
 * flat array of pointers into the sparse hash table.
 */
struct merged_symbol_list {
    /* pointers to symbols in the merged_symbol_chunk */
    struct merged_symbol *symbols[SYMBOL_LIST_HASH_SIZE];

    /* next free location in the symbols array */
    unsigned long used;

    /* next linked symbol list (NULL means no more) */
    struct merged_symbol_list *next;
};


/* the blocks that store the strings; allocated as needed */
struct string_block {
    unsigned long size;		/* the number of bytes in this block */
    unsigned long used;		/* the number of bytes used in this block */
    char *strings;		/* the strings */
    unsigned long index;	/* the relitive index into the final symbol */
				/*  table for the block (set in pass2). */
    enum bool base_strings;	/* TRUE if this block is for strings from the */
				/*  base file (used if strip_base_symbols is */
				/*  TRUE) */
    enum bool dylib_strings;	/* TRUE if this block is for strings from a */
				/*  dylib file (string won't be in output). */
#ifdef RLD
    long set_num;		/* the object file set number these strings */
				/*  come from. */
#endif /* RLD */
    struct string_block *next;	/* the next block */
};

/*
 * The structure for the undefined list and the structure that the items
 * are allocated out of.
 */
struct undefined_list {
    struct merged_symbol
	*merged_symbol;		/* the undefined symbol */
    struct undefined_list *prev;/* previous in the chain */
    struct undefined_list *next;/* next in the chain */
};

/*
 * The structure of the load map for common symbols.  This is only used to help
 * print the load map.  It is created by define_commmon_symbols() in symbols.c
 * and used in print_load_map() in layout.c.
 */
struct common_load_map {
    struct merged_section
	*common_ms;		/* the section common symbol were allocated in*/
    unsigned long
	ncommon_symbols;	/* number of common symbols */
    struct common_symbol	/* a pointer to an array of structures (one */
	*common_symbols;	/*  for each common symbol) */
};
struct common_symbol {
    struct merged_symbol	/* a pointer the merged common symbol */
	*merged_symbol;
    unsigned long common_size;	/* the size of the merged common symbol */
};

/*
 * The head of the symbol list and the total count of all external symbols
 * in the list.  The total count of private externals is included in the total
 * count of the merged symbols.  The count of merged symbols referenced only
 * from dylibs will not be in the output file.
 */
__private_extern__ struct merged_symbol_root *merged_symbol_root;
__private_extern__ unsigned long nmerged_symbols;
__private_extern__ unsigned long nmerged_private_symbols;
__private_extern__ unsigned long nmerged_symbols_referenced_only_from_dylibs;

/*
 * nstripped_merged_symbols is set to the number of merged symbol being stripped
 * out when the strip_level is STRIP_DYNAMIC_EXECUTABLE.
 */
__private_extern__ unsigned long nstripped_merged_symbols;

/*
 * The head of the list of the blocks that store the strings for the merged
 * symbols and the total size of all the strings.  The size of the strings for
 * the private externals is included in the the merge string size.
 */
__private_extern__ struct string_block *merged_string_blocks;
__private_extern__ unsigned long merged_string_size;
__private_extern__ unsigned long merged_private_string_size;

/*
 * The head of the undefined list itself.  This is a circular list so it can be
 * searched from start to end and so new items can be put on the end.  This 
 * structure never has it's merged_symbol filled in but only serves as the
 * head and tail of the list.
 */
__private_extern__ struct undefined_list undefined_list;

/*
 * The common symbol load map.  Only allocated and filled in if load map is
 * requested.
 */
__private_extern__ struct common_load_map common_load_map;

/*
 * The object file that is created for the common symbols to be allocated in.
 */
__private_extern__
struct object_file link_edit_common_object;

/*
 * The number of local symbols that will appear in the output file and the
 * size of their strings.
 */
__private_extern__ unsigned long nlocal_symbols;
__private_extern__ unsigned long local_string_size;

/*
 * For local symbols of an object file that are not to be in the output a
 * local symbol block is created and linked off of the object structure's
 * localsym_blocks pointer.  These blocks are linked in order by the index.
 * If the symbols in that block are simply to be discarded the state is set
 * to DISCARD_SYMBOLS.
 *
 * When a block of local symbols are being excluded from the output because
 * it is a duplicate block of stabs in an include file (a N_BINCL/N_EINCL
 * group) then the state is set to EXCLUDED_INCLUDE and the sum field is filled
 * in and used to create the one N_EXCL stab to replace the group.
 *
 * The first time a N_BINCL is seen its sum needs to be set into its n_value
 * so a local symbol block state will be set to BEGIN_INCLUDE and it will
 * have a count of 1.
 * 
 * For the other cases of when a block of local symbols are being excluded from
 * the output (deleted coalesced symbols or indirect symbols) the state is set
 * to DISCARD_SYMBOLS.
 *
 * The PARSE_SYMBOLS is used temporarily while parsing out N_BINCL/N_EINCL and
 * then are removed from the list after parsing is done.
 */
enum localsym_block_state {
    PARSE_SYMBOLS,
    BEGIN_INCLUDE,
    EXCLUDED_INCLUDE,
    DISCARD_SYMBOLS
};
struct localsym_block {
    unsigned long index;
    unsigned long count;
    enum localsym_block_state state;
    unsigned long input_N_BINCL_n_value;
    unsigned long sum;
    struct localsym_block *next;
};

/*
 * The things to deal with creating local symbols with the object file's name
 * for a given section.  If the section name is (__TEXT,__text) these are the
 * same as a UNIX link editor's file.o symbols for the text section.
 */
struct sect_object_symbols {
    enum bool specified; /* if this has been specified on the command line */
    char *segname;	 /* the segment name */
    char *sectname;	 /* the section name */
    struct merged_section *ms;	/* the merged section structure */
};
__private_extern__ struct sect_object_symbols sect_object_symbols;

/*
 * The indr_symbol_pair structure is used when there are chains of N_INDR
 * that have symbols both from dylibs and not from dylibs.  The routine
 * reduce_indr_symbols() creates this and the routines output_merged_symbols()
 * and indirect_section_merge() both use it.  What is going on is that when
 * producing an output file the N_INDR symbols from a dylib can't be used in
 * a chain of N_INDR symbols.  So this structure contains merged_symbols which
 * are N_INDR which should use the matching indr_symbol from the table instead
 * of going through (struct merged_symbol *)(merged_symbol->nlist.n_value).
 */
struct indr_symbol_pair {
    struct merged_symbol *merged_symbol;
    struct merged_symbol *indr_symbol;
};
__private_extern__ struct indr_symbol_pair *indr_symbol_pairs;
__private_extern__ unsigned long nindr_symbol_pairs;

/*
 * merged_symbols_relocated is set when the merged symbols are relocated to
 * have addresses and section numbers as they would in the output file.
 */
__private_extern__ enum bool merged_symbols_relocated;

/*
 * The strings in the string table can't start at offset 0 because a symbol with
 * a string offset of zero is defined to have a null "" symbol name.  So the
 * first STRING_SIZE_OFFSET bytes are not used and the first string starts after
 * this amount.  Also these first bytes are zero so that if the special case of
 * a zero index is not handled by a program it will happen to work.
 */
#define STRING_SIZE_OFFSET (sizeof(long))

__private_extern__ void merge_symbols(
    void);
__private_extern__ struct merged_symbol *command_line_symbol(
    char *symbol_name);
__private_extern__ struct merged_symbol *lookup_symbol(
    char *symbol_name);
__private_extern__ void command_line_indr_symbol(
    char *symbol_name,
    char *indr_symbol_name);
#ifndef RLD
__private_extern__ void hash_instrument(
    void);
__private_extern__ void merge_dylib_module_symbols(
    struct dynamic_library *dynamic_library);
__private_extern__ void merge_bundle_loader_symbols(
    struct dynamic_library *dynamic_library);
#endif /* !defined(RLD) */
__private_extern__ void delete_from_undefined_list(
    struct undefined_list *undefined);
__private_extern__ void trace_merged_symbol(
    struct merged_symbol *merged_symbol);
__private_extern__ void free_pass1_symbol_data(
    void);
__private_extern__ void free_undefined_list(
    void);
__private_extern__ void define_common_symbols(
    void);
__private_extern__ void define_undefined_symbols_a_way(
    void);
__private_extern__ void mark_globals_live(
    void);
__private_extern__ void mark_N_NO_DEAD_STRIP_local_symbols_live(
    void);
__private_extern__ void set_fine_relocs_for_merged_symbols(
    void);
__private_extern__ void count_live_symbols(
    void);
__private_extern__ void define_link_editor_execute_symbols(
    unsigned long header_address);
__private_extern__ void setup_link_editor_symbols(
    void);
__private_extern__ void define_link_editor_dylib_symbols(
    unsigned long header_address);
__private_extern__ void define_link_editor_preload_symbols(
    enum bool setup);
__private_extern__ void reduce_indr_symbols(
    void);
__private_extern__ void process_undefineds(
    void);
__private_extern__ void reset_prebound_undefines(
    void);
__private_extern__ void assign_output_symbol_indexes(
    void);
#ifndef RLD
__private_extern__ void layout_dylib_tables(
    void);
__private_extern__ void output_dylib_tables(
    void);
#endif
__private_extern__ void layout_merged_symbols(
    void);
__private_extern__ void discard_local_symbols_for_section(
    unsigned long nsect,
    struct nlist *object_symbols,
    char *object_strings,
    struct section *s, 
    struct section_map *section_map);
__private_extern__ void output_local_symbols(
    void);
__private_extern__ unsigned long local_symbol_output_index(
    struct object_file *obj,
    unsigned long index);
__private_extern__ void set_merged_string_block_indexes(
    void);
__private_extern__ void output_merged_symbols(
    void);
#if defined(RLD) && !defined(SA_RLD)
__private_extern__
void output_rld_symfile_merged_symbols(
    void);
#endif /* defined(RLD) && !defined(SA_RLD) */
__private_extern__ enum bool is_output_local_symbol(
    unsigned char n_type,
    unsigned char n_sect,
    unsigned char n_desc,
    struct object_file *obj,
    char *symbol_name,
    unsigned long *output_strlen);
__private_extern__ unsigned long merged_symbol_output_index(
    struct merged_symbol *merged_symbol);
__private_extern__ void clear_read_only_reloc_flags(
    void);
__private_extern__ void flag_read_only_reloc(
    struct section *s,
    unsigned long output_index,
    enum bool *first_time);

#ifdef RLD
__private_extern__ void free_multiple_defs(
    void);
__private_extern__ void remove_merged_symbols(
    void);
#endif /* RLD */

__private_extern__ struct section *get_output_section(
    unsigned long sect);

#ifdef DEBUG
__private_extern__ void print_symbol_list(
    char *string,
    enum bool input_based);
__private_extern__ void print_undefined_list(
    void);
#endif /* DEBUG */
