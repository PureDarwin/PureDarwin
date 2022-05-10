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
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines to manage the merging of the sections that
 * appear in the headers of the input files.  It builds a merged section table
 * (which is a linked list of merged_segments with merged_sections linked to
 * them).  The merged section list becomes the output files's section list.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include <mach-o/hppa/reloc.h>
#include <ar.h>
#include "stuff/arch.h"
#include "stuff/reloc.h"

#include "ld.h"
#include "specs.h"
#include "live_refs.h"
#include "objects.h"
#include "pass1.h"
#include "symbols.h"
#include "sections.h"
#include "cstring_literals.h"
#include "4byte_literals.h"
#include "8byte_literals.h"
#include "literal_pointers.h"
#include "indirect_sections.h"
#include "mod_sections.h"
#include "coalesced_sections.h"
#include "pass2.h"
#include "generic_reloc.h"
#include "i860_reloc.h"
#include "ppc_reloc.h"
#include "m88k_reloc.h"
#include "hppa_reloc.h"
#include "sparc_reloc.h"
#include "arm_reloc.h"
#include "sets.h"
#include "hash_string.h"
#include "layout.h"
#include "dylibs.h"

/* the pointer to the head of the output file's section list */
__private_extern__ struct merged_segment *merged_segments = NULL;
#ifdef RLD
/*
 * The pointer to the head of the output file's section list before they are
 * all placed in one segment for the MH_OBJECT format in layout().  This is
 * used in reset_merged_sections() to put the list back with it's original
 * segments.
 */
__private_extern__ struct merged_segment *original_merged_segments = NULL;
#endif /* RLD */

/*
 * Any debug sections will be merged in the first pass and placed in their
 * merged segments to allow for error checking.  Their contents are not put in
 * the output file so before the segments are layed out they are removed from
 * the list by remove_debug_segments() and placed on this list.
 */
__private_extern__ struct merged_segment *debug_merged_segments = NULL;

/*
 * The total number relocation entries, used only in layout() to help
 * calculate the size of the link edit segment.
 */
__private_extern__ unsigned long nreloc = 0;

/*
 * This is set to TRUE if any of the input objects do not have the
 * MH_SUBSECTIONS_VIA_SYMBOLS bit set in the mach_header flags field.
 */
__private_extern__ enum bool some_non_subsection_via_symbols_objects = FALSE;

/* table for S_* flags (section types) for error messages */
static const char *
#ifndef __DYNAMIC__
const
#endif
section_flags[] = {
	"S_REGULAR",
	"S_ZEROFILL",
	"S_CSTRING_LITERALS",
	"S_4BYTE_LITERALS",
	"S_8BYTE_LITERALS",
	"S_LITERAL_POINTERS",
	"S_NON_LAZY_SYMBOL_POINTERS",
	"S_LAZY_SYMBOL_POINTERS",
	"S_SYMBOL_STUBS",
	"S_MOD_INIT_FUNC_POINTERS",
	"S_MOD_TERM_FUNC_POINTERS",
	"S_COALESCED"
};

#ifndef RLD
/*
 * These are the arrays used for finding archive,object,symbol name
 * triples and object,symbol name pairs when processing -sectorder options.
 * The array of symbol names is the load_order structure pointed to by
 * the cur_load_orders field of the object_file structure.
 */
struct archive_name {
    char *archive_name;	/* name of the archive file */
    struct object_name
	*object_names;	/* names of the archive members */
    unsigned long
	nobject_names;	/* number of archive members */
};
static struct archive_name *archive_names = NULL;
static unsigned long narchive_names = 0;

struct object_name {
    char *object_name;	/* name of object file */
    unsigned long
	index_length;	/* if this is not in an archive its an index into the */
			/*  object_name to the base name of the object file */
			/*  name else it is the length of the object name */
			/*  which is an archive member name that may have */
			/*  been truncated. */
    struct object_file	/* pointer to the object file */
	*object_file;
};
static struct object_name *object_names = NULL;
static unsigned long nobject_names = 0;

struct load_symbol {
    char *symbol_name;	/* the symbol name this is hashed on */
    char *object_name;	/* the loaded object that contains this symbol */
    char *archive_name;	/* the loaded archive that contains this object */
			/*  or NULL if not in an archive */
    unsigned long
	index_length;	/* if archive_name is NULL the this is index into the */
			/*  object_name to the base name of the object file */
			/*  name else it is the length of the object name */
			/*  which is an archive member name that may have */
			/*  been truncated. */
    struct load_order
	*load_order;	/* the load order for the above triple names */
    struct load_symbol
	*other_names;	/* other load symbols for the same symbol_name */
    struct load_symbol
	*next;		/* next hash table pointer */
};
#define LOAD_SYMBOL_HASHTABLE_SIZE 10000
static struct load_symbol **load_symbol_hashtable = NULL;
static struct load_symbol *load_symbols = NULL;
static unsigned long load_symbols_size = 0;
static unsigned long load_symbols_used = 0;
static unsigned long ambiguous_specifications = 0;

static void layout_ordered_section(
    struct merged_section *ms);
static void create_name_arrays(
    void);
static struct archive_name *create_archive_name(
    char *archive_name);
static void create_object_name(
    struct object_name **object_names,
    unsigned long *nobject_names,
    char *object_name,
    unsigned long index_length,
    char *archive_name);
static void free_name_arrays(
    void);
static void create_load_symbol_hash_table(
    unsigned long nsection_symbols,
    struct merged_section *ms);
static void free_load_symbol_hash_table(
    void);
static void create_load_symbol_hash_table_for_object(
    char *archive_name,
    char *object_name,
    unsigned long index_length,
    struct load_order *load_orders,
    unsigned long nload_orders,
    struct merged_section *ms);
static struct load_order *lookup_load_order(
    char *archive_name,
    char *object_name,
    char *symbol_name,
    struct merged_section *ms,
    unsigned long line_number);
static char * trim(
    char *name);
static struct section_map *lookup_section_map(
    char *archive_name,
    char *object_name);
static int qsort_load_order_names(
    const struct load_order *load_order1,
    const struct load_order *load_order2);
static int bsearch_load_order_names(
    char *symbol_name,
    const struct load_order *load_order);
static int qsort_load_order_input_offset(
    const struct load_order *load_order1,
    const struct load_order *load_order2);
static int qsort_archive_names(
    const struct archive_name *archive_name1,
    const struct archive_name *archive_name2);
static int bsearch_archive_names(
    const char *name,
    const struct archive_name *archive_name);
static int qsort_object_names(
    const struct object_name *object_name1,
    const struct object_name *object_name2);
static int bsearch_object_names(
    const char *name,
    const struct object_name *object_name);
static int qsort_fine_reloc_input_offset(
    const struct fine_reloc *fine_reloc1,
    const struct fine_reloc *fine_reloc2);
static int qsort_order_load_map_orders(
    const struct order_load_map *order_load_map1,
    const struct order_load_map *order_load_map2);
static void create_order_load_maps(
    struct merged_section *ms,
    unsigned long norder_load_maps);
#ifdef DEBUG
static void print_symbol_name_from_order_load_maps(
    struct section_map *map,
    unsigned long value);
#endif /* DEBUG */
static void resize_live_section(
    struct merged_section *ms);
static void count_relocs(
    struct section_map *map,
    struct relocation_info *relocs,
    unsigned long *nlocrel,
    unsigned long *nextrel);
#endif /* !defined(RLD) */
static void scatter_copy(
    struct section_map *map,
    char *contents);
#ifndef RLD
static void reloc_output_for_dyld(
    struct section_map *map,
    struct relocation_info *relocs,
    struct relocation_info *output_locrel,
    struct relocation_info *output_extrel,
    unsigned long *nlocrel,
    unsigned long *nextrel);
static enum bool is_merged_section_read_only(
    struct merged_section *key);
static unsigned long scatter_copy_relocs(
    struct section_map *map,
    struct relocation_info *relocs,
    struct relocation_info *output_relocs);
static double calculate_time_used(
    struct timeval *start,
    struct timeval *end);
static void build_references(
    void);
static void print_references(
    void);
static void setup_references_in_section(
    struct merged_section *ms);
static void setup_references(
    struct section_map *map,
    struct object_file *obj);
static void setup_reference(
    struct live_ref *ref,
    struct object_file *obj,
    struct fine_reloc *self_fine_reloc);
static void mark_all_fine_relocs_live_in_section(
    struct merged_section *ms);
/*
 * The routines that walk the references these are the operations:
 * mark live references
 * search down for any live references (and if so mark it live)
 * check for references that touch a live block (and if so mark it live)
 */
enum walk_references_operation {
    MARK_LIVE,
    SEARCH_FOR_LIVE,
    CHECK_FOR_LIVE_TOUCH
};
#ifdef DEBUG
char * walk_references_operation_names[] = {
    "MARK_LIVE",
    "SEARCH_FOR_LIVE",
    "CHECK_FOR_LIVE_TOUCH"
};
#endif /* DEBUG */
static void walk_references_in_section(
    enum walk_references_operation operation,
    struct merged_section *ms);
static enum bool walk_references(
    enum walk_references_operation operation,
    struct fine_reloc *fine_reloc,
    struct section_map *map,
    struct object_file *obj);
static enum bool ref_operation(
    enum walk_references_operation operation,
    struct ref *ref,
    struct object_file *obj);

#endif /* !defined(RLD) */
#ifdef DEBUG
static void print_load_symbol_hash_table(
    void);
#endif /* DEBUG */

/*
 * merge_sections() merges the sections of the current object file (cur_obj)
 * into the merged section list that will be in the output file.  For each
 * section in the current object file it records the offset that section will
 * start in the output file.  It also accumulates the size of each merged
 * section, the number of relocation entries in it and the maximum alignment.
 */
__private_extern__
void
merge_sections(void)
{
    unsigned long i;
    struct section *s;
    struct merged_section *ms;
    struct mach_header *mh;

	/*
	 * We need to preserve the marking of objects that can have their
	 * sections safely divided up by the symbols for dead code stripping.
	 * Only if all input objects are marked with this will the output also
	 * be marked with this.
	 */
	if(cur_obj != base_obj){
	    mh = (struct mach_header *)(cur_obj->obj_addr);
	    if((mh->flags & MH_SUBSECTIONS_VIA_SYMBOLS) !=
	       MH_SUBSECTIONS_VIA_SYMBOLS){
		some_non_subsection_via_symbols_objects = TRUE;
	    }
	}

	for(i = 0; i < cur_obj->nsection_maps; i++){
	    s = cur_obj->section_maps[i].s;
	    ms = create_merged_section(s);
	    if(errors)
		return;
	    cur_obj->section_maps[i].output_section = ms;
	    /*
	     * If this is a debug section it will not be in the output file.
	     * Set the debug attribute in the merged section (if dynamic is
	     * TRUE it would not have been set).  Then just return not
	     * accounting for it size, alignment and number of relocation		     * entries as none of that info will be in the output file.
	     * Also set output_uuid_info.emit to TRUE since we have seen an
	     * input file with a debug section.
	     */
	    if((s->flags & S_ATTR_DEBUG) == S_ATTR_DEBUG){
		ms->s.flags |= S_ATTR_DEBUG;
		output_uuid_info.emit = TRUE;
		continue;
	    }
	    switch(ms->s.flags & SECTION_TYPE){
	    case S_REGULAR:
	    case S_ZEROFILL:
		/*
		 * For the base file of an incremental link all that is needed
		 * is the section (and it's alignment) so the symbols can refer
		 * to them.  Their contents do not appear in the output file.
		 * If the section size is zero then do NOT adjust the merged
		 * section size to the alignment because if the merged size
		 * was not aligned then that area created does not get flushed
		 * because its associated with a section of size 0.
		 */
		if(cur_obj != base_obj && s->size != 0){
		    cur_obj->section_maps[i].flush_offset = ms->s.size;
		    ms->s.size = rnd(ms->s.size, 1 << s->align);
		    cur_obj->section_maps[i].offset = ms->s.size;
		    ms->s.size   += s->size;
		    ms->s.nreloc += s->nreloc;
		    nreloc += s->nreloc;
		}
#ifdef KLD
		/*
		 * For KLD the section's alignment from the base file is NOT
		 * picked up.
		 */
		if(cur_obj != base_obj)
#endif /* KLD */
		    if(s->align > ms->s.align)
			ms->s.align = s->align;
		if(dynamic == TRUE)
		    ms->s.flags |= (s->flags & SECTION_ATTRIBUTES);
		break;

	    case S_CSTRING_LITERALS:
	    case S_4BYTE_LITERALS:
	    case S_8BYTE_LITERALS:
	    case S_LITERAL_POINTERS:
	    case S_SYMBOL_STUBS:
	    case S_NON_LAZY_SYMBOL_POINTERS:
	    case S_LAZY_SYMBOL_POINTERS:
	    case S_MOD_INIT_FUNC_POINTERS:
	    case S_MOD_TERM_FUNC_POINTERS:
	    case S_COALESCED:
		if(arch_flag.cputype == CPU_TYPE_I860)
			error_with_cur_obj("literal section (%.16s,%.16s) "
		    		       "not allowed in I860 cputype objects",
				       ms->s.segname, ms->s.sectname);
#ifdef KLD
		/*
		 * For KLD the section's alignment from the base file is NOT
		 * picked up.
		 */
		if(cur_obj != base_obj)
#endif /* KLD */
		    if(s->align > ms->s.align)
			ms->s.align = s->align;
		if(dynamic == TRUE)
		    ms->s.flags |= (s->flags & SECTION_ATTRIBUTES);
		break;

	    default:
		fatal("internal error: merge_section() called "
		    "with unknown section type (0x%x) for section (%.16s,"
		    "%.16s)", (unsigned int)(ms->s.flags & SECTION_TYPE),
		    ms->s.segname, ms->s.sectname);
		break;
	    }
#ifndef RLD
	    /*
	     * For dynamic shared libraries record the section map of the
	     * (__OBJC,__module_info) section so it can be used to fill in
	     * objc_module_info_{addr,size} of the module table entries.
	     * Also check to see that it is a regular section.
	     */
	    if(filetype == MH_DYLIB &&
	       strcmp(s->segname, SEG_OBJC) == 0 &&
	       strcmp(s->sectname, SECT_OBJC_MODULES) == 0){
		if((ms->s.flags & SECTION_TYPE) != S_REGULAR)
		    error_with_cur_obj("for MH_DYLIB output files section "
			"(%.16s,%.16s) must have a section type of S_REGULAR",
			s->segname, s->sectname);
		cur_obj->objc_module_info = cur_obj->section_maps + i;
	    }
#endif
	}
}

/*
 * create_merged_section() looks for the section passed to it in the merged
 * section list.  If the section is found then it is check to see the flags
 * of the section matches and if so returns a pointer to the merged section
 * structure for it.  If the flags don't match it is an error.  If no merged
 * section structure is found then one is created and added to the end of the
 * list and a pointer to it is returned.
 */
__private_extern__
struct merged_section *
create_merged_section(
struct section *s)
{
    struct merged_segment **p, *msg;
    struct merged_section **q, **r, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    /* see if this is section is in this segment */
	    if(strncmp(msg->sg.segname, s->segname, sizeof(s->segname)) == 0){
		/*
		 * If this segment contains debug sections then this section too
		 * must be a debug section.  And if it exists and does not
		 * contain debug sections then this section must not be a debug
		 * section.
		 */
		if(msg->debug_only == TRUE &&
		   (s->flags & S_ATTR_DEBUG) != S_ATTR_DEBUG){
		    error_with_cur_obj("section's (%.16s,%.16s) does not have "
			"have debug attribute (S_ATTR_DEBUG) which does not "
			"match previously loaded object's sections for this "
			"segment", s->segname, s->sectname);
		    return(NULL);
		}
		if(msg->debug_only == FALSE &&
		   (s->flags & S_ATTR_DEBUG) == S_ATTR_DEBUG){
		    error_with_cur_obj("section's (%.16s,%.16s) has debug "
			"attribute (S_ATTR_DEBUG) which does not match "
			"previously loaded object's sections for this segment",
			s->segname, s->sectname);
		    return(NULL);
		}
		/*
		 * Depending on the flags of the section depends on which list
		 * it might be found in.  In either case it must not be found in
		 * the other list.
		 */
		if((s->flags & SECTION_TYPE) == S_ZEROFILL){
		    q = &(msg->zerofill_sections);
		    r = &(msg->content_sections);
		}
		else{
		    q = &(msg->content_sections);
		    r = &(msg->zerofill_sections);
		}
		/* check to see if it is in the list it might be found in */
		while(*q){
		    ms = *q;
		    if(strncmp(ms->s.sectname, s->sectname,
			       sizeof(s->sectname)) == 0){
			if((ms->s.flags & SECTION_TYPE) !=
			   (s->flags & SECTION_TYPE)){
			    error_with_cur_obj("section's (%.16s,%.16s) type "
				"%s does not match previous objects type %s",
				s->segname, s->sectname,
				section_flags[s->flags & SECTION_TYPE],
				section_flags[ms->s.flags & SECTION_TYPE]);
			    return(NULL);
			}
			if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
			   ms->s.reserved2 != s->reserved2){
			    error_with_cur_obj("section's (%.16s,%.16s) sizeof "
				"stub %u does not match previous objects "
				"sizeof stub %u", s->segname, s->sectname,
				s->reserved2, ms->s.reserved2);
			    return(NULL);
			}
			return(ms);
		    }
		    q = &(ms->next);
		}
		/*
		 * It was not found in the list it might be in so check to make
		 * sure it is not in the other list where it shouldn't be. 
		 */
		while(*r){
		    ms = *r;
		    if(strncmp(ms->s.sectname, s->sectname,
			       sizeof(s->sectname)) == 0){
			error_with_cur_obj("section's (%.16s,%.16s) type %s "
			    "does not match previous objects type %s",
			    s->segname, s->sectname,
			    section_flags[s->flags & SECTION_TYPE],
			    section_flags[ms->s.flags & SECTION_TYPE]);
			return(NULL);
		    }
		    r = &(ms->next);
		}
		/* add it to the list it should be in */
		msg->sg.nsects++;
		*q = allocate(sizeof(struct merged_section));
		ms = *q;
		memset(ms, '\0', sizeof(struct merged_section));
		strncpy(ms->s.sectname, s->sectname, sizeof(s->sectname));
		strncpy(ms->s.segname, s->segname, sizeof(s->segname));
		/*
		 * This needs to be something other than zero (NO_SECT) so the
		 * call to the *_reloc() routines in count_reloc() can determine
		 * if a relocation to a symbol stub is now referencing an
		 * absolute symbol with a pcrel relocation entry.
		 */
		ms->output_sectnum = 1;
		if(dynamic != TRUE)
		    ms->s.flags = (s->flags & ~SECTION_ATTRIBUTES);
		else
		    ms->s.flags = s->flags;
		if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS){
		    ms->literal_data = allocate(sizeof(struct cstring_data));
		    memset(ms->literal_data, '\0', sizeof(struct cstring_data));
		    ms->literal_merge = cstring_merge;
		    ms->literal_order = cstring_order;
		    ms->literal_reset_live = cstring_reset_live;
		    ms->literal_output = cstring_output;
		    ms->literal_free = cstring_free;
		}
		else if((ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS){
		    ms->literal_data = allocate(sizeof(struct literal4_data));
		    memset(ms->literal_data, '\0',sizeof(struct literal4_data));
		    ms->literal_merge = literal4_merge;
		    ms->literal_order = literal4_order;
		    ms->literal_reset_live = literal4_reset_live;
		    ms->literal_output = literal4_output;
		    ms->literal_free = literal4_free;
		}
		else if((ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS){
		    ms->literal_data = allocate(sizeof(struct literal8_data));
		    memset(ms->literal_data, '\0',sizeof(struct literal8_data));
		    ms->literal_merge = literal8_merge;
		    ms->literal_order = literal8_order;
		    ms->literal_reset_live = literal8_reset_live;
		    ms->literal_output = literal8_output;
		    ms->literal_free = literal8_free;
		}
		else if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS){
		    ms->literal_data =
				  allocate(sizeof(struct literal_pointer_data));
		    memset(ms->literal_data, '\0',
					   sizeof(struct literal_pointer_data));
		    ms->literal_merge = literal_pointer_merge;
		    ms->literal_order = literal_pointer_order;
		    ms->literal_reset_live = literal_pointer_reset_live;
		    ms->literal_output = literal_pointer_output;
 		    ms->literal_free = literal_pointer_free;
		}
#ifndef SA_RLD
		else if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		   (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
		   (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS){
		    ms->literal_data =
			allocate(sizeof(struct indirect_section_data));
		    memset(ms->literal_data, '\0',
			   sizeof(struct indirect_section_data));
		    ms->literal_merge = indirect_section_merge;
		    ms->literal_order = indirect_section_order;
		    ms->literal_reset_live = indirect_section_reset_live;
		    ms->literal_output = NULL;
		    ms->literal_free = indirect_section_free;
		    if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS)
			ms->s.reserved2 = s->reserved2;
		}
#endif /* !defined(SA_RLD) */
		else if((ms->s.flags & SECTION_TYPE) ==
			S_MOD_INIT_FUNC_POINTERS ||
		        (ms->s.flags & SECTION_TYPE) ==
			S_MOD_TERM_FUNC_POINTERS){
		    ms->literal_data = allocate(sizeof(struct mod_term_data));
		    memset(ms->literal_data, '\0',
			   sizeof(struct mod_term_data));
		    ms->literal_merge = mod_section_merge;
		    ms->literal_order = mod_section_order;
		    ms->literal_reset_live = mod_section_reset_live;
		    ms->literal_output = NULL;
 		    ms->literal_free = mod_section_free;
		}
		else if((ms->s.flags & SECTION_TYPE) == S_COALESCED){
		    ms->literal_data = NULL;
		    ms->literal_merge = coalesced_section_merge;
		    ms->literal_order = coalesced_section_order;
		    ms->literal_reset_live = coalesced_section_reset_live;
		    ms->literal_output = NULL;
		    ms->literal_free = NULL;
		}
#ifdef RLD
		ms->set_num = cur_set;
#endif /* RLD */
		return(ms);
	    }
	    p = &(msg->next);
	}
	/*
	 * The segment this section is in wasn't found so add a merged segment
	 * for it and add a merged section to that segment for this section.
	 */
	*p = allocate(sizeof(struct merged_segment));
	msg = *p;
	memset(msg, '\0', sizeof(struct merged_segment));
	strncpy(msg->sg.segname, s->segname, sizeof(s->segname));
	msg->sg.nsects = 1;
	msg->filename = outputfile;
#ifdef RLD
	msg->set_num = cur_set;
#endif /* RLD */
	if((s->flags & SECTION_TYPE) == S_ZEROFILL)
	    q = &(msg->zerofill_sections);
	else
	    q = &(msg->content_sections);
	*q = allocate(sizeof(struct merged_section));
	ms = *q;
	memset(ms, '\0', sizeof(struct merged_section));
	strncpy(ms->s.sectname, s->sectname, sizeof(s->sectname));
	strncpy(ms->s.segname, s->segname, sizeof(s->segname));
	/*
	 * This needs to be something other than zero (NO_SECT) so the
	 * call to the *_reloc() routines in count_reloc() can determine
	 * if a relocation to a symbol stub is now referencing an
	 * absolute symbol with a pcrel relocation entry.
	 */
	ms->output_sectnum = 1;
	if(dynamic != TRUE)
	    ms->s.flags = (s->flags & ~SECTION_ATTRIBUTES);
	else
	    ms->s.flags = s->flags;
	if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS){
	    ms->literal_data = allocate(sizeof(struct cstring_data));
	    memset(ms->literal_data, '\0', sizeof(struct cstring_data));
	    ms->literal_merge = cstring_merge;
	    ms->literal_order = cstring_order;
	    ms->literal_reset_live = cstring_reset_live;
	    ms->literal_output = cstring_output;
	    ms->literal_free = cstring_free;
	}
	else if((ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS){
	    ms->literal_data = allocate(sizeof(struct literal4_data));
	    memset(ms->literal_data, '\0', sizeof(struct literal4_data));
	    ms->literal_merge = literal4_merge;
	    ms->literal_order = literal4_order;
	    ms->literal_reset_live = literal4_reset_live;
	    ms->literal_output = literal4_output;
	    ms->literal_free = literal4_free;
	}
	else if((ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS){
	    ms->literal_data = allocate(sizeof(struct literal8_data));
	    memset(ms->literal_data, '\0', sizeof(struct literal8_data));
	    ms->literal_merge = literal8_merge;
	    ms->literal_order = literal8_order;
	    ms->literal_reset_live = literal8_reset_live;
	    ms->literal_output = literal8_output;
	    ms->literal_free = literal8_free;
	}
	else if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS) {
	    ms->literal_data = allocate(sizeof(struct literal_pointer_data));
	    memset(ms->literal_data, '\0', sizeof(struct literal_pointer_data));
	    ms->literal_merge = literal_pointer_merge;
	    ms->literal_order = literal_pointer_order;
	    ms->literal_reset_live = literal_pointer_reset_live;
	    ms->literal_output = literal_pointer_output;
	    ms->literal_free = literal_pointer_free;
	}
#ifndef SA_RLD
	else if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	   (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	   (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS){
	    ms->literal_data = allocate(sizeof(struct indirect_section_data));
	    memset(ms->literal_data, '\0',sizeof(struct indirect_section_data));
	    ms->literal_merge = indirect_section_merge;
	    ms->literal_order = indirect_section_order;
	    ms->literal_reset_live = indirect_section_reset_live;
	    ms->literal_output = NULL;
	    ms->literal_free = indirect_section_free;
	    if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS)
		ms->s.reserved2 = s->reserved2;
	}
#endif /* !defined(SA_RLD) */
	else if((ms->s.flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
		(ms->s.flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS){
	    ms->literal_data = NULL;
	    ms->literal_merge = mod_section_merge;
	    ms->literal_order = mod_section_order;
	    ms->literal_reset_live = mod_section_reset_live;
	    ms->literal_output = NULL;
	    ms->literal_free = NULL;
	}
	else if((ms->s.flags & SECTION_TYPE) == S_COALESCED){
	    ms->literal_data = NULL;
	    ms->literal_merge = coalesced_section_merge;
	    ms->literal_order = coalesced_section_order;
	    ms->literal_reset_live = coalesced_section_reset_live;
	    ms->literal_output = NULL;
	    ms->literal_free = NULL;
	}
#ifdef RLD
	ms->set_num = cur_set;
#endif /* RLD */
	msg->debug_only = (s->flags & S_ATTR_DEBUG) == S_ATTR_DEBUG;
	return(ms);
}

/*
 * lookup_merged_segment() looks up the specified segment name 
 * in the merged segment list and returns a pointer to the
 * merged segment if it exist.  It returns NULL if it doesn't exist.
 */
__private_extern__
struct merged_segment *
lookup_merged_segment(
char *segname)
{
    struct merged_segment **p, *msg;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    if(strncmp(msg->sg.segname, segname, sizeof(msg->sg.segname)) == 0)
		return(msg);
	    p = &(msg->next);
	}
	return(NULL);
}

/*
 * lookup_merged_section() looks up the specified section name 
 * (segname,sectname) in the merged section list and returns a pointer to the
 * merged section if it exist.  It returns NULL if it doesn't exist.
 */
__private_extern__
struct merged_section *
lookup_merged_section(
char *segname,
char *sectname)
{
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    if(strncmp(msg->sg.segname, segname, sizeof(msg->sg.segname)) == 0){
		q = &(msg->content_sections);
		while(*q){
		    ms = *q;
		    if(strncmp(ms->s.sectname, sectname,
			       sizeof(ms->s.sectname)) == 0){
			return(ms);
		    }
		    q = &(ms->next);
		}
		q = &(msg->zerofill_sections);
		while(*q){
		    ms = *q;
		    if(strncmp(ms->s.sectname, sectname,
			       sizeof(ms->s.sectname)) == 0){
			return(ms);
		    }
		    q = &(ms->next);
		}
		return(NULL);
	    }
	    p = &(msg->next);
	}
	return(NULL);
}

