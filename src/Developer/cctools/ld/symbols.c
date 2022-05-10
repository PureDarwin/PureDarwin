/*
 * Copyright (c) 1999-2007 Apple Computer, Inc.  All Rights Reserved.
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
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines to manage the merging of the symbols.
 * It builds a merged symbol table and string table for external symbols.
 * It also contains all other routines that deal with symbols.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <ctype.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include <mach-o/ldsyms.h>
#include <ar.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/macosx_deployment_target.h"
#ifndef RLD
#include "stuff/symbol_list.h"
#endif

#include "ld.h"
#include "specs.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "pass2.h"
#include "sets.h"
#include "hash_string.h"
#include "dylibs.h"
#include "mod_sections.h"

#ifdef RLD
__private_extern__ char *base_name;
#endif

/*
 * The head of the symbol list and the total count of all external symbols
 * in the list.  The total count of private externals is included in the total
 * count of the merged symbols.  The private externals may end up as global or
 * static depending on the -keep_private_externs flag.  The count of merged
 * symbols referenced only from dylibs will not be in the output file.
 */
__private_extern__ struct merged_symbol_root *merged_symbol_root = NULL;
__private_extern__ unsigned long nmerged_symbols = 0;
__private_extern__ unsigned long nmerged_private_symbols = 0;
__private_extern__ unsigned long nmerged_symbols_referenced_only_from_dylibs =0;

/*
 * nstripped_merged_symbols is set to the number of merged symbol being stripped
 * out when -dead_strip is specified or the strip_level is
 * STRIP_DYNAMIC_EXECUTABLE.
 */
__private_extern__ unsigned long nstripped_merged_symbols = 0;
/*
 * When -dead_strip is specified some of these the merged private symbols may
 * get stripped. To allow assign_output_symbol_indexes() to recalculate the
 * value of nstripped_merged_symbols when it sets the strip_level to
 * STRIP_DYNAMIC_EXECUTABLE it starts with this value and then adds the
 * additional merged symbols to strip.  This value is also used to do its
 * consistency check after assigning local symbol indexes.
 * nstripped_merged_private_symbols is set in count_live_symbols().
 */
static unsigned long nstripped_merged_private_symbols = 0;

/*
 * The head of the list of the blocks that store the strings for the merged
 * symbols and the total size of all the strings.
 */
__private_extern__ struct string_block *merged_string_blocks = NULL;
__private_extern__ unsigned long merged_string_size = 0;

/*
 * To order the merged symbol table these arrays are allocated and filled in by
 * assign_output_symbol_indexes() to assign the output symbol indexes and then
 * used by output_merged_symbols() to put the symbols out in that order.
 */
static struct merged_symbol **undefsyms_order = NULL;
static struct merged_symbol **extdefsyms_order = NULL;
/*
 * The current order of an undefined symbol.  This is set into the merged_symbol
 * and later used if bind_at_load is set to sort the undefined symbols by so
 * they are in the order the were seen by the static linker.
 */
static int undef_order = 0;
#ifndef SA_RLD
/*
 * The qsort routines used by assign_output_symbol_indexes() to order the merged
 * symbol table.
 */
static int qsort_by_module(
    const struct merged_symbol **ms1,
    const struct merged_symbol **ms2);
static int qsort_by_name(
    const struct merged_symbol **ms1,
    const struct merged_symbol **ms2);
static int qsort_by_undef_order(
    const struct merged_symbol **ms1,
    const struct merged_symbol **ms2);
#endif /* !defined(SA_RLD) */

/*
 * The number of local symbols that will appear in the output file and the
 * size of their strings.
 */
__private_extern__ unsigned long nlocal_symbols = 0;
__private_extern__ unsigned long local_string_size = 0;

/*
 * The things to deal with creating local symbols with the object file's name
 * for a given section.  If the section name is (__TEXT,__text) these are the
 * same as a UNIX link editor's file.o symbols for the text section.
 */
__private_extern__ struct sect_object_symbols sect_object_symbols = { FALSE };

/*
 * The head of the undefined list and the list of free undefined structures.
 * These are circular lists so they can be searched from start to end and so
 * new items can be put on the end.  These two structure never has their
 * merged_symbol filled in but they only serve as the heads and tails of there
 * lists.
 */
__private_extern__ struct undefined_list undefined_list = {
    NULL, &undefined_list, &undefined_list
};
static struct undefined_list free_list = {
    NULL, &free_list, &free_list
};
/*
 * The structures for the undefined list are allocated in blocks and placed on
 * a free list.  They are allocated in blocks so they can be free()'ed quickly.
 */
#define NUNDEF_BLOCKS	680
static struct undefined_block {
    struct undefined_list undefineds[NUNDEF_BLOCKS];
    struct undefined_block *next;
} *undefined_blocks;

#ifndef RLD
/*
 * The common symbol load map.  Only allocated and filled in if load map is
 * requested.
 */
__private_extern__ struct common_load_map common_load_map = { 0 };

/*
 * These symbols are used by the routines command_line_symbol(),
 * command_line_indr_symbol() and merge_dylib_symbols() to create symbols from
 * the command line options (-u and -i) and from dylibs.
 */
static struct nlist undefined_symbol = {
    {0},		/* n_un.n_strx */
    N_UNDF | N_EXT,	/* n_type */
    NO_SECT,		/* n_sect */
    0,			/* n_desc */
    0			/* n_value */
};
static struct nlist indr_symbol = {
    {0},		/* n_un.n_strx */
    N_INDR | N_EXT,	/* n_type */
    NO_SECT,		/* n_sect */
    0,			/* n_desc */
    0			/* n_value */
};
static struct nlist pbud_symbol = {
    {0},		/* n_un.n_strx */
    N_PBUD | N_EXT,	/* n_type */
    NO_SECT,		/* n_sect */
    REFERENCE_FLAG_UNDEFINED_LAZY, /* n_desc */
    0			/* n_value */
};
static struct nlist pbud_weak_def_symbol = {
    {0},		/* n_un.n_strx */
    N_PBUD | N_EXT,	/* n_type */
    NO_SECT,		/* n_sect */
    REFERENCE_FLAG_UNDEFINED_LAZY | N_WEAK_DEF, /* n_desc */
    0			/* n_value */
};

/*
 * This symbol is used by the routines that define link editor defined symbols.
 * And the routine sets it up.
 */
static struct object_file *link_edit_symbols_object = NULL;
static void setup_link_edit_symbols_object(
    void);

/*
 * Most of the time there are no local symbols marked with the NO_DEAD_STRIP
 * flag.  Since it is time consuming to mark those blocks we check to see if
 * when have any of them in merge_symbols.
 */
static enum bool local_NO_DEAD_STRIP_symbols = FALSE;
static void mark_N_NO_DEAD_STRIP_local_symbols_in_section_live(
    struct merged_section *ms);
static void removed_dead_local_symbols_in_section(
    struct merged_section *ms);
static void remove_dead_N_GSYM_stabs(
    void);
static void setup_link_editor_symbol(
    char *symbol_name);
static void define_link_editor_dylib_symbol(
    unsigned long header_address,
    char *symbol_name);
static void exports_list_processing(
    char *symbol_name,
    struct nlist *symbol);
static char * find_stab_name_end(
    char *name);
static char * find_stab_type_end(
    char *name_end);
#endif /* !defined(RLD) */
static enum bool is_type_stab(
    unsigned char n_type,
    char *symbol_name);

/*
 * These symbols are used when defining common symbols.  In the RLD case they
 * are templates and thus const and the real versions of these symbols are in
 * the sets array.
 */
static
#if defined(RLD) && !defined(__DYNAMIC__)
const
#endif
struct section link_edit_common_section = {
    SECT_COMMON,	/* sectname */
    SEG_DATA,		/* segname */
    0,			/* addr */
    0,			/* size */
    0,			/* offset */
    0,			/* align */
    0,			/* reloff */
    0,			/* nreloc */
    S_ZEROFILL,		/* flags */
    0,			/* reserved1 */
    0,			/* reserved2 */
};

static
#if defined(RLD) && !defined(__DYNAMIC__)
const
#endif
struct section_map link_edit_section_maps = {
#ifdef RLD
    NULL,		/* struct section *s */
#else
    &link_edit_common_section, /* struct section *s */
#endif /* RLD */
    NULL,		/* output_section */
    0,			/* offset */
    0,			/* flush_offset */
    NULL,		/* fine_relocs */
    0,			/* nfine_relocs */
    FALSE,		/* no_load_order */
    0,			/* order */
    NULL,		/* load_orders */
    0			/* nload_orders */
};

#ifndef RLD
__private_extern__
struct symtab_command link_edit_common_symtab = {
    LC_SYMTAB,		/* cmd */
    sizeof(struct symtab_command),	/* cmdsize */
    0,			/* symoff */
    0,			/* nsyms */
    0,			/* stroff */
    1			/* strsize */
};
#endif /* !defined(RLD) */

__private_extern__
struct object_file link_edit_common_object = {
    "\"link editor\"",	/* file_name */
    NULL,		 /* obj_addr */
    0,			/* obj_size */
    FALSE,		/* swapped */
    FALSE,		/* fvmlib_stuff */
    FALSE,		/* dylib */
    FALSE,		/* dylib_stuff */
    FALSE,		/* bundle_loader */
    0,			/* library_ordinal */
    0,			/* isub_image */
    0,			/* nload_dylibs */
    FALSE,		/* dylinker */
    FALSE,		/* command_line */
    NULL,		/* ar_hdr */
    NULL,		/* ar_name */
    0,			/* ar_name_size */
    NULL,		/* dylib_module */
    1,			/* nsection_maps */
#ifdef RLD
    NULL,		/* section_maps */
    NULL,		/* symtab */
#else
    &link_edit_section_maps,	/* section_maps */
    &link_edit_common_symtab,	/* symtab */
#endif /* RLD */
    NULL,		/* dysymtab */
    NULL,		/* rc */
    0,			/* nundefineds */
    NULL,		/* undefined_maps */
    0,			/* nextdefsym */
    0,			/* iextdefsym */
    0,			/* nprivatesym */
    0,			/* iprivatesym */
    0,			/* cprivatesym */
    0,			/* nlocalsym */
    0,			/* ilocalsym */
    NULL,		/* localsym_blocks */
    NULL		/* cur_section_map */
#ifdef RLD
    ,0,			/* set_num */
    FALSE		/* user_obj_addr */
#endif /* RLD */
};

/*
 * This is the list of multiply defined symbol names.  It is used to make sure
 * an error message for each name is only printed once and it is traced only
 * once.
 */
static char **multiple_defs = NULL;
static unsigned long nmultiple_defs = 0;

/*
 * This is the count of indirect symbols in the merged symbol table.  It is used
 * as the size of an array that needed to be allocated to reduce chains of
 * indirect symbols to their final symbol and to detect circular chains.
 */
static unsigned long nindr_symbols = 0;

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
__private_extern__ struct indr_symbol_pair *indr_symbol_pairs = NULL;
__private_extern__ unsigned long nindr_symbol_pairs = 0;

/*
 * commons_exist is set and used in define_common_symbols().  noundefs is set
 * and used in process_undefineds().  These are then used in
 * layout_merged_symbols() to properly set the MH_NOUNDEFS flag (which in turn
 * properly set the execute bits of the file).
 */
static enum bool commons_exist = FALSE;
static enum bool noundefs = TRUE;

/*
 * merged_symbols_relocated is set when the merged symbols are relocated to
 * have addresses and section numbers as they would in the output file.
 */
__private_extern__ enum bool merged_symbols_relocated = FALSE;

static struct merged_symbol *enter_symbol(
    struct merged_symbol *hash_pointer,
    struct nlist *object_symbol,
    char *object_strings,
    struct object_file *definition_object);
static void enter_indr_symbol(
    struct merged_symbol *merged_symbol,
    struct nlist *object_symbol,
    char *object_strings,
    struct object_file *definition_object);
static char *enter_string(
    char *symbol_name,
    unsigned long *len_ret);
static void add_to_undefined_list(
    struct merged_symbol *merged_symbol);
static void multiply_defined(
    struct merged_symbol *merged_symbol,
    struct nlist *object_symbol,
    char *object_strings);
static void trace_object_symbol(
    struct nlist *symbol,
    char *strings);
static void trace_symbol(
    char *symbol_name,
    struct nlist *nlist,
    struct object_file *object_file,
    char *indr_symbol_name);
#ifndef RLD
static void define_link_editor_symbol(
    char *symbol_name,
    unsigned char type,
    unsigned char sect,
    short desc,
    unsigned long value);
static void remove_dead_N_GSYM_stabs_for_cur_obj(
    struct nlist *object_symbols,
    char *object_strings);
#endif /* !defined(RLD) */
static unsigned long merged_symbol_string_index(
    char *symbol_name);
static struct string_block *get_string_block(
    char *symbol_name);
static void get_stroff_and_mtime_for_N_OSO(
    unsigned long *stroff_for_N_OSO,
    unsigned long *mtime);

/*
 * Check all the fields of the given symbol in the current object to make sure
 * it is valid.  This is required to that the rest of the code can assume that
 * use the values in the symbol without futher checks and without causing an
 * error.
 */
static
inline
void
check_symbol(
struct nlist *symbol,
char *strings,
unsigned long index)
{
    unsigned long section_type, library_ordinal;

	/* check the n_strx field of this symbol */
	if(symbol->n_un.n_strx < 0 ||
	   (uint32_t)symbol->n_un.n_strx >= cur_obj->symtab->strsize){
	    error_with_cur_obj("bad string table index (%d) for symbol %lu",
			       symbol->n_un.n_strx, index);
	    return;
	}

	/* check the n_type field of this symbol */
	switch(symbol->n_type & N_TYPE){
	case N_UNDF:
	    if((symbol->n_type & N_STAB) == 0 &&
	       (symbol->n_type & N_EXT) == 0){
		error_with_cur_obj("undefined symbol %lu (%s) is not also "
				   "external symbol (N_EXT)", index,
				   symbol->n_un.n_strx == 0 ? "NULL name" :
				   strings + symbol->n_un.n_strx);
		return;
	    }
	    if(symbol->n_type & N_PEXT && symbol->n_value == 0){
		error_with_cur_obj("undefined symbol %lu (%s) can't be "
				   "private external symbol (N_PEXT)", index,
				   symbol->n_un.n_strx == 0 ? "NULL name" :
				   strings + symbol->n_un.n_strx);
		return;
	    }
	    if((symbol->n_type & N_STAB) == 0 &&
	       (((struct mach_header *)(cur_obj->obj_addr))->flags &
	       MH_TWOLEVEL) == MH_TWOLEVEL){
		library_ordinal = GET_LIBRARY_ORDINAL(symbol->n_desc);
		if((library_ordinal == EXECUTABLE_ORDINAL &&
		    ((struct mach_header *)(cur_obj->obj_addr))->filetype !=
		    MH_BUNDLE) ||
		   (library_ordinal != SELF_LIBRARY_ORDINAL &&
		    (library_ordinal != DYNAMIC_LOOKUP_ORDINAL ||
		     cur_obj->nload_dylibs != DYNAMIC_LOOKUP_ORDINAL) &&
		    library_ordinal-1 >= cur_obj->nload_dylibs) ){
		    error_with_cur_obj("undefined symbol %lu (%s) has bad "
				       "library oridinal %lu", index,
				       symbol->n_un.n_strx == 0 ? "NULL name" :
				       strings + symbol->n_un.n_strx,
				       library_ordinal);
		    return;
		}
	    }
	    /* fall through to the check below */
	case N_ABS:
	    if((symbol->n_type & N_STAB) == 0 &&
	       symbol->n_sect != NO_SECT){
		error_with_cur_obj("symbol %lu (%s) must have NO_SECT for "
			    "its n_sect field given its type", index,
			    symbol->n_un.n_strx == 0 ? "NULL name" :
			    strings + symbol->n_un.n_strx);
		return;
	    }
	    break;
	case N_PBUD:
	    if((symbol->n_type & N_STAB) == 0 &&
	       (symbol->n_type & N_EXT) == 0){
		error_with_cur_obj("undefined symbol %lu (%s) is not also "
				   "external symbol (N_EXT)", index,
				   symbol->n_un.n_strx == 0 ? "NULL name" :
				   strings + symbol->n_un.n_strx);
		return;
	    }
	    if((symbol->n_type & N_STAB) == 0 &&
	       symbol->n_sect != NO_SECT){
		error_with_cur_obj("symbol %lu (%s) must have NO_SECT for "
			    "its n_sect field given its type", index,
			    symbol->n_un.n_strx == 0 ? "NULL name" :
			    strings + symbol->n_un.n_strx);
		return;
	    }
	    break;
	case N_SECT:
	    if((symbol->n_type & N_STAB) == 0 &&
	       symbol->n_sect == NO_SECT){
		error_with_cur_obj("symbol %lu (%s) must not have NO_SECT "
			    "for its n_sect field given its type (N_SECT)",
			    index, symbol->n_un.n_strx == 0 ? "NULL name" :
			    strings + symbol->n_un.n_strx);
		return;
	    }
	    break;
	case N_INDR:
	    if(symbol->n_type & N_EXT){
		/* note n_value is unsigned and can't be < 0 */
		if(symbol->n_value >= cur_obj->symtab->strsize){
		    error_with_cur_obj("bad string table index (%u) for "
			"indirect name for symbol %lu (%s)",
			symbol->n_value, index, symbol->n_un.n_strx == 0 ?
			"NULL name" : strings + symbol->n_un.n_strx);
		    return;
		}
	    }
	    else if((symbol->n_type & N_STAB) == 0){
		error_with_cur_obj("indirect symbol %lu (%s) is not also "
				   "external symbol (N_EXT)", index,
				   symbol->n_un.n_strx == 0 ? "NULL name" :
				   strings + symbol->n_un.n_strx);
		return;
	    }
	    if(symbol->n_type & N_PEXT){
		error_with_cur_obj("indirect symbol %lu (%s) can't be "
				   "private external symbol (N_PEXT)", index,
				   symbol->n_un.n_strx == 0 ? "NULL name" :
				   strings + symbol->n_un.n_strx);
		return;
	    }
	    break;
	default:
	    if((symbol->n_type & N_STAB) == 0){
		error_with_cur_obj("symbol %lu (%s) has unknown n_type field "
				   "(0x%x)", index, symbol->n_un.n_strx == 0 ?
				   "NULL name" : strings + symbol->n_un.n_strx,
				   (unsigned int)(symbol->n_type));
		return;
	    }
	    break;
	}

	/*
	 * Check the n_sect field, note sections are numbered from 1 up to and
	 * including the total number of sections (that is the test is > not
	 * >= ).
	 */
	if((unsigned long)(symbol->n_sect) > cur_obj->nsection_maps){
	    error_with_cur_obj("symbol %lu (%s)'s n_sect field (%d) is "
		"greater than the number of sections in this object (%lu)",
		index, symbol->n_un.n_strx == 0 ? "NULL name" : strings +
		symbol->n_un.n_strx, symbol->n_sect, cur_obj->nsection_maps);
	    return;
	}

	/*
	 * Check to make sure this is not an enternal symbol defined in an
	 * indirect section.
	 */
	if((symbol->n_type & N_EXT) != 0 &&
	   (symbol->n_type & N_TYPE) == N_SECT){
	    section_type = (cur_obj->section_maps[symbol->n_sect - 1].s->flags)
			   & SECTION_TYPE;
	    if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
	       section_type == S_LAZY_SYMBOL_POINTERS ||
	       section_type == S_SYMBOL_STUBS){
		error_with_cur_obj("external symbol %lu (%s) not allowed in an "
		    "indirect section", index, symbol->n_un.n_strx == 0 ?
		    "NULL name" : strings + symbol->n_un.n_strx);
		return;
	    }
	}

	/*
	 * Check to see that any symbol that is marked as a weak_definition
	 * is a global symbol defined in a coalesced section.
	 */
	if((symbol->n_type & N_STAB) == 0 &&
	   (symbol->n_desc & N_WEAK_DEF) == N_WEAK_DEF){
		if((symbol->n_type & N_EXT) == 0 &&
		   (symbol->n_type & N_PEXT) != N_PEXT){
		    error_with_cur_obj("non-external symbol %lu (%s) can't be a"
			" weak definition", index, symbol->n_un.n_strx == 0 ?
			"NULL name" : strings + symbol->n_un.n_strx);
		    return;
		}
		if((symbol->n_type & N_TYPE) == N_UNDF ||
		   (symbol->n_type & N_TYPE) == N_PBUD){
		    error_with_cur_obj("undefined symbol %lu (%s) can't be a "
			"weak definition", index, symbol->n_un.n_strx == 0 ?
			"NULL name" : strings + symbol->n_un.n_strx);
		    return;
		}
		if((symbol->n_type & N_TYPE) != N_SECT){
		    error_with_cur_obj("symbol %lu (%s) can't be a weak "
			"definition (currently only supported in section of "
			"type S_COALESCED)", index, symbol->n_un.n_strx == 0 ?
			"NULL name" : strings + symbol->n_un.n_strx);
		    return;
		}
		else{
		    section_type = (cur_obj->section_maps[symbol->n_sect - 1].
				    s->flags) & SECTION_TYPE;
		    if(section_type != S_COALESCED){
			error_with_cur_obj("symbol %lu (%s) can't be a weak "
			    "definition (currently only supported in section "
			    "of type S_COALESCED)", index, symbol->n_un.n_strx
			    == 0 ? "NULL name" : strings + symbol->n_un.n_strx);
			return;
		    }
		}
	}

	/*
	 * Check to make sure this symbols is not in a debug section.
	 */
	if((symbol->n_type & N_TYPE) == N_SECT &&
	   ((cur_obj->section_maps[symbol->n_sect - 1].s->flags)
	      & S_ATTR_DEBUG) == S_ATTR_DEBUG){
	    error_with_cur_obj("malformed object, symbols not allowed in debug "
		"sections (symbol %lu (%s) is in debug section (%.16s,%.16s)",
		index, symbol->n_un.n_strx == 0 ? "NULL name" :
		strings + symbol->n_un.n_strx,
		cur_obj->section_maps[symbol->n_sect - 1].s->segname,
		cur_obj->section_maps[symbol->n_sect - 1].s->sectname);
	    return;
	}
}

/*
 * relocate_symbol() relocates the specified symbol pointed to by nlist in the
 * object file pointed to by object file.  It modifies the section number of
 * the symbol and the value of the symbol to what it should be in the output
 * file.
 */
static
inline
void
relocate_symbol(
struct nlist *nlist,
struct object_file *object_file)
{
    struct section_map *section_map;

	/*
	 * If this symbol is not in a section then it is not changed.
	 */
	if(nlist->n_sect == NO_SECT)
	    return;
#ifdef RLD
	/*
	 * If this symbol is not in the current set of objects being linked
	 * and loaded it does not get relocated.
	 */
	if(object_file->set_num != cur_set)
	    return;
#endif /* RLD */

	/*
	 * Change the section number of this symbol to the section number it
	 * will have in the output file.  For RLD all section numbers are left
	 * as they are in the input file they came from so that a future call
	 * to trace_symbol() will work.  If they are are written to an output
	 * file then they are updated in the output memory buffer by the
	 * routines that output the symbols so to leave the merged symbol table
	 * data structure the way it is.
	 */
	section_map = &(object_file->section_maps[nlist->n_sect - 1]);
#ifndef RLD
	nlist->n_sect = section_map->output_section->output_sectnum;
#endif /* RLD */

	/*
	 * If this symbol comes from base file of an incremental load
	 * then it's value is not adjusted.
	 */
	if(object_file == base_obj)
	    return;
	/*
	 * Adjust the value of this symbol by it's section.  The base
	 * of the section in the object file it came from is subtracted
	 * the base of the section in the output file is added and the
	 * offset this section appears in the output section is added.
	 *
	 * value += - old_section_base_address
	 *	    + new_section_base_address
	 *	    + offset_in_the_output_section;
	 *
	 * If the symbol is in a section that has fine relocation then
	 * it's value is set to where the value is in the output file
	 * by using the offset in the input file's section and getting
	 * the offset in the output file's section (via the fine
	 * relocation structures) and adding the address of that section
	 * in the output file.
	 */
	if(section_map->nfine_relocs == 0)
	    nlist->n_value += - section_map->s->addr
			      + section_map->output_section->s.addr
			      + section_map->offset;
	else
	    nlist->n_value = fine_reloc_output_offset(section_map,
						      nlist->n_value -
						      section_map->s->addr)
			     + section_map->output_section->s.addr;
}

#ifndef RLD
/*
 * When removing stabs from duplicate include files this hash table and
 * structure keeps the list of ones we have already seen.
 */
struct include_file {
    char *include_file_name;
    unsigned long sum;
#ifdef DEBUG
    char *object_file_name;
    unsigned long index;
#endif
    struct include_file *next;
};
#define INCLUDE_HASH_SIZE 1000
static struct include_file *include_file_hash_table[INCLUDE_HASH_SIZE] = { 0 };

/*
 * lookup_and_enter_include() looks up the include file name to see if we have
 * seen it with this sum before.  If it has not been seen before we return
 * TRUE indicating this is new and record the name and sum in the hash table.
 * If it is not new we return FALSE indicating we have seen this before.
 */
static
enum bool
lookup_and_enter_include(
char *include_file_name,
unsigned long sum,
unsigned long index,
enum bool next_eincl)
{
    unsigned long hash_index;
    struct include_file *include_file, *p, *q;

	hash_index = hash_string(include_file_name, NULL) % INCLUDE_HASH_SIZE;
	if(include_file_hash_table[hash_index] == NULL){
	    include_file = allocate(sizeof(struct include_file));
	    memset(include_file, '\0', sizeof(struct include_file));
	    include_file_hash_table[hash_index] = include_file;
	    include_file->include_file_name = include_file_name;
	    include_file->sum = sum;
#ifdef DEBUG
	    include_file->object_file_name = cur_obj->file_name;
	    include_file->index = index;
#endif
	    include_file_hash_table[hash_index] = include_file;
	    return(TRUE);
	}
	/*
	 * Look through the hash buckets and see if this is the same include
	 * file name with the same sum is found.  If so return FALSE indicating
	 * this is not new.
	 */
	p = include_file_hash_table[hash_index];
	for(;;){
	    if(p->sum == sum &&
	       strcmp(p->include_file_name, include_file_name) == 0)
#ifdef OPTIMISTIC
	    /*
	     * Be very very optimistic and assume if the names match and
	     * as long as neither sum is zero or they are zero and they
	     * match the header really should be the same.
	     */
	    if(strcmp(p->include_file_name, include_file_name) == 0 &&
	       (p->sum == sum || (p->sum != 0 && sum != 0)))
#endif /* OPTIMISTIC */
	    {
#ifdef DEBUG
		if(debug & (1 << 24))
		    printf("include file = %s in object file = %s has same "
		       "sum as previous object file = %s\n",
		       include_file_name, cur_obj->file_name,
		       p->object_file_name);
#endif /* DEBUG */
		return(FALSE);
	    }
	    if(p->next == NULL){
#ifdef DEBUG
		q = include_file_hash_table[hash_index];
		for(;;){
		    if(debug & (1 << 23) &&
		       strcmp(q->include_file_name, include_file_name) == 0 &&
		       q->sum != sum && sum != 0 && q->sum != 0 &&
		       next_eincl == FALSE){
			if(debug & (1 << 23))
			    printf("include file = %s in object file = %s at "
				   "index %lu with different sum than previous "
				   "object file = %s at index %lu\n",
				   include_file_name, cur_obj->file_name, index,
				   q->object_file_name, q->index);
			break;
		    }
		    if(q->next == NULL)
			break;
		    else
			q = q->next;
		}
#endif /* DEBUG */
		break;
	    }
	    else
		p = p->next;
	}

	/*
	 * We did not find this include file with the same sum. So create a new
	 * entry for this one and hang it off the hash chain.
	 */
	include_file = allocate(sizeof(struct include_file));
	memset(include_file, '\0', sizeof(struct include_file));
	p = include_file_hash_table[hash_index];
	include_file_hash_table[hash_index] = include_file;
	include_file->include_file_name = include_file_name;
	include_file->sum = sum;
#ifdef DEBUG
	include_file->object_file_name = cur_obj->file_name;
	include_file->index = index;
#endif
	include_file->next = p;
	return(TRUE);
}
#endif /* !defined(RLD) */

/*
 * count_dwarf_symbols() returns the number of DWARF symbols that would
 * be generated by SYM, which must be in the current object.  It is the
 * pass1 counterpart of add_dwarf_map_for_sym.
 */
static size_t
count_dwarf_symbols(const struct nlist *sym,
		    size_t i, const size_t * debug_ptr)
{
  size_t cnt;

  /* The debug map only represents symbols which are defined in
     a particular section or which are global common symbols.  */
  if ((sym->n_type & (N_TYPE | N_STAB)) != N_SECT
      && ((sym->n_type & (N_TYPE | N_STAB)) != N_UNDF
	  || sym->n_value == 0))
    return 0;
  /* If S_ATTR_STRIP_STATIC_SYMS is set on this symbol's section,
     we don't need a debug symbol for this symbol.  */
  if ((sym->n_type & (N_TYPE | N_STAB)) == N_SECT
      && (cur_obj->section_maps[sym->n_sect - 1].s->flags &
	  S_ATTR_STRIP_STATIC_SYMS))
    return 0;
  if (! debug_ptr || *debug_ptr != i)
    return 1;
  for (cnt = 0; debug_ptr[cnt + 2] & 0x80000000; cnt++)
    ;
  return cnt + 4;
}

/*
 * 'merged_symbol' is about to have its defining object changed.  If the
 * symbol was from a .o file with DWARF, remove the corresponding
 * DWARF symbols.
 */
static void
maybe_remove_dwarf_symbol (struct merged_symbol *merged_symbol)
{
  struct object_file * mo = merged_symbol->definition_object;
  struct object_file * cur_o = cur_obj;
  size_t * debug_ptr = mo->dwarf_source_data;
  size_t n;
  struct nlist * obj_symbols;
  char * obj_strings;
  size_t i;

  if (! merged_symbol->definition_object->dwarf_name)
    return;
  
  obj_symbols = (struct nlist *) (mo->obj_addr + mo->symtab->symoff);
  obj_strings = mo->obj_addr + mo->symtab->stroff;
  i = 0;
  while (strcmp (obj_strings + obj_symbols[i].n_un.n_strx,
		 merged_symbol->nlist.n_un.n_name) != 0)
    i++;
  if (debug_ptr)
    while (*debug_ptr < i)
      for (debug_ptr += 2; *debug_ptr & 0x80000000; debug_ptr++)
      ;
  cur_obj = mo;
  n = count_dwarf_symbols (obj_symbols + i, i, debug_ptr);
  cur_obj = cur_o;
  merged_symbol->definition_object->nlocalsym -= n;
  nlocal_symbols -= n;
}

/*
 * merge_symbols() merges the symbols from the current object (cur_obj) into
 * the merged symbol table.
 */
__private_extern__
void
merge_symbols(void)
{
    unsigned long i, j, object_undefineds, nrefsym, output_strlen;
    struct nlist *object_symbols;
    char *object_strings;
    struct merged_symbol *hash_pointer, *merged_symbol;
    enum bool discarded_coalesced_symbol;
    enum bool discarded_multiply_defined_symbol;
    unsigned short n_desc;
    size_t * debug_ptr;

#ifndef RLD
    unsigned long nest, sum, k;
    enum bool no_exclusion;
    char *stab_string, *include_file_name;
    struct localsym_block *localsym_block, *temp_localsym_block,
			  **next_localsym_block, *cur_localsym_block;
#endif

#if defined(DEBUG) || defined(RLD)
	/* The compiler "warning: `merged_symbol' may be used uninitialized */
	/* in this function" can safely be ignored */
	merged_symbol = NULL;
#endif

	/* If this object file has no symbols then just return */
	if(cur_obj->symtab == NULL)
	    return;

	/* setup pointers to the symbol table and string table */
	object_symbols = (struct nlist *)(cur_obj->obj_addr +
					  cur_obj->symtab->symoff);
	object_strings = (char *)(cur_obj->obj_addr + cur_obj->symtab->stroff);
	if(cur_obj->swapped &&
	   (((struct mach_header *)cur_obj->obj_addr)->filetype != MH_DYLIB ||
	    ((struct mach_header *)cur_obj->obj_addr)->filetype !=
	     MH_DYLIB_STUB))
	    swap_nlist(object_symbols, cur_obj->symtab->nsyms, host_byte_sex);


	/*
	 * For all the strings of the symbols to be valid the string table must
	 * end with a '\0'.
	 */
	if(cur_obj->symtab->strsize > 0 &&
	   object_strings[cur_obj->symtab->strsize - 1] != '\0'){
	    error_with_cur_obj("string table does not end with a '\\0'");
	    return;
	}

	/*
	 * If this object is not the base file count the number of undefined
	 * externals and commons in this object so that an undefined external
	 * map for this object can be allocated and then it will be filled in
	 * as these undefined external symbols are looked up in the merged
	 * symbol table.  This map will be used when doing relocation for
	 * external relocation entries in pass2 (and is not needed for the base
	 * file because that is not relocated or copied in to the output).
	 */
	object_undefineds = 0;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    check_symbol(&(object_symbols[i]), object_strings, i);
	    if(errors)
		return;
	    if((object_symbols[i].n_type & N_EXT) == N_EXT &&
	       (object_symbols[i].n_type & N_TYPE) == N_UNDF)
		object_undefineds++;
	    /*
	     * Note: coalesced symbols are always defined symbols in each object
	     * file but referenced with external relocation entries.  Since they
	     * are always defined they are not in the count of undefined
	     * symbols or in the undefined map.
	     */
#ifndef RLD
	    /*
	     * If we have an -export_symbols_list or -unexport_symbol_list
	     * option set the private extern bit on symbols that are not to
	     * be exported for global symbols that are not undefined.
	     */
	    if((object_symbols[i].n_type & N_EXT) == N_EXT &&
		object_symbols[i].n_type != (N_EXT | N_UNDF))
		exports_list_processing(object_strings +
					object_symbols[i].n_un.n_strx,
					object_symbols + i);
#endif /* !defined(RLD) */
	    /*
	     * If this is a private external defined symbol (but not a common)
	     * increment the count of private exterals for this object and the
	     * total in the output file.
	     */
	    if((object_symbols[i].n_type & N_EXT) &&
	       (object_symbols[i].n_type & N_PEXT) &&
	       (object_symbols[i].n_type & N_TYPE) != N_UNDF){
		cur_obj->nprivatesym++;
		nmerged_private_symbols++;
	    }
	}
	if(cur_obj != base_obj){
	    cur_obj->nundefineds = object_undefineds;
	    if(cur_obj->nundefineds != 0)
		cur_obj->undefined_maps = allocate(object_undefineds *
					      sizeof(struct undefined_map));
	}

#ifndef RLD
	/*
	 * If the output file type is a multi module dynamic shared library then
	 * count the number of defined externals.  And using this count, the
	 * count of undefined symbols and the count of private externs then
	 * reference map, to build the reference table, is allocated.
	 */
	if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
	    /*
	     * Note: coalesced symbols are always defined symbols in each object
	     * file but referenced with external relocation entries.  If the one
	     * in this object is discarded then cur_obj->nextdefsym is
	     * decremented and the reference_maps[].flags field is changed to
	     * some REFERENCE_FLAG_*_UNDEFINED_* value.  But note that even
	     * though cur_obj->nextdefsym is decremented cur_obj->nundefineds is
	     * NOT incremented.
	     */
	    for(i = 0; i < cur_obj->symtab->nsyms; i++){
		if((object_symbols[i].n_type & N_EXT) == N_EXT &&
		   (object_symbols[i].n_type & N_PEXT) != N_PEXT &&
		   (object_symbols[i].n_type & N_TYPE) != N_UNDF)
			cur_obj->nextdefsym++;
	    }
	    cur_obj->nrefsym = cur_obj->nundefineds + cur_obj->nextdefsym +
			       cur_obj->nprivatesym;
	    cur_obj->irefsym =
			output_dysymtab_info.dysymtab_command.nextrefsyms;
	    output_dysymtab_info.dysymtab_command.nextrefsyms +=
			cur_obj->nrefsym;
	    if(cur_obj->nrefsym != 0)
		cur_obj->reference_maps = allocate(cur_obj->nrefsym *
						  sizeof(struct reference_map));
	}
