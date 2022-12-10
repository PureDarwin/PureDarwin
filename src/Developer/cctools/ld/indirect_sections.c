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
 * This file contains the routines that deal with indirect sections (both
 * lazy and non-lazy symbol pointer sections as well as symbol stub sections). 
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
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/arch.h"
#include "stuff/reloc.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "pass2.h"
#include "generic_reloc.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "indirect_sections.h"
#include "dylibs.h"

/*
 * The number of indirect symbol table entries in the output file.
 */
__private_extern__ unsigned long nindirectsyms = 0;

/*
 * If we are still attempting to prebind when indirect_section_merge() is
 * called save_lazy_symbol_pointer_relocs will get set in layout.c.
 * Between the time indirect_section_merge() gets called and the time
 * reloc_output_for_dyld() gets called prebinding may be disabled because of
 * various problems.  But the count of relocs can't change after layout so
 * we'll put them out anyway.
 */
__private_extern__ enum bool save_lazy_symbol_pointer_relocs = FALSE;

#ifndef SA_RLD

static unsigned long lookup_indirect_item(
    struct merged_symbol *merged_symbol,
    struct object_file *obj,
    unsigned long index,
    struct indirect_section_data *data, 
    unsigned long stride,
    enum bool *new);

/*
 * indirect_section_merge() merges items from symbol pointers and symbol stub
 * sections from the specified section in the current object file (cur_obj).
 * When redo_live is FALSE it allocates a fine relocation map and sets the
 * fine_relocs field in the section_map to it (as well as the count).
 *
 * When redo_live is FALSE after all the items for this section in this object
 * file have been merged two more things are done.  First the number of
 * relocation entries that will be in the output file is adjusted (incremented)
 * based on which items are used from this object's section.  Second the number
 * of local symbol table entries and the size of the string table is adjusted
 * (decremented) based on the which symbols are in the items from this object's
 * section that will be in the resulting object file.
 *
 * When redo_live is TRUE it re-merges only the live items from symbol pointers
 * and symbol stub sections from the specified section in the current object
 * file (cur_obj).
 */
__private_extern__
void
indirect_section_merge(
struct indirect_section_data *data, 
struct merged_section *ms,
struct section *s, 
struct section_map *section_map,
enum bool redo_live)
{
    unsigned long i, j, stride, section_type, nitems, index;
    struct fine_reloc *fine_relocs;
    struct nlist *nlists;
    unsigned long *indirect_symtab;
    struct undefined_map *undefined_map;
    struct merged_symbol *merged_symbol, *indr_symbol;
    enum bool new;

    struct relocation_info *relocs, reloc;
    struct scattered_relocation_info *sreloc, *spair_reloc;
    unsigned long r_address, r_pcrel, r_length, r_type, pair_r_type, r_extern,
		  r_symbolnum, r_scattered, r_value, pair;
#ifndef RLD
    enum bool pic;
#endif
    enum bool defined, force_extern_reloc;
    unsigned long nsect;
    char *strings;

	/* to shut up compiler warning messages "may be used uninitialized" */
	merged_symbol = NULL;

	if(s->size == 0)
	    return;

	stride = 0;
	pair_r_type = 0;
	section_type = s->flags & SECTION_TYPE;
	if(section_type == S_LAZY_SYMBOL_POINTERS ||
	   section_type == S_NON_LAZY_SYMBOL_POINTERS){
	    stride = 4;
	}
	else if(section_type == S_SYMBOL_STUBS)
	    stride = s->reserved2;
	else
	    fatal("internal error, in indirect_section_merge() section type "
		  "(%lu) is not correct for an indirect section", section_type);
	nitems = s->size / stride;

	if(s->size % stride != 0){
	    error_with_cur_obj("malformed object (section (%.16s,%.16s) size "
		"is not a multiple of %lu bytes)", s->segname, s->sectname,
		stride);
	    return;
	}
	if(cur_obj->dysymtab == NULL){
	    error_with_cur_obj("malformed object (file has indirect section "
		"(%.16s,%.16s) but no dysymtab_command)", s->segname,
		s->sectname);
	    return;
	}
	if(s->reserved1 > cur_obj->dysymtab->nindirectsyms){
	    error_with_cur_obj("malformed object (index into indirect symbol "
		"table (reserved1 field) for indirect section (%.16s,%.16s) "
		"past the end of the table)", s->segname, s->sectname);
	    return;
	}
	if(s->reserved1 + nitems > cur_obj->dysymtab->nindirectsyms){
	    error_with_cur_obj("malformed object (indirect symbol table entries"
		"for section (%.16s,%.16s) extends past the end of the table)",
		s->segname, s->sectname);
	    return;
	}
	if(s->size == 0){
	    if(redo_live == FALSE){
		section_map->fine_relocs = NULL;
		section_map->nfine_relocs = 0;
	    }
	    return;
	}
#ifdef DEBUG
	if(redo_live == FALSE){
	    data->nfiles++;
	    data->nitems += nitems;
	}
#endif /* DEBUG */