/*
 * remove_debug_segments() removed the debug segments from the list of merged
 * segments.  These segments and the sections in them are on the merged list in
 * pass1 to allow checking that all sections in the segment are debug section.
 * This gets called in layout_segments() so that these segments are not in the
 * output.  The output_sectnum for these sections is set to MAX_SECT+1 so that
 * it can't match any legal section number in the output, so when searching an
 * object's section map for a matching output section number it is never
 * matched.
 */
__private_extern__
void
remove_debug_segments(
void)
{
    struct merged_segment **p, **q, *msg;
    struct merged_section **c, *ms;

	p = &merged_segments;
	q = &debug_merged_segments;
	while(*p){
	    msg = *p;
	    /*
	     * If this is a segment with only debug sections take it off the
	     * list of merged_segments and put it on the list of
	     * debug_merged_segments.
	     */
	    if(msg->debug_only == TRUE){
		*q = msg;
		q = &(msg->next);
		*p = msg->next;
		/*
		 * Set the output_sectnum to an value that is not legal so it
		 * won't be matched when searching for output symbols section
		 * incorrectly to this section.
		 */
		c = &(msg->content_sections);
		while(*c){
		    ms = *c;
		    ms->output_sectnum = MAX_SECT + 1;
		    c = &(ms->next);
		}
		/*
		 * We leave this merged segment to point to the next segment
		 * in the list so we can walk the rest of the list of merged
		 * segments.  Even though this is a debug segment.  We will
		 * terminate the list of debug segments after the end of the
		 * loop.
		 */
	    }
	    p = &(msg->next);
	}
	/*
	 * If we put any debug segments on the list of debug_merged_segments
	 * then set the last one's next pointer to NULL to terminate the list.
	 */
	if(*q != NULL){
	    *q = NULL;
	}
}

/*
 * merge_literal_sections() goes through all the object files to be loaded and
 * merges the literal sections from them.  This is called from layout(), with
 * redo_live == FALSE, and has to be done after all the alignment from all the
 * sections headers have been merged and the command line section alignment has
 * been folded in.  This way the individual literal items from all the objects
 * can be aligned to the output alignment.
 *
 * If -dead_strip is specified this is called a second time from layout() with
 * redo_live == TRUE.  In this case it is used to drive re-merging of only live
 * literals.
 */
__private_extern__
void
merge_literal_sections(
enum bool redo_live)
{
    unsigned long i, j;
    struct object_list *object_list, **p;
    struct merged_section *ms;
#ifndef RLD
    struct merged_segment **q, *msg;
    struct merged_section **content;

	/*
	 * If any literal (except literal pointer) section has an order file
	 * then process it for that section if redo_live == FALSE.  If redo_live
	 * is TRUE then we are being called a second time so instead call the
	 * literal_reset_live function that resets the literal section before
	 * only the live literals are re-merged.
	 */
	q = &merged_segments;
	while(*q){
	    msg = *q;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		   (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS||
		   (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		   (ms->s.flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
		   (ms->s.flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS ||
		   (ms->s.flags & SECTION_TYPE) == S_COALESCED){
		    if(redo_live == FALSE){
			if(ms->order_filename != NULL)
			    (*ms->literal_order)(ms->literal_data, ms);
		    }
		    else
			(*ms->literal_reset_live)(ms->literal_data, ms);
		}
		content = &(ms->next);
	    }
	    q = &(msg->next);
	}
#endif /* !defined(RLD) */

	/*
	 * Merged the literals for each object for each section that is a 
	 * literal (but not a literal pointer section).
	 */
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
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
#ifdef RLD
		if(cur_obj->set_num != cur_set)
		    continue;
#endif /* RLD */
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    ms = cur_obj->section_maps[j].output_section;
		    if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS ||
		       (ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS ||
		       (ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS ||
		       (ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		       (ms->s.flags & SECTION_TYPE) ==
						S_NON_LAZY_SYMBOL_POINTERS ||
		       (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		       (ms->s.flags & SECTION_TYPE) ==
						S_MOD_INIT_FUNC_POINTERS ||
		       (ms->s.flags & SECTION_TYPE) ==
						S_MOD_TERM_FUNC_POINTERS ||
		       (ms->s.flags & SECTION_TYPE) == S_COALESCED)
		       (*ms->literal_merge)(ms->literal_data, ms,
					     cur_obj->section_maps[j].s,
					     &(cur_obj->section_maps[j]),
					     redo_live);
		}
	    }
	}

#ifndef RLD
	/*
	 * Now that the the literals are all merged if any literal pointer
	 * section has an order file then process it for that section if
	 * redo_live == FALSE. If redo_live is TRUE then we are being called a
	 * second time so instead call the literal_reset_live function that
	 * resets the literal section before only the live literals are
	 * re-merged.
	 */
	q = &merged_segments;
	while(*q){
	    msg = *q;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS){
		    if(redo_live == FALSE){
			if(ms->order_filename != NULL)
			    (*ms->literal_order)(ms->literal_data, ms);
		    }
		    else{
			(*ms->literal_reset_live)(ms->literal_data, ms);
		    }
		}
		content = &(ms->next);
	    }
	    q = &(msg->next);
	}
#endif /* !defined(RLD) */
	/*
	 * Now that the the literals are all merged merge the literal pointers
	 * for each object for each section that is a a literal pointer section.
	 */
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
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
#ifdef RLD
		if(cur_obj->set_num != cur_set)
		    continue;
#endif /* RLD */
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    ms = cur_obj->section_maps[j].output_section;
		    if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS)
			(*ms->literal_merge)(ms->literal_data, ms,
					     cur_obj->section_maps[j].s,
					     &(cur_obj->section_maps[j]),
					     redo_live);
		}
	    }
	}
}

#ifndef RLD
/*
 * layout_ordered_sections() calls layout_ordered_section() for each section
 * that has an order file specified with -sectorder, or if -dead_strip is
 * specified.
 */
__private_extern__
void
layout_ordered_sections(void)
{
    enum bool ordered_sections;
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;
    struct object_file *last_object;

	/*
	 * Determine if their are any sections that have an order file or 
	 * -dead_strip is specified and if not just return.  This saves
	 * creating the name arrays when there is no need to.
	 */
	ordered_sections = FALSE;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->order_filename != NULL){
		    ordered_sections = TRUE;
		    break;
		}
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		if(ms->order_filename != NULL){
		    ordered_sections = TRUE;
		    break;
		}
		zerofill = &(ms->next);
	    }
	    if(ordered_sections == TRUE)
		break;
	    p = &(msg->next);
	}
	if(ordered_sections == FALSE && dead_strip == FALSE)
	    return;

	/*
	 * Add the object file the common symbols that the link editor allocated
	 * into the object file list.
	 */
	last_object = add_last_object_file(&link_edit_common_object);

	/*
	 * Build the arrays of archive names and object names which along
	 * with the load order maps will be use to search for archive,object,
	 * symbol name triples from the load order files specified by the user.
	 */
	create_name_arrays();
#ifdef DEBUG
	if(debug & (1 << 13))
	    print_name_arrays();
#endif /* DEBUG */

	/*
	 * For each merged section that has a load order file, or all merged
	 * sections if -dead_strip is specified, layout all objects that have
	 * this section in it.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		/* no load order for this section, or no -dead_strip continue */
		if((ms->order_filename == NULL && dead_strip == FALSE) ||
		   ms->contents_filename != NULL){
		    content = &(ms->next);
		    continue;
		}
		/*
		 * If a regular section (not a literal section) then layout
		 * the sections using symbol names.  Literal sections are
		 * handled by their specific literal merge functions.
		 */
		if((ms->s.flags & SECTION_TYPE) == S_REGULAR)
		    layout_ordered_section(ms);
		if(errors != 0)
		    return;
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		/* no load order for this section, or no -dead_strip continue */
		if(ms->order_filename == NULL && dead_strip == FALSE){
		    zerofill = &(ms->next);
		    continue;
		}
		layout_ordered_section(ms);
		if(errors != 0)
		    return;
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}

	/*
	 * Free the space for the symbol table.
	 */
	free_load_symbol_hash_table();

	/*
	 * Free the space for the name arrays if there has been no load order
	 * map specified (this is because the map has pointers to the object
	 * names that were allocated in the name arrarys).
	 */
	if(load_map == FALSE)
	    free_name_arrays();

	/*
	 * Remove the object file the common symbols that the link editor
	 * allocated from the object file list.
	 */
	remove_last_object_file(last_object);
}

/*
 * layout_ordered_section() creates the fine reloc maps for the section in
 * each object from the load order file specified with -sectorder.
 */
static
void
layout_ordered_section(
struct merged_section *ms)
{
    unsigned long i, j, k, l;
    struct object_list *object_list, **q;

    unsigned long nsect, nload_orders, nsection_symbols;
    struct load_order *load_orders;
    enum bool start_section, any_order;

    struct nlist *object_symbols;
    char *object_strings;

    unsigned long n, order, output_offset, line_number, line_length;
    unsigned long unused_specifications, no_specifications;
    char *line, *archive_name, *object_name, *symbol_name;
    struct load_order *load_order;
    struct section_map *section_map;
    kern_return_t r;

    struct fine_reloc *fine_relocs;
    struct merged_symbol *merged_symbol;

	/*
	 * Reset the count of the number of symbol for this
	 * section (used as the number of load_symbol structs
	 * to allocate).
	 */
	nsection_symbols = 0;

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
		/*
		 * Reset the current section map which points to the
		 * load order map and count in this object that
		 * is being processed for this merged section.  This
		 * will be used in later loops to avoid going through
		 * the section maps again.
		 */
		cur_obj->cur_section_map = NULL;

		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].output_section != ms)
			continue;
		    if(cur_obj->section_maps[j].s->size == 0)
			continue;
		    /*
		     * We can only handle only be one of these sections in
		     * the section maps for a given object file but their might
		     * be more than one.  So this test will check to see if
		     * more than one section with the same name exists in the
		     * same object.
		     */
		    if(cur_obj->cur_section_map != NULL){
			error_with_cur_obj("can't use -sectorder or -dead_strip"
			    " with objects that contain more than one section "
			    "with the same name (section %d and %ld are both "
			    "named (%.16s,%.16s))", cur_obj->cur_section_map -
			    cur_obj->section_maps + 1, j + 1, ms->s.segname,
			    ms->s.sectname);
			return;
		    }

		    cur_obj->cur_section_map = &(cur_obj->section_maps[j]);

		    /*
		     * Count the number of symbols in this section in
		     * this object file.  For this object nsect is the
		     * section number for the merged section.  Also
		     * acount for one extra symbol if there is no symbol
		     * at the beginning of the section.
		     */
		    object_symbols = (struct nlist *)(cur_obj->obj_addr 
					     + cur_obj->symtab->symoff);
		    object_strings = (char *)(cur_obj->obj_addr +
					       cur_obj->symtab->stroff);
		    nsect = j + 1;
		    nload_orders = 0;
		    start_section = FALSE;
		    for(k = 0; k < cur_obj->symtab->nsyms; k++){
			if(object_symbols[k].n_sect == nsect &&
			   (object_symbols[k].n_type & N_STAB) == 0){
			    nload_orders++;
			    if(object_symbols[k].n_value == 
			       cur_obj->section_maps[j].s->addr)
				start_section = TRUE;
			}
		    }
		    if(start_section == FALSE)
			nload_orders++;

		    /*
		     * Allocate the load order map for this section in
		     * this object file and set the current section map
		     * in this object that will point to the load order
		     * map and count.
		     */
		    load_orders = allocate(sizeof(struct load_order) *
					   nload_orders);
		    memset(load_orders, '\0',
			   sizeof(struct load_order) * nload_orders);
		    cur_obj->section_maps[j].nload_orders= nload_orders;
		    cur_obj->section_maps[j].load_orders = load_orders;
		    cur_obj->cur_section_map =
					    &(cur_obj->section_maps[j]);
		    cur_obj->cur_section_map->start_section = start_section;

		    /*
		     * Fill in symbol names and values the load order
		     * map for this section in this object file.
		     */
		    l = 0;
		    if(start_section == FALSE){
			load_orders[l].name = ".section_start";
			load_orders[l].value =
				       cur_obj->section_maps[j].s->addr;
			l++;
		    }
		    for(k = 0; k < cur_obj->symtab->nsyms; k++){
			if(object_symbols[k].n_sect == nsect &&
			   (object_symbols[k].n_type & N_STAB) == 0){
			    load_orders[l].name = object_strings +
					  object_symbols[k].n_un.n_strx;
			    load_orders[l].value =
					  object_symbols[k].n_value;
			    load_orders[l].index = k;
			    l++;
			}
		    }
#ifdef DEBUG
		    if(debug & (1 << 14))
			print_load_order(load_orders, nload_orders, ms,
					 cur_obj, "names and values");
#endif /* DEBUG */

		    /*
		     * Sort the load order map by symbol value so the
		     * size and input offset fields can be set.
		     */
		    qsort(load_orders,
			  nload_orders,
			  sizeof(struct load_order),
			  (int (*)(const void *, const void *))
					       qsort_load_order_values);
		    for(l = 0; l < nload_orders - 1; l++){
			load_orders[l].input_offset =
				       load_orders[l].value -
				       cur_obj->section_maps[j].s->addr;
			load_orders[l].input_size =
					      load_orders[l + 1].value -
					      load_orders[l].value;
		    }
		    load_orders[l].input_offset = load_orders[l].value -
				       cur_obj->section_maps[j].s->addr;
		    load_orders[l].input_size =
				      cur_obj->section_maps[j].s->addr +
				      cur_obj->section_maps[j].s->size -
				      load_orders[l].value;
#ifdef DEBUG
		    if(debug & (1 << 15))
			print_load_order(load_orders, nload_orders, ms,
					 cur_obj, "sizes and offsets");
#endif /* DEBUG */

		    /*
		     * Now sort the load order map by symbol name so
		     * that it can be used for lookup.
		     */
		    qsort(load_orders,
			  nload_orders,
			  sizeof(struct load_order),
			  (int (*)(const void *, const void *))
					       qsort_load_order_names);
#ifdef DEBUG
		    if(debug & (1 << 16))
			print_load_order(load_orders, nload_orders, ms,
					 cur_obj, "sorted by name");
#endif /* DEBUG */
		    /*
		     * Increment the number of load_symbol needed for
		     * this section by the number of symbols in this
		     * object.
		     */
		    nsection_symbols += nload_orders;

		    /*
		     * We can only handle one of these sections in the section
		     * maps for a given object file but their might be more than
		     * one.  So let the loop continue and the test at the top
		     * of the loop will check to see if more than one section
		     * with the same name exists in the same object.
		     */
		}
	    }
	}
	/*
	 * Create the load_symbol hash table.  Used for looking up
	 * symbol names and trying to match load order file lines to
	 * them if the line is not a perfect match.
	 */
	create_load_symbol_hash_table(nsection_symbols, ms);
	
	/*
	 * Clear the counter of ambiguous secifications before the next
	 * section is processed.
	 */
	ambiguous_specifications = 0;
#ifdef DEBUG
	if(debug & (1 << 13))
	    print_load_symbol_hash_table();
#endif /* DEBUG */

	/*
	 * Parse the load order file by changing '\n' to '\0'.
	 */
	for(i = 0; i < ms->order_size; i++){
	    if(ms->order_addr[i] == '\n')
		ms->order_addr[i] = '\0';
	}

	/*
	 * For lines in the order file set the orders and output_offset
	 * in the load maps for this section in all the object files
	 * that have this section.
	 */
	order = 1;
	output_offset = 0;
	line_number = 1;
	unused_specifications = 0;
	for(i = 0; i < ms->order_size; ){
	    line = ms->order_addr + i;
	    line_length = strlen(line);

	    /*
	     * Igore lines that start with a '#'.
	     */
	    if(*line == '#'){
		i += line_length + 1;
		line_number++;
		continue;
	    }

	    parse_order_line(line, &archive_name, &object_name, &symbol_name,
			     ms, line_number);

	    load_order = lookup_load_order(archive_name, object_name,
					   symbol_name, ms, line_number);
	    if(load_order != NULL){
		if(load_order->order != 0){
		    if(archive_name == NULL){
			if(*object_name == '\0'){
			    warning("multiple specification of symbol: %s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s)",
				    symbol_name, ms->order_filename,
				    line_number, ms->s.segname,
				    ms->s.sectname);
			}
			else{
			    warning("multiple specification of %s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s)", object_name,
				    symbol_name, ms->order_filename,
				    line_number, ms->s.segname,
				    ms->s.sectname);
			}
		    }
		    else
			warning("multiple specification of %s:%s:%s in "
				"-sectorder file: %s line %lu for "
				"section (%.16s,%.16s)", archive_name,
				object_name, symbol_name,
				ms->order_filename, line_number,
				ms->s.segname, ms->s.sectname);
		}
		else{
		    load_order->order = order++;
		    load_order->line_number = line_number;
		    output_offset = align_to_input_mod(output_offset,
						       load_order->input_offset,
						       ms->s.align);
		    load_order->output_offset = output_offset;
		    output_offset += load_order->input_size;
		}
	    }
	    else{
		if(strncmp(symbol_name, ".section_offset",
			   sizeof(".section_offset") - 1) == 0){
		    char *p, *endp;
		    unsigned long offset;

		    p = symbol_name + sizeof(".section_offset");
		    offset = strtoul(p, &endp, 0);
		    if(*endp != '\0')
			error("bad specification of .section_offset in "
			      "-sectorder file: %s line %lu for section "
			      "(%.16s,%.16s) (junk after offset value)",
			      ms->order_filename, line_number, ms->s.segname,
			      ms->s.sectname);
		    else if(offset < output_offset)
			error("bad offset value (0x%x) of .section_offset in "
			      "-sectorder file: %s line %lu for section "
			      "(%.16s,%.16s) (value less than current "
			      "offset 0x%x)", (unsigned int)offset,
			      ms->order_filename, line_number, ms->s.segname,
			      ms->s.sectname, (unsigned int)output_offset);
		    else
			output_offset = offset;
		}
		if(strncmp(symbol_name, ".section_align",
			   sizeof(".section_align") - 1) == 0){
		    char *p, *endp;
		    unsigned long align;

		    p = symbol_name + sizeof(".section_align");
		    align = strtoul(p, &endp, 0);
		    if(*endp != '\0')
			error("bad specification of .section_align in "
			      "-sectorder file: %s line %lu for section "
			      "(%.16s,%.16s) (junk after align value)",
			      ms->order_filename, line_number, ms->s.segname,
			      ms->s.sectname);
		    else if(align > MAXSECTALIGN)
			error("bad align value (%lu) of .section_align in "
			      "-sectorder file: %s line %lu for section "
			      "(%.16s,%.16s) (value must be equal to or less "
			      "than %d)", align, ms->order_filename,
			      line_number, ms->s.segname, ms->s.sectname,
			      MAXSECTALIGN);
		    else
			output_offset = rnd(output_offset, 1 << align);
		}
		else if(strcmp(symbol_name, ".section_all") == 0){
		    section_map = lookup_section_map(archive_name,
						     object_name);
		    if(section_map != NULL){
			section_map->no_load_order = TRUE;
			section_map->order = order++;
			output_offset = rnd(output_offset,
					      (1 << section_map->s->align));
			section_map->offset = output_offset;
			output_offset += section_map->s->size;
		    }
		    else if(sectorder_detail == TRUE){
			if(archive_name == NULL){
			    warning("specification of %s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) not used "
				    "(object with that section not in "
				    "loaded objects)", object_name,
				    symbol_name, ms->order_filename,
				    line_number, ms->s.segname,
				    ms->s.sectname);
			}
			else{
			    warning("specification of %s:%s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) not used "
				    "(object with that section not in "
				    "loaded objects)", archive_name,
				    object_name, symbol_name,
				    ms->order_filename, line_number,
				    ms->s.segname, ms->s.sectname);
			}
		    }
		    else{
			unused_specifications++;
		    }
		}
		else if(sectorder_detail == TRUE){
		    if(archive_name == NULL){
			warning("specification of %s:%s in -sectorder "
				"file: %s line %lu for section (%.16s,"
				"%.16s) not found in loaded objects",
				object_name, symbol_name,
				ms->order_filename, line_number,
				ms->s.segname, ms->s.sectname);
		    }
		    else{
			warning("specification of %s:%s:%s in "
				"-sectorder file: %s line %lu for "
				"section (%.16s,%.16s) not found in "
				"loaded objects", archive_name,
				object_name, symbol_name,
				ms->order_filename, line_number,
				ms->s.segname, ms->s.sectname);
		    }
		}
		else{
		    unused_specifications++;
		}
	    }
	    i += line_length + 1;
	    line_number++;
	}

	/*
	 * Deallocate the memory for the load order file now that it is
	 * nolonger needed (since the memory has been written on it is
	 * always deallocated so it won't get written to the swap file
	 * unnecessarily).
	 */
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)
	    ms->order_addr, ms->order_size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() memory for -sectorder "
		       "file: %s for section (%.16s,%.16s)",
		       ms->order_filename, ms->s.segname,
		       ms->s.sectname);
	ms->order_addr = NULL;

	/*
	 * For all entries in the load maps that do not have an order
	 * because they were not specified in the load order file
	 * assign them an order.
	 */
	no_specifications = 0;
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
		if(cur_obj->cur_section_map == NULL)
		    continue;
#ifdef DEBUG
		if(debug & (1 << 17))
		    print_load_order(
				cur_obj->cur_section_map->load_orders,
				cur_obj->cur_section_map->nload_orders,
				ms, cur_obj, "file orders assigned");