#endif /* !defined(RLD) */

	/*
	 * If local section object symbols were specified and if local symbols
	 * are to appear in the output file see if this object file has this
	 * section and if so account for this symbol.
	 */
	if(sect_object_symbols.specified &&
	   strip_level != STRIP_ALL &&
	   strip_level != STRIP_NONGLOBALS &&
	   (cur_obj != base_obj || strip_base_symbols == FALSE)){
	    if(sect_object_symbols.ms == NULL)
	        sect_object_symbols.ms = lookup_merged_section(
						  sect_object_symbols.segname,
						  sect_object_symbols.sectname);
	    if(sect_object_symbols.ms != NULL){
		if((sect_object_symbols.ms->s.flags & SECTION_TYPE) ==
			S_CSTRING_LITERALS ||
		   (sect_object_symbols.ms->s.flags & SECTION_TYPE) ==
			S_4BYTE_LITERALS ||
		   (sect_object_symbols.ms->s.flags & SECTION_TYPE) ==
			S_8BYTE_LITERALS ||
		   (sect_object_symbols.ms->s.flags & SECTION_TYPE) ==
			S_LITERAL_POINTERS){
		    warning("section (%s,%s) is a literal section "
			    "and can't be used with -sectobjectsymbols",
			    sect_object_symbols.segname,
			    sect_object_symbols.sectname);
		    sect_object_symbols.specified = FALSE;
		    sect_object_symbols.ms = NULL;
		}
		else if((sect_object_symbols.ms->s.flags & S_ATTR_DEBUG) ==
			S_ATTR_DEBUG){
		    warning("section (%s,%s) is a debug section "
			    "and can't be used with -sectobjectsymbols",
			    sect_object_symbols.segname,
			    sect_object_symbols.sectname);
		    sect_object_symbols.specified = FALSE;
		    sect_object_symbols.ms = NULL;
		}
		else{
		    /*
		     * See if this object file has the section that the section
		     * object symbols are being created for.
		     */
		    for(i = 0; i < cur_obj->nsection_maps; i++){
			if(sect_object_symbols.ms ==
			   cur_obj->section_maps[i].output_section){
			    cur_obj->nlocalsym++;
			    nlocal_symbols++;
			    if(cur_obj->ar_hdr == NULL)
				local_string_size +=
						 strlen(cur_obj->file_name) + 1;
			    else
				local_string_size += cur_obj->ar_name_size + 1;
			    break;
			}
		    }
		}
	    }
	}

	/* Allocate space for per-object-file DWARF debug map symbols.  */
	if(cur_obj->dwarf_name){
	  /* There are 3 symbols for every .o, two SOs and one OSO.  */
	  nlocal_symbols += 3;
	  cur_obj->nlocalsym += 3;
	  local_string_size += 1 + strlen (cur_obj->dwarf_name);
	  /* If there's a compilation directory, there's an extra SO.  */
	  if (cur_obj->dwarf_comp_dir) {
	    /* The compilation directory has a '/' appended.  */
	    local_string_size += 2 + strlen (cur_obj->dwarf_comp_dir);
	    nlocal_symbols++;
	    cur_obj->nlocalsym++;
	  }
	  if(cur_obj->resolved_path == NULL)
	    set_obj_resolved_path(cur_obj);
	  local_string_size += 1 + cur_obj->resolved_path_len;
	  /* Allocate space for the strings for SOL DWARF debug map symbols. */
	  for (i = 0; i < cur_obj->dwarf_num_paths; i++)
	    if (cur_obj->dwarf_paths[i])
	      local_string_size += 1 + strlen (cur_obj->dwarf_paths[i]);
	}

	/*
	 * Now merge the external symbols are looked up and merged based
	 * what was found if anything.  Locals are counted if they will
	 * appear in the output file based on the strip level.
	 */
	nrefsym = 0;
	object_undefineds = 0;
	debug_ptr = cur_obj->dwarf_source_data;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    discarded_coalesced_symbol = FALSE;
	    discarded_multiply_defined_symbol = FALSE;
	    if (debug_ptr && *debug_ptr < i) {
	      for (debug_ptr += 2; *debug_ptr & 0x80000000; debug_ptr++) ;
	    }

	    if(object_symbols[i].n_type & N_EXT){
		/*
		 * Do the trace of this symbol if specified.
		 */
		if(ntrace_syms != 0){
		    for(j = 0; j < ntrace_syms; j++){
			if(strcmp(trace_syms[j], object_strings +
				  object_symbols[i].n_un.n_strx) == 0){
			    trace_object_symbol(&(object_symbols[i]),
						object_strings);
			    break;
			}
		    }
		}
		/* lookup the symbol and see if it has already been seen */
		hash_pointer = lookup_symbol(object_strings +
					     object_symbols[i].n_un.n_strx);
		if(hash_pointer->name_len == 0){
		    /*
		     * If this is the basefile and the symbol is not a
		     * definition of a symbol (or an indirect) then don't enter
		     * this symbol into the symbol table.
		     */
		    if(cur_obj != base_obj ||
		       (object_symbols[i].n_type != (N_EXT | N_UNDF) &&
		        object_symbols[i].n_type != (N_EXT | N_INDR) ) ){
			/* the symbol has not been seen yet so just enter it */
			merged_symbol = enter_symbol(hash_pointer,
					         &(object_symbols[i]),
						 object_strings, cur_obj);
			merged_symbol->referenced_in_non_dylib = TRUE;
			if(merged_symbol->non_dylib_referenced_obj == NULL)
			    merged_symbol->non_dylib_referenced_obj = cur_obj;
		    }
		}
		/* the symbol has been seen so merge it */
		else{
		    merged_symbol = hash_pointer;
		    /*
		     * If this symbol has only been referenced by a dylib up to
		     * this point re-enter the symbol name so it is in a string
		     * block that will be in the output file and set
		     * referenced_in_non_dylib to TRUE now.
		     */
		    if(merged_symbol->referenced_in_non_dylib == FALSE){
			merged_symbol->nlist.n_un.n_name =
			    enter_string(object_strings +
					 object_symbols[i].n_un.n_strx, NULL);
			merged_symbol->referenced_in_non_dylib = TRUE;
			if(merged_symbol->non_dylib_referenced_obj == NULL)
			    merged_symbol->non_dylib_referenced_obj = cur_obj;
		    }
		    /*
		     * If the object's symbol was undefined ignore it and just
		     * use the merged symbol.
		     */
		    if(object_symbols[i].n_type == (N_EXT | N_UNDF) &&
		       object_symbols[i].n_value == 0){
			/*
			 * If the merged symbol was a lazy reference and the
			 * object's symbol is not then remove the lazy reference
			 * mark from the symbol.
			 */
			if(((merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
			     merged_symbol->nlist.n_value == 0) ||
			     merged_symbol->nlist.n_type == (N_EXT | N_PBUD)) &&
			   (merged_symbol->nlist.n_desc & REFERENCE_TYPE) ==
			    REFERENCE_FLAG_UNDEFINED_LAZY &&
			   (object_symbols[i].n_desc & REFERENCE_TYPE) !=
			    REFERENCE_FLAG_UNDEFINED_LAZY)
			    merged_symbol->nlist.n_desc =
			       (merged_symbol->nlist.n_desc & ~REFERENCE_TYPE) |
				REFERENCE_FLAG_UNDEFINED_NON_LAZY;

			/*
			 * If the undefined symbol is marked as
			 * REFERENCED_DYNAMICALLY keep this mark.
			 */
			merged_symbol->nlist.n_desc |=
			   (object_symbols[i].n_desc & REFERENCED_DYNAMICALLY);

			/*
			 * This is part of the cctools_aek-thumb-hack branch.
			 * It seems to think undefined symbols would be marked
			 * as symbols that are definitions of Thumb symbols.
			 * But since undefined symbols are not definitions I
			 * don't see how this code would ever be used.
			merged_symbol->nlist.n_desc |=
			   (object_symbols[i].n_desc & N_ARM_THUMB_DEF);
			 */

			/*
			 * If the merged symbol is also an undefined deal with
			 * weak reference mismatches if any.
			 */
			if((merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
			    merged_symbol->nlist.n_value == 0) ||
			    merged_symbol->nlist.n_type == (N_EXT | N_PBUD)){
			    /*
			     * The merged symbol may be from an dylib and we
			     * haven't yet seen any undefined symbols before
			     * this object.  If so just set the N_WEAK_REF bit
			     * in the merged symbol to be that in this object
			     * file.
			     */
			    if(merged_symbol->seen_undef == FALSE){
				merged_symbol->nlist.n_desc =
				  (merged_symbol->nlist.n_desc & ~N_WEAK_REF) |
				  (object_symbols[i].n_desc & N_WEAK_REF);
			    }
			    else
				/*
				 * We have seen an undefined symbol before so
				 * if the N_WEAK_REF bits don't match resolve it
				 * based on the -weak_reference_mismatches
				 * setting.
				 */
			    if(((merged_symbol->nlist.n_desc & N_WEAK_REF) ==
				 N_WEAK_REF &&
				(object_symbols[i].n_desc & N_WEAK_REF) !=
				 N_WEAK_REF) ||
			       ((merged_symbol->nlist.n_desc & N_WEAK_REF) !=
				 N_WEAK_REF &&
				(object_symbols[i].n_desc & N_WEAK_REF) ==
				 N_WEAK_REF)){
				if(weak_reference_mismatches ==
				   WEAK_REFS_MISMATCH_ERROR)
				    merged_symbol->weak_reference_mismatch =
					TRUE;
				else if(weak_reference_mismatches ==
					WEAK_REFS_MISMATCH_WEAK)
				    merged_symbol->nlist.n_desc |= N_WEAK_REF;
				else if(weak_reference_mismatches ==
					WEAK_REFS_MISMATCH_NON_WEAK)
				    merged_symbol->nlist.n_desc &=
					~(N_WEAK_REF);
			    }
			    merged_symbol->seen_undef = TRUE;
			}
		    }
		    /*
		     * See if the object's symbol is a common.
		     */
	    	    else if((object_symbols[i].n_type & N_EXT) == N_EXT &&
	       		    (object_symbols[i].n_type & N_TYPE) == N_UNDF &&
			    object_symbols[i].n_value != 0){
			/*
			 * See if the merged symbol is a common or undefined.
			 */
			if((merged_symbol->nlist.n_type & N_EXT) == N_EXT &&
		           (merged_symbol->nlist.n_type & N_TYPE) ==  N_UNDF){
			    /*
			     * If the merged symbol is a common use the larger
			     * of the two commons.  Else the merged symbol is
			     * a common so use the common symbol.
			     */
			    if(merged_symbol->nlist.n_value != 0){
				if((merged_symbol->nlist.n_type & N_PEXT) !=
				   (object_symbols[i].n_type & N_PEXT)){
				    warning("common symbol: %s both as an "
					"external symbol and a private "
					"external symbol", merged_symbol->
					nlist.n_un.n_name);
				    trace_merged_symbol(merged_symbol);
				    trace_object_symbol(&(object_symbols[i]),
							object_strings);
				}
				if(object_symbols[i].n_value >
				   merged_symbol->nlist.n_value){
				    merged_symbol->nlist.n_value =
						     object_symbols[i].n_value;
				    merged_symbol->definition_object = cur_obj;
				    /*
				     * Since we are "using" this common then
				     * "use" the private extern bit from this
				     * object's symbol for the merged symbol.
				     */
				    merged_symbol->nlist.n_type =
				      (merged_symbol->nlist.n_type & ~N_PEXT) |
				      (object_symbols[i].n_type & N_PEXT);
				}
			    }
			    else{
				merged_symbol->nlist.n_value =
						     object_symbols[i].n_value;
				merged_symbol->definition_object = cur_obj;
			    }
			}
			/*
			 * The merged symbol is not a common or undefined and
			 * the object symbol is a common so just ignore the
			 * object's common symbol and use the merged defined
			 * symbol.
			 */
		    }
		    /*
		     * If the merged symbol is undefined or common (and at this
		     * point the object's symbol is known not to be undefined
		     * or common) then use the object's symbol.
		     */
		    else if((merged_symbol->nlist.n_type & N_TYPE) == N_UNDF){
			/* one could also say:
			 *  && merged_symbol->nlist.n_value == 0 &&
			 *     merged_symbol->nlist.n_value != 0
			 * if the above test but that is always true.
			 */
			merged_symbol->nlist.n_type = object_symbols[i].n_type;
			merged_symbol->nlist.n_sect = object_symbols[i].n_sect;
			n_desc = 0;
			/*
			 * If this symbol was previously referenced dynamically
			 * then keep this information.
			 */
			n_desc |= (merged_symbol->nlist.n_desc &
				   REFERENCED_DYNAMICALLY);
			/*
			 * If the object symbol is a symbol defined as Thumb
			 * symbol then keep this information.
			 */
			n_desc |= (object_symbols[i].n_desc & N_ARM_THUMB_DEF);

			/*
			 * If the object symbol is a weak definition it may be
			 * later discarded for a non-weak symbol from a dylib so
			 * if the undefined symbol is a weak reference keep that
			 * information.
			 */
			if((object_symbols[i].n_desc & N_WEAK_DEF) ==
			   N_WEAK_DEF)
			    n_desc |= (merged_symbol->nlist.n_desc &
				       N_WEAK_REF);
			merged_symbol->nlist.n_desc =
			    object_symbols[i].n_desc | n_desc;
			if(merged_symbol->nlist.n_type == (N_EXT | N_INDR))
			    enter_indr_symbol(merged_symbol,
					      &(object_symbols[i]),
					      object_strings, cur_obj);
			else
			    merged_symbol->nlist.n_value =
						      object_symbols[i].n_value;
			merged_symbol->definition_object = cur_obj;
		    }
		    /*
		     * If the object symbol is a weak definition then
		     * it is discarded and the merged symbol is kept,
		     * unless the merged symbol is a weak symbol in a
		     * dylib.  Note currently only symbols in
		     * coalesced sections can have this set and it is
		     * checked for in check_symbol() so it is assumed
		     * it is a coalesced symbol here.
		     */
		    else if((object_symbols[i].n_desc & N_WEAK_DEF) ==
			    N_WEAK_DEF &&
			    (merged_symbol->defined_in_dylib == FALSE ||
			     ! merged_symbol->weak_def_in_dylib)){
			discarded_coalesced_symbol = TRUE;
			if((object_symbols[i].n_type & N_EXT) &&
			   (object_symbols[i].n_type & N_PEXT)){
			    cur_obj->nprivatesym--;
			    nmerged_private_symbols--;
			}
			else{
			    cur_obj->nextdefsym--;
			}
		    }
		    /*
		     * Otherwise, if the merged symbol is a weak
		     * definition then it is discarded and the object
		     * symbol is used.
		     */
		    else if((merged_symbol->nlist.n_desc & N_WEAK_DEF) ==
			    N_WEAK_DEF ||
			    (merged_symbol->defined_in_dylib == TRUE &&
			     merged_symbol->weak_def_in_dylib)){
			if(merged_symbol->defined_in_dylib == FALSE){
			    if((merged_symbol->nlist.n_type & N_EXT) &&
			       (merged_symbol->nlist.n_type & N_PEXT)){
				merged_symbol->definition_object->nprivatesym--;
				nmerged_private_symbols--;
			    }
			    else{
				merged_symbol->definition_object->nextdefsym--;
			    }
			    maybe_remove_dwarf_symbol (merged_symbol);
			}
#ifndef RLD
			/*
			 * If the output file is a multi module MH_DYLIB type
			 * reset the reference map for the merged external
			 * symbol that is being discarded.
			 */
			if(filetype == MH_DYLIB &&
			   multi_module_dylib == TRUE &&
			   merged_symbol->defined_in_dylib == FALSE){
			    /*
			     * Discared coalesced symbols are referenced as
			     * undefined. TODO: to determine if the reference is
			     * lazy or non-lazy we would have to look at all the
			     * relocation entries in this object.  For now just
			     * assume non-lazy to be safe.
			     */
			    for(j = 0;
				j < merged_symbol->definition_object->nrefsym;
				j++){
				if(merged_symbol->definition_object->
			           reference_maps[j].merged_symbol ==
								merged_symbol){
				    if(object_symbols[i].n_type & N_PEXT)
					merged_symbol->definition_object->
					reference_maps[j].flags =
				      REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY;
				    else
					merged_symbol->definition_object->
					reference_maps[j].flags =
					    REFERENCE_FLAG_UNDEFINED_NON_LAZY;
				    break;
				}
			    }
			}
#endif /* RLD */
			merged_symbol->defined_in_dylib = FALSE;
			merged_symbol->coalesced_defined_in_dylib = FALSE;
			merged_symbol->weak_def_in_dylib = FALSE;
			merged_symbol->nlist.n_type = object_symbols[i].n_type;
			merged_symbol->nlist.n_sect = object_symbols[i].n_sect;
			/*
			 * If this symbol was previously referenced
			 * dynamically then keep this information.
			 */
			if(merged_symbol->nlist.n_desc &
			   REFERENCED_DYNAMICALLY)
			    merged_symbol->nlist.n_desc =
				object_symbols[i].n_desc |
				REFERENCED_DYNAMICALLY
				/*
				 * This was part of the cctools_aek-thumb-hack
				 * branch.  It seems to think if the discarded
				 * weak merged symbol was marked as Thumb
				 * definition then that should be preserved.
				 * But since the object symbol is being used
				 * instead it may not be a Thumb definition.
				| (merged_symbol->nlist.n_desc &
				   N_ARM_THUMB_DEF)
				*/
				;
			else
			    merged_symbol->nlist.n_desc =
				object_symbols[i].n_desc
				/*
				 * This was part of the cctools_aek-thumb-hack
				 * branch.  It seems to think if the discarded
				 * weak merged symbol was marked as Thumb
				 * definition then that should be preserved.
				 * But since the object symbol is being used
				 * instead it may not be a Thumb definition.
				| (merged_symbol->nlist.n_desc &
				   N_ARM_THUMB_DEF)
				*/
				;
			if(merged_symbol->nlist.n_type == (N_EXT | N_INDR))
			    enter_indr_symbol(merged_symbol,
					      &(object_symbols[i]),
					      object_strings, cur_obj);
			else
			    merged_symbol->nlist.n_value =
						      object_symbols[i].n_value;
			merged_symbol->definition_object = cur_obj;
		    }
		    /*
		     * If both symbols are coalesced symbols then the this
		     * symbol is discarded.
		     */
		    else if((
		       ((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
		       ((merged_symbol->definition_object->section_maps[
			    merged_symbol->nlist.n_sect - 1].s->flags) &
			    SECTION_TYPE) == S_COALESCED) ||
			(merged_symbol->defined_in_dylib == TRUE &&
 			 merged_symbol->coalesced_defined_in_dylib) )
			&&
		       (object_symbols[i].n_type & N_TYPE) == N_SECT &&
		       ((cur_obj->section_maps[object_symbols[i].n_sect - 1].
			    s->flags) & SECTION_TYPE) == S_COALESCED){

			discarded_coalesced_symbol = TRUE;
			if((object_symbols[i].n_type & N_EXT) &&
			   (object_symbols[i].n_type & N_PEXT)){
			    cur_obj->nprivatesym--;
			    nmerged_private_symbols--;
			}
			else{
			    cur_obj->nextdefsym--;
			}
#ifdef COALESCE_DEBUG
printf("symbol: %s is coalesced\n", merged_symbol->nlist.n_un.n_name);
#endif
		    }
#ifdef KLD
                  /*
                   * For KLD if both symbols are absolute symbols with the
                   * value the symbol is discarded.
                   */
                  else if((merged_symbol->nlist.n_type & N_TYPE) == N_ABS &&
                          (object_symbols[i].n_type & N_TYPE) == N_ABS &&
                          merged_symbol->nlist.n_value ==
                          object_symbols[i].n_value){
                      if((object_symbols[i].n_type & N_EXT) &&
                         (object_symbols[i].n_type & N_PEXT)){
                          cur_obj->nprivatesym--;
                          nmerged_private_symbols--;
                      }
                      else{
                          cur_obj->nextdefsym--;
                      }
                  }
#endif /* KLD */
		    else{
			discarded_multiply_defined_symbol = TRUE;
			multiply_defined(merged_symbol, &(object_symbols[i]),
					 object_strings);
			if(allow_multiply_defined_symbols == TRUE){
			    /*
			     * If this is a private external then decrement
			     * the previous incremented the count of private
			     * exterals for this object and the total in the
			     * output file since we are going to ignore this
			     * this multiply defined symbol.
			     */
			    if((object_symbols[i].n_type & N_EXT) &&
			       (object_symbols[i].n_type & N_PEXT)){
				cur_obj->nprivatesym--;
				nmerged_private_symbols--;
			    }
			}
		    }
		}
		/*
		 * If this symbol was undefined or a common in this object
		 * and the object is not the basefile enter a pointer to the
		 * merged symbol and its index in the object file's undefined
		 * map.
		 */
		if((object_symbols[i].n_type & N_EXT) == N_EXT &&
		   (object_symbols[i].n_type & N_TYPE) == N_UNDF &&
		   cur_obj != base_obj){
		    cur_obj->undefined_maps[object_undefineds].index = i;
		    cur_obj->undefined_maps[object_undefineds].merged_symbol =
								merged_symbol;
		    object_undefineds++;
		    /*
		     * Note: coalesced symbols are always defined symbols in
		     * each object file but referenced with external relocation
		     * entries.  Since they are always defined they are not in
		     * in the undefined map.
		     */
		}
#ifndef RLD
		/*
		 * If the output file is a multi module MH_DYLIB type set the
		 * reference map for this external symbol.
		 */
		if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		    cur_obj->reference_maps[nrefsym].merged_symbol =
								merged_symbol;
		    /*
		     * Discared coalesced symbols are referenced as undefined.
		     * TODO: to determine if the reference is lazy or non-lazy
		     * we would have to look at all the relocation entries in
		     * this object.  For now just assume non-lazy to be safe.
		     */
		    if(discarded_coalesced_symbol == TRUE){
			if(merged_symbol->nlist.n_type & N_PEXT)
			    cur_obj->reference_maps[nrefsym].flags =
				REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY;
			else
			    cur_obj->reference_maps[nrefsym].flags =
				REFERENCE_FLAG_UNDEFINED_NON_LAZY;
		    }
		    else if(object_symbols[i].n_type == (N_EXT | N_UNDF))
			cur_obj->reference_maps[nrefsym].flags =
				      object_symbols[i].n_desc & REFERENCE_TYPE;
		    else if(object_symbols[i].n_type & N_PEXT)
			cur_obj->reference_maps[nrefsym].flags =
						 REFERENCE_FLAG_PRIVATE_DEFINED;
		    else
			cur_obj->reference_maps[nrefsym].flags =
							 REFERENCE_FLAG_DEFINED;
		    nrefsym++;
		}
#endif /* !defined(RLD) */
	    }
	    else if(cur_obj != base_obj || strip_base_symbols == FALSE){
#ifndef RLD
		if(dead_strip &&
		   (object_symbols[i].n_type & N_STAB) == 0 &&
		   (object_symbols[i].n_type & N_TYPE) == N_SECT &&
		   object_symbols[i].n_desc & N_NO_DEAD_STRIP)
		    local_NO_DEAD_STRIP_symbols = TRUE;
#endif /* !defined(RLD) */

		if(strip_level != STRIP_DUP_INCLS &&
		   is_output_local_symbol(object_symbols[i].n_type,
			object_symbols[i].n_sect, object_symbols[i].n_desc,
			cur_obj,
			object_symbols[i].n_un.n_strx == 0 ? "" :
			object_strings + object_symbols[i].n_un.n_strx,
			&output_strlen)){
		    cur_obj->nlocalsym++;
		    nlocal_symbols++;
		    local_string_size += object_symbols[i].n_un.n_strx == 0 ? 0:
					 output_strlen + 1;
		}
	    }

	    /* If this file had DWARF, account for the extra debug_map
	       symbols.  */
	    if (cur_obj->dwarf_name && ! discarded_coalesced_symbol &&
		! discarded_multiply_defined_symbol){
	      size_t n = count_dwarf_symbols (object_symbols + i, i,
					      debug_ptr);
	      cur_obj->nlocalsym += n;
	      nlocal_symbols += n;
	    }
	}

#ifndef RLD
	/*
	 * If we are stripping STABS from duplicate includes then go through the
	 * symbol table determining which local symbols (STABS and non-stabs)
	 * which are to be in the output file.
	 *
	 * The stabs for each N_BINCL/N_EINCL are parsed out as a group.  Since
	 * there can be intermixed nested groups the parsing is a bit strange
	 * as we create blocks for the symbols that have been parsed out and
	 * then restart parsing at the inter nesting level.  This allows outer
	 * groups to be excluded when inter groups can't.  The blocks must be
	 * put and kept on the list in order of their symbol table index.
	 */
	if(strip_level == STRIP_DUP_INCLS){
	    localsym_block = cur_obj->localsym_blocks;
	    next_localsym_block = &(cur_obj->localsym_blocks);
	    for(i = 0; i < cur_obj->symtab->nsyms; i++){
		/* skip blocks of symbols that have already been parsed */
		if(localsym_block != NULL && localsym_block->index == i){
		    i += localsym_block->count - 1; /* the loop will do i++ */
		    next_localsym_block = &(localsym_block->next);
		    localsym_block = localsym_block->next;
		    continue;
		}
		if(object_symbols[i].n_type & N_EXT)
		    continue;
		if((object_symbols[i].n_type & N_STAB) == 0 ||
		   object_symbols[i].n_type != N_BINCL){
		    cur_obj->nlocalsym++;
		    nlocal_symbols++;
		    /*
		     * Even though strip_level is STRIP_DUP_INCLS and we know
		     * we are keeping this symbol, it might be an N_OSO which
		     * we maybe changing the name of so is_output_local_symbol()
		     * is still called to get the output_strlen.
		     */
		    (void)is_output_local_symbol(object_symbols[i].n_type,
			object_symbols[i].n_sect, object_symbols[i].n_desc,
			cur_obj,
			object_symbols[i].n_un.n_strx == 0 ? "" :
			object_strings + object_symbols[i].n_un.n_strx,
			&output_strlen);
		    local_string_size +=
			object_symbols[i].n_un.n_strx == 0 ? 0:
			output_strlen + 1;
		    continue;
		}
		/*
		 * We now have a N_BINCL stab.  We will now see if we can
		 * exclude this stab through its closing N_EINCL stab.
		 * To exclude this group it must not have any non-stabs in it
		 * and must not have any stabs that need relocation (stabs for
		 * definitions of symbols in header files, N_FUN, N_SLINE, etc).
		 *
		 * An N_BINCL symbol indicates the start of the stabs entries
		 * for a header file.  We need to scan ahead to the next N_EINCL
		 * symbol, ignoring nesting, adding up all the characters in the
		 * symbol names, not including the file numbers in types (the
		 * first number after an open parenthesis).
		 */
		no_exclusion = FALSE;
		nest = 0;
		sum = 0;
		/*
		 * Create the first block for this bincl,
		 * then after parsing out the incl's stabs,
		 * the outer loop will start again just after this block
		 */
		localsym_block = allocate(sizeof(struct localsym_block));
		memset(localsym_block, '\0', sizeof(struct localsym_block));
		localsym_block->index = i;
		localsym_block->state = PARSE_SYMBOLS;
		localsym_block->count = 1;
		localsym_block->input_N_BINCL_n_value =
		    object_symbols[i].n_value;
		if(localsym_block->input_N_BINCL_n_value != 0)
		    sum = localsym_block->input_N_BINCL_n_value;

		/* insert the first block in the list */
		localsym_block->next = *next_localsym_block;
		*next_localsym_block = localsym_block;
		next_localsym_block = &(localsym_block->next);

		/*
		 * The current block on the chain for the group starts out
		 * as the first block.
		 */
		cur_localsym_block = localsym_block;

		for(j = i + 1; j < cur_obj->symtab->nsyms; j++){
		    if(object_symbols[j].n_type == N_EINCL){
			if(nest == 0){
			    /* count this symbol as the part of this block */
			    cur_localsym_block->count++;
			    break;
			}
			else{
			    nest--;
			    if(nest == 0){
				/*
				 * If we are going back to nest level zero
				 * we can now set the index to where the
				 * current block starts.
				 */
				cur_localsym_block->index = j + 1;
			    }
			}
		    }
		    else if(object_symbols[j].n_type == N_BINCL ||
			    object_symbols[j].n_type == N_EXCL){
			nest++;
			/*
			 * End the current block and create a new one if we
			 * haven't already.  We don't know the index yet, but
			 * we do know we need a new block as we are nesting
			 * down and expect to come back.
			 */
			if(cur_localsym_block->count != 0){
			    temp_localsym_block = allocate(
					    sizeof(struct localsym_block));
			    memset(temp_localsym_block, '\0',
					    sizeof(struct localsym_block));
			    temp_localsym_block->state = PARSE_SYMBOLS;

			    /* insert it after the current block */
			    temp_localsym_block->next =
				cur_localsym_block->next;
			    cur_localsym_block->next = temp_localsym_block;

			    /* now make it the current block */
			    cur_localsym_block = temp_localsym_block;
			}
			if(object_symbols[j].n_type == N_EXCL){
			    nest--;
			    if(nest == 0){
				/*
				 * If we are going back to nest level zero
				 * we can now set the index to where the
				 * current block starts.
				 */
				cur_localsym_block->index = j + 1;
			    }
			}
		    }
		    else if(nest == 0){
			if((object_symbols[j].n_type & N_STAB) == 0 ||
			   object_symbols[j].n_sect != NO_SECT){
			    no_exclusion = TRUE;
			}
			/*
			 * If this is a local symbol count it as the part of
			 * the current block.
			 */
			if((object_symbols[j].n_type & N_STAB) != 0){
			    cur_localsym_block->count++;

			    if(localsym_block->input_N_BINCL_n_value == 0 &&
			       object_symbols[j].n_un.n_strx != 0){
				stab_string = object_strings +
					      object_symbols[j].n_un.n_strx;
				for( ; *stab_string != '\0'; stab_string++){
				    sum += *stab_string;
				    if(*stab_string == '('){
					/* skip the file number */
					stab_string++;
					while(isdigit((unsigned char)
						      *stab_string))
					    stab_string++;
					stab_string--;
				    }
				    else if(*stab_string == '.' &&
				       stab_string[1] != '\0' &&
				       stab_string[1] == '_'){
					stab_string++; /* one for the '.' */
					sum += *stab_string;
					stab_string++; /* and one for the '_' */
					while(isdigit((unsigned char)
						      *stab_string))
					    stab_string++;
					stab_string--;
				    }
				}
			    }
			}
		    }
		}
		/*
		 * If we did not succesfully parsed a N_BINCL/N_EINCL pair or
		 * the group has symbols that can't be excluded, then just add
		 * these symbols to the count of local symbols and the sizes of
		 * the strings in this group.  Leave the blocks that were
		 * created in the PARSE_SYMBOLS state so they won't be looked
		 * at again and the symbols won't be removed.
		 */
		if(j == cur_obj->symtab->nsyms || no_exclusion == TRUE){
		    temp_localsym_block = localsym_block;
		    while(temp_localsym_block != NULL){
			cur_obj->nlocalsym += temp_localsym_block->count;
			nlocal_symbols += temp_localsym_block->count;
			for(k = temp_localsym_block->index;
			    k < temp_localsym_block->index +
				temp_localsym_block->count;
			    k++){
			    /*
			     * Even though strip_level is STRIP_DUP_INCLS and
			     * we know we are keeping this symbol, it might be
			     * an N_OSO which we maybe changing the name of.
			     * So is_output_local_symbol() is still called to
			     * get the output_strlen.
			     */
			    (void)is_output_local_symbol(
				object_symbols[k].n_type,
				object_symbols[k].n_sect,
				object_symbols[k].n_desc,
				cur_obj,
				object_symbols[k].n_un.n_strx == 0 ? "" :
				object_strings + object_symbols[k].n_un.n_strx,
				&output_strlen);
			    local_string_size +=
				object_symbols[k].n_un.n_strx == 0 ? 0:
				output_strlen + 1;
			}
			if(temp_localsym_block == cur_localsym_block)
			    break;
			else
			    temp_localsym_block = temp_localsym_block->next;
		    }
		    i = i + localsym_block->count
			- 1; /* the loop will do i++ */
		    localsym_block = localsym_block->next;
		}
		else{
		    /*
		     * We succesfully parsed out a set of stabs between a
		     * N_BINCL/N_EINCL pair that now can be considered for
		     * exclusion if we have seen the same include file with
		     * the same sum of its stab strings without file numbers.
		     * lookup_and_enter_include() will return TRUE if this is
		     * new and we have not seen this group before.
		     */
		    include_file_name = object_strings +
					object_symbols[i].n_un.n_strx;
		    if(lookup_and_enter_include(include_file_name, sum, i,
					object_symbols[i+1].n_type == N_EINCL)){
			/*
			 * This is the first time this group is seen, so count
			 * the symbols in the blocks of this include file as to
			 * be in the output (all known to be local symbols) and
			 * add up the sizes of their strings.
			 */
			temp_localsym_block = localsym_block;
			while(temp_localsym_block != NULL){
			    cur_obj->nlocalsym += temp_localsym_block->count;
			    nlocal_symbols += temp_localsym_block->count;
			    for(k = temp_localsym_block->index;
				k < temp_localsym_block->index +
				    temp_localsym_block->count;
				k++){
				/*
				 * Even though strip_level is STRIP_DUP_INCLS
				 * and we know we are keeping this symbol, it
				 * might be an N_OSO which we maybe changing
				 * the name of. So is_output_local_symbol() is
				 * still called to get the output_strlen.
				 */
				(void)is_output_local_symbol(
				    object_symbols[k].n_type,
				    object_symbols[k].n_sect,
				    object_symbols[k].n_desc,
				    cur_obj,
				    object_symbols[k].n_un.n_strx == 0 ? "" :
				    object_strings +
				        object_symbols[k].n_un.n_strx,
				    &output_strlen);
				local_string_size +=
				    object_symbols[k].n_un.n_strx == 0 ? 0:
				    output_strlen + 1;
			    }
			    if(temp_localsym_block == cur_localsym_block)
				break;
			    else
				temp_localsym_block = temp_localsym_block->next;
			}
			/*
			 * The sum for the N_BINCL needs to be set so use the
			 * the first block for this bincl group for this,
			 * resetting its count to 1 after resetting the outer
			 * loop to start after the original size of the block.
			 * The other blocks for this bincl group continue to
			 * have their state set to PARSE_SYMBOLS and will be
			 * removed from the list after all symbols are parsed.
			 * Then the symbols from this include will be in the
			 * output.
			 */
			localsym_block->state = BEGIN_INCLUDE;
			localsym_block->sum = sum;
			i = i + localsym_block->count
			    - 1; /* the loop will do i++ */
			localsym_block->count = 1;
			localsym_block = localsym_block->next;
		    }
		    else{
			/*
			 * This group of stabs has been seen before so it will
			 * be excluded from the output.  Use the the first
			 * block for this bincl group for this marking it as
			 * EXCLUDED_INCLUDE, then set the other blocks in this
			 * group to DISCARD. Then account for the one N_EXCL
			 * stab and it's sting.  Finally reset the outer loop to
			 * start after the first block.
			 */
			localsym_block->state = EXCLUDED_INCLUDE;
			localsym_block->sum = sum;
			if(localsym_block != cur_localsym_block){
			    temp_localsym_block = localsym_block->next;
			    while(temp_localsym_block != NULL){
				temp_localsym_block->state = DISCARD_SYMBOLS;
				if(temp_localsym_block == cur_localsym_block)
				    break;
				else
				    temp_localsym_block =
					temp_localsym_block->next;
			    }
			}

			/* account for the one N_EXCL replacing this group */
			cur_obj->nlocalsym += 1;
			nlocal_symbols += 1;
			/*
			   The path string for an EXCL always re-uses
			   the path from the matching BINCL
			  local_string_size += strlen(include_file_name) + 1;
			 */
			i = i + localsym_block->count - 1;
				/* the loop will do i++ */
			/*
			 * Note the count field of an EXCLUDED_INCLUDE block
			 * contains the #of symbols to that were replaced with
			 * the N_EINCL not a count of 1. So the count is not
			 * changed.
			 */
			localsym_block = localsym_block->next;
		    }
		}
	    }
	    /*
	     * Go through the list of blocks and remove any blocks that were
	     * just needed for parsing.
	     */
	    localsym_block = cur_obj->localsym_blocks;
	    next_localsym_block = &(cur_obj->localsym_blocks);
	    while(localsym_block != NULL){
		if(localsym_block->state == PARSE_SYMBOLS){
		    temp_localsym_block = localsym_block;
		    localsym_block = localsym_block->next;
		    *next_localsym_block = localsym_block;
		    free(temp_localsym_block);
		}
		else{
		    next_localsym_block = &(localsym_block->next);
		    localsym_block = localsym_block->next;
		}
	    }
	}
#endif /* !defined(RLD) */

}

#ifndef RLD
/*
 * exports_list_processing() takes a symbol_name and a defined symbol from an
 * object file and sets the private extern bit is it is not to be exported.  And
 * also marks the symbol in the list as seen.
 */
static
void
exports_list_processing(
char *symbol_name,
struct nlist *symbol)
{
    struct symbol_list *sp;

	if(save_symbols != NULL){
	    sp = bsearch(symbol_name, save_symbols, nsave_symbols,
			 sizeof(struct symbol_list),
			 (int (*)(const void *, const void *))
			    symbol_list_bsearch);
	    if(sp != NULL){
		sp->seen = TRUE;
	    }
	    else{
		if(symbol->n_desc & REFERENCED_DYNAMICALLY){
		    warning("symbol: %s referenced dynamically and must be "
			    "exported", symbol_name);
		}
		else{
		    symbol->n_type |= N_PEXT;
		}
	    }
	}
	if(remove_symbols != NULL){
	    sp = bsearch(symbol_name, remove_symbols, nremove_symbols,
			 sizeof(struct symbol_list),
			 (int (*)(const void *, const void *))
			    symbol_list_bsearch);
	    if(sp != NULL){
		sp->seen = TRUE;
		if(symbol->n_desc & REFERENCED_DYNAMICALLY){
		    warning("symbol: %s referenced dynamically and must be "
			    "exported", symbol_name);
		}
		else{
		    symbol->n_type |= N_PEXT;
		}
	    }
	}
}

/*
 * command_line_symbol() looks up a symbol name that comes from a command line
 * argument (like -u symbol_name) and returns a pointer to the merged symbol
 * table entry for it.  If the symbol doesn't exist it enters an undefined
 * symbol for it.
 */
__private_extern__
struct merged_symbol *
command_line_symbol(
char *symbol_name)
{
    unsigned long i;
    struct merged_symbol *hash_pointer, *merged_symbol;
    struct object_file *command_line_object;

	command_line_object = new_object_file();
	command_line_object->file_name = "command line";
	command_line_object->command_line = TRUE;
	/*
	 * Do the trace of this symbol if specified.
	 */
	if(ntrace_syms != 0){
	    for(i = 0; i < ntrace_syms; i++){
		if(strcmp(trace_syms[i], symbol_name) == 0){
		    trace_symbol(symbol_name, &(undefined_symbol),
			     command_line_object, "error in trace_symbol()");
		    break;
		}
	    }
	}
	/* lookup the symbol and see if it has already been seen */
	hash_pointer = lookup_symbol(symbol_name);
	if(hash_pointer->name_len == 0){
	    /*
	     * The symbol has not been seen yet so just enter it as an
	     * undefined symbol and it will be returned.
	     */
	    merged_symbol = enter_symbol(hash_pointer, &(undefined_symbol),
					 symbol_name, command_line_object);
	    if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		command_line_object->reference_maps =
		    reallocate(command_line_object->reference_maps,
			       (command_line_object->nrefsym + 1) *
			       sizeof(struct reference_map));
		command_line_object->reference_maps[
		    command_line_object->nrefsym].flags =
			REFERENCE_FLAG_UNDEFINED_NON_LAZY;
		command_line_object->reference_maps[
		    command_line_object->nrefsym].merged_symbol =
			merged_symbol;
		command_line_object->irefsym =
		    output_dysymtab_info.dysymtab_command.nextrefsyms;
		command_line_object->nrefsym += 1;
		output_dysymtab_info.dysymtab_command.nextrefsyms += 1;
	    }
	}
	/* the symbol has been seen so just use it */
	else{
	    merged_symbol = hash_pointer;
	    /*
	     * If this symbol has only been referenced by a dylib up to
	     * this point re-enter the symbol name so it is in a string
	     * block that will be in the output file.
	     */
	    if(merged_symbol->referenced_in_non_dylib == FALSE)
		merged_symbol->nlist.n_un.n_name = enter_string(symbol_name,
							        NULL);
	}
	merged_symbol->referenced_in_non_dylib = TRUE;
	if(merged_symbol->non_dylib_referenced_obj == NULL)
	    merged_symbol->non_dylib_referenced_obj = command_line_object;
	return(merged_symbol);
}