	/*
	 * First deal with the contents of the indirect section and determine
	 * based on the indirect symbol for each item in the section if the
	 * contents 1) will in the output and used from this object, 2) used
	 * from a previous merged object or 3) the value of the indirect symbol
	 * will be used instead of the contents of this item.  This information
	 * is encoded into the fine_reloc structures for each item.
	 */
	/* setup pointers to the symbol, indirect symbol and string tables */
	nlists = (struct nlist *)(cur_obj->obj_addr +
				  cur_obj->symtab->symoff);
	indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
	strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
	if(redo_live == FALSE){
	    fine_relocs = allocate(nitems * sizeof(struct fine_reloc));
	    memset(fine_relocs, '\0', nitems * sizeof(struct fine_reloc));
	}
	else{
	    fine_relocs = section_map->fine_relocs;
	    nitems = section_map->nfine_relocs;
	}
	for(i = 0; i < nitems; i++){
	    /* get the indirect symbol table entry for this item */
	    index = indirect_symtab[s->reserved1 + i];
	    /*
	     * With strip(1), nmedit(l) or and "ld(1) -r with private externs
	     * index could be INDIRECT_SYMBOL_LOCAL or INDIRECT_SYMBOL_ABS.
	     */
	    if(index == INDIRECT_SYMBOL_LOCAL ||
	       index == INDIRECT_SYMBOL_ABS){
		if(redo_live == FALSE){
		    fine_relocs[i].local_symbol = TRUE;
		    fine_relocs[i].indirect_defined = FALSE;
		    fine_relocs[i].use_contents = TRUE;
		    fine_relocs[i].input_offset = i * stride;
		    fine_relocs[i].merged_symbol = NULL;
		    if(index == INDIRECT_SYMBOL_LOCAL)
			fine_relocs[i].indirect_symbol_local = TRUE;
		}
		if(redo_live == FALSE || fine_relocs[i].live == TRUE){
		    fine_relocs[i].output_offset = lookup_indirect_item(
				NULL, cur_obj, index, data, stride, &new);
		}
		else if(redo_live == TRUE && fine_relocs[i].live == FALSE){
		    fine_relocs[i].use_contents = FALSE;
		}
		goto account_for_size;
	    }

	    /*
	     * Since the indirect symbol table entry should be for an undefined
	     * symbol use the current object's undefined symbol map to get the
	     * merged symbol.
	     */
	    undefined_map = bsearch(&index, cur_obj->undefined_maps,
		cur_obj->nundefineds, sizeof(struct undefined_map),
		    (int (*)(const void *, const void *))undef_bsearch);
	    if(undefined_map != NULL){
		merged_symbol = undefined_map->merged_symbol;
		/*
		 * If the indirect symbol table entry is for a private extern
		 * it is an error.
		 */
		if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
		   (merged_symbol->nlist.n_type & N_EXT) != N_EXT &&
		   section_type == S_NON_LAZY_SYMBOL_POINTERS &&
		   output_for_dyld == FALSE)
		    fatal("indirect symbol:%s can not be a private extern",
			  strings + nlists[index].n_un.n_strx);
	    }
	    else if((nlists[index].n_type & N_EXT) == N_EXT){
		/*
		 * The indirect symbol table entry was not for an undefined but
		 * it is an external symbol so get the merged_symbol for this
		 * external symbol by looking it up by name.
		 */
		merged_symbol = lookup_symbol(strings +
						nlists[index].n_un.n_strx);
		if(merged_symbol->name_len == 0)
		    fatal("interal error, indirect_section_merge() failed in "
			  "looking up external symbol");
		/*
		 * If this is an indirect symbol resolve indirection (all chains
		 * of indirect symbols have been resolved so that they point at
		 * a symbol that is not an indirect symbol).
		 */
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		/*
		 * If the indirect symbol table entry is for a private extern
		 * it is an error.
		 */
		if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
		   (merged_symbol->nlist.n_type & N_EXT) != N_EXT &&
		   section_type == S_NON_LAZY_SYMBOL_POINTERS &&
		   output_for_dyld == FALSE)
		    fatal("indirect symbol:%s can not be a private extern",
			  strings + nlists[index].n_un.n_strx);
	    }
	    else if((nlists[index].n_type & N_PEXT) == N_PEXT &&
		    (nlists[index].n_type & N_EXT) != N_EXT &&
		    section_type == S_NON_LAZY_SYMBOL_POINTERS &&
		    output_for_dyld == FALSE)
		/*
		 * If the indirect symbol table entry is for a private extern
		 * it is an error.
		 */
		fatal("indirect symbol:%s can not be a private extern",
		      strings + nlists[index].n_un.n_strx);
	    else if((nlists[index].n_type & N_STAB) == 0){
		/*
		 * The indirect symbol table entry was not even an external
		 * symbol but it is a local defined symbol.  So the symbol table
		 * index will be use in place of the merged_symbol.  This is
		 * allowed on input only for stub sections and won't be created
		 * on output.
		 */
		merged_symbol = NULL;
	    }
	    else{
		/*
		 * The indirect symbol table entry was a stab, this is an error.
		 */
		error_with_cur_obj("malformed object (bad indirect symbol "
		    "table entry (%ld) (symbol at index %lu is a stab)",
		    s->reserved1 + i, index);
		return;
	    }
	    /*
	     * Now set the values in the fine relocation entry for this item
	     * based on the type of section and the merged symbol for this
	     * item's indirect symbol and if the output file is a dynamic
	     * shared library file.
	     */
	    if(redo_live == FALSE)
		fine_relocs[i].input_offset = i * stride;
	    if(merged_symbol == NULL){
		/*
		 * Indirect table entries which refer to local defined symbols
		 * are allowed only in symbol stub and lazy pointer sections so
		 * they can always be removed and never created on output.
		 */
		if(section_type == S_NON_LAZY_SYMBOL_POINTERS){
		    if((nlists[index].n_type & N_PEXT) == 0){
			error_with_cur_obj("malformed object (bad indirect "
			    "symbol table entry (%ld) (symbol at index %lu is "
			    "not external)", s->reserved1 + i, index);
			return;
		    }
		    /*
		     * A non-lazy symbol pointer for a private extern that no
		     * longer is external.
		     */
		    if(redo_live == FALSE){
			fine_relocs[i].local_symbol = TRUE;
			fine_relocs[i].indirect_defined = TRUE;
			fine_relocs[i].use_contents = TRUE;
			fine_relocs[i].merged_symbol = NULL;
		    }
		    if(redo_live == FALSE || fine_relocs[i].live == TRUE){
			fine_relocs[i].output_offset = lookup_indirect_item(
				    NULL, cur_obj, index, data, stride, &new);
		    }
		    else if(redo_live == TRUE && fine_relocs[i].live == FALSE){
			fine_relocs[i].use_contents = FALSE;
		    }
		}
		else{
		    if(redo_live == FALSE){
			fine_relocs[i].local_symbol = TRUE;
			fine_relocs[i].indirect_defined = TRUE;
			fine_relocs[i].use_contents = FALSE;
			fine_relocs[i].output_offset = index;
			fine_relocs[i].merged_symbol = NULL;
		    }
		}
	    }
	    else{
		/*
		 * If this is an indirect symbol resolve indirection (all chains
		 * of indirect symbols have been resolved so that they point at
		 * a symbol that is not an indirect symbol).
		 */
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR &&
		   merged_symbol->defined_in_dylib == FALSE){
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
		    merged_symbol = indr_symbol;
		}

		if(redo_live == FALSE)
		    fine_relocs[i].local_symbol = FALSE;

		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		   merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		   (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		    merged_symbol->defined_in_dylib == TRUE)){
		    if(redo_live == FALSE){
			fine_relocs[i].indirect_defined = FALSE;
		    }
		}
		else if((merged_symbol->nlist.n_type & N_TYPE) == N_SECT &&
			(merged_symbol->definition_object->section_maps[
			 merged_symbol->nlist.n_sect - 1].
			 s->flags & SECTION_TYPE) == S_COALESCED &&
			filetype != MH_DYLINKER){
		    if((output_for_dyld &&
		       (has_dynamic_linker_command || filetype == MH_BUNDLE) &&
		       (((merged_symbol->nlist.n_desc & N_WEAK_DEF) !=
			 N_WEAK_DEF) ||
		       ((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
			keep_private_externs == FALSE) ) ) ||
		       (filetype == MH_DYLIB && multi_module_dylib == FALSE &&
			(merged_symbol->nlist.n_type & N_PEXT) == N_PEXT) ){
			if(redo_live == FALSE)
			    fine_relocs[i].indirect_defined = TRUE;
		    }
		    else{
			if(redo_live == FALSE)
			    fine_relocs[i].indirect_defined = FALSE;
		    }
		}
		else{
		    if(redo_live == FALSE)
			fine_relocs[i].indirect_defined = TRUE;
		}

		if((merged_symbol->nlist.n_type & N_TYPE) == N_ABS)
		    section_map->absolute_indirect_defineds = TRUE;

		if((filetype == MH_DYLIB && multi_module_dylib == TRUE) ||
		   section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		   fine_relocs[i].indirect_defined == FALSE){
		    if(redo_live == FALSE || fine_relocs[i].live == TRUE){
			fine_relocs[i].output_offset = lookup_indirect_item(
				    merged_symbol, NULL, 0, data, stride, &new);
		    }
		    else if(redo_live == TRUE && fine_relocs[i].live == FALSE){
			new = FALSE;
		    }
		    fine_relocs[i].use_contents = new;
		    if(redo_live == FALSE)
			fine_relocs[i].merged_symbol = merged_symbol;
		}
		else{
		    if(redo_live == FALSE){
			fine_relocs[i].use_contents = FALSE;
			fine_relocs[i].merged_symbol = merged_symbol;
		    }
		}
	    }

