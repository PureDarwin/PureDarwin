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
 * This file contains the routines to manage the table of object files to be
 * loaded.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <limits.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/mach.h>
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <ar.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "pass1.h"
#include "symbols.h"
#include "sets.h"

/*
 * The head of the object file list and the total count of all object files
 * in the list.  The number objects is only used in main() to tell if there
 * had been any objects loaded into the output file.
 */
__private_extern__ struct object_list *objects = NULL;
__private_extern__ unsigned long nobjects = 0;

/*
 * A pointer to the current object being processed in pass1 or pass2.
 */
__private_extern__ struct object_file *cur_obj = NULL;

/*
 * A pointer to the base object for an incremental link if not NULL.
 */
__private_extern__ struct object_file *base_obj = NULL;

/*
 * new_object_file() returns a pointer to the next available object_file
 * structrure.  The object_file structure is allways zeroed.
 */
__private_extern__
struct object_file *
new_object_file(void)
{
    struct object_list *object_list, **p;
    struct object_file *object_file;

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    if(object_list->used == NOBJECTS)
		continue;
	    object_file = &(object_list->object_files[object_list->used]);
	    object_list->used++;
	    nobjects++;
#ifdef RLD
	    object_file->set_num = cur_set;
#endif /* RLD */
	    return(object_file);
	}
	*p = allocate(sizeof(struct object_list));
	object_list = *p;
	memset(object_list, '\0', sizeof(struct object_list));
	object_file = &(object_list->object_files[object_list->used]);
	object_list->used++;
	nobjects++;
#ifdef RLD
	object_file->set_num = cur_set;
#endif /* RLD */
	return(object_file);
}

#ifndef RLD
/*
 * object_index() returns the index into the module table for a object file
 * structure.  It is only used in the creation of the table of contents entries
 * in a multi module MH_DYLIB file.
 */
__private_extern__
unsigned long
object_index(
struct object_file *obj)
{
    unsigned long index, i;
    struct object_list *object_list, **p;
    struct object_file *cmp_obj;

	if(multi_module_dylib == FALSE)
	    return(0);
	index = 0;
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		cmp_obj = &(object_list->object_files[i]);
		if(cmp_obj->dylib == TRUE)
		    continue;
		if(cmp_obj->bundle_loader == TRUE)
		    continue;
		if(cmp_obj == obj)
		    return(index);
		index++;
	    }
	}
	fatal("internal error: object_index() called with bad object file "
	      "pointer");
	return(0); /* to prevent warning from compiler */
}
#endif /* !defined(RLD) */

/*
 * add_last_object_file() adds the specified object file to the end of the
 * object file list.
 */
__private_extern__
struct object_file *
add_last_object_file(
struct object_file *new_object)
{
    struct object_file *last_object;

	last_object = new_object_file();
	*last_object = *new_object;
	return(last_object);
}

/*
 * remove_last_object_file() removes the specified object file from the end of
 * the object file list.
 */
__private_extern__
void
remove_last_object_file(
struct object_file *last_object)
{
    struct object_list *object_list, **p;
    struct object_file *object_file;

	object_file = NULL;
	object_list = NULL;
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    object_file = &(object_list->object_files[object_list->used - 1]);
	    if(object_list->used == NOBJECTS)
		continue;
	}
	if(object_file == NULL || object_file != last_object)
	    fatal("internal error: remove_last_object_file() called with "
		  "object file that was not the last object file");
	memset(object_file, '\0', sizeof(struct object_file));
	object_list->used--;
}

/*
 * Print the name of the specified object structure in the form: "filename ",
 * "archive(member) " or "dylib(member).
 */
__private_extern__
void
print_obj_name(
struct object_file *obj)
{
    char *strings;

	if(obj->ar_hdr != NULL){
	    print("%s(%.*s) ", obj->file_name, (int)obj->ar_name_size,
		  obj->ar_name);
	}
	else if(obj->dylib_module != NULL){
	    strings = obj->obj_addr + obj->symtab->stroff;
	    print("%s(%s) ", obj->file_name,
		  strings + obj->dylib_module->module_name);
	}
	else
	    print("%s ", obj->file_name);
}