/*
 * command_line_indr_symbol() creates an indirect symbol for symbol_name to
 * indr_symbol_name.  It is used for -i command line options.  Since this is
 * a defining symbol the problems of multiply defined symbols can happen.  This
 * and the tracing is not too neat as far as the code goes but it does exactly
 * what is intended.  That is exactly one error message for each symbol and
 * exactly one trace for each object or command line option for each symbol.
 */
__private_extern__
void
command_line_indr_symbol(
char *symbol_name,
char *indr_symbol_name)
{
    unsigned long i, j;
    enum bool was_traced;
    struct merged_symbol *hash_pointer, *merged_symbol, *merged_indr_symbol;
    struct object_file *command_line_object;

	command_line_object = new_object_file();
	command_line_object->file_name = "command line";
	command_line_object->command_line = TRUE;
	/*
	 * Do the trace of the symbol_name if specified.
	 */
	was_traced = FALSE;
	if(ntrace_syms != 0){
	    for(i = 0; i < ntrace_syms; i++){
		if(strcmp(trace_syms[i], symbol_name) == 0){
		    trace_symbol(symbol_name, &(indr_symbol),
				 command_line_object, indr_symbol_name);
		    was_traced = TRUE;
		    break;
		}
	    }
	}
	/* lookup the symbol_name and see if it has already been seen */
	hash_pointer = lookup_symbol(symbol_name);
	if(hash_pointer->name_len == 0){
	    /*
	     * The symbol has not been seen yet so just enter it as an
	     * undefined and it will be changed to a proper merged indirect
	     * symbol.
	     */
	    merged_symbol = enter_symbol(hash_pointer, &(undefined_symbol),
					 symbol_name, command_line_object);
	    merged_symbol->referenced_in_non_dylib = TRUE;
	    if(merged_symbol->non_dylib_referenced_obj == NULL)
		merged_symbol->non_dylib_referenced_obj = command_line_object;
	}
	else{
	    /*
	     * The symbol exist.  So if the symbol is anything but a common or
	     * undefined then it is multiply defined.
	     */
	    merged_symbol = hash_pointer;
	    /*
	     * If this symbol has only been referenced by a dylib up to
	     * this point re-enter the symbol name so it is in a string
	     * block that will be in the output file.
	     */
	    if(merged_symbol->referenced_in_non_dylib == FALSE)
		merged_symbol->nlist.n_un.n_name = enter_string(symbol_name,
								NULL);
	    merged_symbol->referenced_in_non_dylib = TRUE;
	    if(merged_symbol->non_dylib_referenced_obj == NULL)
		merged_symbol->non_dylib_referenced_obj = command_line_object;
	    if((merged_symbol->nlist.n_type & N_TYPE) != N_UNDF){
		/*
		 * It is multiply defined so the logic of the routine
		 * multiply_defined() is copied here so that tracing a symbol
		 * from the command line can be done.
		 */
		for(i = 0; i < nmultiple_defs; i++){
		    if(strcmp(multiple_defs[i],
			      merged_symbol->nlist.n_un.n_name) == 0)
			break;
		}
		for(j = 0; j < ntrace_syms; j++){
		    if(strcmp(trace_syms[j],
			      merged_symbol->nlist.n_un.n_name) == 0)
			break;
		}
		if(i == nmultiple_defs){
		    if(allow_multiply_defined_symbols == TRUE)
			warning("multiple definitions of symbol %s",
			      merged_symbol->nlist.n_un.n_name);
		    else
			error("multiple definitions of symbol %s",
			      merged_symbol->nlist.n_un.n_name);
		    multiple_defs = reallocate(multiple_defs, (nmultiple_defs +
					       1) * sizeof(char *));
		    multiple_defs[nmultiple_defs++] =
					       merged_symbol->nlist.n_un.n_name;
		    if(j == ntrace_syms)
			trace_merged_symbol(merged_symbol);
		}
		if(was_traced == FALSE)
		    trace_symbol(symbol_name, &(indr_symbol),
				 command_line_object, indr_symbol_name);
		return;
	    }
	}
	nindr_symbols++;
	/* Now change this symbol to an indirect symbol type */
	merged_symbol->nlist.n_type = N_INDR | N_EXT;
	merged_symbol->nlist.n_sect = NO_SECT;
	merged_symbol->nlist.n_desc = 0;

	/* lookup the indr_symbol_name and see if it has already been seen */
	hash_pointer = lookup_symbol(indr_symbol_name);
	if(hash_pointer->name_len == 0){
	    /*
	     * The symbol has not been seen yet so just enter it after tracing
	     * if the symbol is specified.
	     */
	    for(i = 0; i < ntrace_syms; i++){
		if(strcmp(trace_syms[i], indr_symbol_name) == 0){
		    trace_symbol(indr_symbol_name, &(undefined_symbol),
			     command_line_object, "error in trace_symbol()");
		    break;
		}
	    }
	    merged_indr_symbol = enter_symbol(hash_pointer, &(undefined_symbol),
				      indr_symbol_name, command_line_object);
	    merged_indr_symbol->referenced_in_non_dylib = TRUE;
	    if(merged_indr_symbol->non_dylib_referenced_obj == NULL)
		merged_indr_symbol->non_dylib_referenced_obj =
							    command_line_object;
	}
	else{
	    merged_indr_symbol = hash_pointer;
	    /*
	     * If this symbol has only been referenced by a dylib up to
	     * this point re-enter the symbol name so it is in a string
	     * block that will be in the output file.
	     */
	    if(merged_indr_symbol->referenced_in_non_dylib == FALSE)
		merged_indr_symbol->nlist.n_un.n_name =
		    enter_string(indr_symbol_name, NULL);
	    merged_indr_symbol->referenced_in_non_dylib = TRUE;
	    if(merged_indr_symbol->non_dylib_referenced_obj == NULL)
		merged_indr_symbol->non_dylib_referenced_obj =
							    command_line_object;
	}
	merged_symbol->nlist.n_value = (unsigned long)merged_indr_symbol;

	if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
	    command_line_object->nextdefsym = 1;
	    command_line_object->reference_maps =
		reallocate(command_line_object->reference_maps,
			   (command_line_object->nrefsym + 2) *
			   sizeof(struct reference_map));
	    command_line_object->reference_maps[
		command_line_object->nrefsym + 0].flags =
		    REFERENCE_FLAG_DEFINED;
	    command_line_object->reference_maps[
		command_line_object->nrefsym + 0].merged_symbol =
		    merged_symbol;
	    command_line_object->reference_maps[
		command_line_object->nrefsym + 1].flags =
		    REFERENCE_FLAG_UNDEFINED_NON_LAZY;
	    command_line_object->reference_maps[
		command_line_object->nrefsym + 1].merged_symbol =
		    merged_indr_symbol;
	    command_line_object->irefsym =
		output_dysymtab_info.dysymtab_command.nextrefsyms;
	    command_line_object->nrefsym += 2;
	    output_dysymtab_info.dysymtab_command.nextrefsyms += 2;
	}
}

/*
 * merge_dylib_module_symbols() merges the symbols from the current object
 * (cur_obj) which represents a module from a dynamic shared library into
 * the merged symbol table.  The parameter dynamic_library is the dynamic
 * library struct the current object is from.
 */
__private_extern__
void
merge_dylib_module_symbols(
struct dynamic_library *dynamic_library)
{
    unsigned long i, j, k, l, nundefineds, module_index, library_ordinal;
    char *strings, *symbol_name, *name;
    struct nlist *symbols, *fake_trace_symbol;
    struct dylib_reference *refs;
    unsigned long flags;
    enum bool was_traced, resolve_flat;
    struct merged_symbol *hash_pointer, *merged_symbol;
    struct object_file *obj;
    struct dylib_table_of_contents *toc;
    struct dynamic_library *dep;

	strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
	symbols = (struct nlist *)(cur_obj->obj_addr +
				   cur_obj->symtab->symoff);
	refs = (struct dylib_reference *)(cur_obj->obj_addr +
					  cur_obj->dysymtab->extrefsymoff);

	/*
	 * First loop through the symbols defined by this module and merge them
	 * into the merged symbol table.
	 */
	for(i = 0; i < cur_obj->dylib_module->nextdefsym; i++){
	    j = i + cur_obj->dylib_module->iextdefsym;
	    symbol_name = strings + symbols[j].n_un.n_strx;
	    /*
	     * Do the trace of the symbol_name if specified.
	     */
	    if((symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF)
		fake_trace_symbol = &pbud_weak_def_symbol;
	    else
		fake_trace_symbol = &pbud_symbol;
	    was_traced = FALSE;
	    if(ntrace_syms != 0){
		for(k = 0; k < ntrace_syms; k++){
		    if(strcmp(trace_syms[k], symbol_name) == 0){
			trace_symbol(symbol_name, fake_trace_symbol, cur_obj,
			    "error in trace_symbol()");
			was_traced = TRUE;
			break;
		    }
		}
	    }
	    /* lookup the symbol_name and see if it has already been seen */
	    hash_pointer = lookup_symbol(symbol_name);
	    if(hash_pointer->name_len == 0){
		/*
		 * The symbol has not been seen yet so just enter it as a
		 * prebound undefined.
		 */
		merged_symbol = enter_symbol(hash_pointer, &(pbud_symbol),
					     symbol_name, cur_obj);
	    }
	    else{
		merged_symbol = hash_pointer;
		/*
		 * If the merged symbol is not undefined and if this symbol is
		 * a weak definition then it is simply ignored and the merged
		 * symbol is used.  Note currently only coalesced sections can
		 * have this attribute and this is checked for in
		 * check_symbol() so it is assumed it is a coalesced symbol
		 * here.
		 */
		if((merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
		    merged_symbol->nlist.n_value != 0) &&
		   (symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF){
		    continue;
		}
		/*
		 * If the merged symbol is a weak definition then it is
		 * discarded and this symbol definition from this dylib is used.
		 */
		if((merged_symbol->nlist.n_desc & N_WEAK_DEF) == N_WEAK_DEF ||
			(merged_symbol->defined_in_dylib == TRUE &&
			 merged_symbol->weak_def_in_dylib)){
		    if(merged_symbol->defined_in_dylib == FALSE){
			if((merged_symbol->nlist.n_type & N_EXT) &&
			   (merged_symbol->nlist.n_type & N_PEXT)){
			    merged_symbol->definition_object->nprivatesym--;
			    nmerged_private_symbols--;
			}
			else{
			    merged_symbol->definition_object->nextdefsym--;
			}
		    }
		    /*
		     * If the output file is a multi module MH_DYLIB type reset
		     * the reference map for the merged external symbol that
		     * is being discarded.
		     */
		    if(filetype == MH_DYLIB &&
		       multi_module_dylib == TRUE &&
		       merged_symbol->defined_in_dylib == FALSE){
			/*
			 * Discared coalesced symbols are referenced as
			 * undefined. TODO: to determine if the reference is
			 * lazy or non-lazy we would have to look at all the
			 * relocation entries in this object.  For now just
			 * assume non-lazy to be safe.
			 */
			for(k = 0;
			    k < merged_symbol->definition_object->nrefsym;
			    k++){
			    if(merged_symbol->definition_object->
			       reference_maps[k].merged_symbol ==
							    merged_symbol){
				if(symbols[k].n_type & N_PEXT)
				    merged_symbol->definition_object->
				    reference_maps[k].flags =
				  REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY;
				else
				    merged_symbol->definition_object->
				    reference_maps[k].flags =
					REFERENCE_FLAG_UNDEFINED_NON_LAZY;
				break;
			    }
			}
		    }
		    merged_symbol->coalesced_defined_in_dylib = FALSE;
		    merged_symbol->weak_def_in_dylib = FALSE;
		    goto use_symbol_definition_from_this_dylib;
		}
		/*
		 * If both symbols are coalesced symbols then the this
		 * symbol is simply ignored.
		 */
		if((((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
		      ((merged_symbol->definition_object->section_maps[
			   merged_symbol->nlist.n_sect - 1].s->flags) &
			   SECTION_TYPE) == S_COALESCED) ||
		     merged_symbol->coalesced_defined_in_dylib == TRUE) &&
		   (symbols[j].n_type & N_TYPE) == N_SECT &&
		   ((cur_obj->section_maps[symbols[j].n_sect - 1].
			s->flags) & SECTION_TYPE) == S_COALESCED){
		    continue;
		}
		/*
		 * The symbol exists and both are not coalesced symbols.  So if
		 * the merged symbol is anything but a common or undefined then
		 * it is multiply defined.
		 */
		if(merged_symbol->nlist.n_type != (N_UNDF | N_EXT)){
		    /*
		     * If this is a two-level namespace link and this library is
		     * referenced indirectly then don't issue a multiply
		     * defined error or warning about symbols from it.
		     */
		    if(twolevel_namespace == TRUE &&
		       dynamic_library->definition_obj->library_ordinal == 0)
			continue;
		    /*
		     * It is multiply defined so the logic of the routine
		     * multiply_defined() is copied here so that tracing a
		     * symbol from a dylib module can be done.
		     */
		    for(k = 0; k < nmultiple_defs; k++){
			if(strcmp(multiple_defs[k],
				  merged_symbol->nlist.n_un.n_name) == 0)
			    break;
		    }
		    for(l = 0; l < ntrace_syms; l++){
			if(strcmp(trace_syms[l],
				  merged_symbol->nlist.n_un.n_name) == 0)
			    break;
		    }
		    if(k == nmultiple_defs){
			if(allow_multiply_defined_symbols == TRUE){
			    warning("multiple definitions of symbol %s",
				  merged_symbol->nlist.n_un.n_name);
			}
			else if((twolevel_namespace == TRUE &&
			    merged_symbol->defined_in_dylib == FALSE) ||
			   (force_flat_namespace == FALSE &&
			    ((((struct mach_header *)(cur_obj->obj_addr))->
			        flags & MH_TWOLEVEL) == MH_TWOLEVEL ||
			    (merged_symbol->defined_in_dylib == TRUE &&
			     (((struct mach_header *)(merged_symbol->
			        definition_object->obj_addr))->flags &
   				MH_TWOLEVEL) == MH_TWOLEVEL)))){
				if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_WARNING){
				    warning("multiple definitions of symbol %s",
					  merged_symbol->nlist.n_un.n_name);
				    if(nowarnings == TRUE)
					continue;
				}
				else if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_ERROR){
				    error("multiple definitions of symbol %s",
					  merged_symbol->nlist.n_un.n_name);
				}
				else if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_SUPPRESS)
				    continue;
			}
			else{
			    error("multiple definitions of symbol %s",
				  merged_symbol->nlist.n_un.n_name);
			}
			multiple_defs = reallocate(multiple_defs,
			    (nmultiple_defs + 1) * sizeof(char *));
			multiple_defs[nmultiple_defs++] =
			    merged_symbol->nlist.n_un.n_name;
			if(l == ntrace_syms)
			    trace_merged_symbol(merged_symbol);
		    }
		    if(was_traced == FALSE){
			trace_symbol(symbol_name, fake_trace_symbol, cur_obj,
			    "error in trace_symbol()");
		    }
		    continue;
		}
	    }
use_symbol_definition_from_this_dylib:
	    maybe_remove_dwarf_symbol(merged_symbol);
	    merged_symbol->nlist.n_type = N_PBUD | N_EXT;
	    merged_symbol->nlist.n_sect = NO_SECT;
	    if((symbols[j].n_type & N_TYPE) == N_SECT &&
		((cur_obj->section_maps[symbols[j].n_sect - 1].
		  s->flags) & SECTION_TYPE) == S_COALESCED){
		merged_symbol->coalesced_defined_in_dylib = TRUE;
		if((symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF)
		    merged_symbol->weak_def_in_dylib = TRUE;
#ifdef COALESCE_DEBUG
printf("merging in coalesced symbol %s\n", merged_symbol->nlist.n_un.n_name);
#endif
	    }
	    /*
	     * If -twolevel_namespace is in effect and this symbol is referenced
	     * from an object going into the image and will need the library
	     * ordinal recorded check to see that this dynamic library has been
	     * assigned an ordinal (that is it was listed on the link line or
	     * is a sub-framework or sub-umbrella of something listed).  If not
	     * flag this as an illegal reference to an indirect dynamic library
	     * if this library was not flagged already.
	     */
	    if(save_reloc == FALSE &&
	       twolevel_namespace == TRUE &&
	       merged_symbol->referenced_in_non_dylib == TRUE &&
	       dynamic_library->definition_obj->library_ordinal == 0 &&
	       dynamic_library->indirect_twolevel_ref_flagged == FALSE){
		obj = cur_obj;
		cur_obj = merged_symbol->definition_object;
		error_with_cur_obj("illegal reference to symbol: %s defined in "
		    "indirectly referenced dynamic library %s", symbol_name,
		    dynamic_library->dylib_file != NULL ?
		    dynamic_library->file_name : dynamic_library->dylib_name);
		cur_obj = obj;
		dynamic_library->indirect_twolevel_ref_flagged = TRUE;
	    }
	    /*
	     * Don't change the reference type bits of the n_desc field as it
	     * contains the reference type (lazy or non-lazy).
	     */
	    merged_symbol->nlist.n_value = symbols[j].n_value;
	    if(symbols[j].n_desc & N_ARM_THUMB_DEF)
		merged_symbol->nlist.n_value |= 1;
	    merged_symbol->definition_object = cur_obj;
	    merged_symbol->defined_in_dylib = TRUE;
	    merged_symbol->definition_library = dynamic_library;
	    /*
	     * If this shared library is being forced to be weak linked then
	     * set N_WEAK_REF to make this symbol a weak reference.
	     */
	    if(dynamic_library->force_weak_dylib &&
	       merged_symbol->referenced_in_non_dylib == TRUE)
		merged_symbol->nlist.n_desc |= N_WEAK_REF;
	    /*
	     * If the merged symbol we are resolving is not a weak reference
	     * and it is referenced from a non-dylib then set
	     * some_non_weak_refs to TRUE.
	     */
	    if((merged_symbol->nlist.n_desc & N_WEAK_REF) == 0 &&
	       merged_symbol->referenced_in_non_dylib == TRUE)
		dynamic_library->some_non_weak_refs = TRUE;
	    if(merged_symbol->referenced_in_non_dylib == TRUE)
		dynamic_library->some_symbols_referenced = TRUE;
	    if((symbols[j].n_type & N_TYPE) == N_INDR){
		merged_symbol->nlist.n_type = N_INDR | N_EXT;
		enter_indr_symbol(merged_symbol, symbols + j, strings, cur_obj);
	    }
	    /*
	     * If -twolevel_namespace is in effect record the library ordinal
	     * that this symbol definition is in.
	     */
	    if(twolevel_namespace == TRUE){
		SET_LIBRARY_ORDINAL(merged_symbol->nlist.n_desc,
			    dynamic_library->definition_obj->library_ordinal);
		/*
		 * It is possible that a common or undefined symbol could have
		 * been in the merged symbol table and this dylib module is now
		 * replacing it.  If so we have to look it up in the table of
		 * contents to get the correct index into the table of contents
		 * for the hint to be recorded.
		 */
		if(merged_symbol->itoc == 0){
		    bsearch_strings = dynamic_library->strings;
		    bsearch_symbols = dynamic_library->symbols;
		    toc = bsearch(merged_symbol->nlist.n_un.n_name,
			      dynamic_library->tocs,
			      dynamic_library->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		    merged_symbol->itoc = toc - dynamic_library->tocs;
		}
	    }
	}

	/*
	 * If the -Y flag is set (trace undefined symbols) then we create an
	 * undefined map for this object file so process_undefineds() can use it
	 * to do the work for -Y.
	 */
	if(Yflag && cur_obj->dylib_module->nrefsym != 0){
	    nundefineds = 0;
	    for(i = 0; i < cur_obj->dylib_module->nrefsym; i++){
		j = i + cur_obj->dylib_module->irefsym;
		flags = refs[j].flags;
		if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		   flags == REFERENCE_FLAG_UNDEFINED_LAZY){
		    nundefineds++;
		}
	    }
	    cur_obj->undefined_maps = allocate(nundefineds *
					       sizeof(struct undefined_map));
	    cur_obj->nundefineds = nundefineds;
	}
	nundefineds = 0;

	/*
	 * Second loop through the symbols referenced by this module and merge
	 * undefined references into the merged symbol table.
	 */
	for(i = 0; i < cur_obj->dylib_module->nrefsym; i++){
	    j = i + cur_obj->dylib_module->irefsym;
	    flags = refs[j].flags;
	    if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
	       flags == REFERENCE_FLAG_UNDEFINED_LAZY){
		symbol_name = strings + symbols[refs[j].isym].n_un.n_strx;
		/*
		 * Do the trace of this symbol if specified.
		 */
		if(ntrace_syms != 0){
		    for(k = 0; k < ntrace_syms; k++){
			if(strcmp(trace_syms[k], symbol_name) == 0){
			    if(force_flat_namespace == TRUE ||
           		       (((struct mach_header *)(cur_obj->obj_addr))->
				flags & MH_TWOLEVEL) != MH_TWOLEVEL){
				trace_symbol(symbol_name, &(undefined_symbol),
					 cur_obj, "error in trace_symbol()");
			    }
			    else{
				print_obj_name(cur_obj);
				library_ordinal = GET_LIBRARY_ORDINAL(symbols[
				    refs[j].isym].n_desc);
				if(library_ordinal != 0 &&
				   library_ordinal != DYNAMIC_LOOKUP_ORDINAL){
				    dep = dynamic_library->dependent_images[
					      library_ordinal - 1];
				    if(dep->umbrella_name != NULL)
					name = dep->umbrella_name;
				    else if(dep->library_name != NULL)
					name = dep->library_name;
				    else
					name = dep->dylib_name;
				    print("reference to undefined %s (from %s)"
					  "\n", symbol_name, name);
				}
				else
				    print("reference to undefined %s\n",
					  symbol_name);
			    }
			    break;
			}
		    }
		}
		/*
		 * Determine how this reference will be resolved. If
		 * -force_flat_namespace is TRUE it will be resolved flat.
		 * If this dylib is not a two-level namespace dylib it will
		 * also be resolved flat.  It it is a two-level dylib then
		 * if the library_ordinal is DYNAMIC_LOOKUP_ORDINAL it will be
		 * resolved flat.  If it is a two-level namespace dylib and
		 * the library_ordinal is not DYNAMIC_LOOKUP_ORDINAL it will
		 * be resolved with two-level namespace semantics.
		 */
		if(force_flat_namespace == TRUE)
		    resolve_flat = TRUE;
		else{
		    if((((struct mach_header *)(cur_obj->obj_addr))->
			flags & MH_TWOLEVEL) == MH_TWOLEVEL){
			library_ordinal = GET_LIBRARY_ORDINAL(
						symbols[refs[j].isym].n_desc);
			if(library_ordinal == DYNAMIC_LOOKUP_ORDINAL)
			    resolve_flat = TRUE;
			else
			    resolve_flat = FALSE;
		    }
		    else{
			resolve_flat = TRUE;
		    }
		}
		if(resolve_flat == TRUE){
		    /*
		     * The new linking architecture model when building a
		     * two-level namespace image states "with two level
		     * namespace, there is no need to resolve undefines in
		     * dependent dylibs".  So if we are building a two-level
		     * namespace image even when linking against a
		     * flat-namespace dylib, that dylib's undefined references
		     * are not to be resolved.
		     *
		     * This of course has the architectural flaw that error
		     * checking is lost and we could be building a broken
		     * binary.
		     */
		    if(twolevel_namespace == TRUE &&
		       (((struct mach_header *)(cur_obj->obj_addr))->
			  flags & MH_TWOLEVEL) != MH_TWOLEVEL){
			continue; /* with for loop */
		    }
		    /* lookup the symbol and see if it has already been seen */
		    hash_pointer = lookup_symbol(symbol_name);
		    if(hash_pointer->name_len == 0){
			/*
			 * The symbol has not been seen yet so just enter it as
			 * an undefined symbol and it will be returned.
			 */
			merged_symbol = enter_symbol(hash_pointer,
				    &(undefined_symbol), symbol_name, cur_obj);
		    }
		    else{
			merged_symbol = hash_pointer;
		    }
		    merged_symbol->nlist.n_desc |= REFERENCED_DYNAMICALLY;
		}
		else{
		    /*
		     * This is a two-level namespace dylib so this must be
		     * resolved to the symbol from the referenced dylib. To do
		     * this we fake up a merged_symbol and place it on the
		     * undefined list with the twolevel_reference bit set and
		     * the referencing_library field set.  Then
		     * search_dynamic_libs() in pass1.c will figure out which
		     * dylib module is being referenced and load it.
		     */
		    /*
		     * With two level namespace, there is no need to resolve
		     * undefines in dependent dylibs.  Their location was fixed
		     * when that dylib was built.  The check here is that any
		     * undefine which already has an ordinal and the ordinal
		     * refers to a library that can't be accessed by the
		     * being-linked image, then ignore it
		     *
		     * The comment above, from the original change, does not
		     * consider the architectural need to resolve undefines in
		     * dependent dylibs for error checking, and to avoid
		     * building broken programs that could be detected at build
		     * time instead of letting that happen at runtime.  Which
		     * could be very late do to lazy binding.
		     *
		     * The logic below also seems flawed in that undefined
		     * references that are marked to be looked up dynamically
		     * are still searched. But may not be found as the
		     * indirectly referenced dylibs are now removed from the
		     * list of dylibs to be searched.
		     */
		    library_ordinal = GET_LIBRARY_ORDINAL(
						symbols[refs[j].isym].n_desc);
		    if((library_ordinal != SELF_LIBRARY_ORDINAL) &&
		       (library_ordinal != DYNAMIC_LOOKUP_ORDINAL)){
			dep = dynamic_library->dependent_images[
							library_ordinal - 1];
			if(dep->definition_obj->library_ordinal == 0)
			    continue; /* with for loop */
		    }
		    merged_symbol = allocate(sizeof(struct merged_symbol));
		    memset(merged_symbol, '\0', sizeof(struct merged_symbol));

		    merged_symbol->nlist = symbols[refs[j].isym];
		    merged_symbol->nlist.n_un.n_name = symbol_name;
		    merged_symbol->definition_object = cur_obj;
		    merged_symbol->twolevel_reference = TRUE;
		    merged_symbol->referencing_library = dynamic_library;
		    add_to_undefined_list(merged_symbol);
		}
		if(Yflag){
		    cur_obj->undefined_maps[nundefineds++].merged_symbol =
			merged_symbol;
		}
	    }
	}

	/*
	 * Last loop through the private symbols referenced by this module and
	 * make sure the module is linked in.  If not force it to be linked in.
	 * Note this is doing pass1 functionality and causing modules to be
	 * linked in.  So that cur_obj can change through out this loop.
	 */
	obj = cur_obj;
	for(i = 0; i < obj->dylib_module->nrefsym; i++){
	    j = i + obj->dylib_module->irefsym;
	    flags = refs[j].flags;
	    if(flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
	       flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY){
		/*
		 * Using the symbol index, refs[j].isym, figure out which
		 * module owns this symbol and set that into module_index.
		 */
		for(k = 0; k < obj->dysymtab->nmodtab; k++){
		    if(refs[j].isym >= dynamic_library->mods[k].ilocalsym &&
		       refs[j].isym <  dynamic_library->mods[k].ilocalsym +
				       dynamic_library->mods[k].nlocalsym)
			break;
		}
		if(k >= obj->dysymtab->nmodtab){
		    error_with_cur_obj("isym field (%u) of reference table "
			"entry %lu for private reference not in the local "
			"symbols for any module", refs[j].isym, j);
		    return;
		}
		module_index = k;
		if(is_dylib_module_loaded(dynamic_library->mods +
					  module_index) == FALSE){

		    cur_obj = new_object_file();
		    *cur_obj = *(dynamic_library->definition_obj);
		    cur_obj->dylib_module = dynamic_library->mods +
					    module_index;
		    if(dynamic_library->linked_modules != NULL)
			dynamic_library->linked_modules[module_index / 8] |=
				1 << module_index % 8;
		    if(whyload){
			print_obj_name(cur_obj);
			symbol_name = strings +
				      symbols[refs[j].isym].n_un.n_strx;
			print("loaded to resolve private symbol: %s\n",
			      symbol_name);
		    }
		    merge_dylib_module_symbols(dynamic_library);
		    cur_obj = obj;
		}
	    }
	}
}
/*
 * merge_bundle_loader_symbols() merges the symbols from the current object
 * (cur_obj) which represents the bundle loader module into the merged symbol
 * table.  The parameter dynamic_library is the dynamic library struct the
 * current object is from.
 */
__private_extern__
void
merge_bundle_loader_symbols(
struct dynamic_library *dynamic_library)
{
    unsigned long i, j, k, l;
    char *strings, *symbol_name;
    struct nlist *symbols, *fake_trace_symbol;
    enum bool was_traced;
    struct merged_symbol *hash_pointer, *merged_symbol;

	strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
	symbols = (struct nlist *)(cur_obj->obj_addr +
				   cur_obj->symtab->symoff);
	/*
	 * Loop through the symbols defined by the bundle loader and merge them
	 * into the merged symbol table.
	 */
	for(i = 0; i < cur_obj->dysymtab->nextdefsym; i++){
	    j = i + cur_obj->dysymtab->iextdefsym;
	    symbol_name = strings + symbols[j].n_un.n_strx;
	    /*
	     * Do the trace of the symbol_name if specified.
	     */
	    if((symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF)
		fake_trace_symbol = &pbud_weak_def_symbol;
	    else
		fake_trace_symbol = &pbud_symbol;
	    was_traced = FALSE;
	    if(ntrace_syms != 0){
		for(k = 0; k < ntrace_syms; k++){
		    if(strcmp(trace_syms[k], symbol_name) == 0){
			trace_symbol(symbol_name, fake_trace_symbol, cur_obj,
			    "error in trace_symbol()");
			was_traced = TRUE;
			break;
		    }
		}
	    }
	    /* lookup the symbol_name and see if it has already been seen */
	    hash_pointer = lookup_symbol(symbol_name);
	    if(hash_pointer->name_len == 0){
		/*
		 * The symbol has not been seen yet so just enter it as a
		 * prebound undefined.
		 */
		merged_symbol = enter_symbol(hash_pointer, &(pbud_symbol),
					     symbol_name, cur_obj);
	    }
	    else{
		merged_symbol = hash_pointer;
		/*
		 * If the merged symbol is not undefined and if this symbol is
		 * a weak definition then it is simply ignored and the merged
		 * symbol is used.  Note currently only coalesced sections can
		 * have this attribute and this is checked for in check_symbol()
		 * so it is assumed it is a coalesced symbol here.
		 */
		if((merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
		    merged_symbol->nlist.n_value != 0) &&
		   (symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF){
		    continue;
		}
		/*
		 * If the merged symbol is a weak definition then it is
		 * discarded and this symbol definition from this bundle
		 * loader is used.
		 */
		if(((merged_symbol->nlist.n_desc & N_WEAK_DEF) == N_WEAK_DEF) ||
			(merged_symbol->defined_in_dylib == TRUE &&
			 merged_symbol->weak_def_in_dylib)){
		    if(merged_symbol->defined_in_dylib == FALSE){
			if((merged_symbol->nlist.n_type & N_EXT) &&
			   (merged_symbol->nlist.n_type & N_PEXT)){
			    merged_symbol->definition_object->nprivatesym--;
			    nmerged_private_symbols--;
			}
			else{
			    merged_symbol->definition_object->nextdefsym--;
			}
		    }
		    /*
		     * If the output file is a multi module MH_DYLIB type reset
		     * the reference map for the merged external symbol that
		     * is being discarded.
		     */
		    if(filetype == MH_DYLIB &&
		       multi_module_dylib == TRUE &&
		       merged_symbol->defined_in_dylib == FALSE){
			/*
			 * Discared coalesced symbols are referenced as
			 * undefined. TODO: to determine if the reference is
			 * lazy or non-lazy we would have to look at all the
			 * relocation entries in this object.  For now just
			 * assume non-lazy to be safe.
			 */
			for(k = 0;
			    k < merged_symbol->definition_object->nrefsym;
			    k++){
			    if(merged_symbol->definition_object->
			       reference_maps[k].merged_symbol ==
							    merged_symbol){
				if(symbols[k].n_type & N_PEXT)
				    merged_symbol->definition_object->
				    reference_maps[k].flags =
				  REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY;
				else
				    merged_symbol->definition_object->
				    reference_maps[k].flags =
					REFERENCE_FLAG_UNDEFINED_NON_LAZY;
				break;
			    }
			}
		    }
		    merged_symbol->coalesced_defined_in_dylib = FALSE;
		    merged_symbol->weak_def_in_dylib = FALSE;
		    goto use_symbol_definition_from_this_bundle_loader;
		}
		/*
		 * If both symbols are coalesced symbols then the this
		 * symbol is simply ignored.
		 */
		if((((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
		      ((merged_symbol->definition_object->section_maps[
			   merged_symbol->nlist.n_sect - 1].s->flags) &
			   SECTION_TYPE) == S_COALESCED) ||
		     merged_symbol->coalesced_defined_in_dylib == TRUE) &&
		   (symbols[j].n_type & N_TYPE) == N_SECT &&
		   ((cur_obj->section_maps[symbols[j].n_sect - 1].
			s->flags) & SECTION_TYPE) == S_COALESCED){
		    continue;
		}
		/*
		 * The symbol exist and both are not coalesced symbols.  So if
		 * the merged symbol is anything but a common or undefined then
		 * it is multiply defined.
		 */
		if(merged_symbol->nlist.n_type != (N_UNDF | N_EXT)){
		    /*
		     * It is multiply defined so the logic of the routine
		     * multiply_defined() is copied here so that tracing a
		     * symbol from a dylib module can be done.
		     */
		    for(k = 0; k < nmultiple_defs; k++){
			if(strcmp(multiple_defs[k],
				  merged_symbol->nlist.n_un.n_name) == 0)
			    break;
		    }
		    for(l = 0; l < ntrace_syms; l++){
			if(strcmp(trace_syms[l],
				  merged_symbol->nlist.n_un.n_name) == 0)
			    break;
		    }
		    /*
		     * If -private_bundle is used then don't worry about any
		     * multiply defined references.
		     */
		    if(private_bundle == TRUE)
			break;
		    if(k == nmultiple_defs){
			if(allow_multiply_defined_symbols == TRUE){
			    warning("multiple definitions of symbol %s",
				  merged_symbol->nlist.n_un.n_name);
			}
			else if((twolevel_namespace == TRUE &&
			    merged_symbol->defined_in_dylib == FALSE) ||
			   (force_flat_namespace == FALSE &&
			    ((((struct mach_header *)(cur_obj->obj_addr))->
			        flags & MH_TWOLEVEL) == MH_TWOLEVEL ||
			    (merged_symbol->defined_in_dylib == TRUE &&
			     (((struct mach_header *)(merged_symbol->
			        definition_object->obj_addr))->flags &
   				MH_TWOLEVEL) == MH_TWOLEVEL)))){
				if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_WARNING)
				    warning("multiple definitions of symbol %s",
					  merged_symbol->nlist.n_un.n_name);
				else if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_ERROR){
				    error("multiple definitions of symbol %s",
					  merged_symbol->nlist.n_un.n_name);
				}
				else if(multiply_defined_flag ==
				   MULTIPLY_DEFINED_SUPPRESS)
				    continue;
			}
			else{
			    error("multiple definitions of symbol %s",
				  merged_symbol->nlist.n_un.n_name);
			}
			multiple_defs = reallocate(multiple_defs,
			    (nmultiple_defs + 1) * sizeof(char *));
			multiple_defs[nmultiple_defs++] =
			    merged_symbol->nlist.n_un.n_name;
			if(l == ntrace_syms)
			    trace_merged_symbol(merged_symbol);
		    }
		    if(was_traced == FALSE)
			trace_symbol(symbol_name, fake_trace_symbol, cur_obj,
			    "error in trace_symbol()");
		    continue;
		}
	    }
use_symbol_definition_from_this_bundle_loader:
	    maybe_remove_dwarf_symbol(merged_symbol);
	    merged_symbol->nlist.n_type = N_PBUD | N_EXT;
	    merged_symbol->nlist.n_sect = NO_SECT;
	    if((symbols[j].n_type & N_TYPE) == N_SECT &&
		((cur_obj->section_maps[symbols[j].n_sect - 1].
		  s->flags) & SECTION_TYPE) == S_COALESCED){
		merged_symbol->coalesced_defined_in_dylib = TRUE;
		if((symbols[j].n_desc & N_WEAK_DEF) == N_WEAK_DEF)
		    merged_symbol->weak_def_in_dylib = TRUE;
#ifdef COALESCE_DEBUG
printf("merging in coalesced symbol %s\n", merged_symbol->nlist.n_un.n_name);
#endif
	    }

	    /*
	     * Since this is the bundle loader it always has the library
	     * ordinal EXECUTABLE_ORDINAL assigned to it and we don't have to
	     * worry about illegal reference to an indirect "dynamic library".
	     */

	    /*
	     * Don't change the reference type bits if n_desc field as it
	     * contains the reference type (lazy or non-lazy).
	     */
	    merged_symbol->nlist.n_value = symbols[j].n_value;
	    merged_symbol->definition_object = cur_obj;
	    merged_symbol->defined_in_dylib = TRUE;
	    merged_symbol->definition_library = dynamic_library;
	    if((symbols[j].n_type & N_TYPE) == N_INDR){
		merged_symbol->nlist.n_type = N_INDR | N_EXT;
		enter_indr_symbol(merged_symbol, symbols + j, strings, cur_obj);
	    }
	    /*
	     * If -twolevel_namespace is in effect record the library ordinal
	     * that this symbol definition is in.
	     */
	    if(twolevel_namespace == TRUE){
		SET_LIBRARY_ORDINAL(merged_symbol->nlist.n_desc,
			    dynamic_library->definition_obj->library_ordinal);
	    }
	}

	/*
	 * For the bundle loader we simply ignore any undefined references it
	 * might have and since it is a one module image there is nothing to
	 * do for its private symbols.
	 */
}
#endif /* !defined(RLD) */

/*
 * is_output_local_symbol() returns TRUE or FALSE depending if the local symbol
 * type, section, object and name passed to it will be in the output file's
 * symbol table based on the level of symbol stripping.  If it returns TRUE it
 * also indirectly returns the size of the local string for output in
 * output_strlen (possibly truncated if the strip level is STRIP_MIN_DEBUG).
 * The obj passed must be the object this symbol came from so that the the
 * section can be checked for the S_ATTR_STRIP_STATIC_SYMS attribute flag.
 */
__private_extern__
enum bool
is_output_local_symbol(
unsigned char n_type,
unsigned char n_sect,
unsigned char n_desc,
struct object_file *obj,
char *symbol_name,
unsigned long *output_strlen)
{
#ifndef RLD
    char *end;
#endif /* !defined(RLD) */

	*output_strlen = 0;
	switch(strip_level){
	    case STRIP_NONE:
	    case STRIP_DUP_INCLS:
		/*
		 * We are not stripping stabs.  But if we see an N_OSO we
		 * will change it's name in some cases.  In here with just
		 * return the lenght of the new name.  In output_local_symbols()
		 * is where the new name is set for the output file.
		 */
		if(n_type == N_OSO){
		    /*
		     * When the compiler is producing dwarf debug info it
		     * uses stabs for debug notes.  And the N_OSO stabs in this
		     * case has a n_desc field of 1.  If the name is the empty
		     * string (not NULL but a 1 character string with just a
		     * '\0') then change the name to the full path of the
		     * object file.  If the name is not an empty string it is
		     * left unchanged.
		     */
		    if(n_desc == 1){
			if(symbol_name != NULL && *symbol_name == '\0'){
			    if(obj->resolved_path == NULL)
				set_obj_resolved_path(obj);
			    *output_strlen = cur_obj->resolved_path_len;
			}
			else{
			    *output_strlen = strlen(symbol_name);
			}
		    }
		    /*
		     * When the compiler is producing stabs debug info it will
		     * produce an N_OSO stab with an n_desc field of 0.  In this
		     * (saving all stabs an -Sp is not specified) the name gets
		     * reset to "" (that a one character string with only a '\0'
		     * character).  Unless the input n_un.n_strx field is 0 then
		     * it will end up as 0 in the output.
		     */
		    else if(n_desc == 0){
			*output_strlen = 0;
		    }
		    /*
		     * If the n_desc field of this N_OSO stab is something
		     * other than 1 or 0 leave the name unchanged.
		     */
		    else{
			*output_strlen = strlen(symbol_name);
		    }
		}
		else{
		    *output_strlen = strlen(symbol_name);
		}
		return(TRUE);
	    case STRIP_ALL:
	    case STRIP_DYNAMIC_EXECUTABLE:
	    case STRIP_NONGLOBALS:
		return(FALSE);
	    case STRIP_DEBUG:
		if(n_type & N_STAB ||
		   (*symbol_name == 'L' && (n_type & N_STAB) == 0) ||
		   (save_reloc == FALSE &&
		    (n_type & N_TYPE) == N_SECT &&
		    (obj->section_maps[n_sect - 1].s->flags &
		     S_ATTR_STRIP_STATIC_SYMS) == S_ATTR_STRIP_STATIC_SYMS))
		    return(FALSE);
		else{
		    *output_strlen = strlen(symbol_name);
		    return(TRUE);
		}
	    case STRIP_MIN_DEBUG:
#ifndef RLD
		if(n_type & N_STAB){
		    switch(n_type){
		    case N_OSO:
			/*
			 * When the compiler is producing dwarf debug info it
			 * uses stabs for debug notes.  And the N_OSO stabs in
			 * this case has a n_desc field of 1.  If the name is
			 * the empty string (not NULL but a 1 character string
			 * with just a '\0') then change the name to the full
			 * path of the object file.  If the name is not an empty
			 * string it is left unchanged.
			 */
			if(n_desc == 1){
			    if(symbol_name != NULL && *symbol_name == '\0'){
				if(obj->resolved_path == NULL)
				    set_obj_resolved_path(obj);
				*output_strlen = cur_obj->resolved_path_len;
			    }
			    else{
				*output_strlen = strlen(symbol_name);
			    }
			}
			/*
			 * When the compiler is producing stabs debug info it
			 * will produce an N_OSO stab with an n_desc field of 0.
			 * In this case where -Sp is specified name is changed
			 * to the full path of the object file.
			 */
			else if(n_desc == 0){
			    if(obj->resolved_path == NULL)
				set_obj_resolved_path(obj);
			    *output_strlen = cur_obj->resolved_path_len;
			}
			/*
			 * If the n_desc is not 0 or 1 then leave the name
			 * unchanged.
			 */
			else{
			    *output_strlen = strlen(symbol_name);
			}
			return(TRUE);
		    /* keep these and their full strings */
		    case N_SO:
		    case N_SOL:
		    case N_OPT:
			*output_strlen = strlen(symbol_name);
			return(TRUE);
		    /* keep these but truncate the string to just NAME:<type> */
		    case N_LCSYM:
		    case N_STSYM:
		    case N_GSYM:
		    case N_FUN:
			end = find_stab_type_end(
				find_stab_name_end(symbol_name));
			if(end != NULL)
			    *output_strlen = end - symbol_name;
			else
			    /*
			     * The string is not what is expected just leave
			     * the output_strlen the size of whole string.
			     */
			    *output_strlen = strlen(symbol_name);
			return(TRUE);
		    /* strip all other stabs */
		    default:
			return(FALSE);
		    }
		}
		/* it's not a stab see if we still keep it or not */
		else if(*symbol_name == 'L' ||
		   (save_reloc == FALSE &&
		    (n_type & N_TYPE) == N_SECT &&
		    (obj->section_maps[n_sect - 1].s->flags &
		     S_ATTR_STRIP_STATIC_SYMS) == S_ATTR_STRIP_STATIC_SYMS))
		    return(FALSE);
		else{
		    *output_strlen = strlen(symbol_name);
		    return(TRUE);
		}
#endif /* !defined(RLD) */
	    case STRIP_L_SYMBOLS:
		if(*symbol_name == 'L' && (n_type & N_STAB) == 0)
		    return(FALSE);
		else{
		    *output_strlen = strlen(symbol_name);
		    return(TRUE);
		}
	}
	/* never gets here but shuts up a bug in -Wall */
	*output_strlen = strlen(symbol_name);
	return(TRUE);
}

/*
 * is_type_stab() is passed the n_type and the name of a symbol.  It that is a
 * type stab returns TRUE else it returns FALSE.  A type stab is an L_LSYM stab
 * of the form:
 *	NAME:<type>
 * where <type> is a 'T' or 't'.
 */
static
enum bool
is_type_stab(
unsigned char n_type,
char *symbol_name)
#ifdef RLD
{
	return(FALSE);
}
#else /* !defined(RLD) */
{
    char *end;

	if((n_type & N_STAB) == 0 || n_type != N_LSYM)
	    return(FALSE);
	end = find_stab_name_end(symbol_name);
	if(end != NULL && end[1] != '\0' && (end[1] == 'T' || end[1] == 't'))
	    return(TRUE);
	else
	    return(FALSE);
}

/*
 * find_stab_name_end() parses a stab string of the form:
 *	NAME:<type><index>
 * And returns a pointer to the ':' if there is one or NULL.  This is the same
 * way gdb(1) parses this.
 *
 * The NAME part is either a C or C++ variable/function name, or an ObjC method
 * name.  The latter makes looking for the : a little tricky, since it can also
 * contain ":"'s...
 * The <type> field is one or more characters in the set [a-zA-Z].
 *    But gdb only allows any single character or the pair "Tt".
 * The <index> field is either an integer (positive or negative) or a comma
 * delimited pair of integers in parenthesis: "(<INT>,<INT>)".
 */
static
char *
find_stab_name_end(
char *name)
{
    char *first_colon, *next_colon, *s, *first_lbrac, *first_rbrac;

	for(first_colon = strchr(name, ':');
	    first_colon != NULL &&
		first_colon[1] == ':' && first_colon[2] != '\0';
	    /* no increment expression */){

	    /* Check for blah::blah in a C++ name */
	    next_colon = strchr(&first_colon[2], ':');
	    if(next_colon != NULL)
		first_colon = next_colon;
	    else
		break;
	}

	if(first_colon == NULL)
	    return(NULL);
	/*
	 * It's tempting to use strchr to look for the leftmost lbrac but that
	 * would mean scanning the whole stab string, which can be quite long.
	 * Since we only care whether there is a left square bracket BEFORE the
	 * first colon, restrict the search to that.
	 */
	first_lbrac = NULL;
	for(s = name; s < first_colon; s++){
	    if(*s == '['){
		first_lbrac = s;
		break;
	    }
	}
	if(first_lbrac == NULL ||
	   (first_lbrac == name ||
	    (first_lbrac[-1] != '-' && first_lbrac[-1] != '+'))){
	    return first_colon;
	}
	else{
	    first_rbrac = strchr(name, ']');
	    /* If their is no rbrac then it is really an "invalid" symbol name.
	       so in this case just return NULL saying we could not find the
	       colon at the end of the name. */
	    if(first_rbrac == NULL)
		return(NULL);
	    return(strchr(first_rbrac, ':'));
	}
}

/*
 * find_stab_type_end() is passed what find_stab_name_end() above returns and
 * then parses past the <type> in:
 *	NAME:<type><index>
 * and returns a pointer to past the type or NULL. (see above in the comments
 * for find_stab_name_end() for more details).
 *
 * The <type> field is one or more characters in the set [a-zA-Z].
 *    But gdb only allows any single character or the pair "Tt".
 * So that is what is parsed here.
 */
static
char *
find_stab_type_end(
char *name_end)
{
	if(name_end == NULL || name_end[0] != ':')
	    return(NULL);
	if(!isalpha(name_end[1]))
	    return(NULL);
	if(name_end[1] == 'T' && name_end[2] == 't')
	    return(name_end + 3);
	else
	    return(name_end + 2);
}
#endif /* !defined(RLD) */

/*
 * lookup_symbol() returns a pointer to a merged_symbol struct for the symbol
 * name passed to it.  Either the symbol is found in which case the struct
 * pointed to has a non-zero name_len field.  If the symbol is not found the
 * struct pointed to is used by enter_symbol() to enter the symbol.  This
 * is the routine that actually allocates the merged_symbol structs as part of
 * the merged_symbol_chunk structs.  And it allocates the first of the
 * merged_symbol_list structs hang off the merged_symbol_root.
 */
__private_extern__
struct merged_symbol *
lookup_symbol(
char *symbol_name)
{
    struct merged_symbol_chunk *p, *q;
    struct merged_symbol *sym;
    unsigned long hash_index, i, name_len;

	hash_index = hash_string(symbol_name, &name_len) %
		     SYMBOL_LIST_HASH_SIZE;
	if(merged_symbol_root == NULL){
	    merged_symbol_root = allocate(sizeof(struct merged_symbol_root));
	    memset(merged_symbol_root, 0, sizeof(struct merged_symbol_root));
	    merged_symbol_root->list =
		allocate(sizeof(struct merged_symbol_list));
	    memset(merged_symbol_root->list, 0,
		sizeof(struct merged_symbol_list));
	    merged_symbol_root->list->used = 0;
	    merged_symbol_root->list->next = NULL;
	    return(&merged_symbol_root->chunks[hash_index].symbols[0]);
	}
	q = NULL;
	for(p = &merged_symbol_root->chunks[hash_index]; p != NULL;p = p->next){
	    for(i = 0; i < SYMBOL_CHUNK_SIZE; i++){
		sym = &p->symbols[i];
		if(sym->name_len == 0){
			return(sym);
		}
		if(sym->name_len == name_len &&
		   strcmp(sym->nlist.n_un.n_name, symbol_name) == 0){
			return(sym);
		}
	    }
	    q = p;
	}
	q->next = allocate(sizeof(struct merged_symbol_chunk));
	memset(q->next, 0, sizeof(struct merged_symbol_chunk));
	return(&q->next->symbols[0]);
}

#ifndef RLD
/*
 * hash_instrument() is called when -hash_instrument is specified and prints out
 * the info about the hash table and the merged symbols lists.
 */
__private_extern__
void
hash_instrument(void)
{
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol_chunk *p;
    unsigned long n, u, i, j, h, b, c, t;

	n = 0;
	u = 0;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    u += merged_symbol_list->used;
	    n++;
	}
	print("Number of merged_symbol_lists = %lu (containing %d pointers "
	      "each)\n", n, SYMBOL_LIST_HASH_SIZE);
	print("sizeof(struct merged_symbol_list) is %lu (total %lu)\n",
	      sizeof(struct merged_symbol_list),
	      n * sizeof(struct merged_symbol_list));
	print("Number of used pointers in the lists = %lu (%.2f%%)\n",
	      u, ((double)u) / ((double)(SYMBOL_LIST_HASH_SIZE * n)) *
		    100.0);

	h = 0;
	b = 0;
	c = 0;
	for(i = 0; i < SYMBOL_LIST_HASH_SIZE; i++){
	    if(merged_symbol_root->chunks[i].symbols[0].name_len != 0)
		h++;
	    for(p = &merged_symbol_root->chunks[i];
		p != NULL;
		p = p->next){
		if(p != &merged_symbol_root->chunks[i])
		    c++;
		for(j = 0; j < SYMBOL_CHUNK_SIZE; j++){
		    if(p->symbols[j].name_len != 0)
			b++;
		}
	    }
	}
	print("The SYMBOL_LIST_HASH_SIZE is %d\n", SYMBOL_LIST_HASH_SIZE);
	print("sizeof(struct merged_symbol_root) is %lu\n",
	      sizeof(struct merged_symbol_root));
	print("Number of additional chunks: %lu (size of these %lu)\n", c,
	      c * sizeof(struct merged_symbol_chunk));
	print("Number of hash entries used: %lu (%.2f%%) average #buckets "
	      "%.2f\n", h, ((double)h)/ ((double)SYMBOL_LIST_HASH_SIZE) * 100.0,
	      ((double)b) / ((double)h) );
	t = SYMBOL_LIST_HASH_SIZE * SYMBOL_CHUNK_SIZE + c * SYMBOL_CHUNK_SIZE;
	print("Number of buckets (merged symbols) used: %lu out of %lu "
	      "(%.2f%%)\n", b, t, ((double)b)/ ((double)t) * 100.0);

	/* print_symbol_list("from hash_instrument()", FALSE); */
}
#endif /* !defined(RLD) */

/*
 * add_to_symbol_list() adds the passed merged_symbol to our linked list of
 * symbols that complements our hash table lookups.
 */
static
void
add_to_symbol_list(
struct merged_symbol *merged_symbol)
{
    struct merged_symbol_list *prev, *merged_symbol_list, *new;

	prev = NULL;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    if(merged_symbol_list->used != SYMBOL_LIST_HASH_SIZE){
		merged_symbol_list->symbols[merged_symbol_list->used] =
		    merged_symbol;
		merged_symbol_list->used += 1;
		return;
	    }
	    prev = merged_symbol_list;
	}
	new = allocate(sizeof(struct merged_symbol_list));
	prev->next = new;
	memset(new, '\0', sizeof(struct merged_symbol_list));
	new->symbols[0] = merged_symbol;
	new->used = 1;
	new->next = NULL;
}

/*
 * enter_symbol() enters the object_symbol passed to it in the merged symbol.
 * The object's string table and defintion object are also passed in.  The
 * hash_pointer points to an unused merged_symbol to fill in as previously
 * returned by lookup_symbol().
 */
static
struct merged_symbol *
enter_symbol(
struct merged_symbol *hash_pointer,
struct nlist *object_symbol,
char *object_strings,
struct object_file *definition_object)
{
    struct merged_symbol *merged_symbol;

	if((cur_obj != base_obj || strip_base_symbols == FALSE))
	    nmerged_symbols++;

	merged_symbol = hash_pointer;
	memset(merged_symbol, '\0', sizeof(struct merged_symbol));
	merged_symbol->nlist = *object_symbol;
#ifdef RLD
	if(cur_obj == base_obj && base_name == NULL)
	    merged_symbol->nlist.n_un.n_name = object_strings +
					       object_symbol->n_un.n_strx;
	else
#endif
	merged_symbol->nlist.n_un.n_name = enter_string(object_strings +
						 object_symbol->n_un.n_strx,
						 &merged_symbol->name_len);
	merged_symbol->definition_object = definition_object;
	add_to_symbol_list(merged_symbol);

	if(object_symbol->n_type == (N_UNDF | N_EXT) &&
	   object_symbol->n_value == 0)
	    add_to_undefined_list(merged_symbol);
	merged_symbol->undef_order = undef_order++;

	if(object_symbol->n_type == (N_INDR | N_EXT))
	    enter_indr_symbol(merged_symbol, object_symbol, object_strings,
			      definition_object);

	return(merged_symbol);
}

/*
 * enter_indr_symbol() enters the indirect symbol for the object_symbol passed
 * to it into the merged_symbol passed to it.
 */
static
void
enter_indr_symbol(
struct merged_symbol *merged_symbol,
struct nlist *object_symbol,
char *object_strings,
struct object_file *definition_object)
{
    struct merged_symbol *hash_pointer, *indr_symbol;

	nindr_symbols++;
	hash_pointer = lookup_symbol(object_strings + object_symbol->n_value);
	if(hash_pointer->name_len != 0){
	    indr_symbol = hash_pointer;
	}
	else{
	    indr_symbol = hash_pointer;
	    add_to_symbol_list(indr_symbol);
	    if(cur_obj != base_obj || strip_base_symbols == FALSE)
		nmerged_symbols++;
	    indr_symbol->nlist.n_type = N_UNDF | N_EXT;
	    indr_symbol->nlist.n_sect = NO_SECT;
	    if(definition_object != NULL &&
	       definition_object->dylib_module != NULL)
		indr_symbol->nlist.n_desc = REFERENCE_FLAG_UNDEFINED_LAZY;
	    else
		indr_symbol->nlist.n_desc = 0;
	    indr_symbol->nlist.n_value = 0;
#ifdef RLD
	    if(cur_obj == base_obj && base_name == NULL)
		indr_symbol->nlist.n_un.n_name = object_strings +
						 object_symbol->n_value;
	    else
#endif
	    indr_symbol->nlist.n_un.n_name = enter_string(object_strings +
						      object_symbol->n_value,
						      NULL);
	    indr_symbol->definition_object = definition_object;
	    add_to_undefined_list(indr_symbol);
	}
	merged_symbol->nlist.n_value = (unsigned long)indr_symbol;
}
/*
 * enter_string() places the symbol_name passed to it in the first string block
 * that will hold the string.  Since the string indexes will be assigned after
 * all the strings are entered putting the strings in the first block that fits
 * can be done rather than only last block.
 */
static
char *
enter_string(
char *symbol_name,
unsigned long *len_ret)
{
    struct string_block **p, *string_block;
    unsigned long len;
    char *r;

	len = strlen(symbol_name) + 1;
	if(len_ret != NULL)
	    *len_ret = len - 1;
	for(p = &(merged_string_blocks); *p; p = &(string_block->next)){
	    string_block = *p;
	    if(len > string_block->size - string_block->used)
		continue;
#ifdef RLD
	    if(string_block->set_num != cur_set)
		continue;
#endif /* RLD */
	    if(strip_base_symbols == TRUE &&
	       ((cur_obj == base_obj && string_block->base_strings == FALSE) ||
	        (cur_obj != base_obj && string_block->base_strings == TRUE) ) )
		continue;

	    if((cur_obj != NULL && cur_obj->dylib_module != NULL &&
		string_block->dylib_strings == FALSE) ||
	       ((cur_obj == NULL || cur_obj->dylib_module == NULL) &&
		string_block->dylib_strings == TRUE))
		continue;

	    r = strcpy(string_block->strings + string_block->used, symbol_name);
	    string_block->used += len;
	    if((strip_base_symbols == FALSE ||
	        string_block->base_strings == FALSE) &&
		string_block->dylib_strings == FALSE)
		merged_string_size += len;
	    return(r);
	}
	*p = allocate(sizeof(struct string_block));
	string_block = *p;
	string_block->size = (len > host_pagesize ? len : host_pagesize);
	string_block->used = len;
	string_block->next = NULL;
	string_block->strings = allocate(string_block->size);
	string_block->base_strings = cur_obj == base_obj ? TRUE : FALSE;
	if(cur_obj != NULL && cur_obj->dylib_module != NULL)
	    string_block->dylib_strings = TRUE;
	else
	    string_block->dylib_strings = FALSE;
#ifdef RLD
	string_block->set_num = cur_set;
#endif /* RLD */
	r = strcpy(string_block->strings, symbol_name);
	if((strip_base_symbols == FALSE ||
	    string_block->base_strings == FALSE) &&
	    string_block->dylib_strings == FALSE)
	    merged_string_size += len;
	return(r);
}

/*
 * add_to_undefined_list() adds a pointer to a merged symbol to the list of
 * undefined symbols.
 */
static
void
add_to_undefined_list(
struct merged_symbol *merged_symbol)
{
    struct undefined_block **p;
    struct undefined_list *new, *undefineds;
    unsigned long i;

	if(free_list.next == &free_list){
	    for(p = &(undefined_blocks); *p; p = &((*p)->next))
		;
	    *p = allocate(sizeof(struct undefined_block));
	    (*p)->next = 0;
	    undefineds = (*p)->undefineds;

	    /* add the newly allocated items to the empty free_list */
	    free_list.next = &undefineds[0];
	    undefineds[0].prev = &free_list;
	    undefineds[0].next = &undefineds[1];
	    for(i = 1 ; i < NUNDEF_BLOCKS - 1 ; i++){
		undefineds[i].prev  = &undefineds[i-1];
		undefineds[i].next  = &undefineds[i+1];
		undefineds[i].merged_symbol = NULL;
	    }
	    free_list.prev = &undefineds[i];
	    undefineds[i].prev = &undefineds[i-1];
	    undefineds[i].next = &free_list;
	}
	/* take the first one off the free list */
	new = free_list.next;
	new->next->prev = &free_list;
	free_list.next = new->next;

	/* fill in the pointer to the undefined symbol */
	new->merged_symbol = merged_symbol;

	/* put this at the end of the undefined list */
	new->prev = undefined_list.prev;
	new->next = &undefined_list;
	undefined_list.prev->next = new;
	undefined_list.prev = new;
}

/*
 * delete_from_undefined_list() is used by pass1() after a member is loaded from
 * an archive that satisifies an undefined symbol.  It is also called from
 * pass1() when it comes across a symbol on the undefined list that is no longer
 * undefined.
 */
__private_extern__
void
delete_from_undefined_list(
struct undefined_list *undefined)
{
	/* take this out of the list */
	undefined->prev->next = undefined->next;
	undefined->next->prev = undefined->prev;

	/* put this at the end of the free list */
	undefined->prev = free_list.prev;
	undefined->next = &free_list;
	free_list.prev->next = undefined;
	free_list.prev = undefined;
	undefined->merged_symbol = NULL;
}

/*
 * multiply_defined() prints and traces the multiply defined symbol if it hasn't
 * been printed yet.  It's slow with it linear searches and a reallocate() call
 * but this usually is an error case.
 */
static
void
multiply_defined(
struct merged_symbol *merged_symbol,
struct nlist *object_symbol,
char *object_strings)
{
    unsigned long i, j;

	if(allow_multiply_defined_symbols == TRUE && nowarnings == TRUE)
	    return;

	for(i = 0; i < nmultiple_defs; i++){
	    if(strcmp(multiple_defs[i], merged_symbol->nlist.n_un.n_name) == 0)
		break;
	}
	for(j = 0; j < ntrace_syms; j++){
	    if(strcmp(trace_syms[j], merged_symbol->nlist.n_un.n_name) == 0)
		break;
	}
	if(i == nmultiple_defs){
	    if(allow_multiply_defined_symbols == TRUE)
		warning("multiple definitions of symbol %s",
		      merged_symbol->nlist.n_un.n_name);
	    else{
		error("multiple definitions of symbol %s",
		      merged_symbol->nlist.n_un.n_name);
	    }
	    multiple_defs = reallocate(multiple_defs,
				       (nmultiple_defs + 1) * sizeof(char *));
	    multiple_defs[nmultiple_defs++] = merged_symbol->nlist.n_un.n_name;
	    if(j == ntrace_syms)
		trace_merged_symbol(merged_symbol);
	}
	if(j == ntrace_syms)
	    trace_object_symbol(object_symbol, object_strings);
}

/*
 * trace_object_symbol() traces a symbol that comes from an object file.
 */
static
void
trace_object_symbol(
struct nlist *symbol,
char *strings)
{
    char *indr_symbol_name;

	if(symbol->n_type == (N_INDR | N_EXT))
	    indr_symbol_name = strings + symbol->n_value;
	else
	    indr_symbol_name = "error in trace_symbol()";
	trace_symbol(strings + symbol->n_un.n_strx, symbol, cur_obj,
		     indr_symbol_name);
}

/*
 * trace_merged_symbol() traces a symbol that is in the merged symbol table.
 */
__private_extern__
void
trace_merged_symbol(
struct merged_symbol *merged_symbol)
{
    char *indr_symbol_name;

	if(merged_symbol->nlist.n_type == (N_INDR | N_EXT))
	    indr_symbol_name = ((struct merged_symbol *)
			(merged_symbol->nlist.n_value))->nlist.n_un.n_name;
	else
	    indr_symbol_name = "error in trace_symbol()";
	trace_symbol(merged_symbol->nlist.n_un.n_name, &(merged_symbol->nlist),
		     merged_symbol->definition_object, indr_symbol_name);
}

/*
 * trace_symbol() is the routine that really does the work of printing the
 * symbol its type and the file it is in.
 */
static
void
trace_symbol(
char *symbol_name,
struct nlist *nlist,
struct object_file *object_file,
char *indr_symbol_name)
{
	print_obj_name(object_file);
	if(nlist->n_type & N_PEXT)
	    print("private external ");
	switch(nlist->n_type & N_TYPE){
	case N_UNDF:
	    if(nlist->n_value == 0)
		print("%sreference to undefined %s\n",
		      nlist->n_desc & N_WEAK_REF ? "weak " : "", symbol_name);
	    else
		print("definition of common %s (size %u)\n", symbol_name,
		       nlist->n_value);
	    break;
	case N_PBUD:
	    print("%sdefinition of %s\n",
		  nlist->n_desc & N_WEAK_DEF ? "weak " : "", symbol_name);
	    break;
	case N_ABS:
	    print("definition of absolute %s (value 0x%x)\n", symbol_name,
		   (unsigned int)(nlist->n_value));
	    break;
	case N_SECT:
	    print("%sdefinition of %s in section (%.16s,%.16s)\n",
		  nlist->n_desc & N_WEAK_DEF ? "weak " : "", symbol_name,
		  object_file->section_maps[nlist->n_sect - 1].s->segname,
		  object_file->section_maps[nlist->n_sect - 1].s->sectname);
	    break;
	case N_INDR:
	    print("definition of %s as indirect for %s\n", symbol_name,
		   indr_symbol_name);
	    break;
	default:
	    print("unknown type (0x%x) of %s\n", (unsigned int)nlist->n_type,
		  symbol_name);
	    break;
	}
}

#ifndef RLD
/*
 * free_pass1_symbol_data() free()'s all symbol data only used in pass1().
 */
__private_extern__
void
free_pass1_symbol_data(void)
{
	free_undefined_list();
}
#endif /* !defined(RLD) */

/*
 * free_undefined_list() free's up the memory for the undefined list.
 */
__private_extern__
void
free_undefined_list(void)
{
    struct undefined_block *up, *undefined_block;
	/*
	 * Free the undefined list
	 */
	for(up = undefined_blocks; up; ){
	    undefined_block = up->next;
	    free(up);
	    up = undefined_block;
	}
	undefined_blocks = NULL;
	undefined_list.next = &undefined_list;
	undefined_list.prev = &undefined_list;
	free_list.next = &free_list;
	free_list.prev = &free_list;
}

/*
 * define_common_symbols() defines common symbols if there are any in the merged
 * symbol table.  The symbols are defined in the link editor reserved zero-fill
 * section (__DATA,__common) and the segment and section are created if needed.
 * The section is looked up to see it there is a section specification for it
 * and if so the same processing as in process_section_specs() is done here.
 * If there is a spec it uses the alignment if it is greater than the merged
 * alignment and warns if it is less.  Also it checks to make sure that no
 * section is to be created from a file for this reserved section.
 */
__private_extern__
void
define_common_symbols(void)
{
    struct section_spec *sect_spec;
    struct merged_section *ms;
    struct section *s;

    unsigned long i, j, common_size, align;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    struct common_symbol *common_symbol;

    struct object_list *object_list, **q;
    struct object_file *object_file;

    struct nlist *common_nlist;
    char *common_names;
    unsigned long n_strx;
#ifndef RLD
    struct mach_header *link_edit_common_object_mach_header;
#endif

#if defined(DEBUG) || defined(RLD)
	/*
	 * The compiler warning that these symbols may be used uninitialized
	 * in this function can safely be ignored.
	 */
	common_symbol = NULL;
	common_nlist = NULL;
	common_names = NULL;;
	n_strx = 0;
#endif

#ifdef RLD
	*(sets[cur_set].link_edit_common_object) =
		      link_edit_common_object;
	sets[cur_set].link_edit_common_object->set_num =
		      cur_set;
	sets[cur_set].link_edit_common_object->section_maps =
		      sets[cur_set].link_edit_section_maps;
	*(sets[cur_set].link_edit_section_maps) =
		      link_edit_section_maps;
	sets[cur_set].link_edit_section_maps->s =
		      sets[cur_set].link_edit_common_section;
	*(sets[cur_set].link_edit_common_section) =
		      link_edit_common_section;
#endif /* RLD */

#ifndef RLD
	/* see if there is a section spec for (__DATA,__common) */
	sect_spec = lookup_section_spec(SEG_DATA, SECT_COMMON);
	if(sect_spec != NULL){
	    if(sect_spec->contents_filename != NULL){
		error("section (" SEG_DATA "," SECT_COMMON ") reserved for "
		      "allocating common symbols and can't be created from the "
		      "file: %s", sect_spec->contents_filename);
		return;
	    }
	    sect_spec->processed = TRUE;
	}
#else
	sect_spec = NULL;
#endif /* !defined(RLD) */

	/* see if there is a merged section for (__DATA,__common) */
	ms = lookup_merged_section(SEG_DATA, SECT_COMMON);
	if(ms != NULL && (ms->s.flags & SECTION_TYPE) != S_ZEROFILL){
	    error("section (" SEG_DATA "," SECT_COMMON ") reserved for "
		  "allocating common symbols and exists in the loaded "
		  "objects not as a zero fill section");
	    /*
	     * Loop through all the objects and report those that have this
	     * section and then return.
	     */
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    object_file = &(object_list->object_files[i]);
		    if(object_file->dylib)
			continue;
		    if(object_file->bundle_loader)
			continue;
		    if(object_file->dylinker)
			continue;
		    for(j = 0; j < object_file->nsection_maps; j++){
			s = object_file->section_maps[j].s;
			if(strcmp(s->segname, SEG_DATA) == 0 &&
			   strcmp(s->sectname, SECT_COMMON) == 0){
			    print_obj_name(object_file);
			    print("contains section (" SEG_DATA ","
				   SECT_COMMON ")\n");
			}
		    }
		}
	    }
	    return;
	}
#ifndef RLD
	else{
	    /*
	     * This needs to be done here on the chance there is a common
	     * section but no commons get defined.  This is also done below
	     * if the common section is created.
	     */
	    if(sect_spec != NULL && sect_spec->order_filename != NULL &&
	       ms != NULL){
		ms->order_filename = sect_spec->order_filename;
		ms->order_addr = sect_spec->order_addr;
		ms->order_size = sect_spec->order_size;
	    }
	}
#endif /* !defined(RLD) */

	/*
	 * Determine if there are any commons to be defined if not just return.
	 * If a load map is requested then the number of commons to be defined
	 * is determined so a common load map can be allocated.
	 */
	commons_exist = FALSE;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if((merged_symbol->nlist.n_type & N_EXT) == N_EXT &&
		   (merged_symbol->nlist.n_type & N_TYPE) ==  N_UNDF &&
		   merged_symbol->nlist.n_value != 0){
		    /*
		     * If the output format is MH_FVMLIB then commons are not
		     * allowed because it there address may not remain fixed
		     * on sucessive link edits.  Each one is traced below.
		     */
		    if(filetype == MH_FVMLIB)
			error("common symbols not allowed with MH_FVMLIB "
			      "output format");
		    /*
		     * If the output format is multi module MH_DYLIB then			     * commons are not allowed because each symbol can only be
		     * defined in at most one module.
		     */
		    if(filetype == MH_DYLIB && multi_module_dylib == TRUE)
			error("common symbols not allowed with MH_DYLIB "
			      "output format with the -multi_module option");
		    commons_exist = TRUE;
#ifndef RLD
		    if((sect_spec != NULL &&
			sect_spec->order_filename != NULL) ||
		       dead_strip == TRUE){
			link_edit_common_symtab.nsyms++;
			link_edit_common_symtab.strsize +=
				   strlen(merged_symbol->nlist.n_un.n_name) + 1;
		    }
		    else if(load_map)
			common_load_map.ncommon_symbols++;
		    else
#endif /* !defined(RLD) */
			break;
		}
	    }
	}
	if(commons_exist == FALSE)
	    return;

	/*
	 * Now that the checks above have been done if commons are not to be
	 * defined just return.  If the output is for dyld then define common
	 * symbols always as dyld does not define commons.
	 */
	if(define_comldsyms == FALSE && output_for_dyld == FALSE)
	    return;

	/*
	 * Create the (__DATA,__common) section if needed and set the
	 * alignment for it.
	 */
	if(ms == NULL){
#ifdef RLD
	    ms = create_merged_section(sets[cur_set].link_edit_common_section);
#else
	    ms = create_merged_section(&link_edit_common_section);
#endif /* RLD */
	    if(sect_spec != NULL && sect_spec->align_specified)
		ms->s.align = sect_spec->align;
	    else
		ms->s.align = defaultsectalign;
	    if(sect_spec != NULL && sect_spec->order_filename != NULL){
		ms->order_filename = sect_spec->order_filename;
		ms->order_addr = sect_spec->order_addr;
		ms->order_size = sect_spec->order_size;
	    }
	}
	else{
	    if(sect_spec != NULL && sect_spec->align_specified){
		if(ms->s.align > sect_spec->align)
		    warning("specified alignment (0x%x) for section (" SEG_DATA
			    "," SECT_COMMON ") not used (less than the "
			    "required alignment in the input files (0x%x))",
			    (unsigned int)(1 << sect_spec->align),
			    (unsigned int)(1 << ms->s.align));
		else
		    ms->s.align = sect_spec->align;
	    }
	    if(ms->s.align < defaultsectalign)
		ms->s.align = defaultsectalign;
	}

#ifndef RLD
	/*
	 * If the common section has an order file then create a symbol table
	 * and string table for it and the load map will be generated off of
	 * these tables in layout_ordered_section() in sections.c.  If not and
	 * a load map is requested then set up the common load map.  This is
	 * used by print_load_map() in layout.c and the common_symbols allocated
	 * here are free()'ed in there also.
	 */
	if((sect_spec != NULL && sect_spec->order_filename != NULL) ||
	   dead_strip == TRUE){
	    link_edit_common_symtab.strsize =
			rnd(link_edit_common_symtab.strsize, sizeof(long));
	    link_edit_common_object.obj_size =
			sizeof(struct mach_header) +
			link_edit_common_symtab.nsyms * sizeof(struct nlist) +
			link_edit_common_symtab.strsize;
	    link_edit_common_object.obj_addr =
			allocate(link_edit_common_object.obj_size);
	    memset(link_edit_common_object.obj_addr,
		   '\0',
		   link_edit_common_object.obj_size);
	    link_edit_common_object_mach_header = (struct mach_header *)
			link_edit_common_object.obj_addr;
	    link_edit_common_object_mach_header->magic = MH_MAGIC;
	    link_edit_common_object_mach_header->filetype = MH_OBJECT;
	    link_edit_common_object_mach_header->flags =
		MH_SUBSECTIONS_VIA_SYMBOLS;
	    link_edit_common_symtab.symoff = sizeof(struct mach_header);
	    link_edit_common_symtab.stroff = sizeof(struct mach_header) +
					     link_edit_common_symtab.nsyms *
					     sizeof(struct nlist);
	    common_nlist = (struct nlist *)(link_edit_common_object.obj_addr +
			   		    link_edit_common_symtab.symoff);
	    common_names = (char *)(link_edit_common_object.obj_addr +
	    		            link_edit_common_symtab.stroff);
	    n_strx = 1;
	}
	else if(load_map){
	    common_load_map.common_ms = ms;
	    common_load_map.common_symbols = allocate(
					common_load_map.ncommon_symbols *
					sizeof(struct common_symbol));
	    common_symbol = common_load_map.common_symbols;
	}
#endif /* !defined(RLD) */

	/*
	 * Now define the commons.  This is requires building a "link editor"
	 * object file and changing these symbols to be defined in the (__DATA,
	 * __common) section in that "file".  By doing this in this way these
	 * symbols are handled normally throught the rest of the link editor.
	 * Also these symbols are trace as they are defined if they are to be
	 * traced.
	 */
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if((merged_symbol->nlist.n_type & N_EXT) == N_EXT &&
		   (merged_symbol->nlist.n_type & N_TYPE) ==  N_UNDF &&
		   merged_symbol->nlist.n_value != 0){
		    /*
		     * Commons are not allowed with MH_FVMLIB or MH_DYLIB
		     * formats so trace each one.  An error message for this
		     * has been printed above.
		     */
		    if(filetype == MH_FVMLIB ||
		       (filetype == MH_DYLIB && multi_module_dylib == TRUE))
			trace_merged_symbol(merged_symbol);
		    /* determine the alignment of this symbol */
		    common_size = merged_symbol->nlist.n_value;
		    align = 0;
		    while((unsigned long)(1 << align) < common_size &&
			  align < ms->s.align)
			align++;
		    /* round the address of the section to this alignment */
#ifdef RLD
		    sets[cur_set].link_edit_common_section->size = rnd(
		       sets[cur_set].link_edit_common_section->size, 1<< align);
#else
		    link_edit_common_section.size = rnd(
				link_edit_common_section.size, 1 << align);
#endif /* RLD */
		    /*
		     * Change this symbol's type, section number, address and
		     * object file it is defined in to be the (__DATA,__common)
		     * of the "link editor" object file at the address for it.
		     */
		    merged_symbol->nlist.n_type = N_SECT | N_EXT |
			(merged_symbol->nlist.n_type & N_PEXT);
		    merged_symbol->nlist.n_sect = 1;
#ifdef RLD
		    merged_symbol->nlist.n_value =
				   sets[cur_set].link_edit_common_section->size;
		    merged_symbol->definition_object =
				       sets[cur_set].link_edit_common_object;
		    /* Create the space for this symbol */
		    sets[cur_set].link_edit_common_section->size += common_size;
#else
		    merged_symbol->nlist.n_value =link_edit_common_section.size;
		    merged_symbol->definition_object =
						&link_edit_common_object;
		    /* Create the space for this symbol */
		    link_edit_common_section.size += common_size;
		    /*
		     * If we have an -export_symbols_list or
		     * -unexport_symbol_list option set the private extern bit
		     * on the symbol if it is not to be exported.
		     */
		    exports_list_processing(merged_symbol->nlist.n_un.n_name,
					    &(merged_symbol->nlist));
		    /*
		     * If this common symbol got made into a private extern with
		     * the processing of the exports list increment the count of
		     * private exterals.
		     */
		    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT){
			link_edit_common_object.nprivatesym++;
			nmerged_private_symbols++;
		    }