account_for_size:
	    /*
	     * If this item's contents will be used in the output file account
	     * for it's size in the merged section and it's entry in the
	     * indirect symbol table.
	     */
	    if(fine_relocs[i].use_contents){
		/*
		 * When -dead_strip is specified this will be called a second
		 * time and redo_live will be TRUE.  If so only account for this
		 * item if it is live.
		 */
		if(redo_live == FALSE || fine_relocs[i].live == TRUE){
		    ms->s.size += stride;
		    nindirectsyms++;
		}
	    }
	}
	if(redo_live == FALSE){
	    section_map->fine_relocs = fine_relocs;
	    section_map->nfine_relocs = nitems;
	}

	/*
	 * Second deal with the relocation entries for the section in this
	 * object file.  Now that it has been determined for which items the 
	 * contents will be used from this object file.
	 */
	/*
 	 * A lazy symbol pointer section must have one relocation entry for each
	 * pointer.  A non-lazy symbol pointer section can't have any relocation
	 * entries (this was checked in check_cur_obj() before getting here).
	 * And a symbol stub section may have many relocation entries.
	 */
	if(section_type == S_LAZY_SYMBOL_POINTERS && s->nreloc != nitems){
	    error_with_cur_obj("malformed object (lazy symbol pointer section "
		"(%.16s,%.16s) does not have is exactly one relocation entry "
		"for each pointer)", s->segname, s->sectname);
	    return;
	}
	/*
	 * This loop loops through the relocation entries and using the
	 * use_contents field (via a call to fine_reloc_offset_in_output())
	 * of the fine_relocs just created determines how many relocation
	 * entries will be in the output for this section of this object file.
	 */
	relocs = (struct relocation_info *)(cur_obj->obj_addr + s->reloff);
	for(i = 0; i < s->nreloc; i++){
	    reloc = relocs[i];
	    if(cur_obj->swapped &&
	       section_map->input_relocs_already_swapped == FALSE)
		swap_relocation_info(&reloc, 1, host_byte_sex);
	    /*
	     * Break out the fields of the relocation entry we need here.
	     */
	    if((reloc.r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(&reloc);
		r_scattered = 1;
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_length = sreloc->r_length;
		r_value = sreloc->r_value;
		r_type = sreloc->r_type;
		r_extern = 0;

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
			error_with_cur_obj("r_value (0x%x) field of relocation "
			    "entry %lu in section (%.16s,%.16s) out of range",
			    (unsigned int)r_value, i, section_map->s->segname,
			    section_map->s->sectname);
			return;
		    }
		}
	    }
	    else{
		r_scattered = 0;
		r_address = reloc.r_address;
		r_pcrel = reloc.r_pcrel;
		r_length = reloc.r_length;
		r_type = reloc.r_type;
		r_extern = reloc.r_extern;
		r_symbolnum = reloc.r_symbolnum;
	    }
	    /*
	     * Make sure that this is not a stray PAIR relocation entry.
	     */
	    if(r_type == reloc_pair_r_type(arch_flag.cputype)){
		error_with_cur_obj("malformed object (stray relocation PAIR "
		    "entry (%lu) in section (%.16s,%.16s))", i, s->segname,
		    s->sectname);
		continue;
	    }
	    /*
	     * The r_address field is really an offset into the contents of the
	     * section and must reference something inside the section.
	     */
	    if(r_address >= s->size){
		error_with_cur_obj("malformed object (r_address (0x%x) field "
		    "of relocation entry %ld in section (%.16s,%.16s) out of "
		    "range)",(unsigned int)r_address, i,s->segname,s->sectname);
		continue;
	    }
	    /*
	     * If this relocation entry is suppose to have a PAIR make sure it
	     * does.
	     */
	    if(reloc_has_pair(arch_flag.cputype, r_type)){
		if(i + 1 < s->nreloc){
		    reloc = relocs[i + 1];
		    if(cur_obj->swapped &&
		       section_map->input_relocs_already_swapped == FALSE)
			swap_relocation_info(&reloc, 1, host_byte_sex);
		    if((reloc.r_address & R_SCATTERED) != 0){
			spair_reloc = (struct scattered_relocation_info *)
                                      &reloc;
                        pair_r_type = spair_reloc->r_type;
		    }
		    else{
                        pair_r_type = reloc.r_type;
		    }
		}
		if(i + 1 >= s->nreloc ||
		   pair_r_type != reloc_pair_r_type(arch_flag.cputype)){
		    error_with_cur_obj("malformed object (relocation entry "
			"(%lu) in section (%.16s,%.16s) missing following "
			"associated PAIR entry)", i, s->segname, s->sectname);
                    continue;
                }
	    }
	    /*
	     * For a lazy symbol pointer section all relocation entries must be
	     * for one of the pointers or an external relocation entry and
	     * therefore the offset must be a multiple of 4 (the size of a
	     * pointer), have an r_length field of 2 (long) and a r_pcrel field
	     * of 0 (FALSE).
	     */
	    if(section_type == S_LAZY_SYMBOL_POINTERS){
		if(r_address % 4 != 0){
		    error_with_cur_obj("malformed object, illegal reference "
			"(r_address (0x%x) field of relocation entry %lu in "
			"lazy symbol pointer section (%.16s,%.16s) is not a "
			"multiple of 4)", (unsigned int)r_address, i,
			s->segname, s->sectname);
		    continue;
		}
		if(r_length != 2){
		    error_with_cur_obj("malformed object, illegal reference "
			"(r_length (0x%x) field of relocation entry %lu in "
			"lazy symbol pointer section (%.16s,%.16s) is not 2 "
			"(long))", (unsigned int)r_length, i, s->segname,
			s->sectname);
		    continue;
		}
		if(r_pcrel != 0){
		    error_with_cur_obj("malformed object, illegal reference "
			"(r_pcrel (0x%x) field of relocation entry %lu in lazy "
			"symbol pointer section (%.16s,%.16s) is not 0 "
			"(FALSE))", (unsigned int)r_pcrel, i, s->segname,
			s->sectname);
		    continue;
		}
		if(r_type != 0){
		    error_with_cur_obj("malformed object, illegal reference "
			"(r_type (0x%x) field of relocation entry %lu in lazy "
			"symbol pointer section (%.16s,%.16s) is not 0 "
			"(VANILLA))", (unsigned int)r_type, i, s->segname,
			s->sectname);
		    continue;
		}
		if(r_scattered != 0){
		    error_with_cur_obj("malformed object, illegal reference "
			"(relocation entry %lu in lazy symbol pointer section "
			"(%.16s,%.16s) is a scattered type)", i, s->segname,
			s->sectname);
		    continue;
		}
		if(r_extern == 0 && r_symbolnum == R_ABS){
	    	    error_with_cur_obj("malformed object, illegal reference "
			"(reference from a lazy symbol pointer section (%.16s,"
			"%.16s) relocation entry (%lu) which is not to a "
			"symbol stub section)", s->segname, s->sectname, i);
		    continue;
		}
	    }
	    /*
	     * Assumed the symbol for this relocation entry is defined (always
	     * true for local relocation entries).  Then reset the variable
	     * "defined" correctly if this is an external relocation entry based
	     * on if the symbol is defined, where it is defined and the output
	     * file type.
	     */
	    defined = TRUE;
	    force_extern_reloc = FALSE;
	    if(output_for_dyld && r_extern){
		/*
		 * This is an external relocation entry.  So the value to be
		 * added to the item to be relocated is the value of the symbol.
		 * r_symbolnum is an index into the input file's symbol table
		 * of the symbol being refered to.  The symbol must be an
		 * undefined or coalesced symbol to be used in an external
		 * relocation entry.
		 */
		if(r_symbolnum >= cur_obj->symtab->nsyms){
		    error_with_cur_obj("r_symbolnum (%lu) field of external "
			"relocation entry %lu in section (%.16s,%.16s) out of "
			"range", r_symbolnum, i, s->segname, s->sectname);
		    continue;
		}
		undefined_map = bsearch(&r_symbolnum,
		    cur_obj->undefined_maps, cur_obj->nundefineds,
		    sizeof(struct undefined_map),
		    (int (*)(const void *, const void *))undef_bsearch);
		if(undefined_map != NULL){
		    merged_symbol = undefined_map->merged_symbol;
		}
		else{
		    if((nlists[r_symbolnum].n_type & N_EXT) != N_EXT){
			error_with_cur_obj("r_symbolnum (%lu) field of "
			    "external relocation entry %lu in section "
			    "(%.16s,%.16s) refers to a non-external symbol",
			     r_symbolnum, i, s->segname, s->sectname);
			continue;
		    }
		    /*
		     * We must allow and create references to defined global
		     * coalesced symbols with external relocation entries so
		     * that the dynamic linker can relocate all references
		     * to the same symbol.
		     */
		    if((nlists[r_symbolnum].n_type & N_TYPE) == N_SECT &&
		       (cur_obj->section_maps[nlists[r_symbolnum].
			n_sect-1].s->flags & SECTION_TYPE) == S_COALESCED){
			merged_symbol = lookup_symbol(strings +
					   nlists[r_symbolnum].n_un.n_strx);
			if(merged_symbol->name_len == 0){
			    fatal("internal error, in indirect_section_merge() "
				  "failed to lookup coalesced symbol %s",
				  strings + nlists[r_symbolnum].n_un.n_strx);
			}
			if(((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
			    keep_private_externs == FALSE) ||
			   dynamic == FALSE ||
			   (output_for_dyld && has_dynamic_linker_command))
			    force_extern_reloc = FALSE;
			else
			    force_extern_reloc = TRUE;
		    }
		    else{
			if(nlists[r_symbolnum].n_type != (N_EXT | N_UNDF)){
			    error_with_cur_obj("r_symbolnum (%lu) field of "
				"external relocation entry %lu in section "
				"(%.16s,%.16s) refers to a non-undefined "
				"symbol", r_symbolnum, i,
				section_map->s->segname,
				section_map->s->sectname);
			    return;
			}
			print_obj_name(cur_obj);
			fatal("internal error, in indirect_section_merge()"
			    " symbol index %lu in above file not in "
			    "undefined map", r_symbolnum);
		    }
		}
		/*
		 * If this is an indirect symbol resolve indirection (all chains
		 * of indirect symbols have been resolved so that they point at
		 * a symbol that is not an indirect symbol).
		 */
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		/*
		 * For multi module dynamic shared library format files the
		 * merged sections that could have had external relocation
		 * entries must be resolved to private extern symbols.  This is
		 * because for multi module MH_DYLIB files all modules share the
		 * merged sections and the entire section gets relocated when
		 * the library is mapped in. So the above restriction assures
		 * the merged section will get relocated properly and can be
		 * shared amoung library modules.
		 */
		if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		    /*
		     * If the symbol is undefined or not a private extern it is
		     * an error for in this section for a MH_DYLIB file.
		     */
		    if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		       merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		       (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
			merged_symbol->defined_in_dylib == TRUE)){
			if(merged_symbol->error_flagged_for_dylib == 0){
			    error_with_cur_obj("illegal undefined reference "
				"for multi module MH_DYLIB output file to "
				"symbol: %s from section (%.16s,%.16s) "
				"relocation entry: %lu",
				merged_symbol->nlist.n_un.n_name, s->segname,
				s->sectname, i);
			    merged_symbol->error_flagged_for_dylib = 1;
			}
		    }
		    else if((merged_symbol->nlist.n_type & N_PEXT) != N_PEXT){
			if(merged_symbol->error_flagged_for_dylib == 0){
			    error_with_cur_obj("illegal external reference for "
				"multi module MH_DYLIB output file to symbol: "
				"%s (not a private extern symbol) from section "
				"(%.16s,%.16s) relocation entry: %lu",
				merged_symbol->nlist.n_un.n_name,
				s->segname, s->sectname, i);
			    merged_symbol->error_flagged_for_dylib = 1;
			}
		    }
		}
		else{
		    if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		       merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		       (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		        merged_symbol->defined_in_dylib == TRUE))
			defined = FALSE;
		    else
			defined = TRUE;
		}
	    }
	    if(reloc_has_pair(arch_flag.cputype, r_type))
		pair = 1;
	    else
		pair = 0;