__private_extern__
unsigned long
size_ar_name(
struct ar_hdr *ar_hdr)
{
    long i;

	i = sizeof(ar_hdr->ar_name) - 1;
	if(ar_hdr->ar_name[i] == ' '){
	    do{
		if(ar_hdr->ar_name[i] != ' ')
		    break;
		i--;
	    }while(i > 0);
	}
	return(i + 1);
}

/*
 * set_obj_resolved_path() sets the resolved_path field of the specified
 * object file structure to be used for N_OSO names.
 */
__private_extern__
void
set_obj_resolved_path(
struct object_file *obj)
{
#if !defined (SA_RLD) && !(defined(KLD) && defined(__STATIC__))
    char resolved_path[PATH_MAX];

	if(obj->resolved_path == NULL){
	    if(realpath(obj->file_name, resolved_path) == NULL)
		system_error("can't get resolved path to: %s", obj->file_name);
	    if(obj->ar_hdr != NULL){
		obj->resolved_path_len = strlen(resolved_path) +
					 obj->ar_name_size + 2;
		obj->resolved_path = allocate(obj->resolved_path_len + 1);
		strcpy(obj->resolved_path, resolved_path);
		strcat(obj->resolved_path, "(");
		strncat(obj->resolved_path, obj->ar_name, obj->ar_name_size);
		strcat(obj->resolved_path, ")");
	    }
	    else{
		obj->resolved_path_len = strlen(resolved_path);
		obj->resolved_path = allocate(obj->resolved_path_len + 1);
		strcpy(obj->resolved_path, resolved_path);
	    }
	}
#else /* defined(SA_RLD) || defined(KLD) && defined(__STATIC__) */
	obj->resolved_path_len = strlen(obj->file_name + 1);
	obj->resolved_path = obj->file_name;
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */
}

/*
 * print_whatsloaded() prints which object files are loaded.  This has to be
 * called after pass1 to get the correct result.
 */
__private_extern__
void
print_whatsloaded(void)
{
    unsigned long i;
    struct object_list *object_list, **p;
    struct object_file *obj;
    char *strings;

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj->dylib && obj->dylib_module == NULL)
		    continue;
		if(obj->bundle_loader)
		    continue;
		if(obj->dylinker)
		    continue;
		if(obj->ar_hdr){
		    print("%s(%.*s)\n",obj->file_name,
			  (int)obj->ar_name_size, obj->ar_name);
		}
		else if(obj->dylib_module != NULL){
		    strings = obj->obj_addr + obj->symtab->stroff;
		    print("%s(%s)\n", obj->file_name,
			  strings + obj->dylib_module->module_name);
		}
		else
		    print("%s\n", obj->file_name);
	    }
	}
}

/*
 * is_dylib_module_loaded() returns TRUE is the specified dynamic library module
 * is already loaded and FALSE otherwise.  It is used by search_dynamic_libs()
 * to make sure the module that defines the library initialization routine is
 * loaded.
 */
__private_extern__
enum bool
is_dylib_module_loaded(
struct dylib_module *dylib_module)
{
    unsigned long i;
    struct object_list *object_list, **p;
    struct object_file *obj;

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		obj = &(object_list->object_files[i]);
		if(obj->dylib && obj->dylib_module != NULL){
		    if(obj->dylib_module == dylib_module)
			return(TRUE);
		}
	    }
	}
	return(FALSE);
}

/*
 * fine_reloc_output_offset() returns the output offset for the specified 
 * input offset and the section map using the fine relocation entries.
 */