#endif /* RLD */
		    /*
		     * Do the trace of this symbol if specified now that it has
		     * been defined.
		     */
		    if(ntrace_syms != 0){
			for(j = 0; j < ntrace_syms; j++){
			    if(strcmp(trace_syms[j],
				      merged_symbol->nlist.n_un.n_name) == 0){
				trace_merged_symbol(merged_symbol);
				break;
			    }
			}
		    }
#ifndef RLD
		    /*
		     * Set the entries in the common symbol table if the section
		     * is to be ordered or in the load map if producing it
		     */
		    if((sect_spec != NULL &&
			sect_spec->order_filename != NULL) ||
	   		dead_strip == TRUE){
			common_nlist->n_un.n_strx = n_strx;
			common_nlist->n_type = N_SECT | N_EXT;
			common_nlist->n_sect = 1;
			common_nlist->n_desc = 0;
			common_nlist->n_value = merged_symbol->nlist.n_value;
			strcpy(common_names + n_strx,
			       merged_symbol->nlist.n_un.n_name);
			common_nlist++;
			n_strx += strlen(merged_symbol->nlist.n_un.n_name) + 1;
		    }
		    else if(load_map){
			common_symbol->merged_symbol = merged_symbol;
			common_symbol->common_size = common_size;
			common_symbol++;
		    }
#endif /* !defined(RLD) */
		}
	    }
	}

	/*
	 * Now that this section in this "object file" is built merged it into
	 * the merged section list (as would be done in merge_sections()).
	 */
#ifdef RLD
	sets[cur_set].link_edit_common_object->section_maps[0].output_section =
									     ms;
	ms->s.size = rnd(ms->s.size, 1 << ms->s.align);
	sets[cur_set].link_edit_common_object->section_maps[0].offset =
								     ms->s.size;
	ms->s.size += sets[cur_set].link_edit_common_section->size;
#else
	link_edit_common_object.section_maps[0].output_section = ms;
	ms->s.size = rnd(ms->s.size, 1 << ms->s.align);
	link_edit_common_object.section_maps[0].offset = ms->s.size;
	ms->s.size += link_edit_common_section.size;
#endif /* RLD */
}

#ifndef RLD
/*
 * define_undefined_symbols_a_way() is called to setup defining all remaining
 * undefined symbols as private externs.  Their final value gets set by
 * define_link_editor_dylib_symbols().
 */
__private_extern__
void
define_undefined_symbols_a_way(
void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
		   merged_symbol->nlist.n_value == 0){
		    if(dynamic == TRUE &&
		       filetype != MH_EXECUTE &&
		       merged_segments != NULL){
			define_link_editor_symbol(
				      merged_symbol->nlist.n_un.n_name,
				      N_SECT | N_PEXT | N_EXT,	/* n_type */
				      1,			/* n_sect */
				      0,			/* n_desc */
				      0);			/* n_value */
		    }
		    else{
			define_link_editor_symbol(
				      merged_symbol->nlist.n_un.n_name,
				      N_ABS | N_PEXT | N_EXT,	/* n_type */
				      NO_SECT,			/* n_sect */
				      0,			/* n_desc */
				      0);			/* n_value */
		    }
		    /*
		     * This symbol got made into a private extern so increment
		     * the count of private exterals.
		     */
		    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT){
			link_edit_symbols_object->nprivatesym++;
			nmerged_private_symbols++;
		    }
		    merged_symbol->define_a_way = 1;
		}
	    }
	}
}

static
void
setup_link_edit_symbols_object(
void)
{
	if(link_edit_symbols_object == NULL){
	    link_edit_symbols_object = new_object_file();
	    link_edit_symbols_object->file_name = "link editor";
	}
}

#ifndef RLD
/*
 * mark_globals_live() marks all merged symbol definitions which will be global
 * in the output (not private externs turned into statics) or symbols with the
 * N_NO_DEAD_STRIP bit, or symbols in sections with the S_ATTR_NO_DEAD_STRIP
 * section attribute live.  And marks the fine_reloc (if any) for each live
 * symbol live.
 */
__private_extern__
void
mark_globals_live(void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    enum bool only_referenced_dynamically;

	/*
	 * If the output is an MH_EXECUTE and there was no
	 * -exported_symbols_list or -unexported_symbols_list only symbols that
	 * are referenced dynamically are marked live and assumed to be
	 * exported.  For other outputs formats all global symbols are marked
	 * live (note that when symbols were merged the were turned into private
	 * externs if they were not to be exported).
	 */
	if(filetype == MH_EXECUTE &&
	   (save_symbols == NULL && remove_symbols == NULL))
	    only_referenced_dynamically = TRUE;
	else
	    only_referenced_dynamically = FALSE;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		/*
		 * If the symbol is marked REFERENCED_DYNAMICALLY or
		 * N_NO_DEAD_STRIP or defined in a section with the attribute
		 * S_ATTR_NO_DEAD_STRIP mark it live.  Else it has to be
		 * a defined exported symbol to be marked live.
		 */
		if((merged_symbol->nlist.n_desc &
		    REFERENCED_DYNAMICALLY) == REFERENCED_DYNAMICALLY ||

		   (merged_symbol->nlist.n_desc &
		    N_NO_DEAD_STRIP) == N_NO_DEAD_STRIP ||

		   (((merged_symbol->nlist.n_type & N_TYPE) == N_SECT) &&
		    (merged_symbol->definition_object->
		     section_maps[merged_symbol->nlist.n_sect - 1].s->flags &
		     S_ATTR_NO_DEAD_STRIP) == S_ATTR_NO_DEAD_STRIP) ){
		    goto mark_it_live;
		}
		else{
		    /*
		     * Skip symbols defined in dynamic libraries, undefined
		     * symbols and private_extern symbols.
		     */
		    if(merged_symbol->defined_in_dylib == TRUE)
			continue;
		    if((merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
			merged_symbol->nlist.n_value == 0) ||
		       merged_symbol->nlist.n_type == (N_EXT | N_PBUD))
			continue;
		    if((merged_symbol->nlist.n_type & N_EXT) &&
		       (merged_symbol->nlist.n_type & N_PEXT))
			continue;
		    if(only_referenced_dynamically == TRUE)
			continue;
		}
#ifdef DEBUG
mark_it_live:
		if(((debug & (1 << 25)) || (debug & (1 << 26)))){
		    print("** In mark_globals_live() ");
		    if(merged_symbol->nlist.n_desc & N_NO_DEAD_STRIP)
			print("no dead strip symbol ");
		    else
			print("exported symbol ");
		    print_obj_name(merged_symbol->definition_object);
		    print("%s\n", merged_symbol->nlist.n_un.n_name);
		}
#endif /* DEBUG */
		merged_symbol->live = TRUE;
		if(merged_symbol->fine_reloc != NULL)
		    merged_symbol->fine_reloc->live = TRUE;
	    }
	}
}

/*
 * mark_N_NO_DEAD_STRIP_local_symbols_live() is called to cause the fine_relocs
 * for local symbols that have the N_NO_DEAD_STRIP bit set to be marked live.
 */
__private_extern__
void
mark_N_NO_DEAD_STRIP_local_symbols_live(void)
{
    struct merged_segment *msg, **r;
    struct merged_section *ms, **content, **zerofill;

	/*
	 * In merged_symbols() this gets set to TRUE if there were any local
	 * symbols with the N_NO_DEAD_STRIP set.  If not we don't need to do
	 * anything here.
	 */
	if(local_NO_DEAD_STRIP_symbols == FALSE)
	    return;

	r = &merged_segments;
	while(*r){
	    msg = *r;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		mark_N_NO_DEAD_STRIP_local_symbols_in_section_live(ms);
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		mark_N_NO_DEAD_STRIP_local_symbols_in_section_live(ms);
		zerofill = &(ms->next);
	    }
	    r = &(msg->next);
	}
}

/*
 * mark_N_NO_DEAD_STRIP_local_symbols_in_section_live() for the specified
 * merged section marks fine_relocs for local symbols that have the
 * N_NO_DEAD_STRIP bit set live.
 */
static
void
mark_N_NO_DEAD_STRIP_local_symbols_in_section_live(
struct merged_section *ms)
{
    unsigned long i, j, k, nsect, input_offset;
    struct object_list *object_list, **q;
    struct section_map *map;
    struct nlist *object_symbols;
    char *object_strings;
    struct fine_reloc *fine_reloc;

	/*
	 * For each object file that has this section process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		if(cur_obj == base_obj)
		    continue;
		if(cur_obj->dylib)
		    continue;
		if(cur_obj->bundle_loader)
		    continue;
		if(cur_obj->dylinker)
		    continue;
		map = NULL;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].output_section != ms)
			continue;
		    if(cur_obj->section_maps[j].s->size == 0)
			continue;
		    map = &(cur_obj->section_maps[j]);
		    break;
		}
		if(map == NULL)
		    continue;
		object_symbols = NULL;
		object_strings = NULL;
		if(cur_obj->symtab != NULL){
		    object_symbols = (struct nlist *)(cur_obj->obj_addr +
						      cur_obj->symtab->symoff);
		    object_strings = (char *)(cur_obj->obj_addr +
					      cur_obj->symtab->stroff);
		}
		nsect = j + 1;
		/*
		 * Now look through the symbol table for local symbols in this
		 * section that are marked with the N_NO_DEAD_STRIP bit.
		 */
		for(k = 0; k < cur_obj->symtab->nsyms; k++){
		    if((object_symbols[k].n_type & N_EXT) == 0 &&
		       (object_symbols[k].n_type & N_TYPE) == N_SECT &&
		       (object_symbols[k].n_type & N_STAB) == 0 &&
		       object_symbols[k].n_sect == nsect &&
		       object_symbols[k].n_desc & N_NO_DEAD_STRIP){
			input_offset = object_symbols[k].n_value - map->s->addr;
			fine_reloc = fine_reloc_for_input_offset(
			    map, input_offset);
			fine_reloc->live = TRUE;
		    }
		}
	    }
	}
}

/*
 * set_fine_relocs_for_merged_symbols() is called when -dead_strip is specified
 * to set the fine_reloc field of the merged symbols.  Most of these are set
 * in layout_ordered_section() but when a section from an object is linked
 * as one block they are not set.  So this is done here.
 */
__private_extern__
void
set_fine_relocs_for_merged_symbols(void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		/*
		 * Skip symbols that have already had there fine_reloc set which
		 * happens if the block was created using symbol's addresses.
		 */
		if(merged_symbol->fine_reloc != NULL)
		    continue;
		/*
		 * If this symbol is defined in a dylib or undefined then
		 * it will not have a fine_reloc block.
		 */
		if(merged_symbol->defined_in_dylib == TRUE ||
		   merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		   (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD)
		    continue;
		/*
		 * Skip symbols only referenced from dynamic libraries.
		 */
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    continue;

		/*
		 * The remaining symbols might be in a fine_reloc so set it
		 * up if it has one (note this still maybe NULL).
		 */
		merged_symbol->fine_reloc = get_fine_reloc_for_merged_symbol(
						merged_symbol, NULL);

	    }
	}
}

/*
 * count_live_symbols() is called when -dead_strip is specified after things
 * have been marked live.  It adjust the counts and reference maps of symbols
 * to account for just the live symbols.
 */
__private_extern__
void
count_live_symbols(void)
{
    unsigned long i, j, nrefsym;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    struct object_list *object_list, **q;
    struct object_file *obj;
    struct merged_segment *msg, **r;
    struct merged_section *ms, **content, **zerofill;

	/*
	 * In order to not put out strings for merged symbols that are not live
	 * we need to rebuild the merged string table for only the live symbols.
	 * This is done by resetting these two variables and recalling
	 * enter_string() on the live symbols.
	 */
	merged_string_blocks = NULL;
	merged_string_size = 0;

	/*
	 * The value of nstripped_merged_symbols is incremented here for each
	 * merged symbol that would have been in the output but is not live.
	 * Note: nstripped_merged_symbols can be reset to zero later in
	 * assign_output_symbol_indexes() if strip_level is set to
	 * STRIP_DYNAMIC_EXECUTABLE.  This works since any symbol to be
	 * saved with STRIP_DYNAMIC_EXECUTABLE would also be live since
	 * it would have REFERENCED_DYNAMICALLY set.
	 */
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		/*
		 * Because a live fine_reloc's could have multiple global
		 * symbols, we need to check all global symbols to see if they
		 * are in a live block.  And if so mark the global symbol live.
		 */
		if(merged_symbol->live == FALSE){
		    if(merged_symbol->fine_reloc != NULL &&
		       merged_symbol->fine_reloc->live == TRUE)
			merged_symbol->live = TRUE;
		}
		/*
		 * Skip symbols only referenced from dynamic libraries.
		 */
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    continue;
		if(merged_symbol->live == TRUE){
		    merged_symbol->nlist.n_un.n_name =
			enter_string(merged_symbol->nlist.n_un.n_name, NULL);
		}
		else{
		    /*
		     * This symbol is not live so account for it the number of
		     * stripped merged symbols.
		     */
		    nstripped_merged_symbols++;
/*
printf("count_live_symbols() nstripped_merged_symbols %s\n", merged_symbol->nlist.n_un.n_name);
*/

		    /*
		     * If this symbol is defined in a dylib or undefined then
		     * we are done accounting for it being stripped.
		     */
		    if(merged_symbol->defined_in_dylib == TRUE ||
		       merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
                       (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD)
			continue;
		    /*
		     * This symbol was defined in the .o files so adjust the
		     * counts in the object_file struct.
		     */

		    /*
		     * If this is a private symbol adjust the count of
		     * private symbols in this object.  Also increment the
		     * file level static variable for the number of private
		     * merged symbols that are being stripped so later
		     * assign_output_symbol_indexes() can use its value do its
		     * consistency check.
		     */
		    if(merged_symbol->nlist.n_type & N_PEXT){
			merged_symbol->definition_object->nprivatesym--;
			nstripped_merged_private_symbols++;
/*
printf("count_live_symbols() nstripped_merged_private_symbols %s\n", merged_symbol->nlist.n_un.n_name);
*/
		    }
		    /*
		     * This symbol is defined in the .o file and not a
		     * private symbol, so if we are creating a multi-module
		     * dylib this files to adjust the count of the defined
		     * externals in this object.
		     */
		    else if(filetype == MH_DYLIB &&
			    multi_module_dylib == TRUE)
			merged_symbol->definition_object->nextdefsym--;
		}
	    }
	}

	/*
	 * If we are creating a multi-module dylib then we need to update the
	 * reference table and the number of references for each object to only
	 * contain live symbols.
	 */
	output_dysymtab_info.dysymtab_command.nextrefsyms = 0;
	if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    obj = &(object_list->object_files[i]);
		    if(obj->dylib)
			continue;
		    if(obj->bundle_loader)
			continue;
		    if(obj->dylinker)
			continue;
		    nrefsym = 0;
		    for(j = 0; j < obj->nrefsym; j++){
			if(obj->reference_maps[j].merged_symbol->live){
			    obj->reference_maps[nrefsym] =
				obj->reference_maps[j];
			    nrefsym++;
			}
		    }
		    obj->nrefsym = nrefsym;
		    if(nrefsym != 0)
			obj->irefsym =
			    output_dysymtab_info.dysymtab_command.nextrefsyms;
		    output_dysymtab_info.dysymtab_command.nextrefsyms +=
			nrefsym;
		}
	    }
	}

	/*
	 * To get the global variable nlocal_symbols and the nlocal_symbols
	 * field in the object_file structs adjusted to include just live
	 * symbols call removed_dead_local_symbols_in_section() which will call
	 * discard_local_symbols_for_section() for each section in each object.
	 */
	r = &merged_segments;
	while(*r){
	    msg = *r;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		removed_dead_local_symbols_in_section(ms);
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		removed_dead_local_symbols_in_section(ms);
		zerofill = &(ms->next);
	    }
	    r = &(msg->next);
	}

	/*
 	 * Call remove_dead_N_GSYM_stabs() to get rid of dead N_GSYM stabs.
	 * For common symbols this has to be done as they are not defined or
 	 * in a section and so they can't be bracked by N_BNSYM/N_ENSYM stabs.
	 */
	remove_dead_N_GSYM_stabs();
}

/*
 * removed_dead_local_symbols_in_section() is pass a pointer to a merged section
 * and then calls discard_local_symbols_for_section() for each object with that
 * section to get the global variable nlocal_symbols and the nlocal_symbols
 * field in the object_file structs adjusted to include just live symbols.
 */
static
void
removed_dead_local_symbols_in_section(
struct merged_section *ms)
{
    unsigned long i, j;
    struct object_list *object_list, **q;
    struct section_map *map;
    struct nlist *object_symbols;
    char *object_strings;

	/*
	 * For each object file that has this section process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		if(cur_obj == base_obj)
		    continue;
		if(cur_obj->dylib)
		    continue;
		if(cur_obj->bundle_loader)
		    continue;
		if(cur_obj->dylinker)
		    continue;
		map = NULL;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].output_section != ms)
			continue;
		    if(cur_obj->section_maps[j].s->size == 0)
			continue;
		    map = &(cur_obj->section_maps[j]);
		    break;
		}
		if(map == NULL)
		    continue;
		object_symbols = NULL;
		object_strings = NULL;
		if(cur_obj->symtab != NULL){
		    object_symbols = (struct nlist *)(cur_obj->obj_addr +
						      cur_obj->symtab->symoff);
		    object_strings = (char *)(cur_obj->obj_addr +
					      cur_obj->symtab->stroff);
		}
		discard_local_symbols_for_section(j + 1,
		    object_symbols, object_strings,
		    cur_obj->section_maps[j].s, map);
	    }
	}
}

/*
 * remove_dead_N_GSYM_stabs() is used by count_live_symbols() when -dead_strip
 * is specified to get rid of dead N_GSYM stabs.  For common symbols this has
 * to be done as they are not defined or in a section and so they can't be
 * bracked by N_BNSYM/N_ENSYM stabs.
 */
static
void
remove_dead_N_GSYM_stabs(
void)
{
    unsigned long i;
    struct object_list *object_list, **q;
    struct nlist *object_symbols;
    char *object_strings;

	/*
	 * For each object that has symbols process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		if(cur_obj == base_obj)
		    continue;
		if(cur_obj->dylib)
		    continue;
		if(cur_obj->bundle_loader)
		    continue;
		if(cur_obj->dylinker)
		    continue;
		if(cur_obj->symtab != NULL){
		    object_symbols = (struct nlist *)(cur_obj->obj_addr +
						      cur_obj->symtab->symoff);
		    object_strings = (char *)(cur_obj->obj_addr +
					      cur_obj->symtab->stroff);
		    remove_dead_N_GSYM_stabs_for_cur_obj(object_symbols,
			object_strings);

		}
	    }
	}
}
#endif /* !defined(RLD) */

/*
 * define_link_editor_execute_symbols() is called when the output file type is
 * MH_EXECUTE and it sets the address of the loader defined symbols for this
 * file type.  For the MH_EXECUTE file type there are two loader defined symbols
 * which are the address of the header.  Since these symbols are not in a 
 * section (it is before the first section) they are absolute symbols.
 */
__private_extern__
void
define_link_editor_execute_symbols(
unsigned long header_address)
{
    struct merged_symbol *merged_symbol;

	/* look up the first symbol to see if it is present */
	merged_symbol = lookup_symbol(_MH_EXECUTE_SYM);
	/* if it is present set it's correct value */
	if(merged_symbol->name_len != 0)
	    merged_symbol->nlist.n_value = header_address;

	/* look up the second symbol to see if it is present */
	merged_symbol = lookup_symbol("___dso_handle");
	/* if it is present set it's correct value */
	if(merged_symbol->name_len != 0)
	    merged_symbol->nlist.n_value = header_address;
}

#ifndef RLD
/*
 * setup_link_editor_symbols() is called when the output file type can be an
 * output for dyld and it sets up the loader defined symbols for the file
 * type. These symbols have to be set up (defined and made a private extern)
 * before their real addresses are known so that the dylib tables and the
 * relocation entries can be laied out.  For the MH_DYLIB, MH_BUNDLE and
 * MH_DYLINKER file types the loader defined symbols, which is the address of
 * the header, must be relative to the sections even thought it is not in a
 * section (it is before the first section).  So it is set as the an address
 * relative to the first section.  This is done since these output files can be
 * slid by the dynamic link editor.  Also for these file types the symbol is
 * also made a private extern.
 */
__private_extern__
void
setup_link_editor_symbols(
void)
{
	if(filetype == MH_EXECUTE){
	    setup_link_editor_symbol(_MH_EXECUTE_SYM);
	    setup_link_editor_symbol("___dso_handle");
	}
	else if(filetype == MH_BUNDLE){
	    setup_link_editor_symbol(_MH_BUNDLE_SYM);
	    setup_link_editor_symbol("___dso_handle");
	}
	else if(filetype == MH_DYLIB){
	    setup_link_editor_symbol(_MH_DYLIB_SYM);
	    setup_link_editor_symbol("___dso_handle");
	}
	else{ /* filetype == MH_DYLINKER */
	    setup_link_editor_symbol(_MH_DYLINKER_SYM);
	    setup_link_editor_symbol("___dso_handle");
	}
}

/*
 * setup_link_editor_symbol() does the real work of setting up a single loader
 * defined symbol for the name passed to it.
 */ 
static
void
setup_link_editor_symbol(
char *symbol_name)
{
    struct merged_symbol *merged_symbol;
    unsigned long nsects, i, j, n;
    struct section *sections;
    struct section_map *section_maps;
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;

	/* look up the symbol to see if it is present */
	merged_symbol = lookup_symbol(symbol_name);
	/* if it is not present just return */
	if(merged_symbol->name_len == 0)
	    return;
	/*
	 * For MH_BUNDLE files we need to special case the handling of the
	 * link editor defined symbol ___dso_handle since it is allowed to
	 * be defined in the -bundle_loader file.  But it may not referenced
	 * from any of the the objects being linked.  In this case we treat it
	 * like the symbol is not present and just return.
	 */
	if(filetype == MH_BUNDLE &&
	   strcmp(symbol_name, "___dso_handle") == 0 &&
	   merged_symbol->definition_object->bundle_loader == TRUE &&
	   merged_symbol->non_dylib_referenced_obj == NULL)
	    return;

	/*
	 * For MH_EXECUTE file types the symbol is always absolute so just
	 * defined it with a value of zero for now.
	 */
	if(filetype == MH_EXECUTE){
	    define_link_editor_symbol(symbol_name, N_EXT | N_ABS, NO_SECT,
		merged_symbol->nlist.n_desc & REFERENCED_DYNAMICALLY, 0);
	    return;
	}

	/*
	 * For the MH_DYLIB, MH_BUNDLE and MH_DYLINKER file types set up the
	 * defining object file with the correct values for defining one more
	 * private external symbol.
	 */
	setup_link_edit_symbols_object();
	n = link_edit_symbols_object->nprivatesym;
	link_edit_symbols_object->nprivatesym += 1;
	nmerged_private_symbols++;

	link_edit_symbols_object->nrefsym += 1;
	if(n == 0)
	    link_edit_symbols_object->irefsym =
		    output_dysymtab_info.dysymtab_command.nextrefsyms;
	if(filetype == MH_DYLIB)
	    output_dysymtab_info.dysymtab_command.nextrefsyms += 1;
	link_edit_symbols_object->reference_maps =
	    reallocate(link_edit_symbols_object->reference_maps,
		       sizeof(struct reference_map) * (n + 1));
	link_edit_symbols_object->reference_maps[n].flags =
	    REFERENCE_FLAG_PRIVATE_DEFINED;
	link_edit_symbols_object->reference_maps[n].merged_symbol =
	    merged_symbol;

	/* count the number of merged sections */
	nsects = 0;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    nsects += msg->sg.nsects;
	    p = &(msg->next);
	}

	if(nsects > 0 && link_edit_symbols_object->nsection_maps == 0){
	    /*
	     * Create the sections and section maps for the sections in the
	     * "link editor" object file.  To make it easy all merged sections
	     * will be in this object file.  The addr in all of the sections
	     * and the offset in all the maps will be zero so that
	     * layout_symbols() will set the final value of these symbols
	     * to their correct location in the output file.
	     */
	    sections = allocate(nsects * sizeof(struct section));
	    memset(sections, '\0', nsects * sizeof(struct section));
	    section_maps = allocate(nsects * sizeof(struct section_map));
	    memset(section_maps, '\0', nsects * sizeof(struct section_map));
	    setup_link_edit_symbols_object();
	    link_edit_symbols_object->nsection_maps = nsects;
	    link_edit_symbols_object->section_maps = section_maps;

	    i = 0;
	    p = &merged_segments;
	    while(*p){
		msg = *p;
		for(j = 0; j < 2 ; j++){
		    if(j == 0)
			/* process the content sections */
			q = &(msg->content_sections);
		    else
			/* process the zerofill sections */
			q = &(msg->zerofill_sections);
		    while(*q){
			ms = *q;
			/* create the section and map for this section */
			strncpy(sections[i].sectname, ms->s.sectname,
				sizeof(ms->s.sectname));
			strncpy(sections[i].segname, ms->s.segname,
				sizeof(ms->s.segname));
			section_maps[i].s = &(sections[i]);
			section_maps[i].output_section = ms;
			i++;
			q = &(ms->next);
		    }
		}
		p = &(msg->next);
	    }
	}
	if(nsects > 0)
	    define_link_editor_symbol(symbol_name, N_SECT | N_PEXT | N_EXT,
		1, merged_symbol->nlist.n_desc & REFERENCED_DYNAMICALLY, 0);
	else
	    define_link_editor_symbol(symbol_name, N_ABS | N_PEXT | N_EXT,
	      NO_SECT, merged_symbol->nlist.n_desc & REFERENCED_DYNAMICALLY, 0);
}

/*
 * define_link_editor_dylib_symbols() is called when the output file type is
 * MH_DYLIB, MH_BUNDLE or MH_DYLINKER and it defines the loader defined symbols
 * for these file types.  This routine actually sets the value of the symbols
 * where as the above routine defines the symbol. For these file types there are
 * two loader defined symbols which are the address of the header.  Since these
 * output files can be slid this symbol must be relative to the sections even
 * thought it is not in a section (it is before the first section) it is set as
 * the an address relative to the first section.  This symbol is also a private 
 * extern. This routine also sets the define_a_way symbols to their final value.
 */
__private_extern__
void
define_link_editor_dylib_symbols(
unsigned long header_address)
{
    struct merged_symbol *merged_symbol;
    struct merged_symbol_list *merged_symbol_list;
    unsigned long i;

	if(filetype == MH_BUNDLE)
	    define_link_editor_dylib_symbol(header_address, _MH_BUNDLE_SYM);
	else if(filetype == MH_DYLIB)
	    define_link_editor_dylib_symbol(header_address, _MH_DYLIB_SYM);
	else /* filetype == MH_DYLINKER */
	    define_link_editor_dylib_symbol(header_address, _MH_DYLINKER_SYM);
	define_link_editor_dylib_symbol(header_address, "___dso_handle");

	/* set the correct values of the undefined symbols defined a way */
	if(undefined_flag == UNDEFINED_DEFINE_A_WAY){
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
		merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->define_a_way == 1){
			if(merged_symbol->nlist.n_sect == NO_SECT)
			    merged_symbol->nlist.n_value = header_address;
			else
			    merged_symbol->nlist.n_value = header_address -
			       link_edit_symbols_object->section_maps[0].
				    output_section->s.addr;
		    }
		}
	    }
	}
}

/*
 * define_link_editor_dylib_symbol() does the real work of setting the address
 * of the loader defined symbols for filetypes that can be slid.
 */
static
void
define_link_editor_dylib_symbol(
unsigned long header_address,
char *symbol_name)
{
    struct merged_symbol *merged_symbol;

	/* look up the symbol to see if it is present */
	merged_symbol = lookup_symbol(symbol_name);

	/* if it is not present just return */
	if(merged_symbol->name_len == 0)
	    return;

	/* set it's correct value */
	if(merged_symbol->nlist.n_sect == NO_SECT)
	    merged_symbol->nlist.n_value = header_address;
	else
	    merged_symbol->nlist.n_value = header_address -
	       link_edit_symbols_object->section_maps[0].output_section->s.addr;

}
#endif /* !defined(RLD) */

/*
 * define_link_editor_preload_symbols() is called when the output file type is
 * MH_PRELOAD and it defines the loader defined symbols for this file type.
 * For the MH_PRELOAD file type there are loader defined symbols for the
 * beginning and ending of each segment and section.  Their names are of the
 * form: <segname>{,<sectname>}{__begin,__end} .  They are N_SECT symbols for
 * the closest section they belong to (in some cases the *__end symbols will
 * be outside the section).
 */
__private_extern__
void
define_link_editor_preload_symbols(
enum bool setup)
{
    unsigned long nsects, i, j, first_section;
    struct section *sections;
    struct section_map *section_maps;
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;
    struct merged_symbol *merged_symbol;
    char symbol_name[sizeof(ms->s.segname) + sizeof(ms->s.sectname) +
		     sizeof("__begin")];

	sections = NULL;
	section_maps = NULL;
	if(setup == TRUE){
	    /* count the number of merged sections */
	    nsects = 0;
	    p = &merged_segments;
	    while(*p){
		msg = *p;
		nsects += msg->sg.nsects;
		p = &(msg->next);
	    }

	    /*
	     * Create the sections and section maps for the sections in the
	     * "link editor" object file.  To make it easy all merged sections
	     * will be in this object file.  The addr in all of the sections
	     * and the offset in all the maps will be zero so that
	     * layout_symbols() will set the final value of these symbols
	     * to their correct location in the output file.
	     */
	    sections = allocate(nsects * sizeof(struct section));
	    memset(sections, '\0', nsects * sizeof(struct section));
	    section_maps = allocate(nsects * sizeof(struct section_map));
	    memset(section_maps, '\0', nsects * sizeof(struct section_map));
	    setup_link_edit_symbols_object();
	    link_edit_symbols_object->nsection_maps = nsects;
	    link_edit_symbols_object->section_maps = section_maps;
	}

	i = 0;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    /* create the symbol for the beginning of the segment */
	    strncpy(symbol_name, msg->sg.segname, sizeof(msg->sg.segname));
	    strcat(symbol_name, "__begin");
	    if(setup == TRUE)
		define_link_editor_symbol(symbol_name, N_EXT | N_SECT, i+1,0,0);
	    first_section = i + 1;
	    for(j = 0; j < 2 ; j++){
		if(j == 0)
		    /* process the content sections */
		    q = &(msg->content_sections);
		else
		    /* process the zerofill sections */
		    q = &(msg->zerofill_sections);
		while(*q){
		    ms = *q;
		    /* create the section and map for this section */
		    if(setup == TRUE){
			strncpy(sections[i].sectname, ms->s.sectname,
				sizeof(ms->s.sectname));
			strncpy(sections[i].segname, ms->s.segname,
				sizeof(ms->s.segname));
			section_maps[i].s = &(sections[i]);
			section_maps[i].output_section = ms;
		    }
		    /* create the symbol for the beginning of the section */
		    strncpy(symbol_name, ms->s.segname,
			    sizeof(ms->s.segname));
		    strncat(symbol_name, ms->s.sectname,
			    sizeof(ms->s.sectname));
		    strcat(symbol_name, "__begin");
		    if(setup == TRUE)
			define_link_editor_symbol(symbol_name, N_EXT | N_SECT,
						  i+1, 0, 0);
		    /* create the symbol for the end of the section */
		    strncpy(symbol_name, ms->s.segname,
			    sizeof(ms->s.segname));
		    strncat(symbol_name, ms->s.sectname,
			    sizeof(ms->s.sectname));
		    strcat(symbol_name, "__end");
		    if(setup)
			define_link_editor_symbol(symbol_name, N_EXT | N_SECT,
						  i+1, 0, 0);
		    else{
			merged_symbol = lookup_symbol(symbol_name);
			if(merged_symbol->name_len != 0)
			    merged_symbol->nlist.n_value = ms->s.size;
		    }
		    i++;
		    q = &(ms->next);
		}
	    }

	    /* create the symbol for the end of the segment */
	    strncpy(symbol_name, msg->sg.segname,
		    sizeof(msg->sg.segname));
	    strcat(symbol_name, "__end");
	    if(setup)
		define_link_editor_symbol(symbol_name, N_EXT | N_SECT,
					  first_section, 0, 0);
	    else{
		merged_symbol = lookup_symbol(symbol_name);
		if(merged_symbol->name_len != 0)
		    merged_symbol->nlist.n_value = msg->sg.vmsize;
	    }
	    p = &(msg->next);
	}
}

/*
 * define_link_editor_symbol() is passed then name of a link editor defined
 * symbol and the information to define it.  If this symbol exist it must be
 * undefined or it is an error.  If it exist and link editor defined symbols
 * are being defined it is defined using the information passed to it.
 */
static
void
define_link_editor_symbol(
char *symbol_name,
unsigned char type,
unsigned char sect,
short desc,
unsigned long value)
{
    unsigned long i;
    struct merged_symbol *merged_symbol;

	/* look up the symbol to see if it is present */
	merged_symbol = lookup_symbol(symbol_name);
	/* if it is not present just return */
	if(merged_symbol->name_len == 0)
	    return;
	/*
	 * The symbol is present and must be undefined unless it is defined
	 * in the base file of an incremental link or in the -bundle_loader
	 * file (for the case of the ___dso_handle symbol).
	 */
	if((merged_symbol->nlist.n_type & N_EXT) != N_EXT ||
	   (merged_symbol->nlist.n_type & N_TYPE) != N_UNDF ||
	   merged_symbol->nlist.n_value != 0){
	    if(merged_symbol->definition_object != base_obj &&
	       merged_symbol->definition_object->bundle_loader == FALSE){
		error("loaded objects attempt to redefine link editor "
		      "defined symbol %s", symbol_name);
		trace_merged_symbol(merged_symbol);
		return;
	    }
	}

	/*
	 * Now that the checks above have been done if link editor defined
	 * symbols are not to be defined just return.
	 */
	if(define_comldsyms == FALSE)
	    return;

	/* define this symbol */
	setup_link_edit_symbols_object();
	merged_symbol->nlist.n_type = type;
	merged_symbol->nlist.n_sect = sect;
	merged_symbol->nlist.n_desc = desc;
	merged_symbol->nlist.n_value = value;
	merged_symbol->definition_object = link_edit_symbols_object;

#ifndef RLD
	/*
	 * If this symbol is already a private extern then it does not get
	 * processed with the export lists.
	 */
	if((merged_symbol->nlist.n_type & N_PEXT) != N_PEXT){
	    /*
	     * If we have an -export_symbols_list or
	     * -unexport_symbol_list option set the private extern bit
	     * on the symbol if it is not to be exported.
	     */
	    exports_list_processing(merged_symbol->nlist.n_un.n_name,
				    &(merged_symbol->nlist));
	    /*
	     * If this symbol got made into a private extern with the processing
	     * of the exports list increment the count of private exterals.
	     */
	    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT){
		merged_symbol->definition_object->nprivatesym++;
		nmerged_private_symbols++;
	    }
	}
#endif

	/*
	 * Do the trace of this symbol if specified now that it has
	 * been defined.
	 */
	if(ntrace_syms != 0){
	    for(i = 0; i < ntrace_syms; i++){
		if(strcmp(trace_syms[i], symbol_name) == 0){
		    trace_merged_symbol(merged_symbol);
		    break;
		}
	    }
	}
}
#endif /* !defined(RLD) */

/*
 * reduce_indr_symbols() reduces indirect symbol chains to have all the indirect
 * symbols point at their leaf symbol.  Also catch loops of indirect symbols.
 */