#ifndef RLD
	    /*
	     * If saving relocation entries see if this relocation entry is for 
	     * an item that is going to be in the output file and if so count it
	     * as one of the output relocation entries.
	     */
	    if(output_for_dyld &&
	       fine_reloc_offset_in_output(section_map, r_address)){
		/*
		 * Mark this section as being relocated (staticly).
		 */
		if(dead_strip == FALSE || redo_live == TRUE)
		    ms->relocated = TRUE;
		if(r_extern == 0)
		    pic = (enum bool)
			  (reloc_is_sectdiff(arch_flag.cputype, r_type) ||
			   (r_pcrel == 1 && r_symbolnum != NO_SECT));
		else
		    pic = (enum bool)
			  (r_pcrel == 1 &&
			   (merged_symbol->nlist.n_type & N_TYPE) == N_SECT);
		/*
		 * The number of relocation entries in the output file is based
		 * on one of three different cases:
		 *  The output file is a multi module dynamic shared library
		 *  The output file has a dynamic linker load command
		 *  The output does not have a dynamic linker load command
		 */
		if(filetype == MH_DYLIB && multi_module_dylib == TRUE){
		    /*
		     * For multi module dynamic shared library files there are
		     * no external relocation entries that will be left as
		     * external as checked above.  Only non-position-independent
		     * local relocation entries are kept.  Modules of multi
		     * module dylibs are not linked together and can only be
		     * slid keeping all sections relative to each other the
		     * same.
		     */
		    if(pic == FALSE)
			ms->nlocrel += 1 + pair;
		}
		else if(has_dynamic_linker_command){
		    /*
		     * For an file with a dynamic linker load command only
		     * external relocation entries for undefined symbols are
		     * kept.  This output file is a fixed address and can't be
		     * moved.
		     */
		    if(r_extern){
			if(defined == FALSE)
			    ms->nextrel += 1 + pair;
			/*
			 * As of the PowerPC port, relocation entries for
			 * lazy symbol pointers can be external so when the
			 * the symbol is defined if we are doing prebinding and
			 * this is for a lazy symbol pointer then this will
			 * turn into a local relocation entry and we save it
			 * so we can undo the prebinding if needed.
			 */
			else if(save_lazy_symbol_pointer_relocs == TRUE &&
				section_type == S_LAZY_SYMBOL_POINTERS)
			    ms->nlocrel += 1 + pair;
		    }
		    /*
		     * Even though the file can't be moved we may be trying to
		     * prebind.  If we are prebinging we need the local
		     * relocation entries for lazy symbol pointers to be saved
		     * so dyld will have the info to undo this if it fails.
		     */
		    else if(save_lazy_symbol_pointer_relocs == TRUE &&
			    section_type == S_LAZY_SYMBOL_POINTERS){
			ms->nlocrel += 1 + pair;
		    }
		}
		else{
		    /*
		     * For an file without a dynamic linker load command
		     * external relocation entries for undefined symbols are
		     * kept and locals that are non-position-independent are
		     * kept.  This file can only be slid keeping all sections
		     * relative to each other the same.
		     */
		    if(r_extern){
			if(defined == FALSE || force_extern_reloc == TRUE)
			    ms->nextrel += 1 + pair;
			else if(pic == FALSE)
			    ms->nlocrel += 1 + pair;
		    }
		    else if(pic == FALSE)
			ms->nlocrel += 1 + pair;
		}
	    }
	    else if(save_reloc &&
	            fine_reloc_offset_in_output(section_map, r_address)){
		ms->s.nreloc += 1 + pair;
		nreloc += 1 + pair;
	    }