#endif /* DEBUG */
		load_order = cur_obj->cur_section_map->load_orders;
		n = cur_obj->cur_section_map->nload_orders;

		/*
		 * When there is no load order or in the case of dead stripping,
		 * we re-sort the load orders by input_offset to keep them in
		 * the "natural" link order.  For dead code stripping this can
		 * still lead to problems with assembly code where blocks can
		 * get removed or padding can be added between blocks for
		 * alignment.  For sections with order files that are incomplete
		 * the "natural" link order is said to be better for symbols not
		 * listed.
		 */
		qsort(load_order, n, sizeof(struct load_order),
		      (int (*)(const void *, const void *))
				       qsort_load_order_input_offset);

		/*
		 * If there is no orders in this object then cause it to be
		 * treated as if it had a .section_all by default unless
		 * -dead_strip is specified.  This is done to help the default
		 * case work in more cases and not scatter the object when no
		 * symbols were ordered from it.
		 */
		any_order = FALSE;
		for(j = 0; j < n; j++){
		    if(load_order[j].order != 0){
			any_order = TRUE;
			break;
		    }
		}
		if(any_order == FALSE &&
		   cur_obj->cur_section_map->no_load_order == FALSE){
		    /*
		     * If -dead_strip is specified, even if this there were
		     * no orders listed in the order file we need to break up
		     * the section into blocks so it can effectively dead
		     * stripped by falling through to the code below.  However
		     * if this section has the no dead strip attribute we will
		     * be keeping all the blocks.  And in this case it is better
		     * to make one block in order to make more cases work.  Or
		     * If this section comes from an object file that is not
		     * marked with MH_SUBSECTIONS_VIA_SYMBOLS we make one block
		     * for the section.
		     */
		    if(dead_strip == FALSE ||
		       (cur_obj->cur_section_map->s->flags &
			S_ATTR_NO_DEAD_STRIP) == S_ATTR_NO_DEAD_STRIP ||
		       (cur_obj->obj_addr != NULL &&
		        (((struct mach_header *)(cur_obj->obj_addr))->flags &
		         MH_SUBSECTIONS_VIA_SYMBOLS) !=
			 MH_SUBSECTIONS_VIA_SYMBOLS)){

			cur_obj->cur_section_map->no_load_order = TRUE;
			if(cur_obj->cur_section_map->order == 0)
			    cur_obj->cur_section_map->order = order++;
			output_offset = rnd(output_offset,
				  (1 << cur_obj->cur_section_map->s->align));
			cur_obj->cur_section_map->offset = output_offset;
			output_offset += cur_obj->cur_section_map->s->size;

			if(sectorder_detail == TRUE &&
			   ms->order_filename != NULL){
			    if(no_specifications == 0)
				warning("no specification for the following "
					"symbols in -sectorder file: %s for "
					"section (%.16s,%.16s):",
					ms->order_filename,
					ms->s.segname, ms->s.sectname);
			    for(j = 0; j < n; j++){
				if(cur_obj->ar_hdr == NULL){
				    if(nowarnings == FALSE)
					print("%s:%s\n", cur_obj->file_name,
					      load_order[j].name);
				}
				else{
				    if(nowarnings == FALSE)
					print("%s:%.*s:%s\n",
					      cur_obj->file_name,
					      (int)cur_obj->ar_name_size,
					      cur_obj->ar_name,
					      load_order[j].name);
				}
			    }
			}
			no_specifications += n;
		    }
		}

		for(j = 0; j < n; j++){
		    if(load_order[j].order == 0){
			if(cur_obj->cur_section_map->no_load_order == TRUE)
			    continue;
			load_order[j].order = order++;
			output_offset = align_to_input_mod(
						    output_offset,
						    load_order[j].input_offset,
						    ms->s.align);
			load_order[j].output_offset = output_offset;
			output_offset += load_order[j].input_size;
			if(sectorder_detail == TRUE &&
			   ms->order_filename != NULL){
			    if(no_specifications == 0)
				warning("no specification for the following "
					"symbols in -sectorder file: %s for "
					"section (%.16s,%.16s):",
					ms->order_filename,
					ms->s.segname, ms->s.sectname);
			    if(cur_obj->ar_hdr == NULL){
				if(nowarnings == FALSE)
				    print("%s:%s\n", cur_obj->file_name,
					  load_order[j].name);
			    }
			    else{
				if(nowarnings == FALSE)
				    print("%s:%.*s:%s\n", cur_obj->file_name,
					  (int)cur_obj->ar_name_size,
					  cur_obj->ar_name, load_order[j].name);
			    }
			}
			no_specifications++;
		    }
		    else{
			if(cur_obj->cur_section_map->no_load_order == TRUE &&
			   any_order == TRUE){
			    if(cur_obj->ar_hdr == NULL){
				error("specification for both %s:%s "
				      "and %s:%s in -sectorder file: "
				      "%s for section (%.16s,%.16s) "
				      "(not allowed)",
				      cur_obj->file_name,
				      ".section_all",
				      cur_obj->file_name,
				      load_order[j].name,
				      ms->order_filename,
				      ms->s.segname, ms->s.sectname);
			    }
			    else{
				error("specification for both "
				      "%s:%.*s:%s and %s:%.*s:%s "
				      "in -sectorder file: %s for "
				      "section (%.16s,%.16s) "
				      "(not allowed)",
				      cur_obj->file_name,
				      (int)cur_obj->ar_name_size,
				      cur_obj->ar_name,
				      ".section_all",
				      cur_obj->file_name,
				      (int)cur_obj->ar_name_size,
				      cur_obj->ar_name,
				      load_order[j].name,
				      ms->order_filename,
				      ms->s.segname, ms->s.sectname);
			    }
			}
		    }
		}

		/*
		 * If .section_all has been seen for this object (or we forced
		 * that effect) and we are creating a load map or -dead_strip
		 * was specified toss the old load_orders and create one load
		 * order that represents the entire section.
		 */ 
		if(cur_obj->cur_section_map->no_load_order == TRUE &&
		   (load_map == TRUE || dead_strip == TRUE)){
		    free(cur_obj->cur_section_map->load_orders);
		    load_order = allocate(sizeof(struct load_order));
		    n = 1;
		    cur_obj->cur_section_map->load_orders = load_order;
		    cur_obj->cur_section_map->nload_orders = n;
		    load_order->order = cur_obj->cur_section_map->order;
		    if(dead_strip == TRUE)
			load_order->name = ".section_all";
		    else
			load_order->name = NULL;
		    load_order->value = cur_obj->section_maps->s->addr;
		    load_order->input_offset = 0;
		    load_order->output_offset = output_offset;
		    load_order->input_size = cur_obj->cur_section_map->s->size;
		}

#ifdef DEBUG
		if(debug & (1 << 18))
		    print_load_order(
				cur_obj->cur_section_map->load_orders,
				cur_obj->cur_section_map->nload_orders,
				ms, cur_obj, "all orders assigned");
#endif /* DEBUG */
	    }
	}
	if(sectorder_detail == FALSE && ms->order_filename != NULL){
	    if(unused_specifications != 0)
		warning("%lu symbols specified in -sectorder file: %s "
			"for section (%.16s,%.16s) not found in "
			"loaded objects", unused_specifications,
			ms->order_filename, ms->s.segname,
			ms->s.sectname);
	    if(no_specifications != 0)
		warning("%lu symbols have no specifications in "
			"-sectorder file: %s for section (%.16s,"
			"%.16s)",no_specifications, ms->order_filename,
			ms->s.segname, ms->s.sectname);
	    if(ambiguous_specifications != 0)
		warning("%lu symbols have ambiguous specifications in "
			"-sectorder file: %s for section (%.16s,"
			"%.16s)", ambiguous_specifications,
			ms->order_filename, ms->s.segname,
			ms->s.sectname);
	}

	/*
	 * There can be seen a ".section_all" and symbol names for the
	 * same object file and these are reported as an error not a
	 * warning.
	 */
	if(errors)
	    return;

	/*
	 * Now the final size of the merged section can be set with all
	 * the contents of the section laid out.
	 */
	ms->s.size = output_offset;

	/*
	 * Finally the fine relocation maps can be allocated and filled
	 * in from the load order maps.
	 */
	object_symbols = NULL;
	object_strings = NULL;
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
		if(cur_obj->cur_section_map == NULL)
		    continue;
		/*
		 * If this object file has no load orders (a .section_all for it
		 * was specified) or -dead_strip was spefified and this section
		 * has the no dead strip attribute then just create a single
		 * fine relocation entry for it that take care of the whole
		 * section.
		 */
		if(cur_obj->cur_section_map->no_load_order == TRUE ||
		   (dead_strip == TRUE &&
		    (cur_obj->cur_section_map->s->flags &
		     S_ATTR_NO_DEAD_STRIP) == S_ATTR_NO_DEAD_STRIP &&
		    ms->order_filename == NULL) ){
		    fine_relocs = allocate(sizeof(struct fine_reloc));
		    memset(fine_relocs, '\0', sizeof(struct fine_reloc));
		    cur_obj->cur_section_map->fine_relocs = fine_relocs;
		    cur_obj->cur_section_map->nfine_relocs = 1;
		    fine_relocs[0].input_offset = 0;
		    fine_relocs[0].output_offset =
				       cur_obj->cur_section_map->offset;
		    if(dead_strip == TRUE){
			load_orders = cur_obj->cur_section_map->load_orders;
			load_orders[0].fine_reloc = fine_relocs + 0;
			load_orders[0].order = cur_obj->cur_section_map->order;
		    }
		    continue;
		}
		n = cur_obj->cur_section_map->nload_orders;
		load_orders = cur_obj->cur_section_map->load_orders;
		start_section = cur_obj->cur_section_map->start_section;
		fine_relocs = allocate(sizeof(struct fine_reloc) * n);
		memset(fine_relocs, '\0', sizeof(struct fine_reloc) * n);
		cur_obj->cur_section_map->fine_relocs = fine_relocs;
		cur_obj->cur_section_map->nfine_relocs = n;
		if(dead_strip == TRUE){
		    object_symbols = (struct nlist *)(cur_obj->obj_addr 
					     + cur_obj->symtab->symoff);
		    object_strings = (char *)(cur_obj->obj_addr +
					       cur_obj->symtab->stroff);
		}
		for(j = 0; j < n ; j++){
		    fine_relocs[j].input_offset =
					   load_orders[j].input_offset;
		    fine_relocs[j].output_offset =
					   load_orders[j].output_offset;
		    if(dead_strip == TRUE){
			if((start_section == TRUE || j != 0) &&
			   object_symbols[load_orders[j].index].n_type & N_EXT){
			    merged_symbol = lookup_symbol(object_strings +
				object_symbols[load_orders[j].index].
				    n_un.n_strx);
			    if(merged_symbol->name_len != 0 &&
			       merged_symbol->definition_object == cur_obj){
				fine_relocs[j].merged_symbol = merged_symbol;
				merged_symbol->fine_reloc = fine_relocs + j;
			    }
			}
		    }
		}
		/*
		 * Leave the fine relocation map in sorted order by
		 * their input offset so that the pass2 routines can
		 * use them.
		 */
		qsort(fine_relocs,
		      n,
		      sizeof(struct fine_reloc),
		      (int (*)(const void *, const void *))
					 qsort_fine_reloc_input_offset);

		/*
		 * When -dead_strip is specified resize_live_section() walks the
		 * order_load_map that will be created in
		 * create_order_load_maps() when reassigning the output_offset
		 * to live items.  To know what is live the load_order structs
		 * need to point at the fine_reloc so it can use the live field
		 * in there.  We must set the pointer to the fine_reloc after
		 * the above last sort of the fine_relocs.  To do that we also
		 * sort the load_orders by the input_offset so we can then
		 * assign the correct fine_reloc pointer to the corresponding
		 * load_order.
		 */
		if(dead_strip == TRUE){
		    qsort(load_orders, n, sizeof(struct load_order),
			  (int (*)(const void *, const void *))
					   qsort_load_order_input_offset);
		    for(j = 0; j < n ; j++)
			load_orders[j].fine_reloc = fine_relocs + j;
		}

		/*
		 * The load order maps are now no longer needed unless
		 * the load map (-M) has been specified or we are doing dead
		 * stripping (-dead_strip).
		 */
		if(load_map == FALSE && dead_strip == FALSE){
		    free(cur_obj->cur_section_map->load_orders);
		    cur_obj->cur_section_map->load_orders = NULL;
		    cur_obj->cur_section_map->nload_orders = 0;
		}
	    }
	}

	/*
	 * If the load map option (-M) or dead stripping (-dead_strip) is
	 * specified build the structures to print the map.
	 */
	if(load_map == TRUE || dead_strip == TRUE)
	    create_order_load_maps(ms, order - 1);
}
#endif /* !RLD */

/*
 * align_to_input_mod() is passed the current output_offset, and returns the
 * next output_offset aligned to the passed input offset modulus the passed
 * power of 2 alignment.
 */
__private_extern__
unsigned long
align_to_input_mod(
unsigned long output_offset,
unsigned long input_offset,
unsigned long align)
{
    unsigned long output_mod, input_mod;

	output_mod = output_offset % (1 << align);
	input_mod  = input_offset  % (1 << align);
	if(output_mod <= input_mod)
	    return(output_offset + (input_mod - output_mod));
	else
	    return(rnd(output_offset, (1 << align)) + input_mod);
}

#ifndef RLD
/*
 * is_literal_output_offset_live() is passed a pointer to a merged literal
 * section and an output_offset in there and returns if the literal for that
 * is live.  This is only used literal sections that have order files and when
 * -dead_strip is specified.  It is very brute force and not fast.
 */
__private_extern__
enum bool
is_literal_output_offset_live(
struct merged_section *ms,
unsigned long output_offset)
{
    unsigned long i, j, k, n;
    struct object_list *object_list, **q;
    struct fine_reloc *fine_relocs;

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
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].output_section != ms)
			continue;
		    fine_relocs = cur_obj->section_maps[j].fine_relocs;
		    n = cur_obj->section_maps[j].nfine_relocs;
		    for(k = 0; k < n ; k++){
			if(fine_relocs[k].output_offset == output_offset &&
			   fine_relocs[k].live == TRUE)
			    return(TRUE);
		    }
		    /*
		     * Since there can only be one of these sections in
		     * the section map and it was found just break out
		     * of the loop looking for it.
		     */
		    break;
		}
	    }
	}
	return(FALSE);
}

/*
 * parse_order_line() parses a load order line into it's archive name, object
 * name and symbol name.  The format for the lines is the following:
 *
 * [<archive name>:]<object name>:<symbol name>
 *
 * If the archive name is not present NULL is returned, if the object name is
 * not present it is set to point at "" and if the symbol name is not present it
 * is set to "".
 */
__private_extern__
void
parse_order_line(
char *line,
char **archive_name,
char **object_name,
char **symbol_name,
struct merged_section *ms,
unsigned long line_number)
{
    unsigned long line_length;
    char *left_bracket;

	/*
	 * The trim has to be done before the checking for objective-C names
	 * syntax because it could have spaces at the end of the line.
	 */ 
	line = trim(line);

	line_length = strlen(line);
	if(line_length == 0){
	    *archive_name = NULL;
	    (*object_name) = "";
	    (*symbol_name) = "";
	    return;
	}

	/*
	 * To allow the objective-C symbol syntax of:
	 * +-[ClassName(CategoryName) Method:Name]
	 * since the method name can have ':'s the brackets
	 * have to be recognized.  This is the only place where
	 * the link editor knows about this.
	 */
	if(line[line_length - 1] == ']'){
	    left_bracket = strrchr(line, '[');
	    if(left_bracket == NULL)
		fatal("format error in -sectorder file: %s line %lu "
		      "for section (%.16s,%.16s) (no matching "
		      "'[' for ending ']' found in symbol name)",
		      ms->order_filename, line_number,
		      ms->s.segname, ms->s.sectname);
	    *left_bracket = '\0';
	    *symbol_name = strrchr(line, ':');
	    *left_bracket = '[';
	}
	/*
	 * A hack for the 3.2 C++ compiler where the symbol name does not end
	 * with a ']' but with the encoded arguments.
	 */
	else if((left_bracket = strrchr(line, '[')) != NULL){
	    *left_bracket = '\0';
	    *symbol_name = strrchr(line, ':');
	    *left_bracket = '[';
	}
	else
	    *symbol_name = strrchr(line, ':');

	if(*symbol_name == NULL){
	    *symbol_name = line;
	    line = "";
	}
	else{
	    **symbol_name = '\0';
	    (*symbol_name)++;
	}

	*object_name = strrchr(line, ':');
	if(*object_name == NULL){
	    *object_name = line;
	    *archive_name = NULL;
	}
	else{
	    **object_name = '\0';
	    (*object_name)++;
	    *archive_name = line;
	}
}

/*
 * create_name_arrays() build the sorted arrays of archive names and object
 * names which along with the load order maps will be use to search for archive,
 * object,symbol name triples from the load order files specified by the user.
 */
static
void
create_name_arrays(void)
{
    unsigned long i;
    long j;
    struct object_list *object_list, **p;
    struct archive_name *ar;
    char *ar_name, *last_slash;

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
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
		if(cur_obj->command_line)
		    continue;
		if(cur_obj->ar_hdr != NULL){
		    ar = create_archive_name(cur_obj->file_name);
		    ar_name = allocate(cur_obj->ar_name_size + 1);
    		    strncpy(ar_name, cur_obj->ar_name, cur_obj->ar_name_size);
		    ar_name[cur_obj->ar_name_size] = '\0';
		    create_object_name(&(ar->object_names),&(ar->nobject_names),
				       ar_name, strlen(ar_name),
				       cur_obj->file_name);
		}
		else{
		    last_slash = strrchr(cur_obj->file_name, '/');
		    if(last_slash == NULL)
			j = 0;
		    else
			j = last_slash - cur_obj->file_name + 1;
		    create_object_name(&object_names, &nobject_names,
				       cur_obj->file_name, j, NULL);
		}
	    }
	}

	/*
	 * Sort the arrays of names.
	 */
	if(narchive_names != 0){
	    archive_names = reallocate(archive_names, 
				       sizeof(struct archive_name) *
				       narchive_names);
	    qsort(archive_names,
		  narchive_names,
		  sizeof(struct archive_name),
		  (int (*)(const void *, const void *))qsort_archive_names);
	    for(i = 0; i < narchive_names; i++){
		archive_names[i].object_names = reallocate(
						archive_names[i].object_names,
						sizeof(struct object_name) *
						archive_names[i].nobject_names);
		qsort(archive_names[i].object_names,
		      archive_names[i].nobject_names,
		      sizeof(struct object_name),
		      (int (*)(const void *, const void *))qsort_object_names);
	    }
	}
	if(nobject_names != nobjects)
	    object_names = reallocate(object_names,
				  sizeof(struct object_name) * nobject_names);
	qsort(object_names,
	      nobject_names,
	      sizeof(struct object_name),
	      (int (*)(const void *, const void *))qsort_object_names);
}

/*
 * create_archive_name() creates a slot in the archive names array for the name
 * passed to it.  The name may be seen more than once.  The archive name must
 * not have a ':' in it since that is used to delimit names in the -sectorder
 * files.
 */
static
struct archive_name *
create_archive_name(
char *archive_name)
{
    unsigned long i;
    struct archive_name *ar;

	if(strchr(archive_name, ':') != NULL)
	    fatal("archive name: %s has a ':' (it can't when -sectorder "
		  "options are used)", archive_name);
	ar = archive_names;
	for(i = 0; i < narchive_names; i++){
	    if(strcmp(ar->archive_name, archive_name) == 0)
		return(ar);
	    ar++;
	}
	if(archive_names == NULL)
	    archive_names = allocate(sizeof(struct archive_name) * nobjects);
	ar = archive_names + narchive_names;
	narchive_names++;
	ar->archive_name = archive_name;
	ar->object_names = NULL;
	ar->nobject_names = 0;
	return(ar);
}

/*
 * create_object_name() creates a slot in the object names array passed to it
 * for the name passed to it and the current object (cur_obj).  The size of the
 * array is in nobject_names.  Both the object names array and it's size are
 * passed indirectly since it may be allocated to add the name.  The name should
 * not be duplicated in the array.  If this objects array is for an archive
 * the archive_name is passed for error messages and is NULL in not in an
 * archive.  The object name must not have a ':' in it since that is used to
 * delimit names in the -sectorder files.
 */
static
void
create_object_name(
struct object_name **object_names,
unsigned long *nobject_names,
char *object_name,
unsigned long index_length,
char *archive_name)
{
    unsigned long n, i;
    struct object_name *o;

	if(strchr(object_name, ':') != NULL){
	    if(archive_name != NULL)
		fatal("archive member name: %s(%s) has a ':' in it (it can't "
		      "when -sectorder options are used)", archive_name,
		      object_name);
	    else
		fatal("object file name: %s has a ':' in it (it can't when "
		      "-sectorder options are used)", object_name);
	}

	o = *object_names;
	n = *nobject_names;
	for(i = 0; i < n; i++){
	    if(strcmp(o->object_name, object_name) == 0){
		if(archive_name != NULL){
#ifdef notdef
/*
 * Since the 4.4bsd extened format #1 could be used for long member names this
 * warning is now always printed again.
 */
		    struct ar_hdr ar_hdr;
		    /*
		     * The warning is not printed when the name is likely to
		     * have been truncated.  Some tools use the whole ar_name
		     * but ar(1) uses one less so it can put a '\0' in when
		     * in memory.
		     */
		    if(strlen(object_name) != sizeof(ar_hdr.ar_name) &&
		       strlen(object_name) != sizeof(ar_hdr.ar_name) - 1)
#endif
			warning("duplicate archive member name: %s(%s) loaded ("
				"could be ambiguous when -sectorder options "
				"are used)", archive_name, object_name);
		}
		else
		    warning("duplicate object file name: %s loaded (could be "
			    "ambiguous when -sectorder options are used)",
			    object_name);
	    }
	    o++;
	}
	if(*object_names == NULL)
	    *object_names = allocate(sizeof(struct object_name) * nobjects);
	o = *object_names + *nobject_names;
	(*nobject_names)++;
	o->object_name = object_name;
	o->object_file = cur_obj;
	o->index_length = index_length;
}

/*
 * free_name_arrays() frees up the space created for the sorted name arrays.
 */
static
void
free_name_arrays(void)
{
    unsigned long i, j;

	if(archive_names != NULL){
	    for(i = 0; i < narchive_names; i++){
		for(j = 0; j < archive_names[i].nobject_names; j++){
		    free(archive_names[i].object_names[j].object_name);
		}
	    }
	    free(archive_names);
	    archive_names = NULL;
	    narchive_names = 0;
	}
	if(object_names != NULL){
	    free(object_names);
	    object_names = NULL;
	    nobject_names = 0;
	}
}

/*
 * create_load_symbol_hash_table() creates a hash table of all the symbol names
 * in the section for the current section map.  This table is use by
 * lookup_load_order when an exact match for the specification can't be found.
 */
static
void
create_load_symbol_hash_table(
unsigned long nsection_symbols,
struct merged_section *ms)
{
    unsigned long i, j;

	/* set up the hash table */
	if(load_symbol_hashtable == NULL)
	    load_symbol_hashtable = allocate(sizeof(struct load_symbol *) *
					     LOAD_SYMBOL_HASHTABLE_SIZE);
	memset(load_symbol_hashtable, '\0', sizeof(struct load_symbol *) *
					    LOAD_SYMBOL_HASHTABLE_SIZE);

	/* set up the load_symbols */
	if(nsection_symbols > load_symbols_size){
	    load_symbols_size = nsection_symbols;
	    load_symbols = reallocate(load_symbols, sizeof(struct load_symbol) *
						    load_symbols_size);
	}
	memset(load_symbols, '\0', sizeof(struct load_symbol) *
				   load_symbols_size);
	load_symbols_used = 0;

	for(i = 0; i < narchive_names; i++){
	    for(j = 0; j < archive_names[i].nobject_names; j++){
		if(archive_names[i].object_names[j].object_file->
							cur_section_map != NULL)
		    create_load_symbol_hash_table_for_object(
			    archive_names[i].archive_name,
			    archive_names[i].object_names[j].object_name,
			    archive_names[i].object_names[j].index_length,
			    archive_names[i].object_names[j].object_file->
						 cur_section_map->load_orders,
			    archive_names[i].object_names[j].object_file->
						 cur_section_map->nload_orders,
			    ms);
	    }
	}

	for(j = 0; j < nobject_names; j++){
	    if(object_names[j].object_file->cur_section_map != NULL)
		create_load_symbol_hash_table_for_object(
		    NULL,
		    object_names[j].object_name,
		    object_names[j].index_length,
		    object_names[j].object_file->cur_section_map->load_orders,
		    object_names[j].object_file->cur_section_map->nload_orders,
		    ms);
	}
}

/*
 * free_load_symbol_hash_table() frees up the space used by the symbol hash
 * table.
 */
static
void
free_load_symbol_hash_table(
void)
{
	/* free the hash table */
	if(load_symbol_hashtable != NULL)
	    free(load_symbol_hashtable);
	load_symbol_hashtable = NULL;

	/* free the load_symbols */
	if(load_symbols != NULL)
	    free(load_symbols);
	load_symbols_size = 0;
	load_symbols_used = 0;
}

/*
 * create_load_symbol_hash_table_for_object() is used by
 * create_load_symbol_hash_table() to create the hash table of all the symbol
 * names in the section that is being scatter loaded.  This routine enters all
 * the symbol names in the load_orders in to the hash table for the specified
 * archive_name object_name pair.
 */
static
void
create_load_symbol_hash_table_for_object(
char *archive_name,
char *object_name,
unsigned long index_length,
struct load_order *load_orders,
unsigned long nload_orders,
struct merged_section *ms)
{
    unsigned long i, hash_index;
    struct load_symbol *load_symbol, *hash_load_symbol, *other_name;

	for(i = 0; i < nload_orders; i++){
	    /*
	     * Get a new load symbol and set the fields for it this load order
	     * entry.
	     */
	    load_symbol = load_symbols + load_symbols_used;
	    load_symbols_used++;
	    load_symbol->symbol_name = load_orders[i].name;
	    load_symbol->object_name = object_name;
	    load_symbol->archive_name = archive_name;
	    load_symbol->index_length = index_length;
	    load_symbol->load_order = &(load_orders[i]);

	    /* find this symbol's place in the hash table */
	    hash_index = hash_string(load_orders[i].name, NULL) %
			 LOAD_SYMBOL_HASHTABLE_SIZE;
	    for(hash_load_symbol = load_symbol_hashtable[hash_index]; 
		hash_load_symbol != NULL;
		hash_load_symbol = hash_load_symbol->next){
		if(strcmp(load_orders[i].name,
			  hash_load_symbol->symbol_name) == 0)
		    break;
	    }
	    /* if the symbol was not found in the hash table enter it */
	    if(hash_load_symbol == NULL){
		load_symbol->other_names = NULL;
		load_symbol->next = load_symbol_hashtable[hash_index]; 
		load_symbol_hashtable[hash_index] = load_symbol;
	    }
	    else{
		/*
		 * If the symbol was found in the hash table go through the
		 * other load symbols for the same name checking if their is
		 * another with exactly the same archive and object name and
		 * generate a warning if so.  Then add this load symbol to the
		 * list of other names.
		 */
		for(other_name = hash_load_symbol;
		    other_name != NULL && ms->order_filename != NULL;
		    other_name = other_name->other_names){

		    if(archive_name != NULL){
			if(strcmp(other_name->object_name, object_name) == 0 &&
			   other_name->archive_name != NULL &&
			   strcmp(other_name->archive_name, archive_name) == 0){
			    warning("symbol appears more than once in the same "
				    "file (%s:%s:%s) which is ambiguous when "
				    "using a -sectorder option",
				    other_name->archive_name,
				    other_name->object_name,
				    other_name->symbol_name);
			    break;
			}
		    }
		    else{
			if(strcmp(other_name->object_name, object_name) == 0 &&
			   other_name->archive_name == NULL){
			    warning("symbol appears more than once in the same "
				    "file (%s:%s) which is ambiguous when "
				    "using a -sectorder option",
				    other_name->object_name,
				    other_name->symbol_name);
			    break;
			}
		    }
		}
		load_symbol->other_names = hash_load_symbol->other_names;
		hash_load_symbol->other_names = load_symbol;
		load_symbol->next = NULL;
	    }
	}
}

/*
 * lookup_load_order() is passed an archive, object, symbol name triple and that
 * is looked up in the name arrays and the load order map and returns a pointer
 * to the load order map that matches it.  Only archive_name may be NULL on
 * input.  It returns NULL if not found.
 */