__private_extern__
void
reduce_indr_symbols(void)
{
    unsigned long i, j, k, indr_depth, from_dylibs, not_from_dylibs;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol, **indr_symbols, *indr_symbol;
    struct indr_symbol_pair *indr_symbol_pair;

	indr_symbols = allocate(nindr_symbols * sizeof(struct merged_symbol *));
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];

		/*
		 * Reduce indirect symbol chains to have all the indirect
		 * symbols point at their leaf symbol.  Also catch loops of
		 * indirect symbols.  If an indirect symbol was previously
		 * in a loop it's n_value is set to zero so not to print the
		 * loop more than once.
		 */
		if(merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		   merged_symbol->nlist.n_value != 0){
		    if(merged_symbol->defined_in_dylib == TRUE){
			from_dylibs = 1;
			not_from_dylibs = 0;
		    }
		    else{
			from_dylibs = 0;
			not_from_dylibs = 1;
		    }
		    indr_symbols[0] = merged_symbol;
		    indr_depth = 1;
		    indr_symbol = (struct merged_symbol *)
						(merged_symbol->nlist.n_value);
		    while(indr_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		          indr_symbol->nlist.n_value != 0){
			for(j = 0; j < indr_depth; j++){
			    if(indr_symbols[j] == indr_symbol)
				break;
			}
			if(j == indr_depth){
			    if(indr_symbol->defined_in_dylib == TRUE)
				from_dylibs++;
			    else
				not_from_dylibs++;
			    indr_symbols[indr_depth++] = indr_symbol;
			    indr_symbol = (struct merged_symbol *)
						(indr_symbol->nlist.n_value);
			}
			else{
			    error("indirect symbol loop:");
			    for(k = j; k < indr_depth; k++){
				trace_merged_symbol(indr_symbols[k]);
				indr_symbols[k]->nlist.n_value = 0;
			    }
			    indr_symbol->nlist.n_value = 0;
			}
		    }
		    /*
		     * If this N_INDR chain has symbols both from dylib and
		     * not from dylibs record a pair for each merged symbol
		     * not defined in a dylib and the first in the chain
		     * defined in a dylib.
		     */
		    if(from_dylibs != 0 && not_from_dylibs != 0 &&
		       indr_symbol->nlist.n_type != (N_EXT | N_INDR)){
			for(j = 0; j < indr_depth; j++){
			    if(indr_symbols[j]->defined_in_dylib == FALSE){
				for(k = j + 1; k < indr_depth; k++){
				    if(indr_symbols[k]->defined_in_dylib)
					break;
				}
				indr_symbol_pairs = reallocate(
				    indr_symbol_pairs,
				    sizeof(struct indr_symbol_pair) *
					(nindr_symbol_pairs + 1));
				indr_symbol_pair = indr_symbol_pairs +
						   nindr_symbol_pairs;
				nindr_symbol_pairs++;
				indr_symbol_pair->merged_symbol = merged_symbol;
				if(k < indr_depth &&
				   indr_symbols[k]->defined_in_dylib)
				    indr_symbol_pair->indr_symbol =
					indr_symbols[k];
				else
				    indr_symbol_pair->indr_symbol =
					indr_symbol;
			    }
			}
		    }
		    if(indr_symbol->nlist.n_type != (N_EXT | N_INDR)){
			for(j = 0; j < indr_depth; j++){
			    indr_symbols[j]->nlist.n_value =
						    (unsigned long)indr_symbol;
			    /*
			     * If this indirect symbol is pointing to a
			     * private extern then increment the count of
			     * private exterals.
			     */
			    if((indr_symbol->nlist.n_type & N_PEXT) == N_PEXT){
				indr_symbols[j]->definition_object->
				    nprivatesym++;
				nmerged_private_symbols++;
			    }
			}
		    }
		}
	    }
	}
	free(indr_symbols);
}

/*
 * layout_merged_symbols() sets the values and section numbers of the merged
 * symbols.
 */
__private_extern__
void
layout_merged_symbols(void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		relocate_symbol(&(merged_symbol->nlist),
				merged_symbol->definition_object);
	    }
	}
	/*
	 * Set this global variable to let routines that look at merged symbols
	 * in both the first pass and second pass that the merged symbols are
	 * relocated.  This is need for example when looking at the n_sect value
	 * of a merged symbol.  In the first pass it can be used as an index
	 * into definition_object->section_maps[] but in the second pass it can
	 * not.
	 */
	merged_symbols_relocated = TRUE;

	/*
	 * The MH_NOUNDEFS flag is set only if there are no undefined symbols
	 * or commons left undefined.  This is only set if we think the file is
	 * executable as the execute bits are based on this.
	 */
	if(noundefs == TRUE &&
	   (define_comldsyms == TRUE || commons_exist == FALSE))
	    output_mach_header.flags |= MH_NOUNDEFS;
}

/*
 * discard_local_symbols_for_section() is used by coalesced_section_merge(),
 * indirect_section_merge() and removed_dead_local_symbols_in_section() (when
 * -dead_strip is specified) to deal with the symbol table entries for local
 * symbols and N_STAB symbols in this section in the current object file after
 * the fine relocation entries have been set up to determined for which items
 * the contents will be used from current object file object file.
 */
__private_extern__
void
discard_local_symbols_for_section(
unsigned long nsect,
struct nlist *object_symbols,
char *object_strings,
struct section *s,
struct section_map *section_map)
{
    unsigned long i, j, k, output_strlen;
    struct localsym_block *localsym_block, **next_localsym_block;
    struct fine_reloc *fine_reloc;
    enum bool closed_last_block;
    size_t * debug_ptr;

	/*
	 * Previouly in merge_symbols(), nlocal_symbols and local_string_size
	 * were incremented for all symbols in this section. So now we decrement
	 * these variables for symbols in the items that will not be in the
	 * output file.
	 */
	localsym_block = cur_obj->localsym_blocks;
	next_localsym_block = &(cur_obj->localsym_blocks);
	debug_ptr = cur_obj->dwarf_source_data;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    if (debug_ptr && *debug_ptr < i) {
	      for (debug_ptr += 2; *debug_ptr & 0x80000000; debug_ptr++) ;
	    }
	    /* skip blocks of symbols that have already been removed */
	    if(localsym_block != NULL && localsym_block->index == i){
		i += localsym_block->count - 1; /* the loop will do i++ */
		next_localsym_block = &(localsym_block->next);
		localsym_block = localsym_block->next;
		continue;
	    }
	    /*
	     * See if this is a local symbol for this section that would be in
	     * the output file.
	     */
	    if((object_symbols[i].n_type & N_EXT) == 0 &&
	       ((object_symbols[i].n_type & N_TYPE) == N_SECT ||
	        (object_symbols[i].n_type & N_STAB) != 0) &&
	       object_symbols[i].n_sect == nsect &&
	       is_output_local_symbol(object_symbols[i].n_type,
				      object_symbols[i].n_sect,
				      object_symbols[i].n_desc, cur_obj,
				      object_symbols[i].n_un.n_strx == 0 ? "" :
				      object_strings +
				      object_symbols[i].n_un.n_strx,
				      &output_strlen)){
		/*
		 * If this local symbol from this section that part of a
		 * fine_reloc that is not going to be in the output then make
		 * sure the symbol is not going to be in the output by creating
		 * a local symbol block for it marked to discard the symbol.
		 * Unless the symbol is an N_SO stab, which must be kept for
		 * the debugger to work correctly.  Note these stabs would have
		 * addresses that are meaningless if the block they are in is
		 * dead stripped, gdb(1) has code to handle this if the address
		 * zero (see below where this is done).
		 */
		if(fine_reloc_offset_in_output(section_map,
			object_symbols[i].n_value - s->addr) == FALSE &&
		   object_symbols[i].n_type != N_SO){
		    nlocal_symbols--;
		    cur_obj->nlocalsym--;

		    local_string_size -=
			    object_symbols[i].n_un.n_strx == 0 ? 0:
			    output_strlen + 1;

		    /* If this file had DWARF, the debug map symbols
		       for this symbol will also go away.  */
		    if (cur_obj->dwarf_name){
		      size_t n = count_dwarf_symbols (object_symbols + i, i,
						      debug_ptr);
		      cur_obj->nlocalsym -= n;
		      nlocal_symbols -= n;
		    }

		    /*
		     * Create a block for this symbol which will cause it to
		     * be discarded for the output.
		     */
		    localsym_block = allocate(sizeof(struct localsym_block));
		    memset(localsym_block, '\0', sizeof(struct localsym_block));
		    localsym_block->index = i;
		    localsym_block->state = DISCARD_SYMBOLS;
		    localsym_block->count = 1;

		    /* insert this block in the list */
		    localsym_block->next = *next_localsym_block;
		    *next_localsym_block = localsym_block;
		    next_localsym_block = &(localsym_block->next);

		    /*
		     * If this is a begin nsect symbol stab (N_BNSYM) scan
		     * forward to the end nsect symbol stab (N_ENSYM) and cause
		     * all stabs to also be removed.
		     */
		    fine_reloc = fine_reloc_for_input_offset(section_map,
					object_symbols[i].n_value - s->addr);
		    if(object_symbols[i].n_type == N_BNSYM &&
			i + 1 < cur_obj->symtab->nsyms){
			for(j = i + 1; j < cur_obj->symtab->nsyms; j++){
			    if(localsym_block->next != NULL &&
			       j >= localsym_block->next->index)
				break;
			    if(object_symbols[j].n_type == N_ENSYM)
				break;
	    		    if((object_symbols[j].n_type & N_EXT) == N_EXT)
				break;
			    if((object_symbols[j].n_type & N_STAB) == 0)
				break;
			    if(object_symbols[j].n_sect == NO_SECT)
				continue;
			    if(object_symbols[j].n_sect != nsect)
				break;
			    if(fine_reloc != fine_reloc_for_input_offset(
				section_map,
				object_symbols[j].n_value - s->addr))
				break;
			}
			/*
			 * If we really found an end nsect symbol stab (N_ENSYM)
			 * adjust the counts of the symbols and string sizes
			 * for the symbols being removed and add the symbols
			 * being removed to a local symbol block.
			 */
			if(j < cur_obj->symtab->nsyms &&
			   (localsym_block->next == NULL ||
			    j < localsym_block->next->index) &&
			   object_symbols[j].n_type == N_ENSYM &&
			   object_symbols[j].n_sect == nsect){
			    /*
			     * If there are type stabs in the N_BNSYM/N_ENSYM
			     * block these must not be removed to allow
			     * debugging to work.  To do this we scan the
			     * symbols in this block.  If we find a type stab we
			     * close the current local symbol block and
			     * step over this stab.  If we then run into another
			     * non-type stab we create a new local block and
			     * put that in that block.
			     */
			    closed_last_block = FALSE;
			    for(k = i + 1; k <= j; k++){
				if(is_type_stab(object_symbols[k].n_type,
				       object_symbols[k].n_un.n_strx == 0 ? "" :
				       object_strings +
				       object_symbols[k].n_un.n_strx)){
				    closed_last_block = TRUE;
				}
				else{
				    if(closed_last_block == TRUE){
					/*
					 * Create a block for this next group of
					 * symbols which will cause it to be
					 * discarded for the output.
					 */
					localsym_block = allocate(
						sizeof(struct localsym_block));
					memset(localsym_block, '\0',
					       sizeof(struct localsym_block));
					localsym_block->index = k;
					localsym_block->state = DISCARD_SYMBOLS;

					/* insert this block in the list */
					localsym_block->next =
						*next_localsym_block;
					*next_localsym_block = localsym_block;
					next_localsym_block =
						&(localsym_block->next);
					closed_last_block = FALSE;
				    }
				    localsym_block->count++;
				    nlocal_symbols--;
				    cur_obj->nlocalsym--;
				    local_string_size -=
				       object_symbols[k].n_un.n_strx == 0 ? 0:
				       strlen(object_strings +
					   object_symbols[k].n_un.n_strx) + 1;
				}
			    }
			    i = j;
			}
		    }

		    /*
		     * Move the pointer from the block we just created to the
		     * the next block
		     */
		    localsym_block = localsym_block->next;
		}
		/*
		 * This local symbol is from this section and is that part of a
		 * fine_reloc that WILL be in the output.  If this is a begin
		 * nsect symbol stab (N_BNSYM) scan forward to the end nsect
		 * symbol stab (N_ENSYM) and cause all stabs to also be kept.
		 * This scan is needed just like the scan above for removing
		 * symbols when to keep all the stabs for unstripped
		 * fine_reloc's.
		 */
		else if(object_symbols[i].n_type == N_BNSYM &&
			i + 1 < cur_obj->symtab->nsyms){
		    fine_reloc = fine_reloc_for_input_offset(section_map,
					object_symbols[i].n_value - s->addr);
		    for(j = i + 1; j < cur_obj->symtab->nsyms; j++){
			if(localsym_block != NULL &&
			   j >= localsym_block->index)
			    break;
			if(object_symbols[j].n_type == N_ENSYM)
			    break;
			if((object_symbols[j].n_type & N_EXT) == N_EXT)
			    break;
			if((object_symbols[j].n_type & N_STAB) == 0)
			    break;
			if(object_symbols[j].n_sect == NO_SECT)
			    continue;
			if(object_symbols[j].n_sect != nsect)
			    break;
			if(fine_reloc != fine_reloc_for_input_offset(
			    section_map,
			    object_symbols[j].n_value - s->addr))
			    break;
		    }
		    /*
		     * If we really found an end nsect symbol stab (N_ENSYM)
		     * move the index i to skip these stabs so they are kept.
		     */
		    if(j < cur_obj->symtab->nsyms &&
		       (localsym_block == NULL ||
			j < localsym_block->index) &&
		       object_symbols[j].n_type == N_ENSYM &&
		       object_symbols[j].n_sect == nsect){
			i = j;
		    }
		}
		/*
		 * This local symbol is being kept, if it is an N_SO stab,
		 * from this section that part of a fine_reloc that is not
		 * going to be in the output then change it so the debugger
		 * will work correctly.  These stabs would have addresses that
		 * are meaningless if the block they are in is dead stripped,
		 * gdb(1) has code to handle this if the address zero.  So
		 * set the n_sect to NO_SECT and the n_value to zero.
		 */
		else if(object_symbols[i].n_type == N_SO &&
		   fine_reloc_offset_in_output(section_map,
			object_symbols[i].n_value - s->addr) == FALSE){
		       object_symbols[i].n_sect = NO_SECT;
		       object_symbols[i].n_value = 0;
		}
	    }
	}
}

#ifndef RLD
/*
 * remove_dead_N_GSYM_stabs_for_cur_obj() is used by remove_dead_N_GSYM_stabs()
 * when -dead_strip is specified to get rid of dead N_GSYM stabs for the
 * cur_obj.  For common symbols this has to be done as they are not defined or
 * in a section and so they can't be bracked by N_BNSYM/N_ENSYM stabs.
 */
static
void
remove_dead_N_GSYM_stabs_for_cur_obj(
struct nlist *object_symbols,
char *object_strings)
{
    unsigned long i, len, symbol_name_len;
    long output_strlen;
    struct localsym_block *localsym_block, **next_localsym_block;
    char *stab_name, *symbol_name, *p;
    struct merged_symbol *hash_pointer, *merged_symbol;
    size_t * debug_ptr;

	/*
	 * Previouly in merge_symbols(), nlocal_symbols and local_string_size
	 * were incremented for all symbols in this section. So now we decrement
	 * these variables for N_GSYM stabs that will not be in the output file.
	 */
	symbol_name_len = 0;
	symbol_name = NULL;
	localsym_block = cur_obj->localsym_blocks;
	next_localsym_block = &(cur_obj->localsym_blocks);
	debug_ptr = cur_obj->dwarf_source_data;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    if (debug_ptr && *debug_ptr < i) {
	      for (debug_ptr += 2; *debug_ptr & 0x80000000; debug_ptr++) ;
	    }

	    /* Remove the DWARF symbols that correspond to an external
	       symbol which will not be in the output file.
	       This has to be done here rather than in count_live_symbols
	       (where it would be much more efficient) for the same reason
	       that this processing has to be done for GSYM symbols:
	       a common variable which turns out to be dead might have
	       GSYM entries in multiple .o files.  */
	    if (cur_obj->dwarf_name
		&& object_symbols[i].n_type & N_EXT
		&& object_symbols[i].n_un.n_strx != 0) {
	      size_t cnt = count_dwarf_symbols (object_symbols + i, i,
						debug_ptr);
	      if (cnt == 0)
		/* Avoid expensive hash lookup for symbols not actually
		   defined in this object file.  */
		continue;
	      hash_pointer = lookup_symbol(object_strings
					   + object_symbols[i].n_un.n_strx);
	      if (hash_pointer->name_len == 0
		  || hash_pointer->live
		  || (object_symbols[i].n_sect != NO_SECT
		      && hash_pointer->definition_object != cur_obj))
		continue;

	      cur_obj->nlocalsym -= cnt;
	      nlocal_symbols -= cnt;
	    }

	    /* skip blocks of symbols that have already been removed */
	    if(localsym_block != NULL && localsym_block->index == i){
		i += localsym_block->count - 1; /* the loop will do i++ */
		next_localsym_block = &(localsym_block->next);
		localsym_block = localsym_block->next;
		continue;
	    }
	    /*
	     * See if this is a N_GSYM stab and would be in the output file.
	     */
	    output_strlen = -1;
	    if(object_symbols[i].n_type == N_GSYM &&
	       is_output_local_symbol(object_symbols[i].n_type,
				      object_symbols[i].n_sect,
				      object_symbols[i].n_desc, cur_obj,
				      object_symbols[i].n_un.n_strx == 0 ? "" :
				      object_strings +
				      object_symbols[i].n_un.n_strx,
				      (unsigned long *)&output_strlen)){
		/*
		 * If is_output_local_symbol() was called in the if() above then
		 * output_strlen will not be -1.  Else we need to calculate the
		 * output string size. This is without the trailing '\0' since
		 * if it is set by is_output_local_symbol() we could be
		 * truncating the string.
		 */
		if(output_strlen == -1)
		   output_strlen = object_symbols[i].n_un.n_strx == 0 ? 0:
				   strlen(object_strings +
					  object_symbols[i].n_un.n_strx);
		/*
		 * Parse out the global symbol name from the N_GSYM stab and
		 * look it up to see if the merged symbol symbol is live.
		 */
		if(object_symbols[i].n_un.n_strx == 0)
		    continue;
		stab_name = object_strings + object_symbols[i].n_un.n_strx;
		p = strchr(stab_name, ':');
		if(p == NULL)
		    continue;
		len = p - stab_name;
		if(len == 0)
		    continue;
		if(len + 2 > symbol_name_len){
		     if(len + 2 < 4096)
			symbol_name_len = 4096;
		     else
			symbol_name_len = len + 2;
		     symbol_name = reallocate(symbol_name, symbol_name_len);
		}
		strcpy(symbol_name, "_");
		strncat(symbol_name, stab_name, len);
		hash_pointer = lookup_symbol(symbol_name);
		if(hash_pointer->name_len == 0)
		    continue;
		merged_symbol = hash_pointer;
		if(merged_symbol->live == FALSE){
		    nlocal_symbols--;
		    cur_obj->nlocalsym--;
		    local_string_size -= output_strlen == 0 ? 0:
					 output_strlen + 1;
		    /*
		     * Create a block for this symbol which will cause it to
		     * be discarded for the output.
		     */
		    localsym_block = allocate(sizeof(struct localsym_block));
		    memset(localsym_block, '\0', sizeof(struct localsym_block));
		    localsym_block->index = i;
		    localsym_block->state = DISCARD_SYMBOLS;
		    localsym_block->count = 1;

		    /* insert this block in the list */
		    localsym_block->next = *next_localsym_block;
		    *next_localsym_block = localsym_block;
		    next_localsym_block = &(localsym_block->next);

		    /*
		     * Move the pointer from the block we just created to the
		     * the next block
		     */
		    localsym_block = localsym_block->next;
		}
	    }
	}
	if(symbol_name != NULL)
	    free(symbol_name);
}
#endif /* !defined(RLD) */

/* these keep track of BINCL strings, so they can be re-used by EXCL */
struct bincl_entry {
    unsigned long sum;
    unsigned long stroffset;
    const char* path;
};
static struct bincl_entry* bincl_entries = NULL;
static unsigned int bincl_entries_used = 0;
static unsigned int bincl_entry_count = 0;

/*
 * record_bincl() records the string offset of a BINCL for a checksum/path pair.
 */
static
void
record_bincl(
unsigned long sum,
const char* path,
unsigned long output_stroffset)
{
    struct bincl_entry *tmp;
    char *path_copy;

	if(bincl_entries == NULL){
	    bincl_entry_count = 8192;
	    bincl_entries = allocate(sizeof(struct bincl_entry) *
				     bincl_entry_count);
	}
	if(bincl_entries_used == bincl_entry_count){
	    bincl_entry_count *= 2;
	    tmp = allocate(sizeof(struct bincl_entry)*bincl_entry_count);
	    memcpy(tmp, bincl_entries, sizeof(struct bincl_entry) *
				       bincl_entries_used);
	    free(bincl_entries);
	    bincl_entries = tmp;
	}
	bincl_entries[bincl_entries_used].sum = sum;
	bincl_entries[bincl_entries_used].stroffset = output_stroffset;
	path_copy = allocate(strlen(path) + 1);
	strcpy(path_copy, path);
	bincl_entries[bincl_entries_used].path = path_copy;
	bincl_entries_used++;
}

/*
 * find_bincl() finds the string offset of a BINCL for a checksum/path pair.
 */
static
unsigned long
find_bincl(
unsigned long sum,
const char* path)
{
    unsigned long i;

	for(i = 0; i < bincl_entries_used; i++){
	    if(bincl_entries[i].sum == sum &&
	       strcmp(bincl_entries[i].path, path) == 0){
		return(bincl_entries[i].stroffset);
	    }
	}
	return(0);
}


/*
 * add_dwarf_map_entry() adds a single DWARF map symbol to 'nlist'.
 */
static void
add_dwarf_map_entry(struct nlist ** nlist, unsigned long *output_nsyms,
		    int32_t n_strx, uint8_t n_type, uint8_t n_sect,
		    int16_t n_desc, uint32_t n_value)
{
  (*nlist)->n_un.n_strx = n_strx;
  (*nlist)->n_type = n_type;
  (*nlist)->n_sect = n_sect;
  (*nlist)->n_desc = n_desc;
  (*nlist)->n_value = n_value;
  (*nlist)++;
  (*output_nsyms)++;
}

/*
 * add_dwarf_map_for_sym() adds the DWARF symbols for a single object file
 * symbol to 'nlist'.  It is the pass2 counterterpart of count_dwarf_symbols.
 */
static void
add_dwarf_map_for_sym(const struct nlist * sym,
		      size_t i, const size_t * debug_ptr, uint32_t n_strx,
		      uint16_t old_sect,
		      struct nlist ** nlist, unsigned long *output_nsyms)
{
  size_t j;

  /* The debug map only represents symbols which are defined in
     a particular section or which are global common symbols.  */
  if ((sym->n_type & (N_TYPE | N_STAB)) != N_SECT
      && ((sym->n_type & (N_TYPE | N_STAB)) != N_UNDF
	  || sym->n_value == 0))
    return;
  /* If S_ATTR_STRIP_STATIC_SYMS is set on this symbol's section,
     we don't need a debug symbol for this symbol.  */
  if ((sym->n_type & (N_TYPE | N_STAB)) == N_SECT
      && old_sect != NO_SECT
      && (cur_obj->section_maps[old_sect - 1].s->flags &
	  S_ATTR_STRIP_STATIC_SYMS))
    return;

  if (! debug_ptr || *debug_ptr != i)
    {
      if (sym->n_type & N_EXT)
	add_dwarf_map_entry (nlist, output_nsyms, n_strx, N_GSYM, 0, 0, 0);
      else
	add_dwarf_map_entry (nlist, output_nsyms,
			     sym->n_un.n_strx, N_STSYM,
			     sym->n_sect, 0, sym->n_value);
      return;
    }
  add_dwarf_map_entry (nlist, output_nsyms,
		       0, N_BNSYM, sym->n_sect, 0, sym->n_value);
  add_dwarf_map_entry (nlist, output_nsyms,
		       n_strx, N_FUN, sym->n_sect, 0, sym->n_value);
  for (j = 2; debug_ptr[j] & 0x80000000; j++)
    add_dwarf_map_entry (nlist, output_nsyms,
			 (cur_obj->dwarf_paths[debug_ptr[j] & 0x7fffffff]
			  - output_addr
			  - output_symtab_info.symtab_command.stroff),
			 N_SOL, 0, 0, 0);
  add_dwarf_map_entry (nlist, output_nsyms, 0, N_FUN, 0, 0, debug_ptr[1]);
  add_dwarf_map_entry (nlist, output_nsyms, 0, N_ENSYM,
		       sym->n_sect, 0, sym->n_value);
}

/*
 * output_local_symbols() copys the local symbols and their strings from the
 * current object file into the output file's memory buffer.  The symbols also
 * get relocated.
 */
__private_extern__
void
output_local_symbols(void)
{
    unsigned long i, flush_symbol_offset, output_nsyms, flush_string_offset,
		  start_string_size, mtime, stroff_for_N_OSO;
    long output_strlen;
    struct nlist *object_symbols, *nlist;
    char *object_strings, *string;
    struct localsym_block *localsym_block;
    size_t * debug_ptr;

	/* If no symbols are not to appear in the output file just return */
	if(strip_level == STRIP_ALL)
	    return;

	/* If this object file has no symbols then just return */
	if(cur_obj->symtab == NULL)
	    return;

	/* If this is the base file and base file symbols are stripped return */
	if(cur_obj == base_obj && strip_base_symbols == TRUE)
	    return;

#ifdef RLD
	/* If this object is not from the current set then just return */
	if(cur_obj->set_num != cur_set)
	    return;
#endif /* RLD */

	/* setup pointers to the symbol table and string table */
	object_symbols = (struct nlist *)(cur_obj->obj_addr +
					  cur_obj->symtab->symoff);
	object_strings = (char *)(cur_obj->obj_addr + cur_obj->symtab->stroff);

	flush_symbol_offset = output_symtab_info.symtab_command.symoff +
			      cur_obj->ilocalsym * sizeof(struct nlist);
	flush_string_offset = output_symtab_info.symtab_command.stroff +
			      output_symtab_info.output_local_strsize;
	start_string_size = output_symtab_info.output_local_strsize;

	output_nsyms = 0;
	stroff_for_N_OSO = 0;
	nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	/* If we are creating section object symbols, create one if needed */
	if(sect_object_symbols.ms != NULL){
	    /*
	     * See if this object file has the section that the section object
	     * symbols are being created for.
	     */
	    for(i = 0; i < cur_obj->nsection_maps; i++){
		if(sect_object_symbols.ms ==
		   cur_obj->section_maps[i].output_section){
		    /* make the nlist entry in the output file */
		    nlist->n_value =
			    cur_obj->section_maps[i].output_section->s.addr +
			    cur_obj->section_maps[i].offset;
		    nlist->n_sect =
			cur_obj->section_maps[i].output_section->output_sectnum;
		    nlist->n_type = N_SECT;
		    nlist->n_desc = 0;

		    nlist->n_un.n_strx = output_symtab_info.
					 output_local_strsize;
		    string = output_addr +
			     output_symtab_info.symtab_command.stroff +
			     output_symtab_info.output_local_strsize;
		    if(cur_obj->ar_hdr == NULL){
			strcpy(string, cur_obj->file_name);
			output_symtab_info.output_local_strsize +=
						 strlen(cur_obj->file_name) + 1;
		    }
		    else{
			strncpy(string, cur_obj->ar_name,cur_obj->ar_name_size);
			string[cur_obj->ar_name_size] = '\0';
			output_symtab_info.output_local_strsize +=
				       cur_obj->ar_name_size + 1;
		    }
		    nlist++;
		    output_nsyms++;
		    break;
		}
	    }
	}

	/* Add initial DWARF map symbols.  */
	if(cur_obj->dwarf_name){
	  size_t len = strlen (cur_obj->dwarf_name) + 1;
	  char * strbase = (output_addr
			    + output_symtab_info.symtab_command.stroff);

	  if (cur_obj->dwarf_comp_dir){
	    size_t len = strlen (cur_obj->dwarf_comp_dir);
	    add_dwarf_map_entry (&nlist, &output_nsyms,
				 output_symtab_info.output_local_strsize,
				 N_SO, 0, 0, 0);
	    memcpy (strbase + output_symtab_info.output_local_strsize,
		    cur_obj->dwarf_comp_dir, len);
	    output_symtab_info.output_local_strsize += len + 2;
	    strbase[output_symtab_info.output_local_strsize - 2] = '/';
	    strbase[output_symtab_info.output_local_strsize - 1] = 0;
	  }

	  add_dwarf_map_entry (&nlist, &output_nsyms,
			       output_symtab_info.output_local_strsize,
			       N_SO, 0, 0, 0);
	  memcpy (strbase + output_symtab_info.output_local_strsize,
		  cur_obj->dwarf_name, len);
	  output_symtab_info.output_local_strsize += len;

	  get_stroff_and_mtime_for_N_OSO(&stroff_for_N_OSO, &mtime);
	  add_dwarf_map_entry (&nlist, &output_nsyms,
			       stroff_for_N_OSO, N_OSO, 0, 1, mtime);

	  /* Add SOL strings to the local string table, and update
	     cur_obj->dwarf_paths to point into it.  */
	  for (i = 0; i < cur_obj->dwarf_num_paths; i++)
	    if (cur_obj->dwarf_paths[i]){
	      char * st = strbase + output_symtab_info.output_local_strsize;
	      size_t len = strlen (cur_obj->dwarf_paths[i]) + 1;
	      output_symtab_info.output_local_strsize += len;
	      memcpy (st, cur_obj->dwarf_paths[i], len);
	      cur_obj->dwarf_paths[i] = st;
	    }
	}

	/*
	 * Loop through this object file's local symbols.
	 */
	localsym_block = cur_obj->localsym_blocks;
	debug_ptr = cur_obj->dwarf_source_data;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    if (debug_ptr && *debug_ptr < i) {
	      for (debug_ptr += 2; *debug_ptr & 0x80000000; debug_ptr++) ;
	    }
	    /*
	     * Some of the local symbols may be excluded.  These have
	     * localsym_blocks.  The localsym_blocks are ordered by the index
	     * field.  For local symbol blocks marked with the state
	     * EXCLUDED_INCLUDE are for blocks of N_BINCL/N_EINC local symbols
	     * to be exclude and replaced with a single N_EXCL.  For the state
	     * BEGIN_INCLUDE the sum is just set into the n_value.  Others are
	     * simply excluded as the could have been local symbols for
	     * coalesced or indirect symbols that were removed.
	     */
	    if(localsym_block != NULL && localsym_block->index == i){
		if(localsym_block->state ==  EXCLUDED_INCLUDE ||
		   localsym_block->state == BEGIN_INCLUDE){
		    *nlist = object_symbols[i];
		    if(localsym_block->state == EXCLUDED_INCLUDE)
			nlist->n_type = N_EXCL;
		    nlist->n_value = localsym_block->sum;
		    /*
		     * set the string of the N_BINCL/N_EXCL to the output file.
		     * (it should have one)
		     */
		    if(object_symbols[i].n_un.n_strx != 0){
			int doCopy = 1;
			/* try to re-use an existing BINCL string */
			nlist->n_un.n_strx = find_bincl(nlist->n_value,
				object_strings + object_symbols[i].n_un.n_strx);
			if(nlist->n_un.n_strx != 0){
			    if(nlist->n_type == N_EXCL){
				/* EXCL always re-uses BINCL string,
				   n_un.n_strx already set */
				doCopy = 0;
			    }
			}
			else if(nlist->n_type == N_BINCL){
			    /* first time this BINCL has been seen, so add it */
			    record_bincl(nlist->n_value,
				 object_strings +
				 object_symbols[i].n_un.n_strx,
				 output_symtab_info.output_local_strsize);
			}
			if(doCopy){
			    nlist->n_un.n_strx = output_symtab_info.
						 output_local_strsize;
			    string = output_addr +
				     output_symtab_info.symtab_command.stroff +
				     output_symtab_info.output_local_strsize;
			    strcpy(string,
				   object_strings +
				      object_symbols[i].n_un.n_strx);
			    output_symtab_info.output_local_strsize +=
			      strlen(object_strings +
				     object_symbols[i].n_un.n_strx) + 1;
			}
		    }
		    output_nsyms++;
		    nlist++;
		}
		i += localsym_block->count - 1; /* the loop will do i++ */
		localsym_block = localsym_block->next;
		continue;
	    }

	    /* Add debug map entries for global symbols.  */
	    if (cur_obj->dwarf_name
		/* The debug map contains symbols defined in a section... */
		&& (((object_symbols[i].n_type & (N_TYPE | N_STAB | N_EXT))
		     == (N_SECT | N_EXT))
		    /* ... and global common symbols.  */
		    || (((object_symbols[i].n_type & (N_TYPE | N_STAB | N_EXT))
			 == (N_UNDF | N_EXT))
			&& object_symbols[i].n_value != 0))){
	      struct merged_symbol *hash_pointer;
	      hash_pointer = lookup_symbol(object_strings
					   + object_symbols[i].n_un.n_strx);

	      /* Only those symbols whose definitions in this object
		 were actually output get a debug_map entry.  Common symbols
	         are always 'output' if they are live.  */
	      if (hash_pointer->name_len != 0
		  && (! dead_strip || hash_pointer->live)
		  && (object_symbols[i].n_sect == NO_SECT
		      || hash_pointer->definition_object == cur_obj)){
		unsigned long symbol_index;
		symbol_index = (output_symtab_info.output_merged_strsize
				+ (merged_symbol_string_index
				   (hash_pointer->nlist.n_un.n_name)));
		add_dwarf_map_for_sym (&hash_pointer->nlist, i, debug_ptr,
				       symbol_index, object_symbols[i].n_sect,
				       &nlist, &output_nsyms);
	      }
	    }

	    /*
	     * If this is a local symbol and it is to be in the output file then
	     * copy it and it's string into the output file and relocate the
	     * symbol.
	     */
	    output_strlen = -1;
	    if((object_symbols[i].n_type & N_EXT) == 0 &&
	       is_output_local_symbol(object_symbols[i].n_type,
		    object_symbols[i].n_sect, object_symbols[i].n_desc, cur_obj,
		    object_symbols[i].n_un.n_strx == 0 ? "" :
		    object_strings + object_symbols[i].n_un.n_strx,
		    (unsigned long *)&output_strlen)){

		/*
		 * If is_output_local_symbol() was called in the if() above then
		 * output_strlen will not be -1.  Else we need to calculate the
		 * output string size. This is without the trailing '\0' since
		 * if it is set by is_output_local_symbol() we could be
		 * truncating the string.
		 */
		if(output_strlen == -1)
		   output_strlen = object_symbols[i].n_un.n_strx == 0 ? 0:
				   strlen(object_strings +
					  object_symbols[i].n_un.n_strx);
		/* copy the nlist to the output file */
		*nlist = object_symbols[i];
		relocate_symbol(nlist, cur_obj);
		/*
		 * If we are not producing a relocatable file clear the
		 * N_NO_DEAD_STRIP bit in non-stab symbols as it is overloaded
		 * and can't appear in a linked image.
		 */
		if(save_reloc == FALSE &&
		   (nlist->n_type & N_TYPE) == N_SECT &&
		   (nlist->n_type & N_STAB) == 0)
		    nlist->n_desc = nlist->n_desc & ~(N_NO_DEAD_STRIP);
#ifdef RLD
		/*
		 * Now change the section number of this symbol to the section
		 * number it will have in the output file.  For RLD all this
		 * has to be done on for only the symbol in an output file and
		 * not in the merged symbol table.  relocate_symbol() does not
		 * modify n_sect for RLD.
		 */
		if(nlist->n_sect != NO_SECT)
		    nlist->n_sect = cur_obj->section_maps[nlist->n_sect - 1].
				    output_section->output_sectnum;
#endif /* RLD */
		/*
		 * Now if this is an N_OSO we may need to set the n_value
		 * and change its name.
		 */
		if(nlist->n_type == N_OSO){
		    /*
		     * When the compiler is producing dwarf debug info it
		     * uses stabs for debug notes.  And the N_OSO stabs in this
		     * case has a n_desc field of 1.  If the name is the empty
		     * string (not NULL but a 1 character string with just a
		     * '\0') then change the name to the full path of the
		     * object file and set the mod time into n_value.  If the
		     * name is not an empty string it is left unchanged.
		     */
		    if(nlist->n_desc == 1){
			if(object_symbols[i].n_un.n_strx != 0 &&
			   object_strings[object_symbols[i].n_un.n_strx] ==
			   '\0'){
/* GUESS */
			    /*
			     * Copy the string (the full path name) for the
			     * N_OSO to the output file.
			     */
			    get_stroff_and_mtime_for_N_OSO(
				&stroff_for_N_OSO, &mtime);
			    /*
			     * Reset the string index and n_value of the N_OSO.
			     */
			    nlist->n_un.n_strx = stroff_for_N_OSO;
			    nlist->n_value = mtime;
			}
			/*
			 * The name of the dwarf N_OSO debug_note is not empty
			 * so leave it unchanged.
			 */
			else{
			    goto copy_string_to_output_file;
			}
		    }
		    /*
		     * When the compiler is producing stabs debug info it will
		     * produce an N_OSO stab with an n_desc field of 0.
		     */
		    else if(nlist->n_desc == 0){
			/*
			 * When -Sp is specified then change the name to the
			 * full path of the object file and set the mod time
			 * into n_value.
			 */
			if(strip_level == STRIP_MIN_DEBUG){
/* GUESS */
			    /*
			     * Copy the string (the full path name) for the
			     * N_OSO to the output file.
			     */
			    get_stroff_and_mtime_for_N_OSO(
				&stroff_for_N_OSO, &mtime);
			    /*
			     * Reset the string index and n_value of the N_OSO.
			     */
			    nlist->n_un.n_strx = stroff_for_N_OSO;
			    nlist->n_value = mtime;
			}
			/*
			 * When -Sp is not specified the name gets reset to ""
			 * reset to "" (that a one character string with only a
			 * '\0' character).  Unless the input the n_un.n_strx
			 * field is 0 then it will end up as 0 in the output.
			 */
			else{
			    output_strlen = 0;
			    goto copy_string_to_output_file;
			}
		    }
		    /*
		     * If the n_desc is not 0 or 1 then leave the name
		     * unchanged.
		     */
		    else{
			goto copy_string_to_output_file;
		    }
		}
		else{
copy_string_to_output_file:
		    /* copy the string to the output file (if it has one) */
		    if(object_symbols[i].n_un.n_strx != 0){
			nlist->n_un.n_strx = output_symtab_info.
					     output_local_strsize;
			string = output_addr +
				 output_symtab_info.symtab_command.stroff +
				 output_symtab_info.output_local_strsize;
			/*
			 * Note, output_strlen does not include the trailing
			 * '\0' as we could be truncating the string.  But the
			 * increment below does as the output memory contains
			 * zeroes and we use a zero in it after the string as
			 * the trailing '\0'.
			 */
			strncpy(string,
			        object_strings + object_symbols[i].n_un.n_strx,
				output_strlen);
			output_symtab_info.output_local_strsize +=
				        output_strlen + 1;
		    }
		}
		output_nsyms++;
		nlist++;

		/* Add the DWARF map entry/entries for this local symbol,
		   if necessary.  */
		if(cur_obj->dwarf_name)
		  add_dwarf_map_for_sym (nlist - 1, i, debug_ptr,
					 nlist[-1].n_un.n_strx,
					 object_symbols[i].n_sect,
					 &nlist, &output_nsyms);
	    }
	}

	/* Add terminal DWARF map symbol.  */
	if(cur_obj->dwarf_name)
	  add_dwarf_map_entry (&nlist, &output_nsyms, 0, N_SO, 0, 0, 0);

	if(host_byte_sex != target_byte_sex){
	    nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	    swap_nlist(nlist, output_nsyms, target_byte_sex);
	}