#endif /* !defined(RLD) */
	    i += pair;
	}
	/*
	 * If the the number of relocation entries is not zero mark this section
	 * as being relocated (staticly).
	 */
	if(ms->s.nreloc != 0){
	    if(dead_strip == FALSE || redo_live == TRUE)
		ms->relocated = TRUE;
	}

	/*
	 * Third deal with the symbol table entries for local symbols and N_STAB
	 * symbols in this section in this object file.  Now that it has been
	 * determined for which items the contents will be used from this
	 * object file.  If when -dead_strip is specified we have to wait to
	 * do this until we are called a second time when redo_live is TRUE and
	 * all the fine_relocs have had their live field set.
	 */
	if(dead_strip == FALSE || redo_live == TRUE){
	    /* determine the section number this has in this object */
	    for(nsect = 0; nsect < cur_obj->nsection_maps; nsect++)
		if(s == cur_obj->section_maps[nsect].s)
		    break;
	    nsect++; /* section numbers start at 1 (not zero) */
	    /* set up a pointer to the string table */
	    strings = (char *)(cur_obj->obj_addr + cur_obj->symtab->stroff);
	    discard_local_symbols_for_section(nsect, nlists, strings, s,
					      section_map);
	}
}

static
unsigned long
lookup_indirect_item(
struct merged_symbol *merged_symbol,
struct object_file *obj,
unsigned long index,
struct indirect_section_data *data, 
unsigned long stride,
enum bool *new)
{
    unsigned long hashval, output_offset;
    struct indirect_item_block **p, *indirect_item_block;
    struct indirect_item *indirect_item;
    struct indirect_item_bucket *bp;

	if(data->hashtable == NULL){
	    data->hashtable = allocate(sizeof(struct indirect_item_bucket *) *
						  INDIRECT_SECTION_HASHSIZE);
	    memset(data->hashtable, '\0',
		   sizeof(struct indirect_item_bucket *) *
						  INDIRECT_SECTION_HASHSIZE);
	}
#if defined(DEBUG) && defined(PROBE_COUNT)
	data->nprobes++;
#endif
	hashval = ((unsigned long)merged_symbol) % INDIRECT_SECTION_HASHSIZE;
	for(bp = data->hashtable[hashval]; bp; bp = bp->next){
#if defined(DEBUG) && defined(PROBE_COUNT)
	    data->nprobes++;
#endif
	    if(bp->indirect_item->merged_symbol == merged_symbol &&
	       merged_symbol != NULL){
		*new = FALSE;
		return(bp->output_offset);
	    }
	}

	bp = allocate(sizeof(struct indirect_item_bucket));
	output_offset = 0;
	for(p = &(data->indirect_item_blocks);
	    *p ;
	    p = &(indirect_item_block->next)){

	    indirect_item_block = *p;
	    if(indirect_item_block->used != INDIRECT_SECTION_BLOCK_SIZE){
		indirect_item = indirect_item_block->indirect_items +
				  indirect_item_block->used;
		indirect_item->merged_symbol = merged_symbol;
		indirect_item->obj = obj;
		indirect_item->index = index;

		bp->indirect_item = indirect_item;
		bp->output_offset = output_offset +
				    indirect_item_block->used * stride;
		bp->next = data->hashtable[hashval];
		data->hashtable[hashval] = bp;

		indirect_item_block->used++;
		*new = TRUE;
		return(bp->output_offset);
	    }
	    output_offset += indirect_item_block->used * stride;
	}
	*p = allocate(sizeof(struct indirect_item_block));
	indirect_item_block = *p;
	indirect_item = indirect_item_block->indirect_items;
	indirect_item->merged_symbol = merged_symbol;
	indirect_item->obj = obj;
	indirect_item->index = index;
	indirect_item_block->used = 1;
	indirect_item_block->next = NULL;

	bp->indirect_item = indirect_item;
	bp->output_offset = output_offset;
	bp->next = data->hashtable[hashval];
	data->hashtable[hashval] = bp;

	*new = TRUE;
	return(bp->output_offset);
}

/*
 * indirect_section_order() enters indirect symbols from the order_file from the
 * merged section structure.  Since this is called before any call to
 * indirect_section_merge() and it enters the indirect symbols in the order of
 * the file it causes the section to be ordered.
 */