__private_extern__
unsigned long
fine_reloc_output_offset(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    unsigned long section_type;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);

	/*
	 * This routine is used to set the r_address field of relocation
	 * entries.  For entries that are part of a item who's contents is
	 * not used in the output file the return value here for r_address is
	 * to set it past the end of the section in the output file.  This is
	 * then checked for in fine_reloc_offset_in_output_for_output_offset()
	 * to know if the r_address (output_offet) is not in the output.
	 */
	section_type = map->s->flags & SECTION_TYPE;

	if(((section_type == S_LAZY_SYMBOL_POINTERS ||
	     section_type == S_NON_LAZY_SYMBOL_POINTERS ||
	     section_type == S_SYMBOL_STUBS ||
	     section_type == S_COALESCED) &&
	    fine_reloc->use_contents == FALSE) ||
	   (dead_strip == TRUE && fine_reloc->live == FALSE))
	    return(map->output_section->s.size +
	       input_offset - fine_reloc->input_offset);

	return(fine_reloc->output_offset +
	       input_offset - fine_reloc->input_offset);
}

/*
 * fine_reloc_output_address() returns the output_address for the input_offset
 * in the section with the specified section map.  This can return one of two
 * things depending on the fine relocation entry for the input_offset.  If the
 * section is a symbol stub section and the fine_relocation entry's indirect
 * symbol is defined then the value of the symbol is used.  Otherwise the output
 * offset for the fine relocation entry is added to the specified
 * output_base_address.
 */
__private_extern__
unsigned long
fine_reloc_output_address(
struct section_map *map,
unsigned long input_offset,
unsigned long output_base_address)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;
    unsigned long index;
    struct nlist *nlists;
    struct section_map *section_map;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if(fine_reloc->local_symbol == TRUE){
	    if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS){
		index = fine_reloc->output_offset;
		nlists = (struct nlist *)(cur_obj->obj_addr +
					  cur_obj->symtab->symoff);
		if(nlists[index].n_sect == NO_SECT)
		    return(nlists[index].n_value);
		section_map = &(cur_obj->section_maps[nlists[index].n_sect -1]);
		if(section_map->nfine_relocs == 0)
		    return(nlists[index].n_value - section_map->s->addr +
			   section_map->output_section->s.addr +
			   section_map->offset);
		else
		    return(section_map->output_section->s.addr +
			   fine_reloc_output_offset(section_map,
						    nlists[index].n_value -
						    section_map->s->addr));
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		    (map->s->flags & SECTION_TYPE) ==
						  S_NON_LAZY_SYMBOL_POINTERS){
		return(output_base_address +
		       fine_reloc->output_offset +
		       input_offset - fine_reloc->input_offset);
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
		if(fine_reloc->use_contents == TRUE){
		    return(output_base_address +
			   fine_reloc->output_offset +
			   input_offset - fine_reloc->input_offset);
		}
		else{
		    merged_symbol = fine_reloc->merged_symbol;
		    if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
			merged_symbol = (struct merged_symbol *)
					merged_symbol->nlist.n_value;
		    return(merged_symbol->nlist.n_value);
		}
	    }
	    else{
		fatal("internal error, fine_reloc_output_address() called with "
		      "an input_offset which maps to a fine_reloc where "
		      "local_symbol is TRUE but it's not in a S_SYMBOL_STUBS, "
		      "S_LAZY_SYMBOL_POINTERS or S_COALESCED section");
		return(0);
	    }
	}
	else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
	    if(fine_reloc->use_contents == TRUE){
		return(output_base_address +
		       fine_reloc->output_offset +
		       input_offset - fine_reloc->input_offset);
	    }
	    else{
		merged_symbol = fine_reloc->merged_symbol;
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		return(merged_symbol->nlist.n_value);
	    }
	}
	else if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	        fine_reloc->indirect_defined == TRUE){
	    if(filetype != MH_DYLIB ||
	       (filetype == MH_DYLIB && multi_module_dylib == FALSE) ||
	       (cur_obj == fine_reloc->merged_symbol->definition_object &&
		input_offset - fine_reloc->input_offset == 0)){
		if(cur_obj == fine_reloc->merged_symbol->definition_object)
		    merged_symbol = fine_reloc->merged_symbol;
		else
		    merged_symbol = fine_reloc->merged_symbol;
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		return(merged_symbol->nlist.n_value);
	    }
	    else{
		return(output_base_address +
		       fine_reloc->output_offset +
		       input_offset - fine_reloc->input_offset);
	    }
	}
	else{
	    return(output_base_address +
		   fine_reloc->output_offset +
		   input_offset - fine_reloc->input_offset);
	}
}