static
struct load_order *
lookup_load_order(
char *archive_name,
char *object_name,
char *symbol_name,
struct merged_section *ms,
unsigned long line_number)
{
    struct archive_name *a;
    struct object_name *o;
    struct load_order *l;
    unsigned long n;

    unsigned long hash_index, number_of_matches;
    struct load_symbol *hash_load_symbol, *other_name, *first_match;
    char *last_slash, *base_name, *archive_base_name;

	if(archive_name != NULL){
	    a = bsearch(archive_name, archive_names, narchive_names,
			sizeof(struct archive_name),
			(int (*)(const void *, const void *))
						  	 bsearch_archive_names);
	    if(a == NULL)
		goto no_exact_match;
	    o = a->object_names;
	    n = a->nobject_names;
	}
	else{
	    o = object_names;
	    n = nobject_names;
	}

	o = bsearch(object_name, o, n, sizeof(struct object_name),
		    (int (*)(const void *, const void *))bsearch_object_names);
	if(o == NULL)
	    goto no_exact_match;
	if(o->object_file->cur_section_map == NULL)
	    goto no_exact_match;

	l = o->object_file->cur_section_map->load_orders;
	n = o->object_file->cur_section_map->nload_orders;
	l = bsearch(symbol_name, l, n, sizeof(struct load_order),
		    (int (*)(const void *, const void *))
						      bsearch_load_order_names);
	if(l == NULL)
	    goto no_exact_match;
	return(l);

no_exact_match:
	/*
	 * To get here an exact match of the archive_name, object_name, and
	 * symbol_name was not found so try to find some load_order for the
	 * symbol_name using the hash table of symbol names.  First thing here
	 * is to strip leading and trailing blanks from the names.
	 */
	archive_name = trim(archive_name);
	object_name = trim(object_name);
	symbol_name = trim(symbol_name);

	/* find this symbol's place in the hash table */
	hash_index = hash_string(symbol_name, NULL) %
		     LOAD_SYMBOL_HASHTABLE_SIZE;
	for(hash_load_symbol = load_symbol_hashtable[hash_index]; 
	    hash_load_symbol != NULL;
	    hash_load_symbol = hash_load_symbol->next){
	    if(strcmp(symbol_name, hash_load_symbol->symbol_name) == 0)
		break;
	}
	/* if the symbol was not found then give up */
	if(hash_load_symbol == NULL)
	    return(NULL);

	/* if this symbol is in only one object file then use that */
	if(hash_load_symbol->other_names == NULL)
	    return(hash_load_symbol->load_order);

	/*
	 * Now try to see if their is just one name that has not had an order
	 * specified for it and use that if that is the case.  This ignores both
	 * the archive_name and the object_name.
	 */
	number_of_matches = 0;
	first_match = NULL;
	for(other_name = hash_load_symbol;
	    other_name != NULL;
	    other_name = other_name->other_names){
	    if(other_name->load_order->order == 0){
		if(first_match == NULL)
		    first_match = other_name;
		number_of_matches++;
	    }
	}
	if(number_of_matches == 1)
	    return(first_match->load_order);
	if(number_of_matches == 0)
	    return(NULL);

	/*
	 * Now try to see if their is just one name that the object file name
	 * specified for it matches and use that if that is the case.  Only the
	 * object basename is used and matched against the base name of the
	 * objects or the archive member name that may have been truncated.
	 * This ignores the archive name.
	 */
	last_slash = strrchr(object_name, '/');
	if(last_slash == NULL)
	    base_name = object_name;
	else
	    base_name = last_slash + 1;
	number_of_matches = 0;
	first_match = NULL;
	for(other_name = hash_load_symbol;
	    other_name != NULL;
	    other_name = other_name->other_names){
	    if(other_name->load_order->order == 0){
		if(other_name->archive_name != NULL){
		    if(strncmp(base_name, other_name->object_name,
			       other_name->index_length) == 0){
			if(first_match == NULL)
			    first_match = other_name;
			number_of_matches++;
		    }
		}
		else{
		    if(strcmp(base_name, other_name->object_name +
			      other_name->index_length) == 0){
			if(first_match == NULL)
			    first_match = other_name;
			number_of_matches++;
		    }
		}
	    }
	}
	if(number_of_matches == 1)
	    return(first_match->load_order);

	/*
	 * Now try to see if their is just one name that the base name of the
	 * archive file name specified for it matches and use that if that is
	 * the case.  This ignores the object name.
	 */
	if(archive_name != NULL){
	    last_slash = strrchr(archive_name, '/');
	    if(last_slash == NULL)
		base_name = archive_name;
	    else
		base_name = last_slash + 1;
	    number_of_matches = 0;
	    first_match = NULL;
	    for(other_name = hash_load_symbol;
		other_name != NULL;
		other_name = other_name->other_names){
		if(other_name->load_order->order == 0){
		    if(other_name->archive_name != NULL){
			last_slash = strrchr(other_name->archive_name, '/');
			if(last_slash == NULL)
			    archive_base_name = other_name->archive_name;
			else
			    archive_base_name = last_slash + 1;

			if(strcmp(base_name, archive_base_name) == 0){
			    if(first_match == NULL)
				first_match = other_name;
			    number_of_matches++;
			}
		    }
		}
	    }
	    if(number_of_matches == 1)
		return(first_match->load_order);
	}

	/*
	 * Now we know their is more than one possible match for this symbol
	 * name.  So the first one that does not have an order is picked and
	 * either the ambiguous_specifications count is incremented or warnings
	 * are generated.
	 */
	first_match = NULL;
	for(other_name = hash_load_symbol;
	    other_name != NULL;
	    other_name = other_name->other_names){
	    if(other_name->load_order->order == 0){
		first_match = other_name;
		if(sectorder_detail){
		    if(archive_name != NULL){
			if(other_name->archive_name != NULL)
			    warning("ambiguous specification of %s:%s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) using %s:%s:%s",
				    archive_name, object_name, symbol_name,
				    ms->order_filename, line_number,
				    ms->s.segname, ms->s.sectname,
				    other_name->archive_name,
				    other_name->object_name,
				    other_name->symbol_name);
			else
			    warning("ambiguous specification of %s:%s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) using %s:%s",
				    archive_name, object_name, symbol_name,
				    ms->order_filename, line_number,
				    ms->s.segname, ms->s.sectname,
				    other_name->object_name,
				    other_name->symbol_name);
		    }
		    else{
			if(other_name->archive_name != NULL)
			    warning("ambiguous specification of %s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) using %s:%s:%s",
				    object_name, symbol_name,
				    ms->order_filename, line_number,
				    ms->s.segname, ms->s.sectname,
				    other_name->archive_name,
				    other_name->object_name,
				    other_name->symbol_name);
			else
			    warning("ambiguous specification of %s:%s in "
				    "-sectorder file: %s line %lu for "
				    "section (%.16s,%.16s) using %s:%s",
				    object_name, symbol_name,
				    ms->order_filename, line_number,
				    ms->s.segname, ms->s.sectname,
				    other_name->object_name,
				    other_name->symbol_name);
		    }
		}
		break;
	    }
	}
	if(sectorder_detail == TRUE){
	    for(other_name = hash_load_symbol;
		other_name != NULL;
		other_name = other_name->other_names){
		if(other_name->load_order->order == 0 &&
		   first_match != other_name){
		    if(archive_name != NULL){
			if(other_name->archive_name != NULL)
			    warning("specification %s:%s:%s ambiguous with "
				    "%s:%s:%s", archive_name, object_name,
				    symbol_name, other_name->archive_name,
				    other_name->object_name,
				    other_name->symbol_name);
			else
			    warning("specification %s:%s:%s ambiguous with "
				    "%s:%s", archive_name, object_name,
				    symbol_name, other_name->object_name,
				    other_name->symbol_name);
		    }
		    else{
			if(other_name->archive_name != NULL)
			    warning("specification %s:%s ambiguous with "
				    "%s:%s:%s", object_name, symbol_name,
				    other_name->archive_name,
				    other_name->object_name,
				    other_name->symbol_name);
			else
			    warning("specification %s:%s ambiguous with "
				    "%s:%s", object_name, symbol_name,
				    other_name->object_name,
				    other_name->symbol_name);
		    }
		}
	    }
	}
	else{
	    ambiguous_specifications++;
	}
	return(first_match->load_order);
}

/*
 * trim() is passed a name and trims the spaces off the begining and endding of
 * the name.  It writes '\0' in the spaces at the end of the name.  It returns
 * a pointer into the trimed name.
 */
static
char *
trim(
char *name)
{
    char *p;

	if(name == NULL)
	    return(name);
	
	while(*name != '\0' && *name == ' ')
	    name++;
	if(*name == '\0')
	    return(name);

	p = name;
	while(*p != '\0')
	    p++;
	p--;
	while(p != name && *p == ' ')
	    *p-- = '\0';
	return(name);
}

/*
 * lookup_section_map() is passed an archive, object pair and that is looked up
 * in the name arrays and returns a pointer to the section map that matches it.
 * It returns NULL if not found.
 */
static
struct section_map *
lookup_section_map(
char *archive_name,
char *object_name)
{
    struct archive_name *a;
    struct object_name *o;
    unsigned long n;

	if(archive_name != NULL){
	    a = bsearch(archive_name, archive_names, narchive_names,
			sizeof(struct archive_name),
			(int (*)(const void *, const void *))
						  	 bsearch_archive_names);
	    if(a == NULL)
		return(NULL);
	    o = a->object_names;
	    n = a->nobject_names;
	}
	else{
	    o = object_names;
	    n = nobject_names;
	}

	o = bsearch(object_name, o, n, sizeof(struct object_name),
		    (int (*)(const void *, const void *))bsearch_object_names);
	if(o == NULL)
	    return(NULL);
	return(o->object_file->cur_section_map);
}
#endif /* RLD */

/*
 * Function for qsort to sort load_order structs by their value
 */
__private_extern__
int
qsort_load_order_values(
const struct load_order *load_order1,
const struct load_order *load_order2)
{
	/*
	 * This test is needed to fix an obscure bug where two symbols have
	 * the same value.  This fix makes the load_orders and the fine_relocs
	 * sorted by value end up in the same order even though the load_order
	 * was sorted by name between their sorts by value.  Without this the
	 * blocks for the symbols at the same address get placed in the file
	 * in the wrong place because the subtraction of their input offsets
	 * does not yeild the size of the block in this case.  This is kinda
	 * a funky fix to avoid adding a size field to the fine reloc struct
	 * which would be very expensive in space.
	 */
	if(load_order1->value == load_order2->value)
	    return(strcmp(load_order1->name, load_order2->name));
	else
	    return(load_order1->value - load_order2->value);
}

#ifndef RLD
/*
 * Function for qsort to sort load_order structs by their name.
 */
static
int
qsort_load_order_names(
const struct load_order *load_order1,
const struct load_order *load_order2)
{
	return(strcmp(load_order1->name, load_order2->name));
}

/*
 * Function for bsearch to search load_order structs for their name.
 */
static
int
bsearch_load_order_names(
char *symbol_name,
const struct load_order *load_order)
{
	return(strcmp(symbol_name, load_order->name));
}

/*
 * Function for qsort to sort load_order structs by their input_offset.
 */
static
int
qsort_load_order_input_offset(
const struct load_order *load_order1,
const struct load_order *load_order2)
{
	/*
	 * This test is needed to fix an obscure bug where two symbols have
	 * the same value.  This fix makes the load_orders and the fine_relocs
	 * sorted by value end up in the same order even though the load_order
	 * was sorted by name between their sorts by value then by name.
	 */
	if(load_order1->input_offset == load_order2->input_offset)
	    return(strcmp(load_order1->name, load_order2->name));
	else
	    return(load_order1->input_offset - load_order2->input_offset);
}

/*
 * Function for qsort for comparing archive names.
 */
static
int
qsort_archive_names(
const struct archive_name *archive_name1,
const struct archive_name *archive_name2)
{
	return(strcmp(archive_name1->archive_name,
		      archive_name2->archive_name));
}

/*
 * Function for bsearch for finding archive names.
 */
static
int
bsearch_archive_names(
const char *name,
const struct archive_name *archive_name)
{
	return(strcmp(name, archive_name->archive_name));
}

/*
 * Function for qsort for comparing object names.
 */
static
int
qsort_object_names(
const struct object_name *object_name1,
const struct object_name *object_name2)
{
	return(strcmp(object_name1->object_name,
		      object_name2->object_name));
}

/*
 * Function for bsearch for finding object names.
 */
static
int
bsearch_object_names(
const char *name,
const struct object_name *object_name)
{
	return(strcmp(name, object_name->object_name));
}

/*
 * Function for qsort to sort fine_reloc structs by their input_offset
 */
static
int
qsort_fine_reloc_input_offset(
const struct fine_reloc *fine_reloc1,
const struct fine_reloc *fine_reloc2)
{
	return(fine_reloc1->input_offset - fine_reloc2->input_offset);
}

/*
 * Function for qsort to sort order_load_map structs by their order.
 */
static
int
qsort_order_load_map_orders(
const struct order_load_map *order_load_map1,
const struct order_load_map *order_load_map2)
{
	return(order_load_map1->order - order_load_map2->order);
}

/*
 * create_order_load_map() creates the structures to be use for printing the
 * load map and for -dead_strip.
 */
static
void
create_order_load_maps(
struct merged_section *ms,
unsigned long norder_load_maps)
{
    unsigned long i, j, k, l, m, n;
    struct order_load_map *order_load_maps;
    struct load_order *load_orders;

	order_load_maps = allocate(sizeof(struct order_load_map) *
				   norder_load_maps);
	ms->order_load_maps = order_load_maps;
	ms->norder_load_maps = norder_load_maps;
	l = 0;
	for(i = 0; i < narchive_names; i++){
	    for(j = 0; j < archive_names[i].nobject_names; j++){
	        cur_obj = archive_names[i].object_names[j].object_file;
		for(m = 0; m < cur_obj->nsection_maps; m++){
		    if(cur_obj->section_maps[m].output_section != ms)
			continue;
/*
		    if(cur_obj->section_maps[m].no_load_order == TRUE){
			continue;
		    }
*/
		    n = cur_obj->section_maps[m].nload_orders;
		    load_orders = cur_obj->section_maps[m].load_orders;
		    for(k = 0; k < n ; k++){
		        order_load_maps[l].archive_name = 
			    archive_names[i].archive_name;
		        order_load_maps[l].object_name = 
			    archive_names[i].object_names[j].object_name;
		        order_load_maps[l].symbol_name = load_orders[k].name;
		        order_load_maps[l].value = load_orders[k].value;
		        order_load_maps[l].section_map =
			    &(cur_obj->section_maps[m]);
		        order_load_maps[l].size = load_orders[k].input_size;
		        order_load_maps[l].order = load_orders[k].order;
			order_load_maps[l].load_order = load_orders + k;
		        l++;
		    }
		    break;
		}
	    }
	}
	for(j = 0; j < nobject_names; j++){
	    cur_obj = object_names[j].object_file;
	    for(m = 0; m < cur_obj->nsection_maps; m++){
		if(cur_obj->section_maps[m].output_section != ms)
		    continue;
/*
		if(cur_obj->section_maps[m].no_load_order == TRUE)
		    continue;
*/
		n = cur_obj->section_maps[m].nload_orders;
		load_orders = cur_obj->section_maps[m].load_orders;
		for(k = 0; k < n ; k++){
		    order_load_maps[l].archive_name = NULL;
		    order_load_maps[l].object_name =
			object_names[j].object_name;
		    order_load_maps[l].symbol_name = load_orders[k].name;
		    order_load_maps[l].value = load_orders[k].value;
		    order_load_maps[l].section_map =
			&(cur_obj->section_maps[m]);
		    order_load_maps[l].size = load_orders[k].input_size;
		    order_load_maps[l].order = load_orders[k].order;
		    order_load_maps[l].load_order = load_orders + k;
		    l++;
		}
	    }
	}

#ifdef DEBUG
	if(debug & (1 << 19)){
	    for(i = 0; i < norder_load_maps; i++){
		if(order_load_maps[i].archive_name != NULL)
		    print("%s:", order_load_maps[i].archive_name);
		if(order_load_maps[i].symbol_name != NULL)
		    print("%s:%s\n", order_load_maps[i].object_name,
		    order_load_maps[i].symbol_name);
		else
		    print("%s\n", order_load_maps[i].object_name);
	    }
	}
#endif /* DEBUG */

	qsort(order_load_maps,
	      norder_load_maps,
	      sizeof(struct order_load_map),
	      (int (*)(const void *, const void *))qsort_order_load_map_orders);
}

#ifdef DEBUG
/*
 * print_symbol_name_from_order_load_maps() is used in printing a symbol name
 * for a block in the -dead_code stripping debug printing.
 */
static
void
print_symbol_name_from_order_load_maps(
struct section_map *map,
unsigned long value)
{
    unsigned int i, n;
    struct order_load_map *order_load_maps;

	order_load_maps = map->output_section->order_load_maps;
	n = map->output_section->norder_load_maps;
	for(i = 0; i < n; i++){
	   if(order_load_maps[i].value == value){
		print(":%s", order_load_maps[i].symbol_name);
		return;
	   }
	}
}
#endif /* DEBUG */

/*
 * resize_live_sections() resizes the regular and zerofill sections using the
 * live file_reloc sizes.
 */
__private_extern__
void
resize_live_sections(
void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;

	/*
	 * For each merged S_REGULAR and zerofill section cause the section to
	 * be resized to include only the live fine_relocs.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		/*
		 * If a regular section (not a literal section) then call
		 * resize_live_section() on it.
		 */
		if((ms->s.flags & SECTION_TYPE) == S_REGULAR &&
		   ms->contents_filename == NULL)
		    resize_live_section(ms);
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		resize_live_section(ms);
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * resize_live_section() resizes the merged section based on the live
 * fine_relocs.
 */
static
void
resize_live_section(
struct merged_section *ms)
{
    unsigned long n, i, output_offset;
    struct order_load_map *order_load_maps;
    struct load_order *load_order;
    struct fine_reloc *fine_reloc;

	/*
	 * Using the order_load_map of the merged section reassign the
	 * output_offset of each live fine_reloc.
	 */
	output_offset = 0;
	n = ms->norder_load_maps;
	order_load_maps = ms->order_load_maps;
	for(i = 0; i < n; i++){
	    if(order_load_maps[i].load_order->fine_reloc->live == FALSE){
		if(ms->order_filename != NULL &&
		   sectorder_detail == TRUE &&
		   order_load_maps[i].load_order->line_number != 0)
		    warning("specification of symbol: %s in -sectorder file: "
			    "%s line %lu for section (%.16s,%.16s) not used "
			    "(dead stripped)",
			    order_load_maps[i].load_order->name,
			    ms->order_filename,
			    order_load_maps[i].load_order->line_number,
			    ms->s.segname, ms->s.sectname);
		continue;
	    }

	    load_order = order_load_maps[i].load_order;
	    fine_reloc = load_order->fine_reloc;

	    output_offset = align_to_input_mod(output_offset,
					       load_order->input_offset,
					       ms->s.align);
	    load_order->output_offset = output_offset;
	    fine_reloc->output_offset = output_offset;
	    output_offset += load_order->input_size;
	}

	/*
	 * Now the size of the resized merged section can be set with just
	 * the sizes of the live file_relocs included in the section.
	 */
	ms->s.size = output_offset;
}

/*
 * relayout_relocs() resets the counts and indexes for the relocation
 * entries that will be in the output file when output_for_dyld is TRUE or
 * -dead_strip is specified.
 */
__private_extern__
void
relayout_relocs(
void)
{
    unsigned long i, j, section_type, nlocrel, nextrel;
    struct object_list *object_list, **p;
    struct section_map *map;
    struct relocation_info *relocs;

    struct merged_segment **q, *msg;
    struct merged_section **content, *ms;

	/*
	 * For regular and module initialization function pointer sections count
	 * the number of relocation entries that will be in the output file
	 * being created (which in the case of output_for_dyld depends on the
	 * type of output file).
	 */
	nlocrel = 0;
	nextrel = 0;
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
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
		cur_obj->ilocrel = nlocrel;
		cur_obj->iextrel = nextrel;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].s->flags & S_ATTR_DEBUG)
			continue;
		    section_type = cur_obj->section_maps[j].s->flags &
				   SECTION_TYPE;
		    if(section_type == S_REGULAR ||
		       section_type == S_MOD_INIT_FUNC_POINTERS ||
		       section_type == S_MOD_TERM_FUNC_POINTERS){
			map = cur_obj->section_maps + j;
			relocs = (struct relocation_info *)
				 (cur_obj->obj_addr + map->s->reloff);
			count_relocs(map, relocs, &nlocrel, &nextrel);
		    }
		}
		/*
		 * For merged sections that have external relocation entries
		 * they need to be kept with the object's other external
		 * relocation entries so that in a dynamic library they get
		 * relocated.
		 */
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    section_type = cur_obj->section_maps[j].s->flags &
				   SECTION_TYPE;
		    if(section_type == S_COALESCED){
			map = cur_obj->section_maps + j;
			if(map->nextrel != 0){
			    map->iextrel = nextrel;
			    nextrel += map->nextrel;
			    cur_obj->nextrel += map->nextrel;
			}
			if(map->nlocrel != 0){
			    map->ilocrel = nlocrel;
			    nlocrel += map->nlocrel;
			    cur_obj->nlocrel += map->nlocrel;
			}
		    }
		}
	    }
	}

	/*
	 * For merged sections that could have relocation entries the number
	 * that will be in the type of output file being created was counted
	 * up as the section was merged.  So here just set the indexes into
	 * the local and external relocation entries and add their counts to
	 * the total.
	 */
	q = &merged_segments;
	while(*q){
	    msg = *q;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		section_type = ms->s.flags & SECTION_TYPE;
		if(section_type == S_LITERAL_POINTERS ||
		   section_type == S_SYMBOL_STUBS ||
		   section_type == S_LAZY_SYMBOL_POINTERS){
		    if(ms->nlocrel != 0){
			ms->ilocrel = nlocrel;
			nlocrel += ms->nlocrel;
			ms->s.flags |= S_ATTR_LOC_RELOC;
		    }
		    /*
		     * It is an error if one of these types of merged sections
		     * has an external relocation entry and the output is a
		     * multi module dynamic library.  As in a multi module dylib
		     * no library module will "own" it and it will never get
		     * used by the dynamic linker and the item relocated.
		     */
		    if(ms->nextrel != 0){
			if(filetype == MH_DYLIB && multi_module_dylib == TRUE)
			    fatal("internal error: relayout_relocs() "
			      "called with external relocation entries for "
			      "merged section (%.16s,%.16s) for multi module "
			      "MH_DYLIB output", ms->s.segname, ms->s.sectname);
/* TODO: can this ever get here?  even if not MH_DYLIB? */
			ms->iextrel = nextrel;
			nextrel += ms->nextrel;
			ms->s.flags |= S_ATTR_EXT_RELOC;
		    }
		}
		content = &(ms->next);
	    }
	    q = &(msg->next);
	}

	output_dysymtab_info.dysymtab_command.nlocrel = nlocrel;
	output_dysymtab_info.dysymtab_command.nextrel = nextrel;
}

/*
 * count_relocs() increments the counts of the nlocrel and nextrel for the
 * current object for the specified section based on which relocation entries
 * will be in the output file.
 */