__private_extern__
void
indirect_section_order(
struct indirect_section_data *data, 
struct merged_section *ms)
{
#ifndef RLD
    kern_return_t r;
#ifdef __MWERKS__
    struct indirect_section_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif

	warning("section ordering for indirect sections not supported ("
		"-sectorder %s %s %s ignored)", ms->s.segname, ms->s.sectname,
		ms->order_filename);
	/*
	 * Deallocate the memory for the load order file now that it is
	 * nolonger needed.
	 */
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)
	    ms->order_addr, ms->order_size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() memory for -sectorder "
		       "file: %s for section (%.16s,%.16s)",
		       ms->order_filename, ms->s.segname,
		       ms->s.sectname);
	ms->order_addr = NULL;
#else /* RLD */
#ifdef __MWERKS__
    struct indirect_section_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif
#endif /* RLD */
}

/*
 * indirect_section_reset_live() is called when -dead_strip is specified after
 * the indirect sections the input objects are merged. It clears out
 * the indirect_section_data so the live indirect items can be re-merged (by
 * later calling indirect_section_merge() with redo_live == TRUE.
 */
__private_extern__
void
indirect_section_reset_live(
struct indirect_section_data *data, 
struct merged_section *ms)
{
	/*
	 * reset the total number of indirect symbol table entries in the
	 * output file.  Really needs to happen only once.  But placing it
	 * here it will get reset once for each merged section.
	 */
	nindirectsyms = 0;

	/* reset the merge section size back to zero */
	ms->s.size = 0;

	/* reset the count of relocation entries for this merged section */
	if(output_for_dyld){
	    ms->nlocrel = 0;
	    ms->nextrel = 0;
	}
	else if(save_reloc){
	    nreloc -= ms->s.nreloc;
	    ms->s.nreloc = 0;
	}

	/* clear out the previously merged data */
	indirect_section_free(data);
}

#ifndef RLD
/*
 * indirect_live_ref() is called by walk_references() as part of the live
 * marking pass when -dead_strip is specified to get the ref when a
 * indirect section is referenced.  The reference is specified by fine_reloc,
 * map and obj.  This routine sets the ref struct passed to it for the
 * reference if there is one and returns TRUE else it returns FALSE.
 */
__private_extern__
enum bool
indirect_live_ref(
struct fine_reloc *fine_reloc,
struct section_map *map,
struct object_file *obj,
struct ref *r)
{
    unsigned long index, value, r_symbolnum;
    struct nlist *nlists;
    char *contents;

	if((map->s->flags & SECTION_TYPE) != S_SYMBOL_STUBS &&
	   (map->s->flags & SECTION_TYPE) != S_LAZY_SYMBOL_POINTERS &&
	   (map->s->flags & SECTION_TYPE) != S_NON_LAZY_SYMBOL_POINTERS)
	    fatal("internal error: indirect_live_ref() called with map not for "
		  "indirect section");

	if(fine_reloc->local_symbol == TRUE){
	    /*
	     * Indirect table entries which refer to local defined symbols are
	     * allowed only in symbol stub and lazy pointer sections so they can
	     * always be removed and never created on output.  When this happens
	     * the fine_reloc has use_contents set to FALSE and the symbol index
	     * of the local symbol stored in output_offset.
	     */
	    if(fine_reloc->use_contents == FALSE){
		if((map->s->flags & SECTION_TYPE) ==
		   S_NON_LAZY_SYMBOL_POINTERS){
		    fatal("internal error: indirect_live_ref() called "
			  "with fine_reloc for non-lazy pointer with "
			  "local_symbol == TRUE and use_contents == "
			  "FALSE");
		}
		index = fine_reloc->output_offset;
		nlists = (struct nlist *)(obj->obj_addr +
					  obj->symtab->symoff);
		r_symbolnum = r_symbolnum_from_r_value(nlists[index].n_value,
						       obj);
	        r->map = &(obj->section_maps[r_symbolnum - 1]);
	        r->fine_reloc = fine_reloc_for_input_offset(r->map,
				      nlists[index].n_value - r->map->s->addr);
		r->obj = obj;
		r->merged_symbol = NULL;
		return(TRUE);
	    }
	    else{
		/*
		 * Both local_symbol is TRUE and use_contents is TRUE.  This
		 * happens for INDIRECT_SYMBOL_LOCAL and INDIRECT_SYMBOL_ABS.
		 * In this case we need to get the reference from the indirect
		 * symbol table entry (the section contents).  The contents of
		 * the non-lazy pointer has the address of the item being
		 * referenced.  If it is INDIRECT_SYMBOL_LOCAL then it will be
		 * in this object.  If it is INDIRECT_SYMBOL_ABS we will not
		 * return a reference.  If we have a INDIRECT_SYMBOL_ABS then
		 * we will return the ref_type set to LIVE_REF_NONE.
		 */
		if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		   (map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS)
		    fatal("internal error: indirect_live_ref() called "
			  "with fine_reloc for stub or lazy pointer "
			  "with local_symbol == TRUE and use_contents "
			  "== TRUE");
		if(fine_reloc->indirect_symbol_local == TRUE){
		    contents = obj->obj_addr + map->s->offset;
		    memcpy(&value, contents + fine_reloc->input_offset, 4);
		    if(obj->swapped)
			value = SWAP_LONG(value);
		    r_symbolnum = r_symbolnum_from_r_value(value, obj);
		    r->map = &(obj->section_maps[r_symbolnum - 1]);
		    r->fine_reloc = fine_reloc_for_input_offset(r->map,
					value - r->map->s->addr);
		    r->obj = obj;
		    r->merged_symbol = NULL;
		    return(TRUE);
		}
		else{
		    /*
		     * Note: that fine_relocs for indirect symbol which are
		     * INDIRECT_SYMBOL_LOCAL and INDIRECT_SYMBOL_ABS have
		     * local_symbol set to TRUE.
		     */
		    return(FALSE);
		}
	    }
	}
	else{
	    /*
	     * External symbols just set the ref's merged_symbol field.
	     */
	    r->merged_symbol = fine_reloc->merged_symbol;
	    r->fine_reloc = NULL;
	    r->map = NULL;
	    r->obj = NULL;
	    return(TRUE);
	}
}
#endif /* !defined(RLD) */

/*
 * indirect_section_free() free()'s up all space used by the data block except 
 * the data block itself.
 */
__private_extern__
void
indirect_section_free(
struct indirect_section_data *data)
{
    unsigned long i;
    struct indirect_item_bucket *bp, *next_bp;
    struct indirect_item_block *indirect_item_block,
			       *next_indirect_item_block;