/*
 * fine_reloc_output_ref() sets the live_ref for the input_offset in the section
 * with the specified section map as to what is being referenced in the linked
 * output.  This is used when -dead_strip is specified to drive the live marking
 * pass.  The value field of the live_ref for LIVE_REF_VALUE is an address in
 * the input object file (not it's address in the linked output).  This routine
 * can set the live_ref to one of two things depending on the fine relocation
 * entry for the input_offset.  If the section is a symbol stub section and the
 * fine_relocation entry's indirect symbol is defined then the symbol is used 
 * with a LIVE_REF_SYMBOL.  Otherwise the input_offset is added back to the
 * map->s->addr for a LIVE_REF_VALUE.
 */
__private_extern__
void
fine_reloc_output_ref(
struct section_map *map,
unsigned long input_offset,
struct live_ref *ref)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;
    unsigned long index;
    struct nlist *nlists;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if(fine_reloc->local_symbol == TRUE){
	    if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS){
		index = fine_reloc->output_offset;
		nlists = (struct nlist *)(cur_obj->obj_addr +
					  cur_obj->symtab->symoff);
		ref->ref_type = LIVE_REF_VALUE;
		ref->value = nlists[index].n_value;
		return;
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
		    (map->s->flags & SECTION_TYPE) ==
						  S_NON_LAZY_SYMBOL_POINTERS){
		ref->ref_type = LIVE_REF_VALUE;
		ref->value = map->s->addr + input_offset;
		return;
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
		if(fine_reloc->use_contents == TRUE){
		    ref->ref_type = LIVE_REF_VALUE;
		    ref->value = map->s->addr + input_offset;
		    return;
		}
		else{
		    merged_symbol = fine_reloc->merged_symbol;
		    if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
			merged_symbol = (struct merged_symbol *)
					merged_symbol->nlist.n_value;
		    ref->ref_type = LIVE_REF_SYMBOL;
		    ref->merged_symbol = merged_symbol;
		    return;
		}
	    }
	    else{
		fatal("internal error, fine_reloc_output_ref() called with "
		      "an input_offset which maps to a fine_reloc where "
		      "local_symbol is TRUE but it's not in a S_SYMBOL_STUBS, "
		      "S_LAZY_SYMBOL_POINTERS or S_COALESCED section");
		return;
	    }
	}
	else if((map->s->flags & SECTION_TYPE) == S_COALESCED){
	    if(fine_reloc->use_contents == TRUE){
		ref->ref_type = LIVE_REF_VALUE;
		ref->value = map->s->addr + input_offset;
		return;
	    }
	    else{
		merged_symbol = fine_reloc->merged_symbol;
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		ref->ref_type = LIVE_REF_SYMBOL;
		ref->merged_symbol = merged_symbol;
		return;
	    }
	}
	else if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	        fine_reloc->indirect_defined == TRUE){
	    if(filetype != MH_DYLIB ||
	       (filetype == MH_DYLIB && multi_module_dylib == FALSE) ||
	       (cur_obj == fine_reloc->merged_symbol->definition_object &&
		input_offset - fine_reloc->input_offset == 0)){
		if(cur_obj == fine_reloc->merged_symbol->definition_object)
		    merged_symbol = fine_reloc->merged_symbol;
		else
		    merged_symbol = fine_reloc->merged_symbol;
		if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		    merged_symbol = (struct merged_symbol *)
				    merged_symbol->nlist.n_value;
		ref->ref_type = LIVE_REF_SYMBOL;
		ref->merged_symbol = merged_symbol;
		return;
	    }
	    else{
		ref->ref_type = LIVE_REF_VALUE;
		ref->value = map->s->addr + input_offset;
		return;
	    }
	}
	else{
	    ref->ref_type = LIVE_REF_VALUE;
	    ref->value = map->s->addr + input_offset;
	    return;
	}
}

/*
 * fine_reloc_offset_in_output() returns TRUE if the input offset is part of a
 * range that will be in the output file.  This is used in processing the parts
 * of sections (contents, relocation entries, symbols) that get removed when
 * a section is a symbol stub section, lazy pointer section or coalesced
 * section or -dead_strip is specified.
 */