static
void
count_relocs(
struct section_map *map,
struct relocation_info *relocs,
unsigned long *nlocrel,
unsigned long *nextrel)
{
    unsigned long i, j, pair, prev_nlocrel, prev_nextrel;
    struct relocation_info reloc, pair_reloc;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address, r_type, r_extern, r_symbolnum, r_pcrel, r_value,
		  r_length;
    struct undefined_map *undefined_map;
    struct merged_symbol *merged_symbol;
    struct nlist *nlists;
    char *strings;
    enum bool defined, pic;
    struct section_map *local_map;
    struct section_map fake_map;
    struct section fake_s;
    char fake_contents[4];
    struct relocation_info fake_relocs[2];

	/* to shut up compiler warning messages "may be used uninitialized" */
	merged_symbol = NULL;
	defined = FALSE;

	prev_nlocrel = cur_obj->nlocrel;
	prev_nextrel = cur_obj->nextrel;
	for(i = 0; i < map->s->nreloc; i++){
	    /*
	     * Note all errors are not flagged here but left for the *_reloc()
	     * routines to flag them.
	     */
	    reloc = relocs[i];
	    if(cur_obj->swapped && map->input_relocs_already_swapped == FALSE)
		swap_relocation_info(&reloc, 1, host_byte_sex);
	    /*
	     * Break out the fields of the relocation entry we need here.
	     */
	    if((reloc.r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(&reloc);
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_type = sreloc->r_type;
		r_length = sreloc->r_length;
		r_extern = 0;
		r_value = sreloc->r_value;
		/* calculate the r_symbolnum (n_sect) from the r_value */
		r_symbolnum = 0;
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(r_value >= cur_obj->section_maps[j].s->addr &&
		       r_value < cur_obj->section_maps[j].s->addr +
				 cur_obj->section_maps[j].s->size){
			r_symbolnum = j + 1;
			break;
		    }
		}
		if(r_symbolnum == 0){
		    /*
		     * The edge case where the last address past then end of
		     * of the last section is referenced.
		     */
		    for(j = 0; j < cur_obj->nsection_maps; j++){
			if(r_value == cur_obj->section_maps[j].s->addr +
				      cur_obj->section_maps[j].s->size){
			    r_symbolnum = j + 1;
			    break;
			}
		    }
		    if(r_symbolnum == 0){
			return;
		    }
		}
	    }
	    else{
		r_address = reloc.r_address;
		r_pcrel = reloc.r_pcrel;
		r_type = reloc.r_type;
		r_length = reloc.r_length;
		r_extern = reloc.r_extern;
		r_symbolnum = reloc.r_symbolnum;
	    }
	    if(r_extern){
		if(r_symbolnum >= cur_obj->symtab->nsyms)
		    return;
		undefined_map = bsearch(&r_symbolnum, cur_obj->undefined_maps,
		    cur_obj->nundefineds, sizeof(struct undefined_map),
		    (int (*)(const void *, const void *))undef_bsearch);
		if(undefined_map != NULL)
		    merged_symbol = undefined_map->merged_symbol;
		else{
		    nlists = (struct nlist *)(cur_obj->obj_addr +
					      cur_obj->symtab->symoff);
		    strings = (char *)(cur_obj->obj_addr +
				       cur_obj->symtab->stroff);
		    if((nlists[r_symbolnum].n_type & N_EXT) != N_EXT)
			return;
		    /*
		     * We must allow and create references to defined global
		     * coalesced symbols with external relocation entries so
		     * that the dynamic linker can relocate all references to
		     * the same symbol.
		     */
		    if((nlists[r_symbolnum].n_type & N_TYPE) == N_SECT &&
		       (cur_obj->section_maps[nlists[r_symbolnum].n_sect-1].
			s->flags & SECTION_TYPE) == S_COALESCED){
			merged_symbol = lookup_symbol(strings +
					     nlists[r_symbolnum].n_un.n_strx);
			if(merged_symbol->name_len == 0){
			    fatal("internal error, in count_relocs() failed to "
			          "lookup coalesced symbol %s", strings +
				  nlists[r_symbolnum].n_un.n_strx);
			}
		    }
		    else
			return;
		}
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR &&
		   merged_symbol->defined_in_dylib == FALSE)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		   merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		   (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		    merged_symbol->defined_in_dylib == TRUE)){
		    defined = FALSE;
		}
		else{
		    /*
		     * The symbol is defined but may be a coalesced symbol.
		     * If so and the output is not an executable (does not
		     * have a dynamic linker command) this relocation entry
		     * will remain as an external relocation entry so set
		     * the variable 'defined' to FALSE.
		     */
		    if((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
		       (merged_symbol->definition_object->section_maps[
			 merged_symbol->nlist.n_sect-1].
			 s->flags & SECTION_TYPE) == S_COALESCED &&
		       has_dynamic_linker_command == FALSE){
			defined = FALSE;
		    }
		    else{
			defined = TRUE;
		    }
		}
	    }
	    if(reloc_has_pair(arch_flag.cputype, r_type))
		pair = 1;
	    else
		pair = 0;
	    if(r_extern == 0){
		/*
		 * If the r_symbolnum refers to a symbol stub section where
		 * the indirect symbol for what is being reference is now
		 * defined as an N_ABS symbol it will turn r_symbolnum
		 * into NO_SECT.  So what was a pcrel relocation entry refering
		 * to another section now refers to absolute symbol and the
		 * relocation entry is no longer pic and must be kept.
		 */
		if(r_symbolnum > cur_obj->nsection_maps)
		    return;
		local_map = &(cur_obj->section_maps[r_symbolnum - 1]);
		if(r_symbolnum != NO_SECT &&
		   (local_map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
		   local_map->absolute_indirect_defineds == TRUE &&
		   r_pcrel == 1){
		    /*
		     * So we need to know if r_symbolnum will turn into NO_SECT.
		     * We do this by faking doing the relocation and pick up
		     * the resulting r_symbolnum after relocation.
		     */
		    if(r_address >= map->s->size)
			return;
		    if(pair && i == map->s->nreloc - 1)
			return;
		    /* fake up a section contents using just this item */
		    memcpy(fake_contents,
			   cur_obj->obj_addr + map->s->offset + r_address,
			   1 << r_length);
		    /* fake up relocation entries using just the ones for item*/
		    fake_relocs[0] = reloc;
		    if((reloc.r_address & R_SCATTERED) != 0){
			sreloc = (struct scattered_relocation_info *)
				 (&fake_relocs[0]);
			sreloc->r_address = 0;
		    }
		    else
			fake_relocs[0].r_address = 0;
		    if(pair){
			pair_reloc = relocs[i+1];
			if(cur_obj->swapped &&
	       		   map->input_relocs_already_swapped == FALSE)
			    swap_relocation_info(&pair_reloc, 1, host_byte_sex);
			fake_relocs[1] = pair_reloc;
		    }
		    /*
		     * fake up a section map that will cause the correct
		     * r_symbolnum (the relocation may be wrong but we don't
		     * need that).
		     */
		    fake_s = *(map->s);
		    fake_s.nreloc = 1 + pair;
		    fake_s.size = 1 << r_length;
		    fake_s.addr += r_address;
		    fake_map = *map;
		    fake_map.s = &fake_s;
		    fake_map.nfine_relocs = 0;
		    fake_map.fine_relocs = NULL;

		    /* do the fake relocation */
		    if(arch_flag.cputype == CPU_TYPE_MC680x0)
			generic_reloc(fake_contents, fake_relocs, &fake_map,
				      FALSE, NULL, 0);
		    else if(arch_flag.cputype == CPU_TYPE_I386)
			generic_reloc(fake_contents, fake_relocs, &fake_map,
				      TRUE, NULL, 0);
		    else if(arch_flag.cputype == CPU_TYPE_POWERPC ||
			    arch_flag.cputype == CPU_TYPE_VEO)
			ppc_reloc(fake_contents, fake_relocs, &fake_map,
				  NULL, 0);
		    else if(arch_flag.cputype == CPU_TYPE_MC88000)
			m88k_reloc(fake_contents, fake_relocs, &fake_map);
		    else if(arch_flag.cputype == CPU_TYPE_HPPA)
			hppa_reloc(fake_contents, fake_relocs, &fake_map);
		    else if(arch_flag.cputype == CPU_TYPE_SPARC)
			sparc_reloc(fake_contents, fake_relocs, &fake_map);
#ifndef RLD
		    else if(arch_flag.cputype == CPU_TYPE_I860)
			i860_reloc(fake_contents, fake_relocs, map);
#endif /* RLD */
		    else if(arch_flag.cputype == CPU_TYPE_ARM)
			arm_reloc(fake_contents, fake_relocs, &fake_map,
				  NULL, 0);

		    /* now pick up the correct resulting r_symbolnum */
		    r_symbolnum = fake_relocs[0].r_symbolnum;
		}
		/*
		 * If this local relocation entry is refering to a coalesced
		 * section and the r_value is that of a global coalesced
		 * symbol then this relocation entry will into a external
		 * relocation entry and the item to be relocated will be
		 * "unrelocated" removing the value of the global coalesced
		 * symbol. 
		 */
		else if(r_symbolnum != NO_SECT &&
		   (local_map->s->flags & SECTION_TYPE) == S_COALESCED){
		    /*
		     * The address of the item being referenced for a scattered
		     * relocation entry is r_address.  But for local relocation
		     * entries the address is in the contents of the item being
		     * relocated (which is architecure/relocation type dependent
		     * to get).
		     *
		     * Once you have that address you have to go through the 
		     * cur_obj's symbol table trying to matching that address.
		     * If you find a match then to need to determine if that
		     * symbols is global in the output file (not a private
		     * extern turned in to a static).
		     *
		     * If all this is true then this relocation entry will
		     * be turned back into an external relocation entry.
		     */
		    ;
		}
		pic = (enum bool)
		       (reloc_is_sectdiff(arch_flag.cputype, r_type) ||
		        (r_pcrel == 1 && r_symbolnum != NO_SECT));
	    }
	    else
		pic = (enum bool)
		       (r_pcrel == 1 &&
		        (merged_symbol->nlist.n_type & N_TYPE) == N_SECT);
	    /*
	     * For output_for_dyld PPC_RELOC_JBSR and HPPA_RELOC_JBSR's are
	     * never put out.
	     */
	    if((arch_flag.cputype == CPU_TYPE_POWERPC &&
		r_type == PPC_RELOC_JBSR) ||
	       (arch_flag.cputype == CPU_TYPE_HPPA &&
		r_type == HPPA_RELOC_JBSR)){
		i += pair;
		continue;
	    }

	    /*
	     * If -dead_strip is specified then this relocation entry may be
	     * part of a dead block and thus will not be put out.
	     */
	    if(dead_strip == TRUE){
		if(fine_reloc_offset_in_output(map, r_address) == FALSE){
		    i += pair;
		    continue;
		}
	    }

	    /*
	     * When output_for_dyld is FALSE all of relocation entries not in
	     * dead blocks will be in the output if save_reloc == TRUE.  And
	     * will be extern if from an extern reloc and the symbol is not
	     * defined else it will be local.
	     */
	    if(output_for_dyld == FALSE){
		if(save_reloc == TRUE){
		    if(r_extern == TRUE && defined == FALSE){
			(*nextrel) += 1 + pair;
			cur_obj->nextrel += 1 + pair;
		    }
		    else{
			(*nlocrel) += 1 + pair;
			cur_obj->nlocrel += 1 + pair;
		    }
		}
		i += pair;
		continue;
	    }

	    /*
	     * When output_for_dyld is TRUE the number of relocation entries in
	     * the output file is based on one of three different cases:
	     * 	The output file is a multi module dynamic shared library
	     *  The output file has a dynamic linker load command
	     *  The output does not have a dynamic linker load command
	     */
	    if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		/*
		 * For multi module dynamic shared library files all external
		 * relocations are kept as external relocation entries except
		 * for references to private externs (which are kept as locals) 
		 * and all non-position-independent local relocation entries
		 * are kept. Modules of multi module dylibs are not linked
		 * together and can only be slid keeping all sections relative
		 * to each other the same.
		 */
		if(r_extern && (merged_symbol->nlist.n_type & N_PEXT) == 0){
		    (*nextrel) += 1 + pair;
		    cur_obj->nextrel += 1 + pair;
		}
		else if(pic == FALSE){
		    (*nlocrel) += 1 + pair;
		    cur_obj->nlocrel += 1 + pair;
		}
	    }
	    else if(has_dynamic_linker_command){
		/*
		 * For an file with a dynamic linker load command only external
		 * relocation entries for undefined symbols are kept.  This
		 * output file is a fixed address and can't be moved.
		 */
		if(r_extern){
		    if(defined == FALSE){
			(*nextrel) += 1 + pair;
			cur_obj->nextrel += 1 + pair;
		    }
		}
	    }
	    else{
		/*
		 * For an file without a dynamic linker load command external
		 * relocation entries for undefined symbols are kept and locals
		 * that are non-position-independent are kept.  This file can
		 * only be slid keeping all sections relative to each other the
		 * same.
		 */
		if(r_extern && (merged_symbol->nlist.n_type & N_PEXT) == 0){
		    if(defined == FALSE){
			(*nextrel) += 1 + pair;
			cur_obj->nextrel += 1 + pair;
		    }
		    else if(pic == FALSE){
			(*nlocrel) += 1 + pair;
			cur_obj->nlocrel += 1 + pair;
		    }
		}
		else if(pic == FALSE){
		    (*nlocrel) += 1 + pair;
		    cur_obj->nlocrel += 1 + pair;
		}
	    }
	    i += pair;
	}
	map->nextrel = cur_obj->nextrel - prev_nextrel;
	map->nlocrel = cur_obj->nlocrel - prev_nlocrel;
	if(prev_nextrel != cur_obj->nextrel)
	    map->output_section->s.flags |= S_ATTR_EXT_RELOC;
	if(prev_nlocrel != cur_obj->nlocrel)
	    map->output_section->s.flags |= S_ATTR_LOC_RELOC;
}
#endif /* !defined(RLD) */

/*
 * output_literal_sections() causes each merged literal section to be copied
 * to the output file.  It is called from pass2().
 */
__private_extern__
void
output_literal_sections(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS ||
		   (ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS)
		    (*ms->literal_output)(ms->literal_data, ms);
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

#ifndef RLD
/*
 * output_sections_from_files() causes each section created from a file to be
 * copied to the output file.  It is called from pass2().
 */
__private_extern__
void
output_sections_from_files(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;
#ifdef DEBUG
    kern_return_t r;
#endif /* DEBUG */

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->contents_filename != NULL){
		    memcpy(output_addr + ms->s.offset,
			   ms->file_addr, ms->file_size);
		    /*
		     * The entire section size is flushed (ms->s.size) not just
		     * the size of the file used to create it (ms->filesize) so
		     * to flush the padding due to alignment.
		     */
		    output_flush(ms->s.offset, ms->s.size);
#ifdef DEBUG
		    if((r = vm_deallocate(mach_task_self(), (vm_address_t)
			ms->file_addr, ms->file_size)) != KERN_SUCCESS)
			mach_fatal(r, "can't vm_deallocate() memory for file: "
				   "%s used to create section (%.16s,%.16s)",
				   ms->contents_filename, ms->s.segname,
				   ms->s.sectname);
		    ms->file_addr = NULL;
#endif /* DEBUG */
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
}
#endif /* !defined(RLD) */

/*
 * output_section() copies the contents of a section and it's relocation entries
 * (if saving relocation entries) into the output file's memory buffer.  Then it
 * calls the appropriate routine specific to the target machine to relocate the
 * section and update the relocation entries (if saving relocation entries).
 */
__private_extern__
void
output_section(
struct section_map *map)
{
    char *contents;
    struct relocation_info *relocs;
#ifndef RLD
    struct relocation_info *output_relocs, *output_locrel, *output_extrel;
    unsigned long nlocrel, nextrel;
    unsigned long nreloc;
#endif

#ifdef DEBUG
	/* The compiler "warning: `output_relocs' may be used uninitialized */
	/* in this function" can safely be ignored */
	output_relocs = NULL;
#endif

	/*
	 * If this section has no contents and no relocation entries just
	 * return.  This can happen a lot with object files that have empty
	 * sections.
	 */ 
	if(map->s->size == 0 && map->s->nreloc == 0)
	    return;

	/*
	 * Copy the contents of the section from the input file into the memory
	 * buffer for the output file.
	 */
	if(map->nfine_relocs != 0)
	    contents = allocate(map->s->size);
	else{
	    /*
	     * This is a hack to pad an i386 pure instructions sections with
	     * nop's (opcode 0x90) to make disassembly cleaner between object's
	     * sections.
	     */
	    if(arch_flag.cputype == CPU_TYPE_I386 &&
	       (map->s->flags & S_ATTR_PURE_INSTRUCTIONS) != 0){
		contents = output_addr + map->output_section->s.offset +
			   map->flush_offset;
		memset(contents, 0x90, map->offset - map->flush_offset);
	    }
	    contents = output_addr + map->output_section->s.offset +map->offset;
	}
	memcpy(contents, cur_obj->obj_addr + map->s->offset, map->s->size);

	/*
	 * If the section has no relocation entries then no relocation is to be
	 * done so just flush the contents and return.
	 */
	if(map->s->nreloc == 0){
#ifndef RLD
	    if(map->nfine_relocs != 0){
		scatter_copy(map, contents);
		free(contents);
	    }
	    else
		output_flush(map->output_section->s.offset + map->flush_offset,
			     map->s->size + (map->offset - map->flush_offset));
#endif /* !defined(RLD) */
	    return;
	}
	else
	    map->output_section->relocated = TRUE;

	/*
	 * Set up the pointer to the relocation entries to be used by the 
	 * relocation routine.  If the relocation entries appear in the file
	 * the relocation routine will update them.  If only some but not all
	 * of the relocation entries will appear in the output file then copy
	 * them from the input file into the memory buffer.  If all of the
	 * relocation entries will appear in the output file then copy them into
	 * the buffer for the output file.  Lastly if no relocation entries will
	 * appear in the output file just used the input files relocation
	 * entries for the relocation routines.
	 */
	if(output_for_dyld){
	    relocs = allocate(map->s->nreloc * sizeof(struct relocation_info));
	    memcpy(relocs,
		   cur_obj->obj_addr + map->s->reloff,
		   map->s->nreloc * sizeof(struct relocation_info));
	}
	else if(save_reloc){
	    /*
	     * For indirect and coalesced sections only those relocation entries
	     * for items in the section used from this object will be saved.  So
	     * allocate a buffer to put them in to use to do the relocation and
	     * later scatter_copy_relocs() will pick out the the relocation
	     * entries to be put in the output file.
	     */
	    if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	       (map->s->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_COALESCED){
		relocs = allocate(map->s->nreloc *
				  sizeof(struct relocation_info));
		memcpy(relocs,
		       cur_obj->obj_addr + map->s->reloff,
		       map->s->nreloc * sizeof(struct relocation_info));
	    }
	    else{
		relocs = (struct relocation_info *)(output_addr +
			 map->output_section->s.reloff +
			 map->output_section->output_nrelocs *
						sizeof(struct relocation_info));
		memcpy(relocs,
		       cur_obj->obj_addr + map->s->reloff,
		       map->s->nreloc * sizeof(struct relocation_info));
	    }
	}
	else{
	    relocs = (struct relocation_info *)(cur_obj->obj_addr +
					        map->s->reloff);
	}
	if(cur_obj->swapped && map->input_relocs_already_swapped == FALSE){
	    swap_relocation_info(relocs, map->s->nreloc, host_byte_sex);
	    map->input_relocs_already_swapped = TRUE;
	}

	/*
	 * Relocate the contents of the section (based on the target machine)
	 */
	if(arch_flag.cputype == CPU_TYPE_MC680x0)
	    generic_reloc(contents, relocs, map, FALSE, NULL, 0);
	else if(arch_flag.cputype == CPU_TYPE_I386)
	    generic_reloc(contents, relocs, map, TRUE, NULL, 0);
	else if(arch_flag.cputype == CPU_TYPE_POWERPC ||
		arch_flag.cputype == CPU_TYPE_VEO)
	    ppc_reloc(contents, relocs, map, NULL, 0);
	else if(arch_flag.cputype == CPU_TYPE_MC88000)
	    m88k_reloc(contents, relocs, map);
	else if(arch_flag.cputype == CPU_TYPE_HPPA)
	    hppa_reloc(contents, relocs, map);
	else if(arch_flag.cputype == CPU_TYPE_SPARC)
	    sparc_reloc(contents, relocs, map);
#ifndef RLD
	else if(arch_flag.cputype == CPU_TYPE_I860)
	    i860_reloc(contents, relocs, map);
#endif /* RLD */
	else if(arch_flag.cputype == CPU_TYPE_ARM)
	    arm_reloc(contents, relocs, map, NULL, 0);
	else
	    fatal("internal error: output_section() called with unknown "
		  "cputype (%d) set", arch_flag.cputype);

	/*
	 * If the reloc routines caused errors then return as so to not cause
	 * later internal error below.
	 */
	if(errors)
	    return;

	/*
	 * Copy and/or flush the relocated section contents to the output file.
	 */
	if(map->nfine_relocs != 0){
	    scatter_copy(map, contents);
	    free(contents);
	}
#ifndef RLD
	else
	    output_flush(map->output_section->s.offset + map->flush_offset,
			 map->s->size + (map->offset - map->flush_offset));

	/*
	 * If relocation entries will be in the output file copy and/or flush
	 * them to the output file.
	 */
	if(output_for_dyld){
	    /*
	     * Setup pointers in the output file buffer for local and external
	     * relocation entries.
	     */
	    if((map->s->flags & SECTION_TYPE) == S_REGULAR ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS){
		output_locrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.locreloff +
			    cur_obj->ilocrel * sizeof(struct relocation_info));
		output_extrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.extreloff +
			    cur_obj->iextrel * sizeof(struct relocation_info));
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
		output_locrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.locreloff +
			    map->ilocrel * sizeof(struct relocation_info));
		output_extrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.extreloff +
			    map->iextrel * sizeof(struct relocation_info));
	    }
	    else{
		output_locrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.locreloff +
			    map->output_section->ilocrel *
				sizeof(struct relocation_info));
		output_extrel = (struct relocation_info *)(output_addr +
			    output_dysymtab_info.dysymtab_command.extreloff +
			    map->output_section->iextrel *
				sizeof(struct relocation_info));
	    }
	    /*
	     * Copy out the local and external relocation entries to be kept
	     * for the output file type and adjust the r_address values to be
	     * based on the offset from starting address of the first segment
	     * rather than the offset of the section.
	     */
	    reloc_output_for_dyld(map, relocs, output_locrel, output_extrel,
				  &nlocrel, &nextrel);
	    /*
	     * count_reloc() and coalesced_section_merge() counted and recorded
	     * the number of relocation entries the section was to have in the
	     * output.  This should match what reloc_output_for_dyld() copied
	     * out.
	     */
	    if((map->s->flags & SECTION_TYPE) == S_REGULAR ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_COALESCED){
		if(nextrel != map->nextrel)
		    fatal("internal error: output_section() count of external "
			  "relocation entries does not match\n");
		if(nlocrel != map->nlocrel)
		    fatal("internal error: output_section() count of local "
			  "relocation entries does not match\n");
	    }

	    if(host_byte_sex != target_byte_sex){
		swap_relocation_info(output_locrel, nlocrel, target_byte_sex);
		swap_relocation_info(output_extrel, nextrel, target_byte_sex);
	    }
	    /*
	     * Flush output file buffer's local and external relocation entries
	     * and increment the counts.
	     */
	    if((map->s->flags & SECTION_TYPE) == S_REGULAR ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS){
		output_flush(output_dysymtab_info.dysymtab_command.locreloff +
			     cur_obj->ilocrel * sizeof(struct relocation_info),
			     nlocrel * sizeof(struct relocation_info));
		cur_obj->ilocrel += nlocrel;
		output_flush(output_dysymtab_info.dysymtab_command.extreloff +
			     cur_obj->iextrel * sizeof(struct relocation_info),
			     nextrel * sizeof(struct relocation_info));
		cur_obj->iextrel += nextrel;
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
		output_flush(output_dysymtab_info.dysymtab_command.locreloff +
			     map->ilocrel * sizeof(struct relocation_info),
			     nlocrel * sizeof(struct relocation_info));
		/* no increment of map->nlocrel */
		output_flush(output_dysymtab_info.dysymtab_command.extreloff +
			     map->iextrel * sizeof(struct relocation_info),
			     nextrel * sizeof(struct relocation_info));
		/* no increment of map->nextrel */
	    }
	    else{
		output_flush(output_dysymtab_info.dysymtab_command.locreloff +
			     map->output_section->ilocrel *
				sizeof(struct relocation_info),
			     nlocrel * sizeof(struct relocation_info));
		map->output_section->ilocrel += nlocrel;
		output_flush(output_dysymtab_info.dysymtab_command.extreloff +
			     map->output_section->iextrel *
				sizeof(struct relocation_info),
			     nextrel * sizeof(struct relocation_info));
		map->output_section->iextrel += nextrel;
	    }
	    free(relocs);
	}
	else if(save_reloc){
	    if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	       (map->s->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_COALESCED){

		output_relocs = (struct relocation_info *)(output_addr +
			 	 map->output_section->s.reloff +
			 	 map->output_section->output_nrelocs *
						sizeof(struct relocation_info));
		nreloc = scatter_copy_relocs(map, relocs, output_relocs);
		free(relocs);
	    }
	    else{
		nreloc = map->s->nreloc;
		output_relocs = relocs;
	    }
	    if(host_byte_sex != target_byte_sex)
		swap_relocation_info(output_relocs, nreloc, target_byte_sex);
	    output_flush(map->output_section->s.reloff +
			 map->output_section->output_nrelocs *
			 sizeof(struct relocation_info),
			 nreloc * sizeof(struct relocation_info));
	    map->output_section->output_nrelocs += nreloc;
	}
#endif /* !defined(RLD) */
}

#ifndef RLD

/*
 * is_pass2_merged_symbol_coalesced() is passed a merged symbol as it appears
 * in the second pass (that is with its n_sect) set to the output's section
 * number) and returns TRUE if the symbol is in a coalesced section and FALSE
 * otherwise.  This is used by scatter_copy() below to set the value of
 * non-lazy pointers.  This absolutely need to be done by the static linker for
 * private extern coalesced symbols (when -keep_private_extern is not in effect)
 * as their indirect symbol table will be INDIRECT_SYMBOL_LOCAL on output and
 * then the dynamic linker can't fix them up.
 */
static 
enum bool
is_pass2_merged_symbol_coalesced(
struct merged_symbol *merged_symbol)
{
    unsigned long i;

	if(merged_symbol == NULL)
	    return(FALSE);
	if((merged_symbol->nlist.n_type & N_TYPE) != N_SECT)
	    return(FALSE);
	for(i = 0; i < merged_symbol->definition_object->nsection_maps; i++){
	    if(merged_symbol->nlist.n_sect == merged_symbol->definition_object->
			section_maps[i].output_section->output_sectnum)
	    if((merged_symbol->definition_object->section_maps[
		 i].output_section->s.flags & SECTION_TYPE) == S_COALESCED)
		return(TRUE);
	}
	return(FALSE);
}
#endif /* !defined(RLD) */

/*
 * pass2_nsect_merged_symbol_section_type() is passed an n_sect merged symbol as
 * it appears in the second pass (that is with its n_sect) set to the output's
 * section number) and returns the section type of that symbol in the output.
 * otherwise.  This is used by legal_reference() in the case of weak coalesced
 * symbols being discarded for some other symbol to figure out what section is
 * being referenced in the output.
 */
__private_extern__
unsigned long
pass2_nsect_merged_symbol_section_type(
struct merged_symbol *merged_symbol)
{
    unsigned long i;

	if(merged_symbol == NULL ||
	   (merged_symbol->nlist.n_type & N_TYPE) != N_SECT)
	    fatal("internal error, s_pass2_merged_symbol_coalesced() passed "
		  "a non-N_SECT symbol");
	for(i = 0; i < merged_symbol->definition_object->nsection_maps; i++){
	    if(merged_symbol->nlist.n_sect == merged_symbol->definition_object->
			section_maps[i].output_section->output_sectnum)
	    return(merged_symbol->definition_object->section_maps[i].
		   output_section->s.flags & SECTION_TYPE);
	}
	fatal("internal error, s_pass2_merged_symbol_coalesced() failed\n");
	return(0);
}

/*
 * scatter_copy() copies the relocated contents of a section into the output
 * file's memory buffer based on the section's fine relocation maps.
 */