	/*
	 * Free all data for this block.
	 */
	if(data->hashtable != NULL){
	    for(i = 0; i < INDIRECT_SECTION_HASHSIZE; i++){
		for(bp = data->hashtable[i]; bp; ){
		    next_bp = bp->next;
		    free(bp);
		    bp = next_bp;
		}
	    }
	    free(data->hashtable);
	    data->hashtable = NULL;
	}
	for(indirect_item_block = data->indirect_item_blocks;
	    indirect_item_block;
	    indirect_item_block = next_indirect_item_block){

	    next_indirect_item_block = indirect_item_block->next;
	    free(indirect_item_block);
	}
	data->indirect_item_blocks = NULL;
}
#endif /* !defined(SA_RLD) */

/*
 * legal_reference() determines if the specified reference is legal with respect
 * to the correct usage of symbol stub sections and lazy symbol pointer sections
 * (as parts of these section can be removed by the link editor and we must make
 * sure that these things are correctly being referenced so removing them will
 * produce a correct object file).  The specified reference comes from a
 * relocation entry and is specified as section maps and offsets FROM the place
 * of reference to the item being refered TO and the relocation entry index
 * causing the reference.  It returns TRUE if the reference is legal if not it
 * prints the appropate error message and returns FALSE.
 */
__private_extern__
enum bool
legal_reference(
struct section_map *from_map,
unsigned long from_offset,
struct section_map *to_map,
unsigned long to_offset,
unsigned long from_reloc_index,
enum bool sectdiff_reloc)
{
#ifndef SA_RLD
    unsigned long to_section_type, from_section_type;
    unsigned long *indirect_symtab;
    struct fine_reloc *to_fine_reloc, *from_fine_reloc;
    struct merged_symbol *merged_symbol;
    unsigned long stride;

	from_section_type = from_map->s->flags & SECTION_TYPE;
	/*
	 * If we are not using the block where this reference is coming from
	 * then we don't care if this is an illegal reference or not.
	 */
	if(from_section_type == S_COALESCED){
	    from_fine_reloc =
		fine_reloc_for_input_offset(from_map, from_offset);
	    if(from_fine_reloc->use_contents == FALSE)
		return(TRUE);
	}

	to_section_type = to_map->s->flags & SECTION_TYPE;
	/*
	 * If this is a coalesced section then the reference may not be to this
	 * coalesced section if the referenced block's contents is not used,
	 * because the block could have been for a weak definition symbol.
	 */
	if(to_section_type == S_COALESCED){
	    to_fine_reloc = fine_reloc_for_input_offset(to_map, to_offset);
	    /*
	     * If this reference is to a local symbol then it is ok to reference
	     * this coalesced symbol directly from anywhere.
	     */
	    if(to_fine_reloc->local_symbol == TRUE)
		return(TRUE);
	    if(to_fine_reloc->use_contents == FALSE){
		merged_symbol = to_fine_reloc->merged_symbol;
		if(merged_symbol->defined_in_dylib == TRUE){
		    if(sectdiff_reloc == TRUE && dynamic == TRUE){
			error_with_cur_obj("illegal reference for -dynamic "
			    "code (section difference reference from section "
			    "(%.16s,%.16s) relocation entry (%lu) "
			    "to symbol: %s defined in dylib: %s)",
			    from_map->s->segname, from_map->s->sectname,
			    from_reloc_index, merged_symbol->nlist.n_un.n_name,
			    merged_symbol->definition_object->file_name);
			return(FALSE);
		    }
		    to_section_type = S_REGULAR;
		}
		else if((merged_symbol->nlist.n_type & N_TYPE) == N_SECT){
		    if(merged_symbols_relocated == FALSE)
			to_section_type = merged_symbol->definition_object->
			    section_maps[merged_symbol->nlist.n_sect - 1].s->
			    flags & SECTION_TYPE;
		    else
			to_section_type =
			    pass2_nsect_merged_symbol_section_type(
				merged_symbol);
		}
		else{
		    if(sectdiff_reloc == TRUE && dynamic == TRUE){
			if((merged_symbol->nlist.n_type & N_TYPE) == N_ABS){
			    error_with_cur_obj("illegal reference for -dynamic "
				"code (section difference reference from "
				"section (%.16s,%.16s) relocation entry (%lu) "
				"to absolute symbol: %s)",
				from_map->s->segname, from_map->s->sectname,
				from_reloc_index,
				merged_symbol->nlist.n_un.n_name);
			    return(FALSE);
			}
		    }
		    to_section_type = S_REGULAR;
		}
	    }
	}

	/*
	 * To allow the dynamic linker to use the same coalesced symbol through
	 * out the program all references to coalesced symbols must be
	 * relocatable to the same coalesced symbol.  Most references are done
	 * indirectly through symbol stubs and non-lazy pointers, static
	 * initialization of data references are done directly.  The only type
	 * direct reference that can't be relocated by the dynamic linker is
	 * a reference through a section difference relocation entry because
	 * it is pic and does not make it the output_for_dyld files.  This is
	 * bad code generation except for the case the reference is TO the
	 * same block the reference is from or to a block with a local symbol.
	 * The first is usually the picbase of a routine in a coalesced section
	 * and the second allows for unwind tables to reference private extern
	 * coalesced symbols.
	 */
	if(dynamic == TRUE && to_section_type == S_COALESCED &&
	   sectdiff_reloc == TRUE && (from_map != to_map ||
	   fine_reloc_for_input_offset(from_map, from_offset) != 
	   fine_reloc_for_input_offset(to_map, to_offset)) &&
	   fine_reloc_for_input_offset(to_map, to_offset)->local_symbol ==
		FALSE){
	    error_with_cur_obj("malformed object, illegal reference for "
		"-dynamic code (reference to a coalesced section (%.16s,"
		"%.16s) from section (%.16s,%.16s) relocation entry (%lu))",
		to_map->s->segname, to_map->s->sectname, from_map->s->segname,
		from_map->s->sectname, from_reloc_index);
	    return(FALSE);
	}

	/*
	 * If the reference is not to or from a symbol stub section or a
	 * lazy symbol pointer section it is legal as far as used of these
	 * sections goes.
	 */
	if(to_section_type != S_SYMBOL_STUBS &&
	   to_section_type != S_LAZY_SYMBOL_POINTERS &&
	   from_section_type != S_SYMBOL_STUBS &&
	   from_section_type != S_LAZY_SYMBOL_POINTERS)
	    return(TRUE);