__private_extern__
enum bool
fine_reloc_offset_in_output(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    unsigned long section_type;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if(dead_strip == TRUE){
	    section_type = map->s->flags & SECTION_TYPE;
	    if(section_type == S_LAZY_SYMBOL_POINTERS ||
	       section_type == S_NON_LAZY_SYMBOL_POINTERS ||
	       section_type == S_SYMBOL_STUBS ||
	       section_type == S_COALESCED){
		if(fine_reloc->use_contents == FALSE)
		    return(FALSE);
	    }
	    return((enum bool)(fine_reloc->live));
	}
	return((enum bool)(fine_reloc->use_contents));
}

/*
 * fine_reloc_offset_in_output_for_output_offset() does the same as
 * fine_reloc_offset_in_output() but using the output_offset instead of the
 * input offset and returns TRUE if the output offset is part of a range that
 * will be in the output file.  This is used in reloc_output_for_dyld() in
 * determing if a relocation entry will be in the output file and is needed
 * because the r_address of a relocation entry was modified by the reloc
 * routine.
 */
__private_extern__
enum bool
fine_reloc_offset_in_output_for_output_offset(
struct section_map *map,
unsigned long output_offset)
{
	if(map->nfine_relocs == 0)
	    fatal("internal error, fine_reloc_offset_in_output_for_output_"
		  "offset() called with a map->nfine_relocs == 0");
	/*
	 * For relocation entries that were part of a item who's contents is
	 * not used in the output file fine_reloc_output_offset() was used to
	 * set it's r_address to past the end of the section in the output file.
	 * So in here we check for that and if that is the case we say the
	 * output_offset is not in the output otherwise it is in the output.
	 */
	if(output_offset >= map->output_section->s.size)
	    return(FALSE);
	else
	    return(TRUE);
}

/*
 * fine_reloc_output_sectnum() returns the section number in the output file
 * being referenced for the input_offset in the section with the specified
 * section map.  This can return one of two things depending on the fine
 * relocation entry for the input_offset.  If the section being referenced is
 * a symbol stub section and the fine_relocation entry's indirect symbol is
 * define then the section number of the merged symbol is used.  Otherwise the
 * section number of the referenced section has in the output is returned.
 */
__private_extern__
unsigned long
fine_reloc_output_sectnum(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;
    unsigned long index;
    unsigned long *indirect_symtab;
    struct nlist *nlists;
    struct section_map *section_map;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if(fine_reloc->local_symbol == TRUE){
	    if((map->s->flags & SECTION_TYPE) == S_COALESCED){
		return(map->output_section->output_sectnum);
	    }
	    else if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS){
		index = fine_reloc->output_offset;
	    }
	    else if((map->s->flags & SECTION_TYPE) ==
		     S_NON_LAZY_SYMBOL_POINTERS ||
	    	    (map->s->flags & SECTION_TYPE) ==
		     S_LAZY_SYMBOL_POINTERS){
		indirect_symtab = (unsigned long *)(cur_obj->obj_addr +
					    cur_obj->dysymtab->indirectsymoff);
		index = indirect_symtab[map->s->reserved1 + 
			(fine_reloc->input_offset / 4)];
	    }
	    else{
		fatal("internal error, fine_reloc_output_sectnum() called with "
		      "an input_offset which maps to a fine_reloc where "
		      "local_symbol is TRUE but it's not in a S_SYMBOL_STUBS, "
		      "S_NON_LAZY_SYMBOL_POINTERS, S_LAZY_SYMBOL_POINTERS or "
		      "S_COALESCED section");
		index = 0;
	    }
	    nlists = (struct nlist *)(cur_obj->obj_addr +
				      cur_obj->symtab->symoff);
	    if(nlists[index].n_sect == NO_SECT)
		return(NO_SECT);
	    section_map = &(cur_obj->section_maps[nlists[index].n_sect - 1]);
	    return(section_map->output_section->output_sectnum);
	}
	else if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	        fine_reloc->indirect_defined == TRUE &&
	        (filetype != MH_DYLIB || multi_module_dylib == FALSE)){
	    merged_symbol = fine_reloc->merged_symbol;
	    if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		merged_symbol = (struct merged_symbol *)
				merged_symbol->nlist.n_value;
	    return(merged_symbol->nlist.n_sect);
	}
	else{
	    return(map->output_section->output_sectnum);
	}
}