#ifndef RLD
	output_flush(flush_symbol_offset, output_nsyms * sizeof(struct nlist));
	output_flush(flush_string_offset, output_symtab_info.
					  output_local_strsize -
					  start_string_size);
#endif /* !defined(RLD) */
	/*
	 * Check to make sure the count is consistent.
	 */
	if(output_nsyms != cur_obj->nlocalsym)
	    fatal("internal error: output_local_symbols() inconsistent local "
		  "symbol count");
}

/*
 * get_stroff_and_mtime_for_N_OSO() is used when modifying an N_OSO from the
 * the cur_obj.  It places a string with the resolved_path in the string table
 * and returns the offset to it indirectly through stroff_for_N_OSO.  It also
 * returns the modifification time indirectly through mtime.
 */
static
void
get_stroff_and_mtime_for_N_OSO(
unsigned long *stroff_for_N_OSO,
unsigned long *mtime)
{
    char *string, *endptr;
#if !(defined(KLD) && defined(__STATIC__))
    struct stat stat_buf;
#endif /* !(defined(KLD) && defined(__STATIC__)) */

	*stroff_for_N_OSO = output_symtab_info.output_local_strsize;
	string = output_addr +
		 output_symtab_info.symtab_command.stroff +
		 output_symtab_info.output_local_strsize;
	strcpy(string, cur_obj->resolved_path);
	output_symtab_info.output_local_strsize +=
	    cur_obj->resolved_path_len + 1;
	/*
	 * Do not cause errors if we can't get a valid timestamp to use for the
	 * N_OSO n_value.
	 */
	if(cur_obj->ar_hdr != NULL)
	    *mtime = strtol(cur_obj->ar_hdr->ar_date, &endptr, 10);
	else{
#if !(defined(KLD) && defined(__STATIC__))
	    if(stat(cur_obj->file_name, &stat_buf) != -1)
		*mtime = stat_buf.st_mtime;
	    else
#endif /* !(defined(KLD) && defined(__STATIC__)) */
		*mtime = 0;
	}
}

/*
 * local_symbol_output_index() calculates the output symbol offset for the
 * symbol at index in the object file obj.  This is very slow and is only
 * called by output_indirect_symbols() when a symbol that was a private extern
 * that is no longer external is being used as an indirect symbol.
 */
__private_extern__
unsigned long
local_symbol_output_index(
struct object_file *obj,
unsigned long index)
{
    unsigned long i, output_nsyms, output_strlen;
    struct nlist *object_symbols;
    char *object_strings;
    struct localsym_block *localsym_block;

	/* setup pointers to the symbol table and string table */
	object_symbols = (struct nlist *)(obj->obj_addr +
					  obj->symtab->symoff);
	object_strings = (char *)(obj->obj_addr + obj->symtab->stroff);
	output_nsyms = 0;
	/* If we are creating section object symbols, count one if needed */
	if(sect_object_symbols.ms != NULL){
	    /*
	     * See if this object file has the section that the section object
	     * symbols are being created for.
	     */
	    for(i = 0; i < obj->nsection_maps; i++){
		if(sect_object_symbols.ms ==
		   obj->section_maps[i].output_section){
		    output_nsyms++;
		    break;
		}
	    }
	}

	localsym_block = obj->localsym_blocks;
	for(i = 0; i < obj->symtab->nsyms; i++){
	    /* skip blocks of symbols that have been removed */
	    if(localsym_block != NULL && localsym_block->index == i){
		i += localsym_block->count - 1; /* the loop will do i++ */
		localsym_block = localsym_block->next;
		continue;
	    }
	    /*
	     * If this is a local symbol and it is to be in the output file then
	     * count it.
	     */
	    if((object_symbols[i].n_type & N_EXT) == 0 &&
	       is_output_local_symbol(object_symbols[i].n_type,
		    object_symbols[i].n_sect, object_symbols[i].n_desc, obj,
		    object_symbols[i].n_un.n_strx == 0 ? "" :
		    object_strings + object_symbols[i].n_un.n_strx,
		    &output_strlen)){

		/*
		 * This symbol was in the output file if this symbol is at the
		 * the symbol index we are looking for then we know what output
		 * index it is.
		 */
		if(i == index)
		    return(obj->ilocalsym + output_nsyms);
		output_nsyms++;
	    }
	}
	fatal("internal error: local_symbol_output_index() could not determine "
	      "output_index");
	return(0);
}

/*
 * set_merged_string_block_indexes() set the relitive indexes for each merged
 * string block.
 */
__private_extern__
void
set_merged_string_block_indexes(
void)
{
    unsigned long index;
    struct string_block **q, *string_block;

	index = 0;
	for(q = &(merged_string_blocks); *q; q = &(string_block->next)){
	    string_block = *q;
	    if(strip_base_symbols == TRUE && string_block->base_strings == TRUE)
		continue;
	    if(string_block->dylib_strings == TRUE)
		continue;
#ifdef RLD
	    if(string_block->set_num != cur_set)
		continue;
#endif /* RLD */
	    string_block->index = index,
	    index += string_block->used;
	}
}

/*
 * output_merged_symbols() readies the merged symbols for the output file (sets
 * string indexes and handles indirect symbols) and copies the merged symbols
 * and their strings to the output file.  This routine also copies out the
 * two-level namespace hints for the undefined symbols if they are to be in
 * the output.
 */
__private_extern__
void
output_merged_symbols(void)
{
    unsigned long i, j, flush_symbol_offset,
		  flush_string_offset, start_string_size;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol, *indr_symbol;
    struct string_block **q, *string_block;
    struct nlist *nlist;
    struct twolevel_hint *hint;

	if(strip_level == STRIP_ALL)
	    return;

	/*
	 * Indirect symbols are readied for output.  For indirect symbols that
	 * the symbol they are refering to is defined (not undefined or common)
	 * the the type, value, etc. of the refered symbol is propagated to
	 * indirect symbol.  If the indirect symbol is not defined then the
	 * value field is set to the index of the string the symbol is refering
	 * to.
	 */
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->nlist.n_type == (N_EXT | N_INDR)){
		    /*
		     * If this N_INDR symbol was in a chain has symbols both
		     * from dylib and not from dylibs get then there was a
		     * recorded a pair for the merged symbol and the first in
		     * the chain defined in a dylib for the indr_symbol to be
		     * used.  If not then merged_symbol->nlist.n_value can be
		     * used.
		     */
		    indr_symbol = NULL;
		    for(j = 0; j < nindr_symbol_pairs; j++){
			if(indr_symbol_pairs[j].merged_symbol == merged_symbol)
			    indr_symbol = indr_symbol_pairs[j].indr_symbol;
		    }
		    if(indr_symbol == NULL)
			indr_symbol = (struct merged_symbol *)
				(merged_symbol->nlist.n_value);

		    /*
		     * Check to see if this symbol is defined (not undefined or
		     * common)
		     */
		    if(indr_symbol->nlist.n_type != (N_EXT | N_UNDF) &&
		       indr_symbol->nlist.n_type != (N_EXT | N_PBUD) &&
		       (filetype != MH_DYLIB ||
			(filetype == MH_DYLIB && multi_module_dylib == FALSE) ||
			merged_symbol->definition_object ==
				indr_symbol->definition_object)){
			merged_symbol->nlist.n_type = indr_symbol->nlist.n_type;
			merged_symbol->nlist.n_sect = indr_symbol->nlist.n_sect;
			merged_symbol->nlist.n_desc = indr_symbol->nlist.n_desc;
			merged_symbol->nlist.n_value =
						     indr_symbol->nlist.n_value;
		    }
		    else{
			merged_symbol->nlist.n_value =
				output_symtab_info.output_merged_strsize +
				merged_symbol_string_index(
					indr_symbol->nlist.n_un.n_name);
		    }
		}
	    }
	}

	/*
	 * Copy the merged symbols into the memory buffer for the output file
	 * and set their string indexes.  This is done in three groups:
	 * 	the private externals (if keep_private_externs is FALSE)
	 *	the defined external symbols
	 *	the undefined externals
	 */

	/*
	 * First group of merged symbols to be copied to into the buffer for
	 * the output file is the private externs if they are not to be kept
	 * (that is they are to be made static and not kept as global symbols).
	 */
	if(nmerged_private_symbols != 0 &&
	   keep_private_externs == FALSE &&
	   (strip_level != STRIP_NONGLOBALS ||
	    (filetype == MH_DYLIB && multi_module_dylib == TRUE)) ){
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
	        merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->referenced_in_non_dylib == FALSE)
			continue;
		    if(strip_base_symbols == TRUE &&
		       merged_symbol->definition_object == base_obj)
			continue;
#ifdef RLD
		    if(merged_symbol->definition_object->set_num != cur_set)
			continue;
#endif /* RLD */
		    if(strip_level == STRIP_DYNAMIC_EXECUTABLE &&
		       (((merged_symbol->nlist.n_desc &
			  REFERENCED_DYNAMICALLY) != REFERENCED_DYNAMICALLY) ||
			 (merged_symbol->nlist.n_type & N_PEXT) == N_PEXT))
			continue;
		    if(dead_strip == TRUE && merged_symbol->live == FALSE)
			continue;

		    /*
		     * See if this is a defined private extern symbol (but not
		     * still a common private extern symbol).
		     */
		    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
		       (merged_symbol->nlist.n_type & N_TYPE) != N_UNDF){
			/*
			 * Place this symbol with the local symbols for the
			 * object that defined this symbol.  The output symbol
			 * index for private externs was calculated by
			 * assign_output_symbol_indexes() and recorded in
			 * iprivatesym for the definition object.
			 */
			flush_symbol_offset =
				output_symtab_info.symtab_command.symoff +
				merged_symbol->definition_object->iprivatesym *
				sizeof(struct nlist);
			merged_symbol->definition_object->iprivatesym++;
			nlist = (struct nlist *)(output_addr +
						 flush_symbol_offset);
			*nlist = merged_symbol->nlist;
			/*
			 * Since this is a symbol definition make sure the
			 * weak reference bit is off.
			 */
			nlist->n_desc = nlist->n_desc & ~(N_WEAK_REF);
			/*
			 * If we are not producing a relocatable file clear the
			 * N_NO_DEAD_STRIP bit as it is overloaded and can't
			 * appear in a linked image.
			 */
			if(save_reloc == FALSE)
			    nlist->n_desc = nlist->n_desc & ~(N_NO_DEAD_STRIP);
			nlist->n_un.n_strx = output_symtab_info.
					     output_merged_strsize +
					     merged_symbol_string_index(
					     merged_symbol->nlist.n_un.n_name);
			/* make this private extern a non-external symbol */
			nlist->n_type &= ~(N_EXT);
#ifdef RLD
			/*
			 * Now change the section number of this symbol to the
			 * section number it will have in the output file.  For
			 * RLD all this has to be done for only the symbols in
			 * the output file and not in the merged symbol table.
			 * relocate_symbol() does not modify n_sect for RLD.
			 */
			if(nlist->n_sect != NO_SECT)
			    nlist->n_sect = merged_symbol->definition_object->
					    section_maps[nlist->n_sect - 1].
					    output_section->output_sectnum;
#endif /* RLD */
			if(host_byte_sex != target_byte_sex)
			    swap_nlist(nlist, 1, target_byte_sex);
#ifndef RLD
			output_flush(flush_symbol_offset, sizeof(struct nlist));
#endif /* !defined(RLD) */
		    }
		}
	    }
	}
	/*
	 * Second group of merged symbols to be copied to into the buffer for
	 * the output file is the defined externals.  They are copied out in the
	 * order creaded by assign_output_symbol_indexes() and left in the array
	 * extdefsyms_order.
	 */
	flush_symbol_offset = output_symtab_info.symtab_command.symoff +
			      output_dysymtab_info.dysymtab_command.iextdefsym *
			      sizeof(struct nlist);
	nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	for(i = 0; i < output_dysymtab_info.dysymtab_command.nextdefsym; i++){
	    merged_symbol = extdefsyms_order[i];
	    *nlist = merged_symbol->nlist;
	    /*
	     * Since this is a symbol definition make sure the weak reference
	     * bit is off.
	     */
	    nlist->n_desc = nlist->n_desc & ~(N_WEAK_REF);
	    /*
	     * If we are not producing a relocatable file clear the
	     * N_NO_DEAD_STRIP bit as it is overloaded and can't appear in a
	     * linked image.
	     */
	    if(save_reloc == FALSE)
		nlist->n_desc = nlist->n_desc & ~(N_NO_DEAD_STRIP);
	    nlist->n_un.n_strx = output_symtab_info.output_merged_strsize +
				 merged_symbol_string_index(
				    merged_symbol->nlist.n_un.n_name);
	    /*
	     * If this defined external symbol is also a weak definition then
	     * set the MH_WEAK_DEFINES and MH_BINDS_TO_WEAK bits in the
	     * mach_header.
	     */
	    if(nlist->n_desc & N_WEAK_DEF)
		output_mach_header.flags |= MH_WEAK_DEFINES | MH_BINDS_TO_WEAK;
#ifdef RLD
	    /*
	     * Now change the section number of this symbol to the section
	     * number it will have in the output file.  For RLD all this
	     * has to be done on for only the symbol in an output file and
	     * not in the merged symbol table.  relocate_symbol() does not
	     * modify n_sect for RLD.
	     */
	    if(nlist->n_sect != NO_SECT)
		nlist->n_sect = merged_symbol->definition_object->
				section_maps[nlist->n_sect - 1].
				output_section->output_sectnum;
#endif
	    nlist++;
	}
	if(host_byte_sex != target_byte_sex){
	    nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	    swap_nlist(nlist, output_dysymtab_info.dysymtab_command.nextdefsym,
		       target_byte_sex);
	}
#ifndef RLD
	output_flush(flush_symbol_offset,
		     output_dysymtab_info.dysymtab_command.nextdefsym *
		     sizeof(struct nlist));
#endif
	if(extdefsyms_order != NULL){
	    free(extdefsyms_order);
	    extdefsyms_order = NULL;
	}
	/*
	 * Third group of merged symbols to be copied to into the buffer for
	 * the output file is the undefined symbols.  They are copied out in the
	 * order creaded by assign_output_symbol_indexes() and left in the array
	 * undefsyms_order.
	 */
	flush_symbol_offset = output_symtab_info.symtab_command.symoff +
			      output_dysymtab_info.dysymtab_command.iundefsym *
			      sizeof(struct nlist);
	nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	for(i = 0; i < output_dysymtab_info.dysymtab_command.nundefsym; i++){
	    merged_symbol = undefsyms_order[i];
	    *nlist = merged_symbol->nlist;
	    /*
	     * Since this is an undefined symbol make sure the weak definition
	     * bit and N_NO_DEAD_STRIP bits are off.
	     */
	    nlist->n_desc = nlist->n_desc & ~(N_WEAK_DEF & N_NO_DEAD_STRIP);
	    /*
	     * If this undefined symbol is referencing an undefined symbol that
	     * is a weak symbol set the N_REF_TO_WEAK bit and set the
	     * MH_BINDS_TO_WEAK bit in the mach_header.
	     */
	    if(merged_symbol->defined_in_dylib == TRUE &&
	       merged_symbol->weak_def_in_dylib == TRUE){
		nlist->n_desc |= N_REF_TO_WEAK;
		output_mach_header.flags |= MH_BINDS_TO_WEAK;
	    }
	    nlist->n_un.n_strx = output_symtab_info.output_merged_strsize +
				 merged_symbol_string_index(
				    merged_symbol->nlist.n_un.n_name);
	    /* note all undefined symbols have n_sect == NO_SECT */
	    nlist++;
	}
	if(host_byte_sex != target_byte_sex){
	    nlist = (struct nlist *)(output_addr + flush_symbol_offset);
	    swap_nlist(nlist, output_dysymtab_info.dysymtab_command.nundefsym,
		       target_byte_sex);
	}
#ifndef RLD
	output_flush(flush_symbol_offset,
		     output_dysymtab_info.dysymtab_command.nundefsym *
		     sizeof(struct nlist));
#endif
	/*
	 * Copy the merged strings into the memory buffer for the output file.
	 */
	flush_string_offset = output_symtab_info.symtab_command.stroff +
			      output_symtab_info.output_merged_strsize;
	start_string_size = output_symtab_info.output_merged_strsize;
	for(q = &(merged_string_blocks); *q; q = &(string_block->next)){
	    string_block = *q;
	    if(strip_base_symbols == TRUE && string_block->base_strings == TRUE)
		continue;
	    if(string_block->dylib_strings == TRUE)
		continue;
#ifdef RLD
	    if(string_block->set_num != cur_set)
		continue;
#endif /* RLD */
	    memcpy(output_addr + output_symtab_info.symtab_command.stroff +
				 output_symtab_info.output_merged_strsize,
		   string_block->strings,
		   string_block->used);
	    output_symtab_info.output_merged_strsize += string_block->used;
	}

#ifndef RLD
	output_flush(flush_string_offset, output_symtab_info.
					  output_merged_strsize -
					  start_string_size);
#endif /* !defined(RLD) */

	/*
	 * Lastly create and copy out the two-level namespace hints for the
	 * undefined symbols.  This must be in the same order as the undefined
	 * symbols so the undefsyms_order array is used.
	 */
	hint = (struct twolevel_hint *)(output_addr +
			output_hints_info.twolevel_hints_command.offset);
	if(output_for_dyld && twolevel_namespace == TRUE &&
	   twolevel_namespace_hints == TRUE){
	    for(i = 0; i < output_dysymtab_info.dysymtab_command.nundefsym;i++){
		merged_symbol = undefsyms_order[i];
		hint->isub_image = merged_symbol->definition_object->isub_image;
		hint->itoc = merged_symbol->itoc;
		hint++;
	    }
	    if(host_byte_sex != target_byte_sex){
		hint = (struct twolevel_hint *)(output_addr +
			    output_hints_info.twolevel_hints_command.offset);
		swap_twolevel_hint(hint,
			       output_dysymtab_info.dysymtab_command.nundefsym,
			       target_byte_sex);
	    }
#ifndef RLD
	    output_flush(output_hints_info.twolevel_hints_command.offset,
			 output_dysymtab_info.dysymtab_command.nundefsym *
			 sizeof(struct twolevel_hint));
#endif
	}
	/*
	 * Now the undefsyms_order array is no longer needed.
	 */
	if(undefsyms_order != NULL){
	    free(undefsyms_order);
	    undefsyms_order = NULL;
	}
}

#if defined(RLD) && !defined(SA_RLD)
/*
 * output_rld_symfile_merged_symbols() copies the merged symbol table into the
 * output file for the rld symfile.  It makes all the symbols absolute.
 */
__private_extern__
void
output_rld_symfile_merged_symbols(
void)
{
    struct nlist *nlist;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    unsigned long string_offset;
    struct string_block **q, *string_block;
    unsigned long i;

	nlist = (struct nlist *)(output_addr +
				 output_symtab_info.symtab_command.symoff);
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->definition_object->set_num != cur_set)
		    continue;
		*nlist = merged_symbol->nlist;
		nlist->n_un.n_strx = output_symtab_info.output_merged_strsize +
				     merged_symbol_string_index(
					merged_symbol->nlist.n_un.n_name);
		nlist->n_sect = NO_SECT;
		nlist->n_type = N_ABS | N_EXT;
		nlist++;
	    }
	}

	/*
	 * Copy the merged strings into the memory buffer for the output file.
	 */
	string_offset = output_symtab_info.symtab_command.stroff +
			      output_symtab_info.output_merged_strsize;
	for(q = &(merged_string_blocks); *q; q = &(string_block->next)){
	    string_block = *q;
	    if(string_block->set_num != cur_set)
		continue;
	    memcpy(output_addr + output_symtab_info.symtab_command.stroff +
				 output_symtab_info.output_merged_strsize,
		   string_block->strings,
		   string_block->used);
	    output_symtab_info.output_merged_strsize += string_block->used;
	}
}
#endif /* defined(RLD) && !defined(SA_RLD) */

/*
 * merged_symbol_string_index() returns the string index of a merged symbol's
 * name relative to the start of the merged strings.
 */
static
unsigned long
merged_symbol_string_index(
char *symbol_name)
{
#ifndef RLD
    static struct string_block *string_block = NULL;

	if(string_block == NULL)
	    string_block = merged_string_blocks;

	if(symbol_name < string_block->strings ||
	   symbol_name >= string_block->strings + string_block->used)
	    string_block = get_string_block(symbol_name);
#else
    struct string_block *string_block;

	string_block = get_string_block(symbol_name);
#endif
	return(string_block->index + (symbol_name - string_block->strings));
}

/*
 * get_string_block() returns a pointer to the string block the specified
 * merged symbol name is in.
 */
static
struct string_block *
get_string_block(
char *symbol_name)
{
    struct string_block **p, *string_block;

	for(p = &(merged_string_blocks); *p; p = &(string_block->next)){
	    string_block = *p;
	    if(symbol_name >= string_block->strings &&
	       symbol_name < string_block->strings + string_block->used)
		return(string_block);
	}
	fatal("internal error: get_string_block() called with symbol_name (%s) "
	      "not in the string blocks", symbol_name);
	return(NULL); /* to prevent warning from compiler */
}

/*
 * process_undefineds() is called after all the dylibs have been searched in
 * layout.  It first checks for undefined symbols.  Then it sets the value of
 * nmerged_symbols_referenced_only_from_dylibs which is the number of merged
 * symbols that are only referenced from dylibs and will not appear in the
 * output file.
 */
__private_extern__
void
process_undefineds(
void)
{
    unsigned long i, j, Ycount, errors_save;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    enum bool printed_undef, allowed_undef, prebound_undef;
    struct object_list *object_list, **q;
#ifndef RLD
    unsigned long k;
    struct nlist *object_symbols;
    struct object_file *obj;
    struct undefined_list *undefined, *prevs;
    char *short_name;
    struct dynamic_library *dep, *lib, *prev_lib;
    unsigned long library_ordinal;
    enum bool reported, weak_ref_warning;
#endif

	errors_save = 0;
	printed_undef = FALSE;
	prebound_undef = FALSE;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		/*
		 * If the output file is not relocatable check to see if this
		 * symbol is undefined.  If it is and it is not on the allowed
		 * undefined list print it's name.
		 */
		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
		   merged_symbol->nlist.n_value == 0){
		    /*
		     * If we are dead stripping and this undefined symbol is not
		     * live just ignore it.
		     */
		    if(dead_strip == TRUE &&
		       merged_symbol->live == FALSE)
			continue;

		    if(prebinding == TRUE){
			if(ld_trace_prebinding_disabled == TRUE)
			    print("[Logging for XBS] prebinding"
				  " disabled for %s because of undefined "
				  "symbols\n", final_output != NULL ?
		      		  final_output : outputfile);
			warning("prebinding disabled because of undefined "
				"symbols");
			prebinding = FALSE;
			prebound_undef = TRUE;
		    }
		    allowed_undef = FALSE;
		    if(nundef_syms != 0){
			for(j = 0; j < nundef_syms; j++){
			    if(strcmp(undef_syms[j],
				  merged_symbol->nlist.n_un.n_name) == 0){
				allowed_undef = TRUE;
				break;
			    }
			}
		    }
		    if(allowed_undef == FALSE)
			noundefs = FALSE;
		    if(save_reloc == FALSE &&
		       (undefined_flag == UNDEFINED_ERROR ||
			undefined_flag == UNDEFINED_WARNING)){
			if(allowed_undef == FALSE || prebound_undef == TRUE){
			    if(printed_undef == FALSE){
				if(undefined_flag == UNDEFINED_WARNING)
				    warning("undefined symbols:");
				else{
				    if(allowed_undef == TRUE &&
				       prebound_undef == TRUE)
					errors_save = errors;
				    error("Undefined symbols:");
				    if(allowed_undef == TRUE &&
				       prebound_undef == TRUE)
					errors = errors_save;
				}
				printed_undef = TRUE;
			    }
			    else if(errors == 0 &&
				    undefined_flag == UNDEFINED_ERROR &&
				    allowed_undef == FALSE){
				errors = 1;
			    }
			    print("%s\n", merged_symbol->nlist.n_un.n_name);
			}
		    }
		    else if(save_reloc == FALSE &&
			    undefined_flag == UNDEFINED_DYNAMIC_LOOKUP &&
			    twolevel_namespace == TRUE){
			SET_LIBRARY_ORDINAL(merged_symbol->nlist.n_desc,
					    DYNAMIC_LOOKUP_ORDINAL);
		    }
		}
#ifndef RLD
		else {
		    /*
		     * The merged symbol is not an undefined symbol.  But could
		     * be defined in a dynamic library as a coalesed symbol or
		     * a weak symbol.  Where a later symbol was discarded from a
		     * non_dylib.  If -twolevel_namespace is in effect this
		     * symbol, now a reference from an object, is going into
		     * the image and will need the library ordinal recorded.
		     * We need to see that this dynamic library has been
		     * assigned an ordinal (that is it was listed on the link
		     * line or is a sub-framework or sub-umbrella of
		     * something listed).  If not flag this as an illegal
		     * reference to an indirect dynamic library if this library
		     * was not flagged already.
		     */
		    if(save_reloc == FALSE &&
		       twolevel_namespace == TRUE &&
		       merged_symbol->defined_in_dylib == TRUE &&
		       merged_symbol->referenced_in_non_dylib == TRUE &&
		       merged_symbol->definition_library->
			   definition_obj->library_ordinal == 0 &&
		       merged_symbol->definition_library->
			   indirect_twolevel_ref_flagged == FALSE){
			obj = cur_obj;
			cur_obj = merged_symbol->non_dylib_referenced_obj;
			error_with_cur_obj("illegal reference to symbol: %s "
			    "defined in indirectly referenced dynamic library "
			    "%s", merged_symbol->nlist.n_un.n_name,
			    merged_symbol->definition_library->dylib_file
				!= NULL ?
			    merged_symbol->definition_library->file_name :
			    merged_symbol->definition_library->dylib_name);
			cur_obj = obj;
			merged_symbol->definition_library->
			    indirect_twolevel_ref_flagged = TRUE;
		    }
		}
#endif /* !defined(RLD) */
	    }
	}

#ifndef RLD
	/*
	 * Deal with weak references.  If we have them and the target deployment
	 * does not support them generate a warning can clear the weak reference
	 * bit.
	 */
	if(macosx_deployment_target.major <= 1){
	    weak_ref_warning = FALSE;
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
	        merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(((merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
			 merged_symbol->nlist.n_value == 0) ||
		       (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD) &&
			(merged_symbol->nlist.n_desc & N_WEAK_REF) ==
			 N_WEAK_REF){
			if(weak_ref_warning == FALSE){
			    warning("weak symbol references not set in output "
				    "with MACOSX_DEPLOYMENT_TARGET environment "
				    "variable set to: %s",
				    macosx_deployment_target.name);
			    warning("weak referenced symbols:");
			    weak_ref_warning = TRUE;
			}
			merged_symbol->nlist.n_desc &= ~(N_WEAK_REF);
			print("%s\n", merged_symbol->nlist.n_un.n_name);
		    }
		}
	    }
	}
	/*
	 * The target deployment does support weak references.
	 */
	else{
	    /*
	     * If there have been some weak reference mismatches when symbols
	     * were merged make a pass through merged symbols and for any
	     * symbols that had a weak reference mismatch that is still
	     * undefined print the error for it.
	     */
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	        merged_symbol_list != NULL;
	        merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->weak_reference_mismatch == TRUE &&
		       ((merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
			 merged_symbol->nlist.n_value == 0) ||
		       (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD) &&
		       (merged_symbol->defined_in_dylib == FALSE ||
			merged_symbol->definition_library->
						force_weak_dylib == FALSE)){
			error("mismatching weak references for symbol: %s",
			      merged_symbol->nlist.n_un.n_name);
			for(q = &objects; *q; q = &(object_list->next)){
			    object_list = *q;
			    for(j = 0; j < object_list->used; j++){
				cur_obj = &(object_list->object_files[j]);
				if(cur_obj->dylib && cur_obj->dylib_module ==
						     NULL)
				    continue;
				if(cur_obj->bundle_loader)
				    continue;
				if(cur_obj->dylinker)
				    continue;
				for(k = 0; k < cur_obj->nundefineds; k++){
				    if(merged_symbol == cur_obj->
				       undefined_maps[k].merged_symbol){
					print_obj_name(cur_obj);
					object_symbols = (struct nlist *)
					    (cur_obj->obj_addr +
					     cur_obj->symtab->symoff);
					if((object_symbols[
					    cur_obj->undefined_maps[
					    k].index].n_desc & N_WEAK_REF) ==
					    N_WEAK_REF)
					    print("reference to weak %s\n",
					      merged_symbol->nlist.n_un.n_name);
					else
					    print("reference to non-weak %s\n",
					      merged_symbol->nlist.n_un.n_name);
				    }
				}
			    }
			}
		    }
		}
	    }
	}
#endif /* !defined(RLD) */

#ifndef RLD
	lib = NULL;
	prev_lib = NULL;
	/*
	 * There can be two-level references left on the undefined list.  These
	 * are "fake" merged symbols as they are not entered in the symbol
	 * merged table so the will not be reported in the above loop.  There
	 * can be many references to the same symbol expected to be defined in
	 * ( a specific library (from many different modules).
	 */
	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    undefined = undefined->next){
	    if(undefined->merged_symbol->twolevel_reference == TRUE){
		/*
		 * Avoid printing the same undefined symbol expected from a
		 * a specific library more then once by checking if we have
		 * already reported this symbol before.  This is very slow
		 * method but this is an error case.
		 */
		library_ordinal = GET_LIBRARY_ORDINAL(
				    undefined->merged_symbol->nlist.n_desc);
		if(library_ordinal == SELF_LIBRARY_ORDINAL)
		    lib = undefined->merged_symbol->referencing_library;
		/*
		 * Note that if library_ordinal was DYNAMIC_LOOKUP_ORDINAL then
		 * merge_dylib_module_symbols() in symbols.c would not have
		 * set the twolevel_reference field to TRUE in the merged_symbol
		 * and if we get here it with this it is an internal error.
		 */
		else if(library_ordinal == DYNAMIC_LOOKUP_ORDINAL)
		    fatal("internal error: process_undefineds() 1 with a "
			  "merged_symbol (%s) on the undefined list with "
			  "twolevel_reference == TRUE and library_ordinal == "
			  "DYNAMIC_LOOKUP_ORDINAL", undefined->merged_symbol->
			  nlist.n_un.n_name);
		else
		    lib = undefined->merged_symbol->referencing_library->
			    dependent_images[library_ordinal - 1];
		reported = FALSE;
		for(prevs = undefined_list.next;
		    prevs != undefined;
		    prevs = prevs->next){
		    if(prevs->merged_symbol->twolevel_reference == FALSE)
			continue;
		    library_ordinal = GET_LIBRARY_ORDINAL(
					prevs->merged_symbol->nlist.n_desc);
		    if(library_ordinal == SELF_LIBRARY_ORDINAL)
			prev_lib = prevs->merged_symbol->referencing_library;
		    /*
		     * Note that if library_ordinal was DYNAMIC_LOOKUP_ORDINAL
		     * then merge_dylib_module_symbols() in symbols.c would not
		     * have set the twolevel_reference field to TRUE in the
		     * merged_symbol and if we get here it with this it is an
		     * internal error.
		     */
		    else if(library_ordinal == DYNAMIC_LOOKUP_ORDINAL)
			fatal("internal error: process_undefineds() 2 with a "
			      "merged_symbol (%s) on the undefined list with "
			      "twolevel_reference == TRUE and library_ordinal "
			      "== DYNAMIC_LOOKUP_ORDINAL",
			      prevs->merged_symbol->nlist.n_un.n_name);
		    else
			prev_lib = prevs->merged_symbol->referencing_library->
				dependent_images[library_ordinal - 1];
		    if(lib == prev_lib &&
		       strcmp(undefined->merged_symbol->nlist.n_un.n_name,
			      prevs->merged_symbol->nlist.n_un.n_name) == 0){
			reported = TRUE;
			break;
		    }
		}
		if(reported == FALSE){
		    /*
		     * Since these are undefined two-level references they are
		     * never allowed and always cause an error.
		     */
		    if(printed_undef == FALSE){
			error("Undefined symbols:");
			printed_undef = TRUE;
		    }
		    print("%s ", undefined->merged_symbol->nlist.n_un.n_name);
		    dep = undefined->merged_symbol->referencing_library;
		    if(dep->umbrella_name != NULL)
			short_name = dep->umbrella_name;
		    else if(dep->library_name != NULL)
			short_name = dep->library_name;
		    else
			short_name = dep->dylib_name;
		    print("referenced from %s ", short_name);
		    if(lib->umbrella_name != NULL)
			short_name = lib->umbrella_name;
		    else if(lib->library_name != NULL)
			short_name = lib->library_name;
		    else
			short_name = lib->dylib_name;
		    print("expected to be defined in %s\n", short_name);
		}
	    }
	}
#endif /* !defined(RLD) */

	if(printed_undef == TRUE && Yflag != 0){
	    Ycount = 0;
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    cur_obj = &(object_list->object_files[i]);
		    if(cur_obj->dylib)
			continue;
		    if(cur_obj->bundle_loader)
			continue;
		    if(cur_obj->dylinker)
			continue;
		    for(j = 0; j < cur_obj->nundefineds; j++){
			merged_symbol =
			    cur_obj->undefined_maps[j].merged_symbol;
			if(merged_symbol == NULL ||
			   merged_symbol->twolevel_reference == TRUE)
			    continue;
			if(merged_symbol->nlist.n_type == (N_EXT|N_UNDF) &&
			   merged_symbol->nlist.n_value == 0){
			    if(Ycount >= Yflag){
				print("more references to undefined "
				      "symbols ...\n");
				goto done;
			    }
			    print_obj_name(cur_obj);
			    print("%sreference to undefined %s",
				  merged_symbol->nlist.n_desc & N_WEAK_REF ?
				  "weak " : "",
				  merged_symbol->nlist.n_un.n_name);
#ifndef RLD
			    library_ordinal = GET_LIBRARY_ORDINAL(
					       merged_symbol->nlist.n_desc);
			    if(merged_symbol->twolevel_reference == TRUE &&
			       library_ordinal != DYNAMIC_LOOKUP_ORDINAL){
				if(library_ordinal == SELF_LIBRARY_ORDINAL)
				    lib = merged_symbol->referencing_library;
				else
				    lib = merged_symbol->referencing_library->
					  dependent_images[library_ordinal - 1];
				if(lib->umbrella_name != NULL)
				    short_name = lib->umbrella_name;
				else if(lib->library_name != NULL)
				    short_name = lib->library_name;
				else
				    short_name = lib->dylib_name;
				print(" expected to be defined in %s\n",
				      short_name);
			    }
			    else
#endif /* !defined(RLD) */
				print("\n");
			    Ycount++;
			}
		    }
		}
	    }
	}
done:
	/*
	 * Determine the number of merged symbols that are only referenced from
	 * dylibs.  These will not be in the output file so this count is need
	 * so the number of merged symbols in the output file is know for
	 * laying out the output file.
	 */
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		/*
		 * If this symbol is only referenced from a dylib then it will
		 * not be in the file's output.
		 */
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    nmerged_symbols_referenced_only_from_dylibs++;
	    }
	}
}

#ifndef RLD
/*
 * reset_prebound_undefines() resets the prebound undefined symbols back to
 * undefined symbols if prebinding is not to be done.
 */
__private_extern__
void
reset_prebound_undefines(
void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol, *indr_symbol;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if((merged_symbol->nlist.n_type & N_TYPE) == N_PBUD){
		    /*
		     * If not prebinding then reset this prebound undefined
		     * to an undefined symbol.
		     */
		    if(prebinding == FALSE){
			merged_symbol->nlist.n_type = N_UNDF | N_EXT;
			merged_symbol->nlist.n_value = 0;
			merged_symbol->nlist.n_desc &= ~REFERENCED_DYNAMICALLY;
		    }
		}
		else if(merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		        merged_symbol->defined_in_dylib == TRUE){
		    /*
		     * If not prebinding then reset this indirect symbol that
		     * was defined in a dylib back to an undefined symbol.
		     */
		    if(prebinding == FALSE){
			merged_symbol->nlist.n_type = N_UNDF | N_EXT;
			merged_symbol->nlist.n_value = 0;
			merged_symbol->nlist.n_desc &= ~REFERENCED_DYNAMICALLY;
		    }
		    else{
			/*
			 * When prebinding if the indirect symbol is defined
			 * (not an undefined of common) then change the indirect
			 * symbol to a prebound undefined using the value of the
			 * indr symbol.  Else make it an undefined symbol.
			 */
			indr_symbol = (struct merged_symbol *)
				      (merged_symbol->nlist.n_value);
			if(indr_symbol->nlist.n_type != (N_EXT | N_UNDF)){
			    merged_symbol->nlist.n_type = N_PBUD | N_EXT;
			    merged_symbol->nlist.n_sect = NO_SECT;
			    /*
			     * Do not change the n_desc of the symbol as it
			     * contains the proper LAZY or NON-LAZY reference
			     * bits as well as the REFERENCED_DYNAMICALLY bit.
			     */
			    merged_symbol->nlist.n_value =
						    indr_symbol->nlist.n_value;
			}
			else{
			    merged_symbol->nlist.n_type = N_UNDF | N_EXT;
			    merged_symbol->nlist.n_value = 0;
			    merged_symbol->nlist.n_desc &=
							~REFERENCED_DYNAMICALLY;
			}
		    }
		}
	    }
	}
}
#endif /* !defined(RLD) */

/*
 * assign_output_symbol_indexes() assigns the symbol indexes to all symbols in
 * the output file based on the type of output file (MH_DYLIB or not).  The
 * difference for the MH_DYLIB format is that the external symbol are grouped
 * by the module they are defined in instead of being sorted by symbol name.
 * The order of the symbol table is as follows:
 * 	Local Symbols
 *	    Grouped by the module they are defined in
 *		sect_object_symbol (if specified)
 *		local symbols in the same order as the input module
 *		private_extern symbols (if -keep_private_externs is FALSE)
 *	Exterally defined Symbols
 *	    Sorted by name for non-MH_DYLIB format
 *	    Grouped by the module they are defined in for MH_DYLIB format
 *	Undefinded Symbols
 *	    Sorted by name
 */