	/*
	 * See if this reference is to a symbol stub section.
	 */
	if(to_section_type == S_SYMBOL_STUBS){
	    /*
	     * For references to a symbol stub section that are made from any
	     * thing but a lazy pointer section the reference must be made to
	     * the start of a symbol stub in that section.
	     */
	    if(from_section_type != S_LAZY_SYMBOL_POINTERS){
		if((to_offset % to_map->s->reserved2) != 0){
		    /*
		     * A reference to a symbol stub section may be made from the
		     * same symbol stub section from the same stub which is what
		     * happens for a pic style symbol stub.
		     */
		    if(from_section_type == S_SYMBOL_STUBS &&
		       from_map == to_map &&
		       from_offset / from_map->s->reserved2 ==
		       to_offset / to_map->s->reserved2)
			return(TRUE);
		    error_with_cur_obj("malformed object, illegal reference "
			"(reference to a symbol stub not at the start of the "
			"stub from section (%.16s,%.16s) relocation entry "
			"(%lu))", from_map->s->segname, from_map->s->sectname,
			from_reloc_index);
		    return(FALSE);
		}
		else
		    return(TRUE);
	    }
	    else{
		/*
		 * This reference is to a symbol stub section from a lazy
		 * pointer section.  The stub being referenced and the pointer
		 * must both have the same indirect symbol.  By same in this
		 * case we check for the same indirect symbol table entry
		 * (that is the same index into the symbol table).
		 */
		indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
		stride = 4;
		if(indirect_symtab[from_map->s->reserved1 + from_offset /
							stride] !=
		   indirect_symtab[to_map->s->reserved1 + to_offset /
							to_map->s->reserved2]){
		    error_with_cur_obj("malformed object, illegal reference "
			"(reference to symbol stub in section (%.16s,%.16s) "
			"at address 0x%x from lazy pointer section "
			"(%.16s,%.16s) relocation entry (%lu) with non-"
			"matching indirect symbols)", to_map->s->segname,
			to_map->s->sectname,
			(unsigned int)(to_map->s->addr + to_offset),
			from_map->s->segname, from_map->s->sectname,
			from_reloc_index);
		    return(FALSE);
		}
		else
		    return(TRUE);
	    }
	}

	/*
	 * See if this reference is to a lazy symbol pointer section.
	 */
	if(to_section_type == S_LAZY_SYMBOL_POINTERS){
	    /*
	     * Legal references to lazy pointer sections can only be made from a
	     * symbol stub section.
	     */
	    if(from_section_type != S_SYMBOL_STUBS){
		error_with_cur_obj("malformed object, illegal reference "
		    "(reference to a lazy symbol pointer section from section "
		    "(%.16s,%.16s) relocation entry (%lu) which is not a "
		    "symbol stub section)", from_map->s->segname,
		    from_map->s->sectname, from_reloc_index);
		return(FALSE);
	    }
	    else{
		/*
		 * This reference is to a lazy pointer section from a symbol
		 * stub section.  The pointer being referenced and the stub
		 * must both have the same indirect symbol.  By same in this
		 * case we check for the same indirect symbol table entry
		 * (that is the same index into the symbol table).
		 */
		indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
		stride = (arch_flag.cputype == CPU_TYPE_POWERPC64 ? 8 : 4);
		if(indirect_symtab[from_map->s->reserved1 + from_offset /
				   from_map->s->reserved2] !=
		   indirect_symtab[to_map->s->reserved1 + to_offset / stride]){
		    error_with_cur_obj("malformed object, illegal reference "
			"(reference to lazy symbol pointer section "
			"(%.16s,%.16s) at address 0x%x from symbol stub "
			"section (%.16s,%.16s) relocation entry (%lu) with "
			"non-matching indirect symbols)",
			to_map->s->segname, to_map->s->sectname,
			(unsigned int)(to_map->s->addr + to_offset),
			from_map->s->segname, from_map->s->sectname,
			from_reloc_index);
		    return(FALSE);
		}
		else
		    return(TRUE);
	    }
	}

	/*
	 * See if this reference is from a symbol stub section.
	 */
	if(from_section_type == S_SYMBOL_STUBS){
	    /*
	     * At this point we know this reference from a symbol stub section
	     * is not to lazy symbol pointer section.  The only thing else this
	     * section should be referencing is the stub binding helper routine.
	     */
	    return(TRUE);
	}
#endif /* !defined(SA_RLD) */
	return(TRUE);
}

#ifndef SA_RLD
/*
 * output_indirect_symbols() copies the indirect symbol table into the output
 * file.
 */
__private_extern__
void
output_indirect_symbols(
void)
{
    unsigned long i, nindirect_symbols;
    uint32_t *indirect_symbols;
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;
    struct indirect_section_data *data;
    struct indirect_item_block **q, *indirect_item_block;
    struct indirect_item *indirect_item;

	if(nindirectsyms == 0)
	    return;
	if(strip_level == STRIP_ALL)
	    fatal("can't use -s with input files containing indirect symbols "
		  "(output file must contain at least global symbols, for "
		  "maximum stripping use -x)");
	indirect_symbols = (uint32_t *)(output_addr +
			output_dysymtab_info.dysymtab_command.indirectsymoff);
	nindirect_symbols = 0;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if((ms->s.flags & SECTION_TYPE) == S_SYMBOL_STUBS ||
		   (ms->s.flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
		   (ms->s.flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS){
		    data = (struct indirect_section_data *)ms->literal_data;
		    ms->s.reserved1 = nindirect_symbols;
		    for(q = &(data->indirect_item_blocks);
			*q ;
			q = &(indirect_item_block->next)){

			indirect_item_block = *q;
			for(i = 0; i < indirect_item_block->used; i++){
			    indirect_item = indirect_item_block->
					    indirect_items + i;
			    if(indirect_item->merged_symbol != NULL){
				/*
				 * If this is a non-lazy symbol pointer section
				 * and the symbol is a private extern then
				 * change the indirect symbol to
				 * INDIRECT_SYMBOL_LOCAL or
				 * INDIRECT_SYMBOL_ABS.
				 */
				if((ms->s.flags & SECTION_TYPE) ==
				   S_NON_LAZY_SYMBOL_POINTERS &&
				   (indirect_item->merged_symbol->
					nlist.n_type & N_PEXT) == N_PEXT &&
				    keep_private_externs == FALSE){
				    if((indirect_item->merged_symbol->
					nlist.n_type & N_TYPE) == N_ABS)
					indirect_symbols[nindirect_symbols++] =
						INDIRECT_SYMBOL_ABS;
				    else
					indirect_symbols[nindirect_symbols++] =
						INDIRECT_SYMBOL_LOCAL;
				}
				else{
				    indirect_symbols[nindirect_symbols++] =
					merged_symbol_output_index(
					    indirect_item->merged_symbol);
				}
			    }
			    else{
				if(indirect_item->index ==
				       INDIRECT_SYMBOL_LOCAL ||
				   indirect_item->index ==
				       INDIRECT_SYMBOL_ABS){
				    indirect_symbols[nindirect_symbols++] =
					indirect_item->index;
				}
				else{
				    indirect_symbols[nindirect_symbols++] =
					local_symbol_output_index(
					    indirect_item->obj,
					    indirect_item->index);
				}
			    }
			}
		    }
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
	if(nindirect_symbols != nindirectsyms)
	    fatal("internal error, nindirect_symbols != nindirectsyms in "
		   "output_indirect_symbols()");
	if(host_byte_sex != target_byte_sex)
	    swap_indirect_symbols(indirect_symbols, nindirect_symbols,
		target_byte_sex);
#ifndef RLD
	output_flush(output_dysymtab_info.dysymtab_command.indirectsymoff,
		     nindirect_symbols * sizeof(uint32_t));
#endif /* !defined(RLD) */
}
#endif /* !defined(SA_RLD) */