/*
 * fine_reloc_arm() returns TRUE if fine relocation entry for the input_offset
 * in the section specified is for a symbol stub for a defined external symbol 
 * that is an arm symbol and stub will not be used.  This information is needed
 * when relocating an arm branch instruction that is targetted to a symbol stub
 * that ends up going to target the address of an arm symbol. Then in this case
 * the branch instruction needs to be changed from a blx to a bl instruction.
 */
__private_extern__
enum bool
fine_reloc_arm(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	    fine_reloc->indirect_defined == TRUE &&
	    (filetype != MH_DYLIB || multi_module_dylib == FALSE)){
	    merged_symbol = fine_reloc->merged_symbol;
	    if(merged_symbol != NULL)
		return((merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF)
			!= N_ARM_THUMB_DEF);
	}
	return(FALSE);
}

/*
 * fine_reloc_thumb() returns TRUE if fine relocation entry for the input_offset
 * in the section specified is for a symbol stub for a defined external symbol 
 * that a thumb symbol and stub will not be used.  This information is needed
 * when relocating an arm branch instruction that is targetted to a symbol stub
 * that ends up going to target the address of a thumb symbol. Then in this case
 * the branch instruction needs to be changed to a branch and exchange
 * instuction.
 */
__private_extern__
enum bool
fine_reloc_thumb(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	    fine_reloc->indirect_defined == TRUE &&
	    (filetype != MH_DYLIB || multi_module_dylib == FALSE)){
	    merged_symbol = fine_reloc->merged_symbol;
	    if(merged_symbol != NULL)
		return((merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF)
			== N_ARM_THUMB_DEF);
	}
	return(FALSE);
}

/*
 * fine_reloc_local() returns TRUE if fine relocation entry for the input_offset
 * in the section specified is for a symbol stub for a defined local symbol.
 */
__private_extern__
enum bool
fine_reloc_local(
struct section_map *map,
unsigned long input_offset)
{
    struct fine_reloc *fine_reloc;
    struct merged_symbol *merged_symbol;

	fine_reloc = fine_reloc_for_input_offset(map, input_offset);
	if((map->s->flags & SECTION_TYPE) == S_SYMBOL_STUBS &&
	    fine_reloc->indirect_defined == TRUE &&
	    (filetype != MH_DYLIB || multi_module_dylib == FALSE)){
	    merged_symbol = fine_reloc->merged_symbol;
	    if(merged_symbol == NULL)
		return(TRUE);
	}
	return(FALSE);
}

/*
 * fine_reloc_for_input_offset() returns the fine relocation entry for the
 * specified input offset and the section map.
 */
__private_extern__
struct fine_reloc *
fine_reloc_for_input_offset(
struct section_map *map,
unsigned long input_offset)
{
    int l = 0;
    int u = map->nfine_relocs - 1;
    int m;
    int r;

	if(map->nfine_relocs == 0)
	    fatal("internal error, fine_reloc_for_input_offset() called with a "
		  "section_map->nfine_relocs == 0");
	l = 0;
	m = 0;
	u = map->nfine_relocs - 1;
	while(l <= u){
	    m = (l + u) / 2;
	    if((r = (input_offset - map->fine_relocs[m].input_offset)) == 0)
		return(map->fine_relocs + m);
	    else if (r < 0)
		u = m - 1;
	    else
		l = m + 1;
	}
	if(m == 0 || input_offset > map->fine_relocs[m].input_offset)
	    return(map->fine_relocs + m);
	else
	    return(map->fine_relocs + (m - 1));
}

#ifdef RLD
/*
 * clean_objects() does two things.  For each object file in the current set
 * it first it deallocates the memory used for the object file.  Then it sets
 * the pointer to the section in each section map to point at the merged section
 * so it still can be used by trace_symbol() on future rld_load()'s (again only
 * for object files in the current set).
 */