static
void
scatter_copy(
struct section_map *map,
char *contents)
{
    unsigned long i;
#ifndef RLD
    unsigned long j;
    struct nlist *nlists;
    unsigned long *indirect_symtab, index, value;
    struct undefined_map *undefined_map;
    struct merged_symbol *merged_symbol;
    char *strings;
    struct section_map *section_map;
    long delta;
    char *jmpEntry;

	/*
	 * For non-lazy pointer type indirect sections only copy those parts of
	 * the section who's contents are used in the output file and if the
	 * symbol for the non-lazy pointer is defined then use that as instead
	 * of the contents.  This bit of code assumes that all the checks done
	 * when merging the indirect section are valid and so none of them are
	 * done here.  It also assumes that the fine relocation entries each
	 * cover the 4 byte non-lazy pointer.
	 *
	 * If prebinding and this is a lazy pointer section do the same as for
	 * non-lazy pointers.  That is use the value of the indirect symbol.
	 */
	if((map->s->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	   (prebinding == TRUE &&
	    (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS)){
	    /* setup pointers to the symbol table and indirect symbol table */
	    nlists = (struct nlist *)(cur_obj->obj_addr +
				      cur_obj->symtab->symoff);
	    indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
	    strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
	    for(i = 0; i < map->nfine_relocs - 1; i++){
		if(map->fine_relocs[i].use_contents == TRUE &&
		   (dead_strip == FALSE || map->fine_relocs[i].live == TRUE)){
		    index = indirect_symtab[map->s->reserved1 + 
			    (map->fine_relocs[i].input_offset / 4)];
		    if(map->fine_relocs[i].indirect_defined == TRUE ||
		       is_pass2_merged_symbol_coalesced(
			    map->fine_relocs[i].merged_symbol) == TRUE ||
		       (prebinding == TRUE && 
			(index != INDIRECT_SYMBOL_LOCAL &&
			 index != INDIRECT_SYMBOL_ABS))){
			if(is_pass2_merged_symbol_coalesced(
			    map->fine_relocs[i].merged_symbol) == TRUE){
    			    value = map->fine_relocs[i].merged_symbol->
				    nlist.n_value;
			    if((map->fine_relocs[i].merged_symbol->
				nlist.n_desc & N_ARM_THUMB_DEF))
				value |= 1;
			}
			else if(map->fine_relocs[i].local_symbol == FALSE){
			    undefined_map = bsearch(&index,
				cur_obj->undefined_maps, cur_obj->nundefineds,
				sizeof(struct undefined_map),
				(int (*)(const void *, const void *))
				undef_bsearch);
			    if(undefined_map == NULL){
				merged_symbol = lookup_symbol(strings +
						    nlists[index].n_un.n_strx);
				if(merged_symbol->name_len == 0)
				    fatal("interal error, scatter_copy() failed"
					  " in looking up external symbol");
			    }
			    else
				merged_symbol = undefined_map->merged_symbol;
			    if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
				merged_symbol = (struct merged_symbol *)
						merged_symbol->nlist.n_value;
			    value = merged_symbol->nlist.n_value;
			    if((merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF))
				value |= 1;
			}
			else{
			    if(nlists[index].n_sect == NO_SECT)
				value = nlists[index].n_value;
			    else{
				section_map = &(cur_obj->section_maps[
				    nlists[index].n_sect -1]);
				if(section_map->nfine_relocs == 0)
				    value = nlists[index].n_value -
					   section_map->s->addr +
					   section_map->output_section->s.addr +
					   section_map->offset;
				else
				    value =
					section_map->output_section->s.addr +
					   fine_reloc_output_offset(section_map,
							nlists[index].n_value -
							section_map->s->addr);
			    }
			    if((nlists[index].n_desc & N_ARM_THUMB_DEF))
				value |= 1;
			}
			if(host_byte_sex != target_byte_sex)
			    value = SWAP_LONG(value);
			memcpy(output_addr + map->output_section->s.offset +
			       map->fine_relocs[i].output_offset,
			       &value, sizeof(unsigned long));
		    }
		    else{
			/*
			 * If the indirect symbol table entry is
			 * INDIRECT_SYMBOL_LOCAL the value of the symbol pointer
			 * neededs to be adjusted to where it is in the output.
			 */
			if(index == INDIRECT_SYMBOL_LOCAL){
			    memcpy(&value, contents +
				   map->fine_relocs[i].input_offset, 4);
			    if(cur_obj->swapped)
				value = SWAP_LONG(value);
			    for(j = 0; j < cur_obj->nsection_maps; j++){
				if(value >= cur_obj->section_maps[j].s->addr &&
				   value < cur_obj->section_maps[j].s->addr +
					     cur_obj->section_maps[j].s->size){
				    break;
				}
			    }
			    if(j >= cur_obj->nsection_maps){
				error_with_cur_obj("value of symbol pointer "
				    "(0x%x) in section (%.16s,%.16s) at index "
				    "%ld out of range for an indirect symbol "
				    "table value of INDIRECT_SYMBOL_LOCAL",
				    (unsigned int)value,
				    map->output_section->s.segname,
				    map->output_section->s.sectname, i);
				return;
			    }
			    section_map = &(cur_obj->section_maps[j]);
			    if(section_map->nfine_relocs == 0)
				value = value -
				       section_map->s->addr +
				       section_map->output_section->s.addr +
				       section_map->offset;
			    else
				value =
				    section_map->output_section->s.addr +
				       fine_reloc_output_offset(section_map,
						value - section_map->s->addr);
			    if(host_byte_sex != target_byte_sex)
				value = SWAP_LONG(value);
			    memcpy(output_addr + map->output_section->s.offset +
				   map->fine_relocs[i].output_offset,
				   &value, sizeof(unsigned long));
			}
			else{
			    memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
			       contents + map->fine_relocs[i].input_offset,
			       map->fine_relocs[i+1].input_offset -
					      map->fine_relocs[i].input_offset);
			}
		    }
		}
	    }
	    if(map->fine_relocs[i].use_contents == TRUE &&
	       (dead_strip == FALSE || map->fine_relocs[i].live == TRUE)){
		index = indirect_symtab[map->s->reserved1 + 
			(map->fine_relocs[i].input_offset / 4)];
		if(map->fine_relocs[i].indirect_defined == TRUE ||
		   is_pass2_merged_symbol_coalesced(
			map->fine_relocs[i].merged_symbol) == TRUE ||
		   (prebinding == TRUE && 
		    (index != INDIRECT_SYMBOL_LOCAL &&
		     index != INDIRECT_SYMBOL_ABS))){
		    if(is_pass2_merged_symbol_coalesced(
			map->fine_relocs[i].merged_symbol) == TRUE){
			value = map->fine_relocs[i].merged_symbol->
				nlist.n_value;
			if((map->fine_relocs[i].merged_symbol->
			    nlist.n_desc & N_ARM_THUMB_DEF))
			    value |= 1;
		    }
		    else if(map->fine_relocs[i].local_symbol == FALSE){
			undefined_map = bsearch(&index,
			    cur_obj->undefined_maps, cur_obj->nundefineds,
			    sizeof(struct undefined_map),
			    (int (*)(const void *, const void *))
			    undef_bsearch);
			if(undefined_map == NULL){
			    merged_symbol = lookup_symbol(strings +
						nlists[index].n_un.n_strx);
			    if(merged_symbol->name_len == 0)
				fatal("interal error, scatter_copy() failed"
				      " in looking up external symbol");
			}
			else
			    merged_symbol = undefined_map->merged_symbol;
			if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
			    merged_symbol = (struct merged_symbol *)
					    merged_symbol->nlist.n_value;
			value = merged_symbol->nlist.n_value;
			if((merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF))
			    value |= 1;
		    }
		    else{
			if(nlists[index].n_sect == NO_SECT)
			    value = nlists[index].n_value;
			else{
			    section_map = &(cur_obj->section_maps[
				nlists[index].n_sect -1]);
			    if(section_map->nfine_relocs == 0)
				value = nlists[index].n_value -
				       section_map->s->addr +
				       section_map->output_section->s.addr +
				       section_map->offset;
			    else
				value =
				    section_map->output_section->s.addr +
				       fine_reloc_output_offset(section_map,
						    nlists[index].n_value -
						    section_map->s->addr);
			}
			if((nlists[index].n_desc & N_ARM_THUMB_DEF))
			    value |= 1;
		    }
		    if(host_byte_sex != target_byte_sex)
			value = SWAP_LONG(value);
		    memcpy(output_addr + map->output_section->s.offset +
			   map->fine_relocs[i].output_offset,
			   &value, sizeof(unsigned long));
		}
		else{
		    /*
		     * If the indirect symbol table entry is
		     * INDIRECT_SYMBOL_LOCAL the value of the symbol pointer
		     * neededs to be adjusted to where it is in the output.
		     */
		    if(index == INDIRECT_SYMBOL_LOCAL){
			memcpy(&value, contents +
			       map->fine_relocs[i].input_offset, 4);
			if(cur_obj->swapped)
			    value = SWAP_LONG(value);
			for(j = 0; j < cur_obj->nsection_maps; j++){
			    if(value >= cur_obj->section_maps[j].s->addr &&
			       value < cur_obj->section_maps[j].s->addr +
					 cur_obj->section_maps[j].s->size){
				break;
			    }
			}
			if(j >= cur_obj->nsection_maps){
			    error_with_cur_obj("value of symbol pointer (0x%x) "
				"in section (%.16s,%.16s) at index %ld out of "
				"range for an indirect symbol table value of "
				"INDIRECT_SYMBOL_LOCAL", (unsigned int)value,
				map->output_section->s.segname,
				map->output_section->s.sectname, i);
			    return;
			}
			section_map = &(cur_obj->section_maps[j]);
			if(section_map->nfine_relocs == 0)
			    value = value -
				   section_map->s->addr +
				   section_map->output_section->s.addr +
				   section_map->offset;
			else
			    value =
				section_map->output_section->s.addr +
				   fine_reloc_output_offset(section_map,
						value - section_map->s->addr);
			if(host_byte_sex != target_byte_sex)
			    value = SWAP_LONG(value);
			memcpy(output_addr + map->output_section->s.offset +
			       map->fine_relocs[i].output_offset,
			       &value, sizeof(unsigned long));
		    }
		    else{
			memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
			   contents + map->fine_relocs[i].input_offset,
			   map->s->size - map->fine_relocs[i].input_offset);
		    }
		}
	    }
	}
	/*
	 * The i386 has a special 5 byte stub that is modify by dyld to become 
	 * a JMP instruction.  When building prebound, we set the stub to be
	 * the JMP instruction. 
	 */
	else if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
		prebinding == TRUE &&
		arch_flag.cputype == CPU_TYPE_I386 &&
	        (map->s->flags & S_ATTR_SELF_MODIFYING_CODE) ==
		    S_ATTR_SELF_MODIFYING_CODE &&
		map->s->reserved2 == 5){
	    nlists = (struct nlist *)(cur_obj->obj_addr +
				      cur_obj->symtab->symoff);
	    indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
	    strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
	    for(i = 0; i < map->nfine_relocs; i++){
		if(map->fine_relocs[i].use_contents == TRUE &&
		   (dead_strip == FALSE || map->fine_relocs[i].live == TRUE)){
		    index = indirect_symtab[map->s->reserved1 + 
			    (map->fine_relocs[i].input_offset / 5)];
					    undefined_map = bsearch(&index,
			cur_obj->undefined_maps, cur_obj->nundefineds,
			sizeof(struct undefined_map),
			(int (*)(const void *, const void *))
			undef_bsearch);
		    if(undefined_map == NULL){
			merged_symbol = lookup_symbol(strings +
					    nlists[index].n_un.n_strx);
			if(merged_symbol->name_len == 0)
				fatal("interal error, scatter_copy() failed"
				  " in looking up external symbol");
		    }
		    else
			merged_symbol = undefined_map->merged_symbol;
		    value = merged_symbol->nlist.n_value;
		    delta = value - (map->output_section->s.addr +
				     map->fine_relocs[i].output_offset + 5);
		    jmpEntry = output_addr + map->output_section->s.offset +
			       map->fine_relocs[i].output_offset;
		    if(host_byte_sex != target_byte_sex)
			delta = SWAP_LONG(delta);
		    *jmpEntry = 0xE9; /* JMP rel32 */
		    memcpy(jmpEntry + 1, &delta, sizeof(unsigned long));
		}
	    }
	}
	else
#endif /* !defined(RLD) */
	/*
	 * For other indirect sections and coalesced sections only copy those
	 * parts of the section who's contents are used in the output file.
	 */
	 if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	    (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	    (map->s->flags & SECTION_TYPE) == S_COALESCED){
	    for(i = 0; i < map->nfine_relocs - 1; i++){
		if(map->fine_relocs[i].use_contents == TRUE &&
		   (dead_strip == FALSE || map->fine_relocs[i].live == TRUE)){
		    memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
			   contents + map->fine_relocs[i].input_offset,
			   map->fine_relocs[i+1].input_offset -
					      map->fine_relocs[i].input_offset);
		}
	    }
	    if(map->fine_relocs[i].use_contents == TRUE &&
	       (dead_strip == FALSE || map->fine_relocs[i].live == TRUE)){
		memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
		       contents + map->fine_relocs[i].input_offset,
		       map->s->size - map->fine_relocs[i].input_offset);
	    }
	}
	else{
	    for(i = 0; i < map->nfine_relocs - 1; i++){
		if(dead_strip == FALSE || map->fine_relocs[i].live == TRUE){
		    memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
			   contents + map->fine_relocs[i].input_offset,
			   map->fine_relocs[i+1].input_offset -
					      map->fine_relocs[i].input_offset);
		}
	    }
	    if(dead_strip == FALSE || map->fine_relocs[i].live == TRUE){
		memcpy(output_addr + map->output_section->s.offset +
					      map->fine_relocs[i].output_offset,
		       contents + map->fine_relocs[i].input_offset,
		       map->s->size - map->fine_relocs[i].input_offset);
	    }
	}
}

#ifndef RLD
/*
 * reloc_output_for_dyld() takes the relocation entries after being processed by
 * a relocation routine and copys the ones to be in the output file for a file
 * the is output for dyld.  It also changes the r_address field of the of the
 * relocation entries to be relative to the first segment's address rather than
 * the section's address.
 */
static
void
reloc_output_for_dyld(
struct section_map *map,
struct relocation_info *relocs,
struct relocation_info *output_locrel,
struct relocation_info *output_extrel,
unsigned long *nlocrel,
unsigned long *nextrel)
{
    unsigned long i, addr_adjust, temp, pair;
    struct relocation_info *reloc;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address, r_extern, r_type, r_scattered, r_pcrel,
		  r_symbolnum, r_value;
    enum bool partial_section, sectdiff, pic, has_sect_diff_relocs;
    enum bool flag_relocs, first_time;

	/* to shut up compiler warning messages "may be used uninitialized" */
	sreloc = NULL;

	/*
	 * If we are flagging relocation entries in read only sections set up
	 * to do that.
	 */
	first_time = TRUE;
	if(read_only_reloc_flag != READ_ONLY_RELOC_SUPPRESS)
	    flag_relocs = is_merged_section_read_only(map->output_section);
	else
	    flag_relocs = FALSE;
	if(flag_relocs == TRUE)
	    clear_read_only_reloc_flags();
	has_sect_diff_relocs = FALSE;

	*nlocrel = 0;
	*nextrel = 0;
	partial_section = (enum bool)
		(dead_strip == TRUE ||
		 (map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	         (map->s->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	         (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	         (map->s->flags & SECTION_TYPE) == S_COALESCED);
	/*
	 * For MH_SPLIT_SEGS images the r_address is relative to the first
	 * read-write segment and there are no relocation entries allowed in
	 * the read-only segments.  This is needed because the r_address field
	 * is 24 bits which means that the normal split of 265meg wouldn't allow
	 * the use of 24 bits from the address of the first segment which is
	 * what is normally used for outputs for dyld.
	 */
	if(segs_read_only_addr_specified == TRUE)
	    addr_adjust = map->output_section->s.addr - 
	    		  segs_read_write_addr;
	else
	    addr_adjust = map->output_section->s.addr - 
			  merged_segments->sg.vmaddr;
	/*
	 * These relocation entries have been processed by a relocation routine
	 * turning external relocation entries into local relocation entries and
	 * updating the r_address field to be relative to the output section's
	 * address.
	 */
	for(i = 0; i < map->s->nreloc; i++){
	    reloc = relocs + i;
	    /*
	     * Break out the fields of the relocation entry we need here.
	     */
	    if((relocs[i].r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		r_scattered = 1;
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_type = sreloc->r_type;
		r_extern = 0;
		r_value = sreloc->r_value;
		/*
		 * For a scattered relocation entry the r_symbolnum is never
		 * NO_SECT and that is all we need to know in this routine for
		 * a scattered relocation entry.  Calculating the r_symbolnum
		 * (n_sect) from the r_value now that it has been processed by
		 * a relocation routine is not easy as the value could be
		 * in a section with fine relocation entries.  It is doable but
		 * not needed here.  So we fake r_symbolnum to be anything but
		 * NO_SECT (the first section ordinal is used).
		 */
		r_symbolnum = 1;
	    }
	    else{
		r_scattered = 0;
		r_address = reloc->r_address;
		r_pcrel = reloc->r_pcrel;
		r_type = reloc->r_type;
		r_extern = reloc->r_extern;
		r_symbolnum = reloc->r_symbolnum;
	    }
	    if(reloc_has_pair(arch_flag.cputype, r_type))
		pair = 1;
	    else
		pair = 0;
	    if(partial_section){
		if(fine_reloc_offset_in_output_for_output_offset(map,
							r_address) == FALSE){
		    i += pair;
		    continue;
		}
	    }
	    if(r_extern == 0){
		sectdiff = reloc_is_sectdiff(arch_flag.cputype, r_type);
		has_sect_diff_relocs |= sectdiff;
		pic = (enum bool)
		      (sectdiff == TRUE ||
		       (r_pcrel == 1 && r_symbolnum != NO_SECT));
	    }
	    else
		pic = FALSE;
	    /*
	     * For output_for_dyld PPC_RELOC_JBSR and HPPA_RELOC_JBSR's are
	     * never put out.
	     */
	    if((arch_flag.cputype == CPU_TYPE_POWERPC &&
		r_type == PPC_RELOC_JBSR) ||
	       (arch_flag.cputype == CPU_TYPE_HPPA &&
		r_type == HPPA_RELOC_JBSR)){
		i += pair;
		continue;
	    }

	    /*
	     * The relocation entries in the output file is based on one of
	     * three different cases:
	     * 	The output file is a multi module dynamic shared library
	     *  The output file has a dynamic linker load command
	     *  The output does not have a dynamic linker load command
	     */
	    if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		/*
		 * For multi module dynamic shared library files all external
		 * relocations are kept as external relocation entries except
		 * for references to private externs (which are have been turned
		 * into locals and kept as locals) and all non-position-
		 * independent local relocation entrie are kept. Modules of
		 * multi module dylibs are not linked together and can only be
		 * slid keeping all sections relative to each other the same.
		 */
		if(r_extern){
		    reloc->r_address += addr_adjust;
		    memcpy(output_extrel + *nextrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nextrel) += 1 + pair;
		    if(flag_relocs == TRUE)
			flag_read_only_reloc(map->s, r_symbolnum, &first_time);
		}
		else if(pic == FALSE){
		    if(r_scattered)
			sreloc->r_address += addr_adjust;
		    else
			reloc->r_address += addr_adjust;
		    memcpy(output_locrel + *nlocrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nlocrel) += 1 + pair;
		}
	    }
	    else if(has_dynamic_linker_command){
		/*
		 * For an file with a dynamic linker load command only external
		 * relocation entries for undefined symbols (those that have
		 * not been turned into locals) are kept.  This output file is
		 * at a fixed address and can't be moved.
		 */
		if(r_extern){
		    reloc->r_address += addr_adjust;
		    memcpy(output_extrel + *nextrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nextrel) += 1 + pair;
		    if(flag_relocs == TRUE)
			flag_read_only_reloc(map->s, r_symbolnum, &first_time);
		}
		/*
		 * Even though the file can't be moved we may be trying to
		 * prebind.  If we are prebinging we need the local
		 * relocation entries for lazy symbol pointers to be saved
		 * so dyld will have the info to undo this if it fails.
		 */
		else if(save_lazy_symbol_pointer_relocs == TRUE &&
			(map->s->flags & SECTION_TYPE) ==
				S_LAZY_SYMBOL_POINTERS){
		    if(r_scattered){
			temp = sreloc->r_address + addr_adjust;
			sreloc->r_address += addr_adjust;
			if(sreloc->r_address != temp)
			    error_with_cur_obj("can't create relocation entry "
				"for prebinding (address of section (%.16s,"
				"%.16s) more than 24-bits away from first "
				"segment, use -noprebind)",
				map->s->segname, map->s->sectname);
		    }
		    else
			reloc->r_address += addr_adjust;
		    memcpy(output_locrel + *nlocrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nlocrel) += 1 + pair;
		}
	    }
	    else{
		/*
		 * For an file without a dynamic linker load command external
		 * relocation entries for undefined symbols (those that have
		 * not been turned into locals) are kept and locals that are
		 * non-position-independent are kept.  This file can only be
		 * slid keeping all sections relative to each other the same.
		 */
		if(r_extern){
		    reloc->r_address += addr_adjust;
		    memcpy(output_extrel + *nextrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nextrel) += 1 + pair;
		    if(flag_relocs == TRUE)
			flag_read_only_reloc(map->s, r_symbolnum, &first_time);
		}
		else if(pic == FALSE){
		    if(r_scattered)
			sreloc->r_address += addr_adjust;
		    else
			reloc->r_address += addr_adjust;
		    memcpy(output_locrel + *nlocrel, reloc,
			   sizeof(struct relocation_info) * (1 + pair));
		    (*nlocrel) += 1 + pair;
		}
	    }
	    i += pair;
	}
	if(flag_relocs == TRUE && *nlocrel != 0){
	    if(read_only_reloc_flag == READ_ONLY_RELOC_ERROR)
		error_with_cur_obj("has local relocation entries in "
		    "non-writable section (%.16s,%.16s)",
		    map->s->segname, map->s->sectname);
	    else
		warning_with_cur_obj("has local relocation entries in "
		    "non-writable section (%.16s,%.16s)",
		    map->s->segname, map->s->sectname);
	}
	if(sect_diff_reloc_flag != SECT_DIFF_RELOC_SUPPRESS &&
	   has_sect_diff_relocs == TRUE){
	    if(sect_diff_reloc_flag == SECT_DIFF_RELOC_ERROR)
		error_with_cur_obj("has section difference relocation entries "
		    "in section (%.16s,%.16s)", map->s->segname,
		    map->s->sectname);
	    else
		warning_with_cur_obj("has section difference relocation entries"
		    " in section (%.16s,%.16s)", map->s->segname,
		    map->s->sectname);
	}
}

/*
 * is_merged_section_read_only() returns TRUE if the merged section is in a
 * segment that does not have write permision.  Otherwise it returns FALSE.
 */
static
enum bool
is_merged_section_read_only(
struct merged_section *key)
{
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    q = &(msg->content_sections);
	    while(*q){
		ms = *q;
		if(ms == key){
		    if((msg->sg.initprot & VM_PROT_WRITE) == 0)
			return(TRUE);
		    else
			return(FALSE);
		}
		q = &(ms->next);
	    }
	    q = &(msg->zerofill_sections);
	    while(*q){
		ms = *q;
		if(ms == key){
		    if((msg->sg.initprot & VM_PROT_WRITE) == 0)
			return(TRUE);
		    else
			return(FALSE);
		}
		q = &(ms->next);
	    }
	    p = &(msg->next);
	}
	fatal("internal error: is_merged_section_read_only() called with "
	      "bad merged section");
	return(FALSE);
}

/*
 * is_merged_symbol_coalesced() is needed by the relocation routines to check
 * for illegal references to coalesced symbols via external relocation routines.
 * The section number in a merged symbol when relocation is done is the section
 * number in the output file so we have to look through the merged sections to
 * find which section this is.  This routine returns TRUE if the symbol is in
 * a coalesced section.
 */
__private_extern__
enum bool
is_merged_symbol_coalesced(
struct merged_symbol *merged_symbol)
{
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;

	if((merged_symbol->nlist.n_type & N_TYPE) != N_SECT)
	    return(FALSE);

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    q = &(msg->content_sections);
	    while(*q){
		ms = *q;
		if(ms->output_sectnum == merged_symbol->nlist.n_sect){
		    if((ms->s.flags & SECTION_TYPE) == S_COALESCED)
			return(TRUE);
		    else
			return(FALSE);
		}
		q = &(ms->next);
	    }
	    p = &(msg->next);
	}
	fatal("internal error: is_merged_symbol_coalesced() called with "
	      "bad merged symbol");
	return(FALSE);
}

/*
 * scatter_copy_relocs() copies the relocation entries for an indirect section
 * or coalesced section (or any regular section if -dead_strip is specified)
 * into the output file based on which items in the section are in the output
 * file.  It returns the number of relocation entries that were put in the
 * output file.
 */
static
unsigned long
scatter_copy_relocs(
struct section_map *map,
struct relocation_info *relocs,
struct relocation_info *output_relocs)
{
    unsigned long i, nreloc;
    struct relocation_info *reloc;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address, r_type;

	/*
	 * No checks done here as they were previously done and if there were
	 * errors this will not even get called.
	 */
	nreloc = 0;
	for(i = 0; i < map->s->nreloc; i++){
	    reloc = relocs + i;
	    /*
	     * Break out the fields of the relocation entry we need here.
	     */
	    if((relocs[i].r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		r_address = sreloc->r_address;
		r_type = sreloc->r_type;
	    }
	    else{
		r_address = reloc->r_address;
		r_type = reloc->r_type;
	    }
	    if(fine_reloc_offset_in_output_for_output_offset(map, r_address)){
		/* copy reloc into output file */
		memcpy(output_relocs + nreloc, reloc,
		       sizeof(struct relocation_info));
		nreloc++;
		if(reloc_has_pair(arch_flag.cputype, r_type)){
		    /* copy the reloc's pair into output file */
		    memcpy(output_relocs + nreloc, reloc + 1,
			   sizeof(struct relocation_info));
		    nreloc++;
		    i++;
		}
	    }
	    else if(reloc_has_pair(arch_flag.cputype, r_type))
		i++;
	}
	return(nreloc);
}

/*
 * nop_pure_instruction_scattered_sections() is a hack to pad an i386 pure
 * instructions sections with nop's (opcode 0x90) to make disassembly cleaner
 * between scatter loaded symbols.
 */
__private_extern__
void
nop_pure_instruction_scattered_sections(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;
    char *contents;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->order_filename != NULL &&
		    (ms->s.flags & SECTION_TYPE) == S_REGULAR) ||
		   ((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		    (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS||
		    (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		    (ms->s.flags & SECTION_TYPE) == S_COALESCED)){
		    if(arch_flag.cputype == CPU_TYPE_I386 &&
		       (ms->s.flags & S_ATTR_PURE_INSTRUCTIONS) != 0){
			contents = output_addr + ms->s.offset;
			memset(contents, 0x90, ms->s.size);
		    }
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * flush_scatter_copied_sections() flushes the entire merged section's output
 * for each merged regular (non-literal) content section that has a load order
 * (and indirect sections).
 */
__private_extern__
void
flush_scatter_copied_sections(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->contents_filename == NULL &&
		    (ms->order_filename != NULL || dead_strip == TRUE) &&
		    (ms->s.flags & SECTION_TYPE) == S_REGULAR) ||
		   ((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		    (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS||
		    (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		    (ms->s.flags & SECTION_TYPE) == S_COALESCED)){
		    output_flush(ms->s.offset, ms->s.size);
		}
		else if((dead_strip == TRUE) &&
		   ((ms->s.flags & SECTION_TYPE) == S_MOD_INIT_FUNC_POINTERS ||
		    (ms->s.flags & SECTION_TYPE) == S_MOD_TERM_FUNC_POINTERS)){
		    output_flush(ms->s.offset, ms->s.size);
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * live_marking() is called when -dead_strip is specified and marks the
 * fine_relocs of the sections and symbols live if they can be reached by the
 * exported symbols or other live blocks.
 */
__private_extern__
void
live_marking(void)
{
    struct merged_symbol *merged_symbol;
    enum bool found;
    unsigned long i, input_offset, section_type;
    struct fine_reloc *fine_reloc;
    struct merged_segment *msg, **p;
    struct merged_section *ms, **content, **zerofill;
    struct timeval t0, t1, t2, t3, t4, t5;
    double time_used;

	if(dead_strip_times == TRUE)
	    gettimeofday(&t0, NULL);

	/*
	 * First set up all the refs arrays in each fine_reloc and the pointer
	 * to the fine_reloc in each merged_symbol.
	 */
	build_references();
	/*
	 * If build_references() encountered a relocation error just return now.
	 */
	if(errors)
	    return;

	if(dead_strip_times == TRUE)
	    gettimeofday(&t1, NULL);

	/*
	 * If the output filetype has an entry point mark it live.  If the
	 * entry point symbol was specified mark it live and if it is in a
	 * section mark the fine_reloc for it live.  Else if no entry point
	 * symbol symbol specified mark the first non-zero sized fine_reloc in
	 * the first content section if there is one.
	 */
 	if(filetype != MH_FVMLIB &&
	   filetype != MH_DYLIB &&
	   filetype != MH_BUNDLE){
	    if(entry_point_name != NULL){
		merged_symbol = lookup_symbol(entry_point_name);
		/*
		 * If the symbol is not found the entry point it can't be
		 * marked live. Note: the error of specifying a bad entry point
		 * name is handled in layout_segments() in layout.c .
		 */
		if(merged_symbol->name_len != 0){
#ifdef DEBUG
		    if(((debug & (1 << 25)) || (debug & (1 << 26)))){
			print("** In live_marking() -e symbol ");
			print_obj_name(merged_symbol->definition_object);
			print("%s\n", merged_symbol->nlist.n_un.n_name);
		    }
#endif /* DEBUG */
		    merged_symbol->live = TRUE;
		    fine_reloc = merged_symbol->fine_reloc;
		    if(fine_reloc != NULL)
			fine_reloc->live = TRUE;
		}
	    }
	    else{
		/*
		 * To find the first non-zero sized fine_reloc in the the first
		 * content section we require that we have the info for the
		 * load maps.
		 */
		found = FALSE;
		for(msg = merged_segments;
		    msg != NULL && found == FALSE;
		    msg = msg->next){
		    for(ms = msg->content_sections;
			ms != NULL && found == FALSE;
			ms = ms->next){
			for(i = 0;
			    i < ms->norder_load_maps && found == FALSE;
			    i++){
			    if(ms->order_load_maps[i].size != 0){
				input_offset = ms->order_load_maps[i].value -
				    ms->order_load_maps[i].section_map->s->addr;
				fine_reloc = fine_reloc_for_input_offset(
				    ms->order_load_maps[i].section_map,
				    input_offset);
				fine_reloc->live = TRUE;
				if(fine_reloc->merged_symbol != NULL)
				    fine_reloc->merged_symbol->live = TRUE;
				found = TRUE;
#ifdef DEBUG
				if(((debug & (1 << 25)) ||
				   (debug & (1 << 26)))){
				    print("** In live_marking() entry point ");
				    if(ms->order_load_maps[i].archive_name !=
				       NULL)
					print("%s(%s):",
					    ms->order_load_maps[i].archive_name,
					    ms->order_load_maps[i].object_name);
				    else
					print("%s:",
					    ms->order_load_maps[i].object_name);
				    print("(%.16s,%.16s):0x%x:",
					  ms->s.segname, ms->s.sectname,
					  (unsigned int)
						(fine_reloc->input_offset));
		       		    print("%s\n",
					  ms->order_load_maps[i].symbol_name);
				}
#endif /* DEBUG */
			    }
			}
		    }
		}
	    }
	}

	/*
	 * If this is a shared library and a -init symbol was specified mark it
	 * live.
	 */
	if(filetype == MH_DYLIB && init_name != NULL){
	    merged_symbol = lookup_symbol(init_name);
	    /*
	     * If the symbol is not found the init routine it can't be marked
	     * live. Note: the error of specifying a bad init routine name is
	     * handled in layout_segments() in layout.c .
	     */
	    if(merged_symbol->name_len != 0){
#ifdef DEBUG
		if(((debug & (1 << 25)) || (debug & (1 << 26)))){
		    print("** In live_marking() -init symbol ");
		    print_obj_name(merged_symbol->definition_object);
		    print("%s\n", merged_symbol->nlist.n_un.n_name);
		}
#endif /* DEBUG */
		merged_symbol->live = TRUE;
		if(merged_symbol->fine_reloc != NULL)
		    merged_symbol->fine_reloc->live = TRUE;
	    }
	}

	/*
	 * Now mark the "exported" merged symbols and their fine_relocs live.
	 */
	mark_globals_live();

	/*
         * Now mark the fine_relocs for local symbols with the N_NO_DEAD_STRIP
	 * bit set live.
	 */
	mark_N_NO_DEAD_STRIP_local_symbols_live();

	/*
	 * Now mark all the fine_relocs in sections with the
	 * S_ATTR_NO_DEAD_STRIP attribute live.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->s.flags & S_ATTR_NO_DEAD_STRIP)
		    mark_all_fine_relocs_live_in_section(ms);
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		if(ms->s.flags & S_ATTR_NO_DEAD_STRIP)
		    mark_all_fine_relocs_live_in_section(ms);
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}

	/*
	 * If -no_dead_strip_inits_and_terms is specified for all things in
	 * mod init and term sections to be live.
	 */
	if(no_dead_strip_inits_and_terms == TRUE){
	    p = &merged_segments;
	    while(*p){
		msg = *p;
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    section_type = ms->s.flags & SECTION_TYPE;
		    if(section_type == S_MOD_INIT_FUNC_POINTERS ||
		       section_type == S_MOD_TERM_FUNC_POINTERS)
			mark_all_fine_relocs_live_in_section(ms);
		    content = &(ms->next);
		}
		p = &(msg->next);
	    }
	}

	if(dead_strip_times == TRUE)
	    gettimeofday(&t2, NULL);

	/*
	 * Now with the above code marking the initial fine_relocs live cause
	 * the references from the live fine relocs to be marked live.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
#ifdef DEBUG
		if(debug & (1 << 25))
		    print("In live_marking() for section (%.16s,%.16s)\n",
			  ms->s.segname, ms->s.sectname);
#endif /* DEBUG */
		walk_references_in_section(MARK_LIVE, ms);
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}

	if(dead_strip_times == TRUE)
	    gettimeofday(&t3, NULL);

	/*
	 * If -no_dead_strip_inits_and_terms was not specified, now with all
	 * other things marked live determine if the mod init and term
	 * routines are touching something live and if so mark them and their
	 * references live.
	 */
	if(no_dead_strip_inits_and_terms == FALSE){
	    p = &merged_segments;
	    while(*p){
		msg = *p;
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    section_type = ms->s.flags & SECTION_TYPE;
		    if(section_type == S_MOD_INIT_FUNC_POINTERS ||
		       section_type == S_MOD_TERM_FUNC_POINTERS)
			walk_references_in_section(SEARCH_FOR_LIVE, ms);
		    content = &(ms->next);
		}
		p = &(msg->next);
	    }
	}

	if(dead_strip_times == TRUE)
	    gettimeofday(&t4, NULL);

	/*
	 * Now with all other things marked live, for the sections marked with
	 * the S_ATTR_LIVE_SUPPORT attribute check to see if any of the
	 * references for each the fine_reloc touches some thing live.  If so
	 * cause it and its references to be live.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->s.flags & S_ATTR_LIVE_SUPPORT)
		    walk_references_in_section(CHECK_FOR_LIVE_TOUCH, ms);
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}

	if(dead_strip_times == TRUE){
	    gettimeofday(&t5, NULL);
	    time_used = calculate_time_used(&t0, &t1);
	    print("building of references: %f\n", time_used);
	    time_used = calculate_time_used(&t1, &t2);
	    print("mark initial live blocks: %f\n", time_used);
	    time_used = calculate_time_used(&t2, &t3);
	    print("mark live blocks referenced: %f\n", time_used);
	    time_used = calculate_time_used(&t3, &t4);
	    print("mark live constructors: %f\n", time_used);
	    time_used = calculate_time_used(&t4, &t5);
	    print("mark live exception frames: %f\n", time_used);
	}
}

/*
 * calculate_time_used() takes a start timeval and and an end time value and
 * calculates the difference as a double value and returns that.
 */
static
double
calculate_time_used(
struct timeval *start,
struct timeval *end)
{
    double time_used;

	time_used = end->tv_sec - start->tv_sec;
	if(end->tv_usec >= start->tv_usec)
	    time_used += ((double)(end->tv_usec - start->tv_usec)) / 1000000.0;
	else
	    time_used += -1.0 +
		((double)(1000000 + end->tv_usec - start->tv_usec) / 1000000.0);
	return(time_used);
}

/*
 * build_references() is called by live_marking() to set up the references
 * arrays off each fine reloc structure in each section.
 */
static
void
build_references(
void)
{
    struct merged_segment *msg, **p;
    struct merged_section *ms, **content;

	/*
	 * For objects that there sections loaded as one block we need to get
	 * all the merged symbols in those blocks to have their fine_reloc
	 * pointer set correctly.
	 */
	set_fine_relocs_for_merged_symbols();

	/*
	 * Set up the references for each section.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
#ifdef DEBUG
		if(debug & (1 << 27))
		    print("In build_references() for section (%.16s,%.16s)\n",
			  ms->s.segname, ms->s.sectname);
#endif /* DEBUG */
		setup_references_in_section(ms);
		/*
		 * If setup_references_in_section() encountered a relocation
		 * error just return now.
		 */
		if(errors)
		    return;
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}

#ifdef DEBUG
	if(debug & (1 << 28))
	    print_references();
#endif /* DEBUG */
}

#ifdef DEBUG
/*
 * print_references() is a debugging routine to print the references off each
 * fine_reloc after build_references() has been called.
 */
static
void
print_references(
void)
{
    unsigned long i, j, k;
    struct object_list *object_list, **q;
    struct object_file *obj;
    struct fine_reloc *fine_relocs, *fine_reloc;
    struct section_map *map;
    struct ref *ref;

	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj == base_obj)
		    continue;
		if(obj->dylib)
		    continue;
		if(obj->bundle_loader)
		    continue;
		if(obj->dylinker)
		    continue;
		for(j = 0; j < obj->nsection_maps; j++){
		    if(obj->section_maps[j].s->size == 0)
			continue;
		    map = &(obj->section_maps[j]);
		    if(map->nfine_relocs == 0)
			continue;
		    print("references for ");
		    print_obj_name(obj);
		    print("in section (%.16s,%.16s)\n",
			  map->s->segname, map->s->sectname);
		    fine_relocs = map->fine_relocs;
		    for(k = 0; k < map->nfine_relocs; k++){
			fine_reloc = map->fine_relocs + k;
			if(fine_reloc->refs != NULL){
			    print("  offset:0x%x",
				  (unsigned int)(fine_reloc->input_offset));
			    if(fine_reloc->merged_symbol != NULL)
				print(":%s", fine_reloc->
					     merged_symbol->nlist.n_un.n_name);
			    else
				print_symbol_name_from_order_load_maps(map,
				      map->s->addr + fine_reloc->input_offset);
			    print("\n");
			}
			for(ref = fine_reloc->refs;
			    ref != NULL;
			    ref = ref->next){
			    if(ref->merged_symbol != NULL)
				print("    %s\n",
				      ref->merged_symbol->nlist.n_un.n_name);
			    else{
				print("    (%.16s,%.16s):0x%x",
				  ref->map->s->segname,
				  ref->map->s->sectname,
				  (unsigned int)
				   (ref->fine_reloc->input_offset));
				print_symbol_name_from_order_load_maps(
				  ref->map,
				  ref->map->s->addr +
				  ref->fine_reloc->input_offset);
				printf("\n");
			    }
			}
		    }
		}
	    }
	}
}
#endif /* DEBUG */

/*
 * setup_references_in_section() is called with a merged section and sets up all
 * the references of the fine_relocs in that section.
 */
static
void
setup_references_in_section(
struct merged_section *ms)
{
    unsigned long i, j;
    struct object_list *object_list, **q;
    struct object_file *obj;
    struct section_map *map;

	/*
	 * For each object file that has this section process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj == base_obj)
		    continue;
		if(obj->dylib)
		    continue;
		if(obj->bundle_loader)
		    continue;
		if(obj->dylinker)
		    continue;
		map = NULL;
		for(j = 0; j < obj->nsection_maps; j++){
		    if(obj->section_maps[j].output_section != ms)
			continue;
		    if(obj->section_maps[j].s->size == 0)
			continue;
		    map = &(obj->section_maps[j]);
		    break;
		}
		if(map == NULL)
		    continue;
#ifdef DEBUG
		if(debug & (1 << 27)){
		    print(" In setup_references_in_section() with object ");
		    print_obj_name(obj);
		    print("\n");
		}
#endif /* DEBUG */
		setup_references(map, obj);
	    }
	}
}