__private_extern__
void
assign_output_symbol_indexes(
void)
{
    unsigned long index, i, nextdefsym, nundefsym, n_pext;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol, *indr_symbol;
    struct object_list *object_list, **q;
    struct object_file *last_object;
    enum bool rebuild_merged_string_table;

	rebuild_merged_string_table = FALSE;
	if(strip_level == STRIP_ALL){
	    if(has_dynamic_linker_command){
		strip_level = STRIP_DYNAMIC_EXECUTABLE;
		/*
		 * In order to not put out strings for merged symbols that will
		 * be discared we need to rebuild the merged string table for
		 * only the symbols not stripped.
		 */
		merged_string_blocks = NULL;
		merged_string_size = 0;
		rebuild_merged_string_table = TRUE;
		/*
		 * The value of nstripped_merged_symbols is reset here since we
		 * are setting the strip_level to STRIP_DYNAMIC_EXECUTABLE.
		 * This routine will then increment nstripped_merged_symbols
		 * for the symbols to be stripped. It may have previouly held
		 * the count of dead symbols to strip if -dead_strip was
		 * specified. We start with the the number of dead stripped
		 * merged private symbols then add the live merged symbols to
		 * strip.  This works since any symbol to be saved with
		 * STRIP_DYNAMIC_EXECUTABLE would also be live since it would
		 * have REFERENCED_DYNAMICALLY set.  Except for undefined
		 * symbols, so if -dead_strip is specified and the undefined
		 * symbol is not live then nstripped_merged_symbols
		 * is incremented to account for these.
		 */
		nstripped_merged_symbols = nstripped_merged_private_symbols;
	    }
	    else{
		seglinkedit = FALSE;
		return;
	    }
	}
	/*
	 * If we are stripping non-globals and we are not keeping private
	 * externs and we have some private externs in the merged symbol table,
	 * and the output is not a multi-module dylib, then in order to not put
	 * out strings for them we also need to rebuild the merged string table
	 * without these symbols.
	 */
	else if(strip_level == STRIP_NONGLOBALS &&
		keep_private_externs == FALSE &&
		nmerged_private_symbols != 0 &&
		(filetype != MH_DYLIB || multi_module_dylib == FALSE)){
	    merged_string_blocks = NULL;
	    merged_string_size = 0;
	    rebuild_merged_string_table = TRUE;
	}

	/*
	 * Add a copy of the object file for the common symbols that the link
	 * editor allocated into the object file list.  Since it is possible
	 * that some of the common symbols are not on the export list they could
	 * have been made into private externs.
	 */
	last_object = add_last_object_file(&link_edit_common_object);

	/*
	 * Private exterals are always kept when any symbols are kept (except in
	 * the case of STRIP_DYNAMIC_EXECUTABLE).  The private externals on the
	 * merged symbol list may be kept as local symbols or external depending
	 * on the keep_private_externs flag. Private externals that are local
	 * symbols (no N_EXT bit set) are always counted in the
	 * cur_obj->nlocalsym unless the strip level is STRIP_ALL.
	 */

	/*
	 * Note if -dead_strip is specified the values of the nlocalsym and
	 * nprivatesym fields in the object_file structs were previously
	 * adjusted to account for only the live symbols in
	 * count_live_symbols().
	 */

	index = 0;
	output_dysymtab_info.dysymtab_command.ilocalsym = index;
	/*
	 * Set the indexes into the symbol table for local symbols.
	 * Private exterals are counted as local symbols if keep_private_externs
	 * is FALSE.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		if(cur_obj->dylib)
		    continue;
		if(cur_obj->bundle_loader)
		    continue;
		if(cur_obj->dylinker)
		    continue;
#ifdef RLD
		/*
		 * If this object is not from the current set
		 * don't count these.
		 */
		if(cur_obj->set_num != cur_set)
		    continue;
#endif /* RLD */
		cur_obj->ilocalsym = index;
/*
print_obj_name(cur_obj);
print(" cur_obj->nlocalsym %lu\n", cur_obj->nlocalsym);
*/
		index += cur_obj->nlocalsym;

		if(keep_private_externs == FALSE){
		    cur_obj->iprivatesym = index;
		    cur_obj->cprivatesym = index;
		    if(strip_level != STRIP_DYNAMIC_EXECUTABLE &&
		       (strip_level != STRIP_NONGLOBALS ||
			(filetype == MH_DYLIB && multi_module_dylib == TRUE))){
			index += cur_obj->nprivatesym;
/*
print(" adding cur_obj->nprivatesym %lu to index\n", cur_obj->nprivatesym);
*/
		    }
		    else{
			nstripped_merged_symbols +=
			    cur_obj->nprivatesym;
			nstripped_merged_private_symbols +=
			    cur_obj->nprivatesym;
/*
print("assign_output_symbol_indexes() adding cur_obj->nprivatesym %lu to nstripped_merged_symbols\n", cur_obj->nprivatesym);
*/
		    }
		}
	    }
	}
	/*
	 * Check to make sure the counts are consistent.
	 */
	if((keep_private_externs == TRUE && index != nlocal_symbols) ||
	   (keep_private_externs == FALSE && index != nlocal_symbols +
	    nmerged_private_symbols - nstripped_merged_private_symbols))
		fatal("internal error: assign_output_symbol_indexes() "
		      "inconsistent local symbol counts");
	output_dysymtab_info.dysymtab_command.nlocalsym = index;

	/*
	 * Copy the values that got set in the above loop back into the
	 * object file for the the common symbols.  Then remove the copy of
	 * the object file from the object file list.
	 */
	link_edit_common_object = *last_object;
	remove_last_object_file(last_object);


	/*
	 * Count the number of undefined symbols and defined external symbols.
	 * Private exterals are counted as defined externals if
	 * keep_private_externs is TRUE.
	 */
	nundefsym = 0;
	nextdefsym = 0;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    continue;
		if(strip_base_symbols == TRUE &&
		   merged_symbol->definition_object == base_obj)
		    continue;
#ifdef RLD
		if(merged_symbol->definition_object->set_num != cur_set)
		    continue;
#endif /* RLD */
		/*
		 * The value of nstripped_merged_symbols is recalculated if we
		 * set the strip_level to STRIP_DYNAMIC_EXECUTABLE in the case
		 * -dead_strip is specified.
		 */
		if(strip_level != STRIP_DYNAMIC_EXECUTABLE &&
		   dead_strip == TRUE && merged_symbol->live == FALSE)
		    continue;
		if((merged_symbol->nlist.n_type & N_EXT) == N_EXT &&
		   ((merged_symbol->nlist.n_type & N_TYPE) == N_UNDF ||
		    (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD) |
		   (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		    merged_symbol->defined_in_dylib == TRUE)){
		    if(dead_strip == FALSE || merged_symbol->live == TRUE){
			nundefsym++;
			if(rebuild_merged_string_table == TRUE)
			    merged_symbol->nlist.n_un.n_name =
				enter_string(merged_symbol->nlist.n_un.n_name,
					     NULL);
		    }
		    else{
/*
printf("assign_output_symbol_indexes() nstripped_merged_symbols incremented for undefined %s\n", merged_symbol->nlist.n_un.n_name);
*/
			nstripped_merged_symbols++;
		    }
		}
		else{
		    if(merged_symbol->nlist.n_type == (N_EXT | N_INDR)){
			indr_symbol = (struct merged_symbol *)
				    (merged_symbol->nlist.n_value);
			n_pext = indr_symbol->nlist.n_type & N_PEXT;
		    }
		    else{
			n_pext = merged_symbol->nlist.n_type & N_PEXT;
		    }
		    if(keep_private_externs == TRUE || n_pext == 0){
			if(strip_level != STRIP_DYNAMIC_EXECUTABLE ||
			   (merged_symbol->nlist.n_desc &
			    REFERENCED_DYNAMICALLY) == REFERENCED_DYNAMICALLY){
			    nextdefsym++;
			    if(rebuild_merged_string_table == TRUE)
				merged_symbol->nlist.n_un.n_name =
				    enter_string(merged_symbol->
						 nlist.n_un.n_name, NULL);
			}
			else{
/*
printf("assign_output_symbol_indexes() nstripped_merged_symbols incremented for %s\n", merged_symbol->nlist.n_un.n_name);
*/
			    nstripped_merged_symbols++;
			}
		    }
		}
	    }
	}

	/*
	 * Allocate arrays to order the undefined symbols and defined external
	 * symbols.
	 */
	undefsyms_order  = allocate(nundefsym *
				    sizeof(struct merged_symbol *));
	extdefsyms_order = allocate(nextdefsym *
				    sizeof(struct merged_symbol *));
	/*
	 * Fill in the arrays with their respective symbols.
	 */
	nundefsym = 0;
	nextdefsym = 0;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    continue;
		if(strip_base_symbols == TRUE &&
		   merged_symbol->definition_object == base_obj)
		    continue;
#ifdef RLD
		if(merged_symbol->definition_object->set_num != cur_set)
		    continue;
#endif /* RLD */
		if(dead_strip == TRUE && merged_symbol->live == FALSE)
		    continue;
		if(((merged_symbol->nlist.n_type & N_EXT) == N_EXT &&
		    ((merged_symbol->nlist.n_type & N_TYPE) == N_UNDF ||
		     (merged_symbol->nlist.n_type & N_TYPE) == N_PBUD)) ||
		   (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		    merged_symbol->defined_in_dylib == TRUE))
		    undefsyms_order[nundefsym++] = merged_symbol;
		else{
		    if(merged_symbol->nlist.n_type == (N_EXT | N_INDR)){
			indr_symbol = (struct merged_symbol *)
				    (merged_symbol->nlist.n_value);
			n_pext = indr_symbol->nlist.n_type & N_PEXT;
		    }
		    else{
			n_pext = merged_symbol->nlist.n_type & N_PEXT;
		    }
		    if(keep_private_externs == TRUE || n_pext == 0){
			if(strip_level != STRIP_DYNAMIC_EXECUTABLE ||
			   (merged_symbol->nlist.n_desc &
			    REFERENCED_DYNAMICALLY) == REFERENCED_DYNAMICALLY){
			    extdefsyms_order[nextdefsym++] = merged_symbol;
			}
			else{
			    if((merged_symbol->nlist.n_type & N_TYPE) == N_ABS)
				merged_symbol->output_index =
				    INDIRECT_SYMBOL_ABS | INDIRECT_SYMBOL_LOCAL;
			    else
				merged_symbol->output_index =
				    INDIRECT_SYMBOL_LOCAL;
			}
		    }
		}
	    }
	}
#ifndef SA_RLD
	/*
	 * Sort the defined symbols by module for MH_DYLIB formats and by
	 * name for other formats.
	 */
	if(filetype == MH_DYLIB)
	    qsort(extdefsyms_order, nextdefsym, sizeof(struct merged_symbol *),
		  (int (*)(const void *, const void *))qsort_by_module);
	else
	    qsort(extdefsyms_order, nextdefsym, sizeof(struct merged_symbol *),
		  (int (*)(const void *, const void *))qsort_by_name);
	/*
	 * Sort the undefined symbols.  If we are doing bind_at_load then sort
	 * them by the order the symbols were seen else sort them by name.
	 */
	if(bind_at_load == TRUE)
	    qsort(undefsyms_order, nundefsym, sizeof(struct merged_symbol **),
		  (int (*)(const void *, const void *))qsort_by_undef_order);
	else
	    qsort(undefsyms_order, nundefsym, sizeof(struct merged_symbol **),
		  (int (*)(const void *, const void *))qsort_by_name);
#endif /* !defined(SA_RLD) */

	/*
	 * Assign the symbol indexes to the defined symbols.
	 */
	output_dysymtab_info.dysymtab_command.iextdefsym = index;
	output_dysymtab_info.dysymtab_command.nextdefsym = nextdefsym;
	cur_obj = NULL;
	for(i = 0; i < nextdefsym; i++){
	    if(filetype == MH_DYLIB){
		if(cur_obj != extdefsyms_order[i]->definition_object){
		    cur_obj = extdefsyms_order[i]->definition_object;
		    cur_obj->iextdefsym = index;
		}
	    }
	    extdefsyms_order[i]->output_index = index++;
	}

	/*
	 * Assign the symbol indexes to the undefined symbols.
	 */
	output_dysymtab_info.dysymtab_command.iundefsym = index;
	output_dysymtab_info.dysymtab_command.nundefsym = nundefsym;
	for(i = 0; i < nundefsym; i++){
	    undefsyms_order[i]->output_index = index++;
	}

	/*
	 * If -twolevel_namespace is in effect set the number of the two-level
	 * hints in the hints table to the number of undefined symbols.
	 */
	if(twolevel_namespace == TRUE)
	    output_hints_info.twolevel_hints_command.nhints = nundefsym;

	/*
	 * Assign the symbol indexes to the private extern symbols if they are
	 * turned into local symbols.
	 */
	if(nmerged_private_symbols != 0 && keep_private_externs == FALSE){
	    cur_obj = NULL;
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
		merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->referenced_in_non_dylib == FALSE)
			continue;
		    if(strip_base_symbols == TRUE &&
		       merged_symbol->definition_object == base_obj)
			continue;
#ifdef RLD
		    if(merged_symbol->definition_object->set_num != cur_set)
			continue;
#endif /* RLD */
		    if(dead_strip == TRUE && merged_symbol->live == FALSE)
			continue;
		    if(merged_symbol->nlist.n_type & N_PEXT){
			merged_symbol->output_index =
				merged_symbol->definition_object->cprivatesym++;
		    }
		}
	    }
	}
}

#ifndef SA_RLD
/*
 * qsort_by_module() is used by assign_output_symbol_indexes() to sort (in
 * this case group) the defined external symbols by the module they are defined
 * in for the MH_DYLIB format.
 */
static
int
qsort_by_module(
const struct merged_symbol **ms1,
const struct merged_symbol **ms2)
{
	return((int)((*ms1)->definition_object) -
	       (int)((*ms2)->definition_object));
}

/*
 * qsort_by_name() is used by assign_output_symbol_indexes() to sort the
 * the defined external symbols and the undefined symbols by symbol name.
 */
static
int
qsort_by_name(
const struct merged_symbol **ms1,
const struct merged_symbol **ms2)
{
	return(strcmp((*ms1)->nlist.n_un.n_name, (*ms2)->nlist.n_un.n_name));
}

/*
 * qsort_by_undef_order() is used by assign_output_symbol_indexes() to sort the
 * the undefined symbols by the order the undefined symbol appeared.
 */
static
int
qsort_by_undef_order(
const struct merged_symbol **ms1,
const struct merged_symbol **ms2)
{
	return(((*ms1)->undef_order - (*ms2)->undef_order));
}

/*
 * merged_symbol_output_index() returns the index in the output file's symbol
 * table for the merged_symbol pointer passed to it.
 */
__private_extern__
unsigned long
merged_symbol_output_index(
struct merged_symbol *merged_symbol)
{
    return(merged_symbol->output_index);
}
#endif /* !defined(SA_RLD) */

#ifndef RLD
/*
 * This is a pointer to the module name saved in the merged string table for
 * the one module table entry for a single module dylib.
 */
char *dylib_single_module_name;

/*
 * layout_dylib_tables() sizes and readys the tables for a dynamic library file.
 * The merged symbol indexes have already been assigned before this is called.
 * There are three tables:
 *	The reference table
 *	The module table
 *	The table of contents
 */
__private_extern__
void
layout_dylib_tables(
void)
{
    unsigned long i, j, flags;
    struct merged_symbol *merged_symbol;
    struct object_list *object_list, **q;
    char *p;

	if(multi_module_dylib == TRUE){
	    /*
	     * For multi module dylibs the reference table was sized as the
	     * symbols were merged.  All that is left to do for the reference
	     * table is to adjust the flags for undefined references that ended
	     * up referencing private externs.
	     */
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    cur_obj = &(object_list->object_files[i]);
		    if(cur_obj->dylib)
			continue;
		    if(cur_obj->bundle_loader)
			continue;
		    if(cur_obj->dylinker)
			continue;
		    for(j = 0; j < cur_obj->nrefsym; j++){
			merged_symbol =
			    cur_obj->reference_maps[j].merged_symbol;
			if(merged_symbol->nlist.n_type & N_PEXT){
			    flags = cur_obj->reference_maps[j].flags;
			    if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY)
				cur_obj->reference_maps[j].flags =
				    REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY;
			    else if(flags == REFERENCE_FLAG_UNDEFINED_LAZY)
				cur_obj->reference_maps[j].flags =
				    REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY;
			}
			else{
			    /*
			     * The merged symbol is not a private extern. So it
			     * might be a non-weak symbol that is being used and
			     * some weak private externs refs were discarded.
			     * If so we need to make the refs non-weak.
			     */
			    flags = cur_obj->reference_maps[j].flags;
			    if(flags ==
			       REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY)
				cur_obj->reference_maps[j].flags =
				    REFERENCE_FLAG_UNDEFINED_NON_LAZY;
			    else if(flags ==
				    REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY)
				cur_obj->reference_maps[j].flags =
				    REFERENCE_FLAG_UNDEFINED_LAZY;
			}
		    }
		}
	    }
	}
	else{
	    /*
	     * For single module dylibs the reference table size is reset here
	     * from the defined and undefined merged symbols.  The contents of
	     * the reference table for single module dylibs will be filled in
	     * output_dylib_tables() from the merged symbol table.
	     */
	    output_dysymtab_info.dysymtab_command.nextrefsyms =
		output_dysymtab_info.dysymtab_command.nextdefsym +
		output_dysymtab_info.dysymtab_command.nundefsym;
	}

	if(multi_module_dylib == TRUE){
	    /*
	     * For multi module dylibs the module table is sized from the number
	     * of modules loaded.  The module_name of each module in the dynamic
	     * shared library is set from base name or archive member name of
	     * the object loaded.  The string for the module_name is then saved
	     * with the merged strings so that it can be converted to a string
	     * table index on output.
	     */
	    output_dysymtab_info.dysymtab_command.nmodtab = 0;
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    cur_obj = &(object_list->object_files[i]);
		    if(cur_obj->dylib == TRUE)
			continue;
		    if(cur_obj->bundle_loader == TRUE)
			continue;
		    cur_obj->imodtab =
			output_dysymtab_info.dysymtab_command.nmodtab;
		    output_dysymtab_info.dysymtab_command.nmodtab++;
		    if(cur_obj->ar_hdr){
			p = allocate(cur_obj->ar_name_size + 1);
			memcpy(p, cur_obj->ar_name, cur_obj->ar_name_size);
			p[cur_obj->ar_name_size] = '\0';
			cur_obj->module_name = enter_string(p, NULL);
			free(p);
		    }
		    else{
			p = strrchr(cur_obj->file_name, '/');
			if(p != NULL)
			    p++;
			else
			    p = cur_obj->file_name;
			cur_obj->module_name = enter_string(p, NULL);
		    }
		}
	    }
	}
	else{
	    /*
	     * For single module dylibs there is one module table entry.
	     * The module_name is set to "single module".  The string for the
	     * module_name is then saved with the merged strings so that it can
	     * be converted to a string table index on output.
	     */
	    output_dysymtab_info.dysymtab_command.nmodtab = 1;
	    dylib_single_module_name = enter_string("single module", NULL);
	}

	/*
	 * The table of contents is sized from the number of defined external
	 * symbols.
	 */
	output_dysymtab_info.dysymtab_command.ntoc =
	    output_dysymtab_info.dysymtab_command.nextdefsym;
}

/*
 * output_dylib_tables() outputs the tables for a dynamic library file.
 * There are three tables:
 *	The reference table
 *	The module table
 *	The table of contents
 */
__private_extern__
void
output_dylib_tables(
void)
{
    unsigned long i, j, flush_offset, ntoc;
    struct object_list *object_list, **q;
    struct dylib_reference *ref, *refs;
    struct dylib_module *mod, *mods;
    struct merged_symbol **toc_order;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;
    struct dylib_table_of_contents *tocs, *toc;
    struct merged_section *ms;

	/*
	 * Output the reference table.
	 */
	flush_offset = output_dysymtab_info.dysymtab_command.extrefsymoff;
	refs = (struct dylib_reference *)(output_addr + flush_offset);
	ref = refs;
	if(multi_module_dylib == TRUE){
	    /*
	     * For multi module dylibs there is a reference table for each
	     * object loaded built from the reference_maps.
	     */
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    cur_obj = &(object_list->object_files[i]);
		    if(cur_obj->dylib)
			continue;
		    if(cur_obj->bundle_loader)
			continue;
		    if(cur_obj->dylinker)
			continue;
		    for(j = 0; j < cur_obj->nrefsym; j++){
			ref->isym = merged_symbol_output_index(
				    cur_obj->reference_maps[j].merged_symbol);
			ref->flags = cur_obj->reference_maps[j].flags;
			ref++;
		    }
		}
	    }
	}
	else{
	    /*
	     * For single module dylibs there is one reference table and it is
	     * built from the merged symbol table.
	     */
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
		merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->referenced_in_non_dylib == FALSE)
			continue;
		    if(dead_strip == TRUE && merged_symbol->live == FALSE)
			continue;
		    if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		       merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		       (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
			merged_symbol->defined_in_dylib == TRUE)){
			ref->isym = merged_symbol_output_index(merged_symbol);
			ref->flags = merged_symbol->nlist.n_desc &
				     REFERENCE_TYPE;
			ref++;
		    }
		    else if((merged_symbol->nlist.n_type & N_PEXT) == 0){
			ref->isym = merged_symbol_output_index(merged_symbol);
			ref->flags = REFERENCE_FLAG_DEFINED;
			ref++;
		    }
		}
	    }
	}
	if(host_byte_sex != target_byte_sex){
	    swap_dylib_reference(refs,
			    output_dysymtab_info.dysymtab_command.nextrefsyms,
			    target_byte_sex);
	}
	output_flush(flush_offset,
		     output_dysymtab_info.dysymtab_command.nextrefsyms *
		     sizeof(struct dylib_reference));

	/*
	 * Output the module table.
	 */
	flush_offset = output_dysymtab_info.dysymtab_command.modtaboff;
	mods = (struct dylib_module *)(output_addr + flush_offset);
	mod = mods;
	if(multi_module_dylib == TRUE){
	    /*
	     * For multi module dylibs there is a module table for each
	     * object loaded built from the info saved in the object struct.
	     */
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    cur_obj = &(object_list->object_files[i]);
		    if(cur_obj->dylib == TRUE)
			continue;
		    if(cur_obj->bundle_loader == TRUE)
			continue;
		    mod->module_name = STRING_SIZE_OFFSET +
			       merged_symbol_string_index(cur_obj->module_name);
		    mod->iextdefsym = cur_obj->iextdefsym;
		    mod->nextdefsym = cur_obj->nextdefsym;

		    mod->nrefsym = cur_obj->nrefsym;
		    if(mod->nrefsym == 0)
    			mod->irefsym = 0;
		    else
			mod->irefsym = cur_obj->irefsym;

		    mod->nlocalsym = cur_obj->nlocalsym + cur_obj->nprivatesym;
		    if(mod->nlocalsym == 0)
			mod->ilocalsym = 0;
		    else
			mod->ilocalsym = cur_obj->ilocalsym;

		    mod->nextrel = cur_obj->nextrel;
		    if(mod->nextrel == 0)
			mod->iextrel = 0;
		    else
			mod->iextrel    = cur_obj->iextrel;

		    mod->ninit_nterm = (cur_obj->nterm << 16) | cur_obj->ninit;
		    if(cur_obj->ninit == 0)
			cur_obj->iinit = 0;
		    if(cur_obj->nterm == 0)
			cur_obj->iterm = 0;
		    mod->iinit_iterm = (cur_obj->iterm << 16) | cur_obj->iinit;
		    if(cur_obj->objc_module_info != NULL){
			mod->objc_module_info_addr =
			    cur_obj->objc_module_info->output_section->s.addr +
			    cur_obj->objc_module_info->offset;
			mod->objc_module_info_size =
			    cur_obj->objc_module_info->s->size;
		    }
		    else{
			mod->objc_module_info_addr = 0;
			mod->objc_module_info_size = 0;
		    }
		    mod++;
		}
	    }
	}
	else{
	    /*
	     * For single module dylibs there is one module table entry.
	     */
	    mod->module_name = STRING_SIZE_OFFSET +
			   merged_symbol_string_index(dylib_single_module_name);
	    mod->iextdefsym =
		output_dysymtab_info.dysymtab_command.iextdefsym;
	    mod->nextdefsym =
		output_dysymtab_info.dysymtab_command.nextdefsym;
	    mod->irefsym = 0;
	    mod->nrefsym =
		output_dysymtab_info.dysymtab_command.nextrefsyms;
	    mod->ilocalsym =
		output_dysymtab_info.dysymtab_command.ilocalsym;
	    mod->nlocalsym =
		output_dysymtab_info.dysymtab_command.nlocalsym;
	    mod->iextrel = 0;
	    mod->nextrel =
		output_dysymtab_info.dysymtab_command.nextrel;
	    mod->iinit_iterm = 0;
	    mod->ninit_nterm = (nterm << 16) | ninit;
	    ms = lookup_merged_section(SEG_OBJC, SECT_OBJC_MODULES);
	    if(ms != NULL){
		mod->objc_module_info_addr = ms->s.addr;
		mod->objc_module_info_size = ms->s.size;
	    }
	    else{
		mod->objc_module_info_addr = 0;
		mod->objc_module_info_size = 0;
	    }
	}
	if(host_byte_sex != target_byte_sex){
	    swap_dylib_module(mods,
			      output_dysymtab_info.dysymtab_command.nmodtab,
			      target_byte_sex);
	}
	output_flush(flush_offset,
		     output_dysymtab_info.dysymtab_command.nmodtab *
		     sizeof(struct dylib_module));

	/*
	 * Output the table of contents.
	 */
	toc_order = allocate(output_dysymtab_info.dysymtab_command.ntoc *
			     sizeof(struct merged_symbol *));
	ntoc = 0;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->referenced_in_non_dylib == FALSE)
		    continue;
		if((merged_symbol->nlist.n_type & N_TYPE) != N_UNDF &&
		   (merged_symbol->nlist.n_type & N_TYPE) != N_PBUD &&
		   (merged_symbol->nlist.n_type & N_PEXT) == 0)
		    toc_order[ntoc++] = merged_symbol;
	    }
	}
	if(ntoc != output_dysymtab_info.dysymtab_command.ntoc)
	    fatal("internal error: output_dylib_tables() inconsistent toc "
		  "counts");
	qsort(toc_order, ntoc, sizeof(struct merged_symbol *),
	      (int (*)(const void *, const void *))qsort_by_name);
	flush_offset = output_dysymtab_info.dysymtab_command.tocoff;
	tocs = (struct dylib_table_of_contents *)(output_addr +
							 flush_offset);
	toc = tocs;
	for(i = 0; i < ntoc; i++){
	    toc->symbol_index = merged_symbol_output_index(toc_order[i]);
	    toc->module_index = object_index(toc_order[i]->definition_object);
	    toc++;
	}
	if(host_byte_sex != target_byte_sex){
	    swap_dylib_table_of_contents(tocs, ntoc, target_byte_sex);
	}
	output_flush(flush_offset, ntoc *
				   sizeof(struct dylib_table_of_contents));
	free(toc_order);
}

/*
 * When any merged_symbol has its flagged_read_only_reloc set then this static
 * is also set.  This allows clear_read_only_reloc_flags() to avoid doing any
 * work.
 */
static enum bool some_read_only_reloc_flags_set = FALSE;

/*
 * clear_read_only_reloc_flags() clears the flagged_read_only_reloc flags on
 * all the merged symbols.
 */
__private_extern__
void
clear_read_only_reloc_flags(
void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	if(some_read_only_reloc_flags_set == FALSE)
	    return;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		merged_symbol->flagged_read_only_reloc = FALSE;
	    }
	}
	some_read_only_reloc_flags_set = FALSE;
}

/*
 * flag_read_only_reloc() is called to flag an external relocation entry
 * refering to output_index in the specified section.  If the symbol has not
 * already been flaged it's name is printed.  Also if first_time point to
 * a TRUE value a leading print statement is done.
 */
__private_extern__
void
flag_read_only_reloc(
struct section *s,
unsigned long output_index,
enum bool *first_time)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	if(*first_time == TRUE){
	    if(read_only_reloc_flag == READ_ONLY_RELOC_ERROR)
		error_with_cur_obj("has external relocation entries in "
		    "non-writable section (%.16s,%.16s) for symbols:",
		    s->segname, s->sectname);
	    else
		warning_with_cur_obj("has external relocation entries in "
		    "non-writable section (%.16s,%.16s) for symbols:",
		    s->segname, s->sectname);
	    *first_time = FALSE;
	}

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if(merged_symbol->output_index == output_index){
		    if(merged_symbol->flagged_read_only_reloc == FALSE){
			print("%s\n", merged_symbol->nlist.n_un.n_name);
			merged_symbol->flagged_read_only_reloc = TRUE;
			some_read_only_reloc_flags_set = TRUE;
		    }
		    return;
		}
	    }
	}
}
#endif /* !defined(RLD) */

#ifdef RLD
/*
 * free_multiple_defs() frees the multiple_defs array and resets the count to
 * zero if it exist.
 */
__private_extern__
void
free_multiple_defs(void)
{
	if(nmultiple_defs != 0){
	    free(multiple_defs);
	    multiple_defs = NULL;
	    nmultiple_defs = 0;
	}
}

/*
 * remove_merged_symbols() removes the merged symbols that are defined in the
 * current object file set and their strings.  This take advantage of the fact
 * that symbols from the current set of symbols were all merged after the
 * previous set and appear last in symbol list and hash table.
 */
__private_extern__
void
remove_merged_symbols(void)
{
    long i;
    unsigned long j;
    struct merged_symbol_list *m, *merged_symbol_list, *prev_merged_symbol_list,
			      *next_merged_symbol_list;
    enum bool have_some_symbols;
    struct merged_symbol_chunk *p, *first_chunk, *prev_chunk, *next_chunk;
    struct string_block *string_block, *prev_string_block, *next_string_block;

	/*
	 * Clear all the merged symbol table entries for symbols that come
	 * from the current set of object files.
	 */

	/*
	 * First clear all symbol pointers in the merged_symbol_list from this
	 * set.  Then if there are any symbol lists with no used symbols free
	 * them.
	 */
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = merged_symbol_list->used - 1; i >= 0; i--){
		if(merged_symbol_list->symbols[i] != NULL &&
		   merged_symbol_list->symbols[i]->name_len != 0 &&
		   merged_symbol_list->symbols[i]->definition_object != NULL &&
		   merged_symbol_list->symbols[i]->definition_object->set_num ==
		   cur_set){
		    merged_symbol_list->symbols[i] = NULL;
		    merged_symbol_list->used--;
		}
	    }
	}
	/*
	 * Find the first symbol list that now has 0 entries used if any.
	 */
	prev_merged_symbol_list = merged_symbol_root == NULL ? NULL :
				  merged_symbol_root->list;
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    if(merged_symbol_list->used == 0)
		break;
	    prev_merged_symbol_list = merged_symbol_list;
	}
	/*
	 * If there are any symbol lists with 0 entries used free it and the
	 * chain of lists that follows.
	 */
	if(merged_symbol_list != NULL && merged_symbol_list->used == 0){
	    /*
	     * First set the pointer to this list in the previous list to NULL.
	     */
	    if(merged_symbol_list == merged_symbol_root->list)
		merged_symbol_root->list = NULL;
	    else
		prev_merged_symbol_list->next = NULL;
	    for(m = merged_symbol_list; m != NULL; m = next_merged_symbol_list){
		next_merged_symbol_list = m->next;
		free(m);
	    }
	}

	/*
	 * Second clear out the hash table entries and free any allocated chunks
	 * that only have symbols from this set.
	 */
	have_some_symbols = FALSE;
	for(i = 0; i < SYMBOL_LIST_HASH_SIZE; i++){
	    if(merged_symbol_root == NULL)
		break;
	    first_chunk = NULL;
	    prev_chunk = NULL;
	    for(p = &merged_symbol_root->chunks[i]; p != NULL; p = p->next){
		for(j = 0; j < SYMBOL_CHUNK_SIZE; j++){
		    if(p->symbols[j].name_len != 0 &&
		       p->symbols[j].definition_object->set_num == cur_set){
			memset(p->symbols + j, '\0',
			       sizeof(struct merged_symbol));
			/*
			 * Save a pointer to the first chunk that is allocated
			 * in the chain who first symbol (and all remaining) is
			 * for this set.
			 */
			if(first_chunk == NULL &&
			   p != &merged_symbol_root->chunks[i] &&
			   j == 0)
			    first_chunk = p;
		    }
		    else{
			if(p->symbols[j].name_len != 0)
			    have_some_symbols = TRUE;
		    }
		}
		/*
		 * If we have not yet found a first allocated chunk that has all
		 * symbols from this set, save the pointer to this chunk as it
		 * may end up being the previous chunk.
		 */
		if(first_chunk == NULL)
		    prev_chunk = p;
	    }
	    /*
	     * Free any allocated chunks after the first one in the hash table
	     * that had symbols all from this set and clear the next pointer to
	     * this chain.
	     */
	    if(first_chunk != NULL){
		for(p = first_chunk; p != NULL; p = next_chunk){
		    next_chunk = p->next;
		    free(p);
		}
		prev_chunk->next = NULL;
	    }
	}
	/*
	 * If there are no symbol left in the hash table then free it too.
	 */
	if(have_some_symbols == FALSE){
	    free(merged_symbol_root);
	    merged_symbol_root = NULL;
	}

	/*
	 * Third, find the first string block for the current set of object
	 * files to clear them out.
	 */
	prev_string_block = NULL;
	for(string_block = merged_string_blocks;
	    string_block != NULL;
	    string_block = string_block->next){
	    if(string_block->set_num == cur_set)
		break;
	    prev_string_block = string_block;
	}
	/*
	 * If there are any string blocks for the current set of object files
	 * free their strings and the blocks.
	 */
	if(string_block != NULL && string_block->set_num == cur_set){
	    /*
	     * First set the pointer to this block in the previous block to
	     * NULL.
	     */
	    if(string_block == merged_string_blocks)
		merged_string_blocks = NULL;
	    else
		prev_string_block->next = NULL;
	    /*
	     * Now free the stings for this block the block itself and do the
	     * same for all remaining blocks.
	     */
	    do {
		free(string_block->strings);
		next_string_block = string_block->next;
		free(string_block);
		string_block = next_string_block;
	    }while(string_block != NULL);
	}
}
#endif /* RLD */

#ifdef DEBUG
/*
 * print_symbol_list() prints the merged symbol table.  Used for debugging.
 */
__private_extern__
void
print_symbol_list(
char *string,
enum bool input_based)
{
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol_chunk *p;
    unsigned long i, j;
    struct nlist *nlist;
    struct section *s;
    struct section_map *maps;

	print("Merged symbol list (%s)\n", string);
	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    print("merged_symbols\n");
	    for(i = 0; i < merged_symbol_list->used; i++){
		print("%-4lu[0x%x]\n", i,
		      (unsigned int)(merged_symbol_list->symbols + i));
		nlist = &(merged_symbol_list->symbols[i]->nlist);
		print("    n_name %s\n", nlist->n_un.n_name);
		print("    n_type ");
		switch(nlist->n_type & N_TYPE){
		case N_UNDF:
		    if(nlist->n_value == 0)
			print("N_UNDF\n");
		    else
			print("common (size %u)\n", nlist->n_value);
		    break;
		case N_PBUD:
		    print("N_PBUD\n");
		    break;
		case N_ABS:
		    print("N_ABS\n");
		    break;
		case N_SECT:
		    print("N_SECT\n");
		    break;
		case N_INDR:
		    print("N_INDR for %s\n", ((struct merged_symbol *)
				(nlist->n_value))->nlist.n_un.n_name);
		    break;
		default:
		    print("unknown 0x%x\n", (unsigned int)(nlist->n_type));
		    break;
		}
		print("    n_sect %d ", nlist->n_sect);
		maps = merged_symbol_list->symbols[i]->
		       definition_object->section_maps;
		if(nlist->n_sect == NO_SECT)
		    print("NO_SECT\n");
		else{
		    if(input_based == TRUE)
			print("(%.16s,%.16s)\n",
			       maps[nlist->n_sect - 1].s->segname,
			       maps[nlist->n_sect - 1].s->sectname);
		    else{
			s = get_output_section(nlist->n_sect);
			if(s != NULL)
			    print("(%.16s,%.16s)\n",s->segname, s->sectname);
			else
			    print("(bad section #%d)\n", nlist->n_sect);
		    }
		}
		print("    n_desc 0x%04x\n", (unsigned int)(nlist->n_desc));
		print("    n_value 0x%08x\n", (unsigned int)(nlist->n_value));
#ifdef RLD
		print("    definition_object ");
		print_obj_name(
		       merged_symbol_list->merged_symbols[i].definition_object);
		print("\n");
		print("    set_num %d\n", merged_symbol_list->merged_symbols[i].
		      definition_object->set_num);
#endif
	    }
	}

	print("Hash table (merged_symbol_root 0x%x)\n",
	      (unsigned int)(merged_symbol_root));
	for(i = 0; i < SYMBOL_LIST_HASH_SIZE; i++){
	    for(p = &merged_symbol_root->chunks[i]; p != NULL; p = p->next){
		for(j = 0; j < SYMBOL_CHUNK_SIZE; j++){
		    if(p->symbols[j].name_len != 0){
			print("    %-5lu %-2lu [0x%x] %s\n", i, j,
			      (unsigned int)(p->symbols + j),
			       p->symbols[j].nlist.n_un.n_name);
		    }
		}
	    }
	}
}

#endif /* DEBUG */
/*
 * get_output_section() returns a pointer to the output section structure for
 * the section number passed to it.  It returns NULL for section numbers that
 * are not in the output file.
 */
__private_extern__
struct section *
get_output_section(
unsigned long sect)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->output_sectnum == sect)
		    return(&(ms->s));
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		if(ms->output_sectnum == sect)
		    return(&(ms->s));
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}
	return(NULL);
}

#ifdef DEBUG

#ifndef RLD
/*
 * print_undefined_list() prints the undefined symbol list.  Used for debugging.
 */
__private_extern__
void
print_undefined_list(void)
{
    struct undefined_list *undefined;

	print("Undefined list\n");
	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    undefined = undefined->next){
	    print("    %s", undefined->merged_symbol->nlist.n_un.n_name);
	    if(undefined->merged_symbol->nlist.n_type == (N_UNDF | N_EXT) ||
	       undefined->merged_symbol->nlist.n_value != 0)
		print("\n");
	    else
		print(" (no longer undefined)\n");
	}
}
#endif /* !defined(RLD) */
#endif /* DEBUG */