__private_extern__
void
clean_objects(void)
{
    unsigned long i, j;
    struct object_list *object_list, **p;
    struct object_file *object_file;
#ifndef SA_RLD
    kern_return_t r;
#endif /* !defined(SA_RLD) */

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		object_file = &(object_list->object_files[i]);
		if(object_file->set_num != cur_set)
		    continue;
		if(object_file->ar_hdr == NULL &&
		   object_file->from_fat_file == FALSE &&
		   object_file->obj_size != 0 &&
		   object_file->user_obj_addr == FALSE){
#ifndef SA_RLD
		    if((r = vm_deallocate(mach_task_self(),
				  (vm_address_t)object_file->obj_addr,
				  object_file->obj_size)) != KERN_SUCCESS)
			mach_fatal(r, "can't vm_deallocate() memory for "
				   "mapped file %s",object_file->file_name);
#ifdef RLD_VM_ALLOC_DEBUG
		    print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
			  (unsigned int)object_file->obj_addr,
			  (unsigned int)object_file->obj_size);
#endif /* RLD_VM_ALLOC_DEBUG */
#endif /* !defined(SA_RLD) */
		}
		object_file->obj_addr = NULL;
		object_file->obj_size = 0;
	        object_file->user_obj_addr = FALSE;

		/*
		 * Since the tracing of symbols and the creation of the common
		 * section both use the section's segname and sectname feilds
		 * these need to still be valid after the memory for the file
		 * has been deallocated.  So just set the pointer to point at
		 * the merged section.
		 */
		if(object_file->section_maps != NULL){
		    for(j = 0; j < object_file->nsection_maps; j++){
			object_file->section_maps[j].s = 
			    &(object_file->section_maps[j].output_section->s);
		    }
		}
	    }
	}
}

/*
 * remove_objects() removes the object structures that are from the
 * current object file set.  This takes advantage of the fact
 * that objects from the current set come after the previous set.
 */
__private_extern__
void
remove_objects(void)
{
    unsigned long i, removed;
    /* The compiler "warning: `prev_object_list' may be used uninitialized in */
    /* this function" can safely be ignored */
    struct object_list *object_list, *prev_object_list, *next_object_list;
    struct object_file *object_file;

	/* The compiler "warning: `prev_object_list' may be used */
	/* uninitialized in this function" can safely be ignored */
	prev_object_list = NULL;

	for(object_list = objects;
	    object_list != NULL;
	    object_list = object_list->next){
	    removed = 0;
	    for(i = 0; i < object_list->used; i++){
		object_file = &(object_list->object_files[i]);
		if(object_file->set_num == cur_set){
		    if(cur_set != -1)
			free(object_file->file_name);
		    if(object_file->section_maps != NULL)
			free(object_file->section_maps);
		    if(object_file->undefined_maps != NULL)
			free(object_file->undefined_maps);
		    memset(object_file, '\0', sizeof(struct object_file));
		    removed++;
		}
	    }
	    object_list->used -= removed;
	    nobjects -= removed;
	}
	/*
	 * Find the first object list that now has 0 entries used.
	 */
	for(object_list = objects;
	    object_list != NULL;
	    object_list = object_list->next){
	    if(object_list->used == 0)
		break;
	    prev_object_list = object_list;
	}
	/*
	 * If there are any object lists with 0 entries used free them.
	 */
	if(object_list != NULL && object_list->used == 0){
	    /*
	     * First set the pointer to this list in the previous list to
	     * NULL.
	     */
	    if(object_list == objects)
		objects = NULL;
	    else
		prev_object_list->next = NULL;
	    /*
	     * Now free this list and do the same for all remaining lists.
	     */
	    do {
		next_object_list = object_list->next;
		free(object_list);
		object_list = next_object_list;
	    }while(object_list != NULL);
	}
}
#endif /* RLD */

#ifdef DEBUG
/*
 * print_object_list() prints the object table.  Used for debugging.
 */