/*
 * setup_references() is passed a section map and the object file it is in.  It
 * digs through the relocation entries of the section creating a references for
 * each.
 */
static
void
setup_references(
struct section_map *map,
struct object_file *obj)
{
    unsigned long i, pair;
    struct relocation_info *relocs, reloc;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address, r_type;
    char *contents;
    struct live_refs refs;
    struct fine_reloc *fine_reloc;

#ifdef DEBUG
	if(debug & (1 << 27)){
	    print("  In setup_references() ");
	    print_obj_name(obj);
	    print("(%.16s,%.16s)\n", map->s->segname, map->s->sectname);
	}
#endif /* DEBUG */

	/*
	 * Walk all the relocation entries of this section creating the
	 * references between blocks.
	 */
	relocs = (struct relocation_info *)(obj->obj_addr + map->s->reloff);
	if(obj->swapped && map->input_relocs_already_swapped == FALSE){
	    swap_relocation_info(relocs, map->s->nreloc, host_byte_sex);
	    map->input_relocs_already_swapped = TRUE;
	}
	for(i = 0; i < map->s->nreloc; i++){
	    /*
	     * Note all errors are not flagged here but left for the *_reloc()
	     * routines to flag them.  So for errors we just return from here.
	     */
	    reloc = relocs[i];
	    /*
	     * Break out just the fields of the relocation entry we need to
	     * determine if this entry (and its possible pair) are for this
	     * fine_reloc.
	     */
	    if((reloc.r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(&reloc);
		r_address = sreloc->r_address;
		r_type = sreloc->r_type;
	    }
	    else{
		r_address = reloc.r_address;
		r_type = reloc.r_type;
	    }
	    if(reloc_has_pair(arch_flag.cputype, r_type)){
		if(i + 1 >= map->s->nreloc)
		    return;
		pair = 1;
	    }
	    else
		pair = 0;
#ifdef DEBUG
	    if(debug & (1 << 27)){
		print("    reloc entry %lu", i);
		if(pair)
		    print(",%lu", i+1);
		print("\n");
	    }
#endif /* DEBUG */

	    /*
	     * Get the references for this relocation entry(s).
	     */
	    cur_obj = obj;
	    contents = obj->obj_addr + map->s->offset;
	    if(arch_flag.cputype == CPU_TYPE_POWERPC ||
		    arch_flag.cputype == CPU_TYPE_VEO)
		ppc_reloc(contents, relocs, map, &refs, i);
	    else if(arch_flag.cputype == CPU_TYPE_MC680x0)
		generic_reloc(contents, relocs, map, FALSE, &refs, i);
	    else if(arch_flag.cputype == CPU_TYPE_I386)
		generic_reloc(contents, relocs, map, TRUE, &refs, i);
	    else if(arch_flag.cputype == CPU_TYPE_ARM)
	      arm_reloc(contents, relocs, map, &refs, i);
	    else if(arch_flag.cputype == CPU_TYPE_MC88000 ||
		    arch_flag.cputype == CPU_TYPE_HPPA ||
		    arch_flag.cputype == CPU_TYPE_SPARC ||
		    arch_flag.cputype == CPU_TYPE_I860)
		fatal("-dead_strip not supported with cputype (%d)",
		      arch_flag.cputype);
	    else
		fatal("internal error: setup_references() "
		      "called with unknown cputype (%d) set",
		      arch_flag.cputype);
	    /* if there was a problem with the relocation just return now */
	    if(errors)
		return;

	    /*
	     * If this reloc has references then add them to the tmp_refs[] 
	     * array if they are new.
	     */
	    if(refs.ref1.ref_type != LIVE_REF_NONE){
		fine_reloc = fine_reloc_for_input_offset(map, r_address);
		setup_reference(&refs.ref1, obj, fine_reloc);
	    }
	    if(refs.ref2.ref_type != LIVE_REF_NONE){
		fine_reloc = fine_reloc_for_input_offset(map, r_address);
		setup_reference(&refs.ref2, obj, fine_reloc);
	    }
	    i += pair;
	}
}

/*
 * setup_reference() is passed a pointer to a live_ref struct in the specified
 * object and the fine_reloc that reference comes from.  It creates a struct
 * ref for the live_ref struct.  Then if that reference is not to itself and
 * already on the list for the fine_reloc it is added to the list.
 */
static
void
setup_reference(
struct live_ref *ref,
struct object_file *obj,
struct fine_reloc *self_fine_reloc)
{
    unsigned long r_symbolnum;
    struct section_map *local_map;
    struct fine_reloc *ref_fine_reloc;
    struct ref r, *refs, *new_ref;

	r.next = NULL;
	r.fine_reloc = NULL;
	r.map = NULL;
	r.obj = NULL;
	r.merged_symbol = NULL;
	if(ref->ref_type == LIVE_REF_VALUE){
	    r_symbolnum = r_symbolnum_from_r_value(ref->value, obj);
	    local_map = &(obj->section_maps[r_symbolnum - 1]);
	    ref_fine_reloc = fine_reloc_for_input_offset(local_map,
			      ref->value - local_map->s->addr);
#ifdef DEBUG
	    if(debug & (1 << 27)){
		print("      ref ");
		print("(%.16s,%.16s):0x%x", local_map->s->segname,
		      local_map->s->sectname,
		      (unsigned int)(ref_fine_reloc->input_offset));
		print_symbol_name_from_order_load_maps(local_map,
		    local_map->s->addr + ref_fine_reloc->input_offset);
		printf("\n");
	    }
#endif /* DEBUG */
	    if(self_fine_reloc == ref_fine_reloc)
		return;
	    r.fine_reloc = ref_fine_reloc;
	    r.map = local_map;
	    r.obj = obj;
	    r.merged_symbol = NULL;
	}
	else if(ref->ref_type == LIVE_REF_SYMBOL){
	    r.merged_symbol = ref->merged_symbol;
	    r.fine_reloc = NULL;
	    r.map = NULL;
	    r.obj = NULL;
#ifdef DEBUG
	    if(debug & (1 << 27)){
		print("      ref ");
		print("%s", ref->merged_symbol->nlist.n_un.n_name);
		printf("\n");
	    }
#endif /* DEBUG */
	}
	/*
	 * See if this reference is already in the list of references.
	 */
	for(refs = self_fine_reloc->refs ; refs != NULL ; refs = refs->next){
	    if(r.fine_reloc == refs->fine_reloc &&
	       r.map == refs->map &&
	       r.obj == refs->obj &&
	       r.merged_symbol == refs->merged_symbol)
		return;
	}
	/* it is not in the list so add it */
	new_ref = allocate(sizeof(struct ref));
	*new_ref = r;
	new_ref->next = self_fine_reloc->refs;
	self_fine_reloc->refs = new_ref;
}

/*
 * get_fine_reloc_for_merged_symbol() finds the fine_reloc for the
 * specified merged_symbol (if any) and returns it or NULL.  It also returns
 * the section map for the symbol if local_map is not NULL.
 */
__private_extern__
struct fine_reloc *
get_fine_reloc_for_merged_symbol(
struct merged_symbol *merged_symbol,
struct section_map **local_map)
{
    unsigned long n_sect, input_offset, i;
    struct section_map *map;
    struct fine_reloc *fine_reloc;

	/* N_INDR symbols have had their indirection resolved at this point. */
	if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
	    merged_symbol = (struct merged_symbol *)
			    merged_symbol->nlist.n_value;

	/*
	 * Find the fine_reloc for this symbol if any.  That will be in the
	 * section map for the object that defines this symbol.
	 */
	if(merged_symbol->defined_in_dylib == FALSE &&
	   (merged_symbol->nlist.n_type & N_TYPE) == N_SECT){
	    n_sect = merged_symbol->nlist.n_sect;
	    map = &(merged_symbol->definition_object->section_maps[n_sect - 1]);
	    if(map->nfine_relocs != 0){
		/*
		 * The value of the merged symbol is the value in the input
		 * file at this point.
		 */
		input_offset = merged_symbol->nlist.n_value - map->s->addr;
		fine_reloc = fine_reloc_for_input_offset(map, input_offset);
		/*
		 * It is possible that there may be more than one merged symbol
		 * at the same input_offset.  If this fine_reloc is not for this
		 * merged symbol search the fine_relocs for one that has this
		 * merged_symbol.
		 */
		if(fine_reloc->merged_symbol != merged_symbol){
		    for(i = 0; i < map->nfine_relocs; i++){
			if(map->fine_relocs[i].merged_symbol == merged_symbol){
			    fine_reloc = map->fine_relocs + i;
			    break;
			}
		    }
		}
		if(local_map != NULL)
		    *local_map = map;
		return(fine_reloc);
	    }
	}
	return(NULL);
}

/*
 * mark_all_fine_relocs_live_in_section() is called for sections that have the
 * S_ATTR_NO_DEAD_STRIP section attribute and marks all the fine_relocs in
 * objects with that section live.
 */
static
void
mark_all_fine_relocs_live_in_section(
struct merged_section *ms)
{
    unsigned long i, j;
    struct object_list *object_list, **q;
    struct fine_reloc *fine_relocs;
    struct object_file *obj;
    struct section_map *map;

	/*
	 * For each object file that has this section process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj == base_obj)
		    continue;
		if(obj->dylib)
		    continue;
		if(obj->bundle_loader)
		    continue;
		if(obj->dylinker)
		    continue;
		map = NULL;
		for(j = 0; j < obj->nsection_maps; j++){
		    if(obj->section_maps[j].output_section != ms)
			continue;
		    if(obj->section_maps[j].s->size == 0)
			continue;
		    map = &(obj->section_maps[j]);
		    break;
		}
		if(map == NULL)
		    continue;

#ifdef DEBUG
		if(debug & (1 << 25)){
		    print(" mark_all_fine_relocs_live_in_section() with "
			  "object");
		    print_obj_name(obj);
		    print("\n");
		}
#endif /* DEBUG */

		fine_relocs = map->fine_relocs;
		for(j = 0; j < map->nfine_relocs; j++){
		    fine_relocs[j].live = TRUE;
		}
	    }
	}
}

/*
 * walk_references_in_section() is called with an operation and a merged section
 * and walks the references of the fine_relocs in that section.
 *
 * If the operation is MARK_LIVE it looks for live fine_relocs in the specified
 * section and causes its references to be marked live.
 *
 * If the operation is SEARCH_FOR_LIVE it is being called with a mod init or
 * term section and for each fine_reloc in the section it determines if it's
 * references reach something that is live.  If it does it marks the fine_reloc
 * live and causes all of its references to be marked live.
 *
 * If the operation is CHECK_FOR_LIVE_TOUCH it is being called for a section
 * with the live support attribute and for each fine_reloc in the section it
 * determines if it directly references something that is live.  If it does it
 * marks the fine_reloc live and causes all of its references to be marked live.
 */
static
void
walk_references_in_section(
enum walk_references_operation operation,
struct merged_section *ms)
{
    unsigned long i, j;
    struct object_list *object_list, **q;
    struct fine_reloc *fine_relocs;
    struct object_file *obj;
    struct section_map *map;
    enum bool found_live;

	/*
	 * For each object file that has this section process it.
	 */
	for(q = &objects; *q; q = &(object_list->next)){
	    object_list = *q;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj == base_obj)
		    continue;
		if(obj->dylib)
		    continue;
		if(obj->bundle_loader)
		    continue;
		if(obj->dylinker)
		    continue;
		map = NULL;
		for(j = 0; j < obj->nsection_maps; j++){
		    if(obj->section_maps[j].output_section != ms)
			continue;
		    if(obj->section_maps[j].s->size == 0)
			continue;
		    map = &(obj->section_maps[j]);
		    break;
		}
		if(map == NULL)
		    continue;

#ifdef DEBUG
		if(debug & (1 << 25)){
		    print(" In walk_references_in_section() with object ");
		    print_obj_name(obj);
		    print("\n");
		}
#endif /* DEBUG */

		fine_relocs = map->fine_relocs;
		for(j = 0; j < map->nfine_relocs; j++){
		    if(operation == MARK_LIVE){
			if(fine_relocs[j].live == TRUE &&
			   fine_relocs[j].refs_marked_live == FALSE){
			    walk_references(
				MARK_LIVE,
				fine_relocs + j,
				map,
				obj);
			}
		    }
		    else if(operation == SEARCH_FOR_LIVE ||
		            operation == CHECK_FOR_LIVE_TOUCH){
			found_live = FALSE;
			if(fine_relocs[j].searched_for_live_refs == FALSE ||
			   operation == CHECK_FOR_LIVE_TOUCH){
			    found_live = walk_references(
			        operation,
				fine_relocs + j,
				map,
				obj);
			    /*
			     * If something reached or touched from this
			     * fine_reloc was found to be live then mark it
			     * live and cause its references to be marked live.
			     */ 
			    if(found_live == TRUE){
#ifdef DEBUG
				if(debug & (1 << 25)){
				    print("  In walk_references_in_section("
					  "%s) with object ",
					  walk_references_operation_names[
					  operation]);
				    print_obj_name(obj);
				    print("fine_relocs[%lu].input_offset = "
					  "0x%x found_live == TRUE\n", j,
					  (unsigned int)
					  (fine_relocs[j].input_offset) );
				}
#endif /* DEBUG */
				fine_relocs[j].live = TRUE;
				if(fine_relocs[j].refs_marked_live == FALSE){
				    walk_references(
					MARK_LIVE,
					fine_relocs + j,
					map,
					obj);
				}
			    }
			}
		    }
		    else{
			fatal("internal error: walk_references_in_section() "
			      "called with unknown operation (%d)", operation);
		    }
		}
	    }
	}
}