__private_extern__
void
print_object_list(void)
{
    unsigned long i, j, k;
    struct object_list *object_list, **p;
    struct object_file *object_file;
    struct fine_reloc *fine_relocs;

	print("Object file list\n");
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    print("    object_list 0x%x\n", (unsigned int)object_list);
	    print("    used %lu\n", object_list->used);
	    print("    next 0x%x\n", (unsigned int)object_list->next);
	    for(i = 0; i < object_list->used; i++){
		object_file = &(object_list->object_files[i]);
		print("\tfile_name %s\n", object_file->file_name);
		print("\tobj_addr 0x%x\n", (unsigned int)object_file->obj_addr);
		print("\tobj_size %lu\n", object_file->obj_size);
    		print("\tar_hdr 0x%x", (unsigned int)object_file->ar_hdr);
    		if(object_file->ar_hdr != NULL)
		    print(" (%.12s)\n", (char *)object_file->ar_hdr);
		else
		    print("\n");
    		print("\tnsection_maps %lu\n", object_file->nsection_maps);
		for(j = 0; j < object_file->nsection_maps; j++){
		    print("\t    (%s,%s)\n",
			   object_file->section_maps[j].s->segname,
			   object_file->section_maps[j].s->sectname);
		    print("\t    offset 0x%x\n",
			   (unsigned int)object_file->section_maps[j].offset);
		    print("\t    fine_relocs 0x%x\n",
		       (unsigned int)object_file->section_maps[j].fine_relocs);
		    print("\t    nfine_relocs %lu\n",
			   object_file->section_maps[j].nfine_relocs);
		    fine_relocs = object_file->section_maps[j].fine_relocs;
		    for(k = 0;
			k < object_file->section_maps[j].nfine_relocs;
			k++){
			print("\t\t%-6lu %-6lu\n",
			       fine_relocs[k].input_offset,
			       fine_relocs[k].output_offset);
		    }
		}
    		print("\tnundefineds %lu\n", object_file->nundefineds);
		for(j = 0; j < object_file->nundefineds; j++){
		    print("\t    (%lu,%s)\n",
			   object_file->undefined_maps[j].index,
			   object_file->undefined_maps[j].merged_symbol->nlist.
								n_un.n_name);
		}
#ifdef RLD
		print("\tset_num = %d\n", object_file->set_num);
#endif /* RLD */
	    }
	}
}

/*
 * print_fine_relocs() prints fine_relocs.  Used for debugging.
 */
__private_extern__
void
print_fine_relocs(
struct fine_reloc *fine_relocs,
unsigned long nfine_relocs,
char *string)
{
    unsigned long i;

	print("%s\n", string);
	for(i = 0; i < nfine_relocs; i++){
	    print("fine_reloc[%lu]\n", i);
	    print("\t         input_offset  0x%x\n",
		  (unsigned int)(fine_relocs[i].input_offset));
	    print("\t         output_offset 0x%x\n",
		  (unsigned int)(fine_relocs[i].output_offset));
	    print("\t      indirect_defined %s\n",
		  fine_relocs[i].indirect_defined == 1 ? "TRUE" : "FALSE");
	    print("\t          use_contents %s\n",
		  fine_relocs[i].use_contents == 1 ? "TRUE" : "FALSE");
	    print("\t          local_symbol %s\n",
		  fine_relocs[i].local_symbol == 1 ? "TRUE" : "FALSE");
	    print("\t                  live %s\n",
		  fine_relocs[i].live == 1 ? "TRUE" : "FALSE");
	    print("\t      refs_marked_live %s\n",
		  fine_relocs[i].refs_marked_live == 1 ? "TRUE" : "FALSE");
	    print("\tsearched_for_live_refs %s\n",
		  fine_relocs[i].searched_for_live_refs == 1 ? "TRUE" :"FALSE");
	    print("\t         merged_symbol 0x08%x %s\n",
		  (unsigned int)(fine_relocs[i].merged_symbol),
		  fine_relocs[i].merged_symbol == NULL ? "" :
		      fine_relocs[i].merged_symbol->nlist.n_un.n_name);
	}
}
#endif /* DEBUG */