/*
 * walk_references() walks the references of the specified fine_reloc in the
 * specified map, in the the specified object.
 *
 * For the MARK_LIVE operation it is called with a fine_reloc that is live,
 * this routine marks all the fine_reloc references live.  The return value is
 * meaningless for the MARK_LIVE operation.
 *
 * For the SEARCH_FOR_LIVE operation it searches for any referenced fine_reloc
 * that is marked live.  If it finds a referenced fine_reloc that is marked
 * live it returns TRUE and stops searching.  If it completes it search without 
 * finding a fine_reloc that is marked live it returns FALSE.
 *
 * For the CHECK_FOR_LIVE_TOUCH operation it checks only the directly referenced
 * fine_reloc's to see if one of them is live. If it finds a referenced
 * fine_reloc that is marked live it returns TRUE.  If it does not it returns
 * FALSE.
 */
static
enum bool
walk_references(
enum walk_references_operation operation,
struct fine_reloc *fine_reloc,
struct section_map *map,
struct object_file *obj)
{
    enum bool found_live;
    struct ref r, *ref;

#ifdef DEBUG
	if(debug & (1 << 25)){
	    print("  In walk_references(%s) ",
		  walk_references_operation_names[operation]);
	    print_obj_name(obj);
	    print("(%.16s,%.16s):0x%x", map->s->segname, map->s->sectname,
		  (unsigned int)(fine_reloc->input_offset));
	    if(fine_reloc->merged_symbol != NULL)
		print(":%s", fine_reloc->merged_symbol->nlist.n_un.n_name);
	    print("\n");
	}
#endif /* DEBUG */

	if(operation == MARK_LIVE){
	    fine_reloc->refs_marked_live = TRUE;
	    /*
	     * Since this fine_reloc is live if this is in a symbol stub or
	     * or symbol pointer section we need to mark the indirect symbol
	     * for it live.
	     */
	    if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
	       (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	       (map->s->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS){
		if(indirect_live_ref(fine_reloc, map, obj, &r) == TRUE)
		    ref_operation(MARK_LIVE, &r, obj);
	    }
	}
	else{ /* operation == SEARCH_FOR_LIVE */
	    fine_reloc->searched_for_live_refs = TRUE;
	    if(fine_reloc->live == TRUE)
		return(TRUE);
	}

	/*
	 * Walk the references of this fine_reloc.
	 */
	for(ref = fine_reloc->refs; ref != NULL; ref = ref->next){
	    if(operation == MARK_LIVE){
		ref_operation(MARK_LIVE, ref, obj);
	    }
	    else if(operation == SEARCH_FOR_LIVE ||
		    operation == CHECK_FOR_LIVE_TOUCH){
		found_live = ref_operation(operation, ref, obj);
		    if(found_live == TRUE)
			return(TRUE);
	    }
	    else{
		fatal("internal error: walk_references() called with "
		      "unknown operation (%d)", operation);
	    }
	}
	return(FALSE);
}

/*
 * ref_operation() gets the fine_reloc being referenced from the specified
 * ref in the specified object then preforms the specified operation.
 *
 * For the MARK_LIVE operation if the fine_reloc is not marked live, it is
 * marked live and then walk_references() is called with the MARK_LIVE operation
 * to mark its references live.  If the specified ref is a symbol defined
 * in a dylib then
mark_dylib_references_live() is called to mark that dylib's module's
 * references live. For the MARK_LIVE operation the return value is meaningless.
 *
 * For the SEARCH_FOR_LIVE operation if the fine_reloc is marked live then TRUE
 * is returned.  Else walk_references() is called with the SEARCH_FOR_LIVE
 * operation and if it returns TRUE then TRUE is returned.  Else FALSE is
 * returned.  If the specified ref is a symbol defined in a dylib then
something will called to walk the references of the dylib searching for a live
 * symbol.
 * 
 * For the CHECK_FOR_LIVE_TOUCH operation if the fine_reloc is marked live the
 * TRUE is returned else FALSE is returned.
 */
static
enum bool
ref_operation(
enum walk_references_operation operation,
struct ref *ref,
struct object_file *obj)
{
    unsigned long n_sect;
    struct section_map *local_map;
    struct merged_symbol *indr_merged_symbol;
    enum bool found_live;
    struct fine_reloc *ref_fine_reloc;

	if(ref->merged_symbol == NULL){
#ifdef DEBUG
	    if((((debug & (1 << 25)) || (debug & (1 << 26))) &&
	       ref->fine_reloc->live != TRUE)){
		print("** In ref_operation(%s) ",
		      walk_references_operation_names[operation]);
		print_obj_name(obj);
		print("(%.16s,%.16s):0x%x", ref->map->s->segname,
		      ref->map->s->sectname,
		      (unsigned int)(ref->fine_reloc->input_offset));
		if(ref->fine_reloc->merged_symbol != NULL){
		    print(":%s",
			  ref->fine_reloc->merged_symbol->nlist.n_un.n_name);
		}
		else{
		    print_symbol_name_from_order_load_maps(ref->map,
			ref->map->s->addr + ref->fine_reloc->input_offset);
		}
		print("\n");
	    }
#endif /* DEBUG */

	    if(operation == MARK_LIVE){
		ref->fine_reloc->live = TRUE;
		if(ref->fine_reloc->merged_symbol != NULL)
		    ref->fine_reloc->merged_symbol->live = TRUE;
		if(ref->fine_reloc->refs_marked_live == FALSE){
		    walk_references(
			MARK_LIVE,
			ref->fine_reloc,
			ref->map,
			ref->obj);
		}
	    }
	    else if(operation == SEARCH_FOR_LIVE){
		if(ref->fine_reloc->live == TRUE)
		    return(TRUE);
		else{
		    if(ref->fine_reloc->searched_for_live_refs == FALSE){
			found_live = walk_references(
			    SEARCH_FOR_LIVE,
			    ref->fine_reloc,
			    ref->map,
			    ref->obj);
			if(found_live == TRUE)
			    return(TRUE);
		    }
		}
	    }
	    else if(operation == CHECK_FOR_LIVE_TOUCH){
		if(ref->fine_reloc->live == TRUE && 
		   (ref->map->s->flags & S_ATTR_LIVE_SUPPORT) == 0)
		    return(TRUE);
		else
		    return(FALSE);
	    }
	    else{
		fatal("internal error: ref_operation() called with "
		      "unknown operation (%d)", operation);
	    }
	}
	else /* ref->merged_symbol != NULL */ {
	    if(operation == MARK_LIVE){
		ref->merged_symbol->live = TRUE;
		if((ref->merged_symbol->nlist.n_type & N_TYPE) == N_INDR){
		    indr_merged_symbol = (struct merged_symbol *)
				    ref->merged_symbol->nlist.n_value;
		    indr_merged_symbol->live = TRUE;
		}
	    }
	    else if(operation == SEARCH_FOR_LIVE){
		if(ref->merged_symbol->live == TRUE)
		    return(TRUE);
	    }
	    else if(operation == CHECK_FOR_LIVE_TOUCH){
		if(ref->merged_symbol->live == TRUE)
		    return(TRUE);
		else
		    return(FALSE);
	    }
	    else{
		fatal("internal error: ref_operation() called with "
		      "unknown operation (%d)", operation);
	    }
	    
	    if(ref->merged_symbol->defined_in_dylib == TRUE){
		if(operation == MARK_LIVE){
		    /*
		     * If this is defined in a dylib then we need to mark
		     * that module's referernces live in case the references
		     * loop back and reference something in the objects
		     * being loaded.
		    mark_dylib_references_live(ref->merged_symbol);
		     */
		    ;
		}
		else{ /* operation == SEARCH_FOR_LIVE */
/* need to add code to search through a dylib's references for live symbols */
		    ;
		}
	    }
	    else /* merged_symbol->defined_in_dylib == FALSE */ {
		ref_fine_reloc = ref->merged_symbol->fine_reloc;
		if((ref->merged_symbol->nlist.n_type & N_TYPE) == N_SECT){
		    n_sect = ref->merged_symbol->nlist.n_sect;
		    local_map = &(ref->merged_symbol->definition_object->
				  section_maps[n_sect - 1]);
		}
		else
		    local_map = NULL;
		if(ref_fine_reloc != NULL){
#ifdef DEBUG
		    if((((debug & (1 << 25)) || (debug & (1 << 26))) &&
		       ref_fine_reloc->live != TRUE)){
			print("** In ref_operation(%s) ",
			      walk_references_operation_names[operation]);
			print_obj_name(ref->merged_symbol->definition_object);
			if(local_map != NULL)
			    print("(%.16s,%.16s)", local_map->s->segname,
				  local_map->s->sectname);
			print(":%s\n", ref->merged_symbol->nlist.n_un.n_name);
		    }
#endif /* DEBUG */
		    if(operation == MARK_LIVE){
			ref_fine_reloc->live = TRUE;
			if(ref_fine_reloc->refs_marked_live == FALSE){
			    walk_references(
				MARK_LIVE,
				ref_fine_reloc,
				local_map,
				ref->merged_symbol->definition_object);
			}
		    }
		    else{ /* operation == SEARCH_FOR_LIVE */
			if(ref_fine_reloc->live == TRUE)
			    return(TRUE);
			if(ref_fine_reloc->searched_for_live_refs == FALSE){
			    found_live = walk_references(
				SEARCH_FOR_LIVE,
				ref_fine_reloc,
				local_map,
				ref->merged_symbol->definition_object);
			    if(found_live == TRUE)
				return(TRUE);
			}
		    }
		}
	    }
	}
	return(FALSE);
}

/*
 * r_symbolnum_from_r_value calculates the r_symbolnum (n_sect) from the 
 * specified r_value in the specified object_file.  If the r_value is not in
 * any section then 0 (NO_SECT) is returned.
 */
__private_extern__
unsigned long
r_symbolnum_from_r_value(
unsigned long r_value,
struct object_file *obj)
{
    unsigned i, r_symbolnum;

	r_symbolnum = 0;
	for(i = 0; i < obj->nsection_maps; i++){
	    if(r_value >= obj->section_maps[i].s->addr &&
	       r_value < obj->section_maps[i].s->addr +
			 obj->section_maps[i].s->size){
		r_symbolnum = i + 1;
		break;
	    }
	}
	if(r_symbolnum == 0){
	    /*
	     * The edge case where the last address past the end of
	     * of the last section is referenced.
	     */
	    for(i = 0; i < obj->nsection_maps; i++){
		if(r_value == obj->section_maps[i].s->addr +
			      obj->section_maps[i].s->size){
		    r_symbolnum = i + 1;
		    break;
		}
	    }
	}
	return(r_symbolnum);
}

#endif /* !defined(RLD) */

#ifdef RLD
/*
 * reset_merged_sections() is called from rld_load() to place the merged
 * sections back on their merged segment (layout() placed all of them on the
 * object_segment for the MH_OBJECT filetype) and it zeros the size of each the
 * merged section so it can be accumulated for the next rld_load().
 */
__private_extern__
void
reset_merged_sections(void)
{
    struct merged_segment *msg;
    struct merged_section *ms, *prev_ms;

	/*
	 * First add the debug segments back on the end of the list of the
	 * original merged segments.
	 */
	for(msg = original_merged_segments;
	    msg != NULL;
	    /* no increment expression */){
	    if(msg->next == NULL)
		break;
	    msg = msg->next;
	}
	if(msg != NULL)
	    msg->next = debug_merged_segments;

	msg = original_merged_segments;
	if(msg != NULL && merged_segments->content_sections != NULL){
	    ms = merged_segments->content_sections;
	    while(ms != NULL){
		if(strncmp(ms->s.segname, msg->sg.segname,
			   sizeof(msg->sg.segname)) == 0){
		    msg->content_sections = ms;
		    ms->s.size = 0;
		    prev_ms = ms;
		    ms = ms->next;
		    while(ms != NULL && strncmp(ms->s.segname, msg->sg.segname,
			   			sizeof(msg->sg.segname)) == 0){
			ms->s.size = 0;
			prev_ms = ms;
			ms = ms->next;
		    }
		    prev_ms->next = NULL;
		}
		else{
		    msg = msg->next;
		}
	    }
	}

	msg = original_merged_segments;
	if(msg != NULL && merged_segments->zerofill_sections != NULL){
	    ms = merged_segments->zerofill_sections;
	    while(ms != NULL){
		if(strncmp(ms->s.segname, msg->sg.segname,
			   sizeof(msg->sg.segname)) == 0){
		    msg->zerofill_sections = ms;
		    ms->s.size = 0;
		    prev_ms = ms;
		    ms = ms->next;
		    while(ms != NULL && strncmp(ms->s.segname, msg->sg.segname,
			   			sizeof(msg->sg.segname)) == 0){
			ms->s.size = 0;
			prev_ms = ms;
			ms = ms->next;
		    }
		    prev_ms->next = NULL;
		}
		else{
		    msg = msg->next;
		}
	    }
	}
	merged_segments = original_merged_segments;
	original_merged_segments = NULL;
}

/*
 * zero_merged_sections_sizes() is called from rld_load() to zero the size field
 * in the merged sections so the sizes can be accumulated and free the literal
 * data for any literal sections.  Also the alignment of the existing sections
 * is reset to zero.
 */
__private_extern__
void
zero_merged_sections_sizes(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **q, *ms;

	p = &merged_segments;
	while(*p){
	    msg = *p;
	    q = &(msg->content_sections);
	    while(*q){
		ms = *q;
		ms->s.size = 0;
		ms->s.align = 0;
		if(ms->literal_data != NULL){
		    if(ms->literal_free != NULL){
			(*ms->literal_free)(ms->literal_data, ms);
		    }
		}
		q = &(ms->next);
	    }
	    q = &(msg->zerofill_sections);
	    while(*q){
		ms = *q;
		ms->s.size = 0;
		ms->s.align = 0;
		q = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * remove_sections() removes the sections and segments that first came from the
 * current set from the merged section list.  The order that sections are
 * merged on to the lists is taken advantaged of here.
 */
__private_extern__
void
remove_merged_sections(void)
{
    struct merged_segment *msg, *prev_msg, *next_msg;
    struct merged_section *ms, *prev_ms, *next_ms;

	/* The compiler "warning: `prev_msg' and `prev_ms' may be used */
	/* uninitialized in this function" can safely be ignored */
	prev_msg = NULL;
	prev_ms = NULL;

	if(original_merged_segments != NULL)
	    reset_merged_sections();

	for(msg = merged_segments; msg != NULL; msg = msg->next){
	    /*
	     * If this segment first comes from the current set then all
	     * remaining segments also come from this set and all of their
	     * sections.  So they are all removed from the list.
	     */
	    if(msg->set_num == cur_set){
		if(msg == merged_segments)
		    merged_segments = NULL;
		else
		    prev_msg->next = NULL;
		while(msg != NULL){
		    ms = msg->content_sections;
		    while(ms != NULL){
			if(ms->literal_data != NULL){
			    if(ms->literal_free != NULL){
				(*ms->literal_free)(ms->literal_data, ms);
				free(ms->literal_data);
				ms->literal_data = NULL;
			    }
			}
			next_ms = ms->next;
			free(ms);
			ms = next_ms;
		    }
		    ms = msg->zerofill_sections;
		    while(ms != NULL){
			next_ms = ms->next;
			free(ms);
			ms = next_ms;
		    }
		    next_msg = msg->next;
		    free(msg);
		    msg = next_msg;
		}
		break;
	    }
	    else{
		/*
		 * This segment first comes from other than the current set
		 * so check to see in any of it's sections from from the 
		 * current set and if so remove them.  Again advantage of the
		 * order is taken so that if a section if found to come from
		 * the current set all remaining sections in that list also come
		 * from that set.
		 */
		for(ms = msg->content_sections; ms != NULL; ms = ms->next){
		    if(ms->set_num == cur_set){
			if(ms == msg->content_sections)
			    msg->content_sections = NULL;
			else
			    prev_ms->next = NULL;
			while(ms != NULL){
			    msg->sg.nsects--;
			    if(ms->literal_data != NULL)
				free(ms->literal_data);
			    next_ms = ms->next;
			    free(ms);
			    ms = next_ms;
			}
			break;
		    }
		    prev_ms = ms;
		}
		for(ms = msg->zerofill_sections; ms != NULL; ms = ms->next){
		    if(ms->set_num == cur_set){
			if(ms == msg->zerofill_sections)
			    msg->zerofill_sections = NULL;
			else
			    prev_ms->next = NULL;
			while(ms != NULL){
			    msg->sg.nsects--;
			    next_ms = ms->next;
			    free(ms);
			    ms = next_ms;
			}
			break;
		    }
		    prev_ms = ms;
		}
	    }
	    prev_msg = msg;
	}
}
#endif /* RLD */

#ifdef DEBUG
/*
 * print_merged_sections() prints the merged section table.  For debugging.
 */
__private_extern__
void
print_merged_sections(
char *string)
{
    struct merged_segment *msg;
    struct merged_section *ms;

	print("Merged section list (%s)\n", string);
	for(msg = merged_segments; msg ; msg = msg->next){
	    print("    Segment %.16s\n", msg->sg.segname);
	    print("\tcmd %u\n", msg->sg.cmd);
	    print("\tcmdsize %u\n", msg->sg.cmdsize);
	    print("\tvmaddr 0x%x ", (unsigned int)msg->sg.vmaddr);
	    print("(addr_set %s)\n", msg->addr_set ? "TRUE" : "FALSE");
	    print("\tvmsize 0x%x\n", (unsigned int)msg->sg.vmsize);
	    print("\tfileoff %u\n", msg->sg.fileoff);
	    print("\tfilesize %u\n", msg->sg.filesize);
	    print("\tmaxprot ");
	    print_prot(msg->sg.maxprot);
	    print(" (prot_set %s)\n", msg->prot_set ? "TRUE" : "FALSE");
	    print("\tinitprot ");
	    print_prot(msg->sg.initprot);
	    print("\n");
	    print("\tnsects %u\n", msg->sg.nsects);
	    print("\tflags %u\n", msg->sg.flags);
#ifdef RLD
	    print("\tset_num %lu\n", msg->set_num);
#endif /* RLD */
	    print("\tfilename %s\n", msg->filename);
	    print("\tcontent_sections\n");
	    for(ms = msg->content_sections; ms ; ms = ms->next){
		print("\t    Section (%.16s,%.16s)\n",
		       ms->s.segname, ms->s.sectname);
		print("\t\taddr 0x%x\n", (unsigned int)ms->s.addr);
		print("\t\tsize %u\n", ms->s.size);
		print("\t\toffset %u\n", ms->s.offset);
		print("\t\talign %u\n", ms->s.align);
		print("\t\tnreloc %u\n", ms->s.nreloc);
		print("\t\treloff %u\n", ms->s.reloff);
		print("\t\tflags %s\n",
		      section_flags[ms->s.flags & SECTION_TYPE]);
#ifdef RLD
		print("\t\tset_num %d\n", ms->set_num);
#endif /* RLD */
		if(ms->relocated == TRUE)
    		    print("\t    relocated TRUE\n");
		else
    		    print("\t    relocated FALSE\n");
		if(ms->referenced == TRUE)
    		    print("\t    referenced TRUE\n");
		else
    		    print("\t    referenced FALSE\n");
		if(ms->contents_filename){
		    print("\t    contents_filename %s\n",
			  ms->contents_filename);
		    print("\t    file_addr 0x%x\n",
			  (unsigned int)ms->file_addr);
		    print("\t    file_size %lu\n", ms->file_size);
		}
		if(ms->order_filename){
		    print("\t    order_filename %s\n",
			  ms->order_filename);
		    print("\t    order_addr 0x%x\n",
			  (unsigned int)ms->order_addr);
		    print("\t    order_size %lu\n", ms->order_size);
		}
		if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS)
		    print_cstring_data(ms->literal_data, "\t    ");
		if((ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS)
		    print_literal4_data(ms->literal_data, "\t    ");
		if((ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS)
		    print_literal8_data(ms->literal_data, "\t    ");
		if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS)
		    print_literal_pointer_data(ms->literal_data, "\t    ");
	    }
	    print("\tzerofill_sections\n");
	    for(ms = msg->zerofill_sections; ms ; ms = ms->next){
		print("\t    Section (%.16s,%.16s)\n",
		       ms->s.segname, ms->s.sectname);
		print("\t\taddr 0x%x\n", (unsigned int)ms->s.addr);
		print("\t\tsize %u\n", ms->s.size);
		print("\t\toffset %u\n", ms->s.offset);
		print("\t\talign %u\n", ms->s.align);
		print("\t\tnreloc %u\n", ms->s.nreloc);
		print("\t\treloff %u\n", ms->s.reloff);
		print("\t\tflags %s\n",
		      section_flags[ms->s.flags & SECTION_TYPE]);
#ifdef RLD
		print("\t\tset_num %lu\n", ms->set_num);
#endif /* RLD */
	    }
	}
}

/*
 * print_merged_section_stats() prints the stats for the merged sections.
 * For tuning..
 */
__private_extern__
void
print_merged_section_stats(void)
{
    struct merged_segment *msg;
    struct merged_section *ms;

	for(msg = merged_segments; msg ; msg = msg->next){
	    for(ms = msg->content_sections; ms ; ms = ms->next){
		if((ms->s.flags & SECTION_TYPE) == S_LITERAL_POINTERS)
		    literal_pointer_data_stats(ms->literal_data, ms);
		else if((ms->s.flags & SECTION_TYPE) == S_CSTRING_LITERALS)
		    cstring_data_stats(ms->literal_data, ms);
		else if((ms->s.flags & SECTION_TYPE) == S_4BYTE_LITERALS)
		    literal4_data_stats(ms->literal_data, ms);
		else if((ms->s.flags & SECTION_TYPE) == S_8BYTE_LITERALS)
		    literal8_data_stats(ms->literal_data, ms);
	    }
	}
}

/*
 * print_load_order() prints the load_order array passed to it.
 * For debugging.
 */
__private_extern__
void
print_load_order(
struct load_order *load_order,
unsigned long nload_order,
struct merged_section *ms,
struct object_file *object_file,
char *string)
{
    unsigned long i;

	print("Load order 0x%x %lu entries for (%.16s,%.16s) of ",
	      (unsigned int)load_order, nload_order,
	      ms->s.segname, ms->s.sectname);
	print_obj_name(object_file);
	print("(%s)\n", string);
	for(i = 0; i < nload_order; i++){
	    print("entry[%lu]\n", i);
	    print("           name %s\n", load_order[i].name == NULL ? "null" :
		  load_order[i].name);
	    print("          value 0x%08x\n",(unsigned int)load_order[i].value);
	    print("          order %lu\n", load_order[i].order);
	    print("   input_offset %lu\n", load_order[i].input_offset);
	    print("     input_size %lu\n", load_order[i].input_size);
	    print("  output_offset %lu\n", load_order[i].output_offset);
	}
}

/*
 * print_name_arrays() prints the sorted arrays of archive and object names.
 * For debugging.
 */
__private_extern__
void
print_name_arrays(void)
{
    unsigned long i, j;

	print("Sorted archive names:\n");
	for(i = 0; i < narchive_names; i++){
	    print("    archive name %s\n", archive_names[i].archive_name);
	    print("    number of objects %lu\n",archive_names[i].nobject_names);
	    print("    Sorted object names:\n");
	    for(j = 0; j < archive_names[i].nobject_names; j++){
		print("\tobject name %s\n",
		      archive_names[i].object_names[j].object_name);
		print("\tlength %lu\n",
		      archive_names[i].object_names[j].index_length);
		print("\tobject file 0x%x ", (unsigned int)
		      (archive_names[i].object_names[j].object_file));
		print_obj_name(archive_names[i].object_names[j].object_file);
		print("\n");
	    }
	}
	print("Sorted object names:\n");
	for(j = 0; j < nobject_names; j++){
	    print("\tobject name %s\n", object_names[j].object_name);
	    print("\tindex %lu\n", object_names[j].index_length);
	    print("\tobject file 0x%x ",
		  (unsigned int)(object_names[j].object_file));
	    print_obj_name(object_names[j].object_file);
	    print("\n");
	}
}

static
void
print_load_symbol_hash_table(void)
{
    unsigned long i;
    struct load_symbol *load_symbol, *other_name;

	print("load_symbol_hash_table:\n");
	if(load_symbol_hashtable == NULL)
	    return;
	for(i = 0; i < LOAD_SYMBOL_HASHTABLE_SIZE; i++){
	    if(load_symbol_hashtable[i] != NULL)
		print("[%lu]\n", i);
	    for(load_symbol = load_symbol_hashtable[i]; 
	        load_symbol != NULL;
	        load_symbol = load_symbol->next){
		print("load symbol: %lu\n", i);
		if(load_symbol->archive_name != NULL){
		    print("    (%s:%s:%s) length %lu\n",
			  load_symbol->archive_name,
			  load_symbol->object_name,
			  load_symbol->symbol_name,
			  load_symbol->index_length);
		}
		else{
		    print("    (%s:%s) index %lu\n",
			  load_symbol->object_name,
			  load_symbol->symbol_name,
			  load_symbol->index_length);
		}
		print("    load_order 0x%x\n",
		      (unsigned int)(load_symbol->load_order));
		print("    other_names 0x%x\n",
		      (unsigned int)(load_symbol->other_names));
		print("    next 0x%x\n",
		      (unsigned int)(load_symbol->next));
		for(other_name = load_symbol->other_names;
		    other_name != NULL;
		    other_name = other_name->other_names){
		    print("other name\n");
		    if(other_name->archive_name != NULL){
			print("    (%s:%s:%s) length %lu\n",
			      other_name->archive_name,
			      other_name->object_name,
			      other_name->symbol_name,
			      other_name->index_length);
		    }
		    else{
			print("    (%s:%s) index %lu\n",
			      other_name->object_name,
			      other_name->symbol_name,
			      other_name->index_length);
		    }
		    print("    load_order 0x%x\n",
			  (unsigned int)(other_name->load_order));
		    print("    other_names 0x%x\n",
			  (unsigned int)(other_name->other_names));
		    print("    next 0x%x\n",
			  (unsigned int)(load_symbol->next));
		}
	    }
	}
}
#endif /* DEBUG */
