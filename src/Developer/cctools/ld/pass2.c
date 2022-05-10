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
 * This file contains the routines that drives pass2 of the link-editor.  In
 * pass2 the output is created and written.  The sections from the input files
 * are copied into the output and relocated.  The headers, relocation entries,
 * symbol table and string table are all copied into the output file.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <libc.h>
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/macosx_deployment_target.h"
#include "stuff/unix_standard_mode.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "fvmlibs.h"
#include "dylibs.h"
#include "sections.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "pass2.h"
#include "sets.h"
#include "indirect_sections.h"

/*
 * The total size of the output file and the memory buffer for the output file.
 */
__private_extern__ unsigned long output_size = 0;
__private_extern__ char *output_addr = NULL;

/*
 * This is used to setting the SG_NORELOC flag in the segment flags correctly.
 * This is an array of pointers to the merged sections in the output file that
 * is used by the relocation routines to set the field 'referenced' in the
 * merged section structure by indexing this array (directly without subtracting
 * one from the section number) with the section number of a merged symbol that
 * is refered to in a relocation entry.  The array is created by the routine
 * create_output_sections_array() in here. Then after the 'referenced' field is
 * set by the relocation routines (like generic_reloc() in generic_reloc.c) and
 * the 'relocated' field is set by output_section() in sections.c then the
 * routine set_SG_NORELOC_flags() in here can use these two fields to set the
 * SG_NORELOC flag in the segments that have no relocation to or for them.
 */
__private_extern__ struct merged_section **output_sections = NULL;

#ifndef RLD
/* the file descriptor of the output file */
static int fd = 0;

/*
 * This structure is used to describe blocks of the output file that are flushed
 * to the disk file with output_flush.  It is kept in an ordered list starting
 * with output_blocks.
 */
static struct block {
    unsigned long offset;	/* starting offset of this block */
    unsigned long size;		/* size of this block */
    unsigned long written_offset;/* first page offset after starting offset */
    unsigned long written_size;	/* size of written area from written_offset */
    struct block *next; /* next block in the list */
} *output_blocks;

static void setup_output_flush(void);
static void final_output_flush(void);
#ifdef DEBUG
static void print_block_list(void);
#endif /* DEBUG */
static struct block *get_block(void);
static void remove_block(struct block *block);
static unsigned long trnc(unsigned long v, unsigned long r);
#endif /* !defined(RLD) */
static void create_output_sections_array(void);
static void set_SG_NORELOC_flags(void);
static void output_headers(void);

/*
 * pass2() creates the output file and the memory buffer to create the file
 * into.  It drives the process to get everything copied into the buffer for
 * the output file.  It then writes the output file and deallocates the buffer.
 */ 
__private_extern__
void
pass2(void)
{
    unsigned long i, j, section_type;
    struct object_list *object_list, **p;
#ifndef RLD
    int mode;
    struct stat stat_buf;
    kern_return_t r;

	/*
	 * In UNIX standard conformance mode we are not allowed to replace
	 * a file that is not writeable.
	 */
	if(get_unix_standard_mode() == TRUE && 
	   access(outputfile, F_OK) == 0 &&
	   access(outputfile, W_OK) == -1)
	    system_fatal("can't write output file: %s", outputfile);

	/*
	 * Create the output file.  The unlink() is done to handle the problem
	 * when the outputfile is not writable but the directory allows the
	 * file to be removed (since the file may not be there the return code
	 * of the unlink() is ignored).
	 */
	(void)unlink(outputfile);
	if((fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0777)) == -1)
	    system_fatal("can't create output file: %s", outputfile);
#ifdef F_NOCACHE
        /* tell filesystem to NOT cache the file when reading or writing */
	(void)fcntl(fd, F_NOCACHE, 1);
#endif
	if(fstat(fd, &stat_buf) == -1)
	    system_fatal("can't stat file: %s", outputfile);
	/*
	 * Turn the execute bits on or off depending if there are any undefined
	 * symbols in the output file.  If the file existed before the above
	 * open() call the creation mode in that call would have been ignored
	 * so it has to be set explicitly in any case.
	 */
	if(output_mach_header.flags & MH_NOUNDEFS ||
	   (has_dynamic_linker_command && output_for_dyld))
	    mode = (stat_buf.st_mode & 0777) | (0111 & ~umask(0));
	else
	    mode = (stat_buf.st_mode & 0777) & ~0111;
	if(fchmod(fd, mode) == -1)
	    system_fatal("can't set execution permissions output file: %s",
			 outputfile);

	/*
	 * Create the buffer to copy the parts of the output file into.
	 */
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&output_addr,
			    output_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() buffer for output file of size "
		       "%lu", output_size);

	/*
	 * Set up for flushing pages to the output file as they fill up.
	 */
	if(flush)
	    setup_output_flush();

	/*
	 * Make sure pure_instruction sections are padded with nop's.
	 */
	nop_pure_instruction_scattered_sections();

#endif /* !defined(RLD) */

	/*
	 * The strings indexes for the merged string blocks need to be set
	 * before the dylib tables are output because the module names are in
	 * them as well as the merged symbol names.
	 */
	set_merged_string_block_indexes();

#ifndef RLD
	/*
	 * Copy the dylib tables into the output file.  This is done before the
	 * sections are outputted so that the indexes to the local and external
	 * relocation entries for each object can be used as running indexes as
	 * each section in the object is outputted.
	 */
	if(filetype == MH_DYLIB)
	    output_dylib_tables();
#endif /* !defined(RLD) */

	/*
	 * Create the array of pointers to merged sections in the output file
	 * so the relocation routines can use it to set the 'referenced' fields
	 * in the merged section structures.
	 */
	create_output_sections_array();

	/*
	 * Copy the merged literal sections and the sections created from files
	 * into the output object file.
	 */
	output_literal_sections();
#ifndef RLD
	output_sections_from_files();
#endif /* !defined(RLD) */

	/*
	 * For each non-literal content section in each object file loaded 
	 * relocate it into the output file (along with the relocation entries).
	 * Then relocate local symbols into the output file for the loaded
	 * objects.
	 */
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		/* print the object file name if tracing */
		if(trace){
		    print_obj_name(cur_obj);
		    print("\n");
		}
		if(cur_obj->dylib)
		    continue;
		if(cur_obj->bundle_loader)
		    continue;
		if(cur_obj->dylinker)
		    continue;
		if(cur_obj != base_obj){
		    for(j = 0; j < cur_obj->nsection_maps; j++){
			if(cur_obj->section_maps[j].s->flags & S_ATTR_DEBUG)
			    continue;
#ifdef RLD
			if(cur_obj->set_num == cur_set)
#endif /* RLD */
			{
			    section_type = (cur_obj->section_maps[j].s->flags &
                                                   SECTION_TYPE);
			    if(section_type == S_REGULAR ||
			       section_type == S_SYMBOL_STUBS ||
			       section_type == S_NON_LAZY_SYMBOL_POINTERS ||
			       section_type == S_LAZY_SYMBOL_POINTERS ||
			       section_type == S_COALESCED ||
			       section_type == S_MOD_INIT_FUNC_POINTERS ||
			       section_type == S_MOD_TERM_FUNC_POINTERS){
				output_section(&(cur_obj->section_maps[j]));
			    }
			}
		    }
		}
		output_local_symbols();
#ifdef VM_SYNC_DEACTIVATE
		vm_msync(mach_task_self(), (vm_address_t)cur_obj->obj_addr,
			 (vm_size_t)cur_obj->obj_size, VM_SYNC_DEACTIVATE);
#endif /* VM_SYNC_DEACTIVATE */
	    }
	}
	/*
	 * If there were errors in output_section() then return as so not
	 * to cause later internal errors.
	 */
	if(errors != 0)
	    return;

#ifdef RLD
	/*
	 * For each content section clean up the data structures not needed
	 * after rld is run.  This must be done after ALL the sections are
	 * output'ed because the fine relocation entries could be used by any
	 * of the sections.
	 */
	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		cur_obj = &(object_list->object_files[i]);
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(cur_obj->section_maps[j].nfine_relocs != 0){
			free(cur_obj->section_maps[j].fine_relocs);
			cur_obj->section_maps[j].fine_relocs = NULL;
			cur_obj->section_maps[j].nfine_relocs = 0;
		    }
		}
		if(cur_obj->nundefineds != 0){
		    free(cur_obj->undefined_maps);
		    cur_obj->undefined_maps = NULL;
		    cur_obj->nundefineds = 0;
		}
	    }
	}
#endif /* RLD */

	/*
	 * Set the SG_NORELOC flag in the segments that had no relocation to
	 * or for them.
	 */
	set_SG_NORELOC_flags();

#ifndef SA_RLD
	/*
	 * Copy the indirect symbol table into the output file.
	 */
	output_indirect_symbols();
#endif /* SA_RLD */

	/*
	 * Copy the merged symbol table into the output file.
	 */
	output_merged_symbols();

	/*
	 * Copy the headers into the output file.
	 */
	output_headers();

#ifndef RLD
	if(flush){
	    /*
	     * Flush the sections that have been scatter loaded.
	     */
	    flush_scatter_copied_sections();
	    /*
	     * flush the remaining part of the object file that is not a full
	     * page.
	     */
	    final_output_flush();
	}
	else{
	    /*
	     * Write the entire object file.
	     */
	    if(write(fd, output_addr, output_size) != (int)output_size)
		system_fatal("can't write output file");

	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)output_addr,
				  output_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() buffer for output file");
	}
#ifdef F_NOCACHE
	/* re-enable caching of file reads/writes */
	(void)fcntl(fd, F_NOCACHE, 0);
#endif
	if(close(fd) == -1)
	    system_fatal("can't close output file");
#endif /* RLD */
}

#if defined(RLD) && !defined(SA_RLD)
/*
 * pass2_rld_symfile() drives the process to get everything copied into the
 * buffer for the output file.
 */ 
__private_extern__
void
pass2_rld_symfile(void)
{
	/*
	 * Copy the merged symbol table into the output file.
	 */
	output_rld_symfile_merged_symbols();

	/*
	 * Copy the headers into the output file.
	 */
	/* first the mach header */
	memcpy(output_addr, &output_mach_header, sizeof(struct mach_header));

	/* next the symbol table load command */
	memcpy(output_addr + sizeof(struct mach_header),
	       &(output_symtab_info.symtab_command),
	       output_symtab_info.symtab_command.cmdsize);
}
#endif /* defined(RLD) && !defined(SA_RLD) */

/*
 * create_output_sections_array() creates the output_sections array and fills
 * it in with the pointers to the merged sections in the output file.  This 
 * is used by the relocation routines to set the field 'referenced' in the
 * merged section structure by indexing this array (directly without subtracting
 * one from the section number) with the section number of a merged symbol that
 * is refered to in a relocation entry.
 */
__private_extern__
void
create_output_sections_array(void)
{
    unsigned long i, nsects;
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;

	nsects = 1;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    nsects += msg->sg.nsects;
	    p = &(msg->next);
	}

	output_sections = (struct merged_section **)
			  allocate(nsects * sizeof(struct merged_section *));

	i = 1;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		output_sections[i++] = ms;
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		output_sections[i++] = ms;
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * set_SG_NORELOC_flags() sets the SG_NORELOC flag in the segment that have no
 * relocation to or from them.  This uses the fields 'referenced' and
 * 'relocated' in the merged section structures.  The array that was created
 * by the routine create_output_sections_array() to help set the above
 * 'referenced' field is deallocated in here.
 */
static
void
set_SG_NORELOC_flags(void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;
    enum bool relocated, referenced;

	free(output_sections);
	output_sections = NULL;

	p = &merged_segments;
	while(*p){
	    relocated = FALSE;
	    referenced = FALSE;
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->relocated == TRUE)
		    relocated = TRUE;
		if(ms->referenced == TRUE)
		    referenced = TRUE;
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		/* a zero fill section can't be relocated */
		if(ms->referenced == TRUE)
		    referenced = TRUE;
		zerofill = &(ms->next);
	    }
	    if(relocated == FALSE && referenced == FALSE)
		msg->sg.flags |= SG_NORELOC;
	    p = &(msg->next);
	}
}

#ifndef RLD
/*
 * setup_output_flush() flushes the gaps between things in the file that are
 * holes created by alignment.  This must stay in lock step with the layout
 * routine that lays out the file (layout_segments() in layout.c).
 */
static
void
setup_output_flush(void)
{
    unsigned long offset;
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;

	offset = sizeof(struct mach_header) + output_mach_header.sizeofcmds;

	/* the offsets to the contents of the sections */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->s.size != 0){
		    if(ms->s.offset != offset)
			output_flush(offset, ms->s.offset - offset);
		    offset = ms->s.offset + ms->s.size;
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}

	/* the offsets to the relocation entries */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->s.nreloc != 0){
		    if(ms->s.reloff != offset)
			output_flush(offset, ms->s.reloff - offset);
		    offset = ms->s.reloff +
			     ms->s.nreloc * sizeof(struct relocation_info);
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
	if(output_dysymtab_info.dysymtab_command.nlocrel != 0){
	    output_flush(offset,
			 output_dysymtab_info.dysymtab_command.locreloff -
			 offset);
	    offset = output_dysymtab_info.dysymtab_command.locreloff +
		     output_dysymtab_info.dysymtab_command.nlocrel * 
		     sizeof(struct relocation_info);
	}
	if(output_for_dyld){
	    if(strip_level != STRIP_ALL){
		/* the offset to the symbol table */
		if(output_symtab_info.symtab_command.symoff != offset)
		    output_flush(offset,
			output_symtab_info.symtab_command.symoff - offset);
		offset = output_symtab_info.symtab_command.symoff +
			 output_symtab_info.symtab_command.nsyms *
							sizeof(struct nlist);
	    }
	}
	if(output_for_dyld && twolevel_namespace == TRUE &&
	   twolevel_namespace_hints == TRUE){
	    output_flush(offset,
			 output_hints_info.twolevel_hints_command.offset -
			 offset);
	    offset = output_hints_info.twolevel_hints_command.offset +
		     output_hints_info.twolevel_hints_command.nhints *
		     sizeof(struct twolevel_hint);
	}
	if(output_dysymtab_info.dysymtab_command.nextrel != 0){
	    output_flush(offset,
			 output_dysymtab_info.dysymtab_command.extreloff -
			 offset);
	    offset = output_dysymtab_info.dysymtab_command.extreloff +
		     output_dysymtab_info.dysymtab_command.nextrel *
		     sizeof(struct relocation_info);
	}
	/* the offset to the indirect symbol table */
	if(output_dysymtab_info.dysymtab_command.nindirectsyms != 0){
	    if(output_dysymtab_info.dysymtab_command.indirectsymoff != offset)
		output_flush(offset, output_dysymtab_info.
			     dysymtab_command.indirectsymoff - offset);
	    offset = output_dysymtab_info.dysymtab_command.indirectsymoff +
		     output_dysymtab_info.dysymtab_command.nindirectsyms *
							sizeof(unsigned long);
	}
#ifndef RLD
	/* the offset to the dylib table of contents */
	if(output_dysymtab_info.dysymtab_command.ntoc != 0){
	    if(output_dysymtab_info.dysymtab_command.tocoff != offset)
		output_flush(offset, output_dysymtab_info.
			     dysymtab_command.tocoff - offset);
	    offset = output_dysymtab_info.dysymtab_command.tocoff +
		     output_dysymtab_info.dysymtab_command.ntoc *
					sizeof(struct dylib_table_of_contents);
	}

	/* the offset to the dylib module table */
	if(output_dysymtab_info.dysymtab_command.nmodtab != 0){
	    if(output_dysymtab_info.dysymtab_command.modtaboff != offset)
		output_flush(offset, output_dysymtab_info.
			     dysymtab_command.modtaboff - offset);
	    offset = output_dysymtab_info.dysymtab_command.modtaboff +
		     output_dysymtab_info.dysymtab_command.nmodtab *
					sizeof(struct dylib_module);
	}

	/* the offset to the dylib reference table */
	if(output_dysymtab_info.dysymtab_command.nextrefsyms != 0){
	    if(output_dysymtab_info.dysymtab_command.extrefsymoff != offset)
		output_flush(offset, output_dysymtab_info.
			     dysymtab_command.extrefsymoff - offset);
	    offset = output_dysymtab_info.dysymtab_command.extrefsymoff +
		     output_dysymtab_info.dysymtab_command.nextrefsyms *
					sizeof(struct dylib_reference);
	}
#endif /* !defined(RLD) */

	if(output_for_dyld == FALSE){
	    if(strip_level != STRIP_ALL){
		/* the offset to the symbol table */
		if(output_symtab_info.symtab_command.symoff != offset)
		    output_flush(offset,
			output_symtab_info.symtab_command.symoff - offset);
		offset = output_symtab_info.symtab_command.symoff +
			 output_symtab_info.symtab_command.nsyms *
							sizeof(struct nlist);
	    }
	}

	if(strip_level != STRIP_ALL){
	    /* the offset to the string table */
	    /*
	     * This is flushed to output_symtab_info.symtab_command.stroff plus
	     * output_symtab_info.output_merged_strsize and not just to
	     * output_symtab_info.symtab_command.stroff because the first byte
	     * can't be used to store a string because a symbol with a string
	     * offset of zero (nlist.n_un.n_strx == 0) is defined to be a symbol
	     * with a null name "".  So this byte(s) have to be flushed.
	     */
	    if(output_symtab_info.symtab_command.stroff +
	       output_symtab_info.output_merged_strsize != offset)
		output_flush(offset, output_symtab_info.symtab_command.stroff +
			     output_symtab_info.output_merged_strsize - offset);
	    /* flush the string table pad if any */
	    if(output_symtab_info.output_strpad != 0){
		output_flush(output_symtab_info.symtab_command.stroff +
			     output_symtab_info.symtab_command.strsize -
			     output_symtab_info.output_strpad,
			     output_symtab_info.output_strpad);
	    }
	    offset = output_symtab_info.symtab_command.stroff +
		     output_symtab_info.symtab_command.strsize;
	}

	/* the offset to the end of the file */
	if(offset != output_size)
	    output_flush(offset, output_size - offset);
}

/*
 * output_flush() takes an offset and a size of part of the output file, known
 * in the comments as the new area, and causes any fully flushed pages to be
 * written to the output file the new area in combination with previous areas
 * creates.  The data structure output_blocks has ordered blocks of areas that
 * have been flushed which are maintained by this routine.  Any area can only
 * be flushed once and an error will result is the new area overlaps with a
 * previously flushed area.
 *
 * The goal of this is to again minimize the number of dirty pages the link
 * editor has and hopfully improve performance in a memory starved system and
 * to prevent these pages to be written to the swap area when they could just be
 * written to the output file (if only external pagers worked well ...).
 */
__private_extern__
void
output_flush(
unsigned long offset,
unsigned long size)
{ 
    unsigned long write_offset, write_size;
    struct block **p, *block, *before, *after;
    kern_return_t r;

	if(flush == FALSE)
	    return;

/*
if(offset == 588824 && size != 0)
printf("in output_flush() offset = %lu size = %lu\n", offset, size);
*/

	if(offset + size > output_size)
	    fatal("internal error: output_flush(offset = %lu, size = %lu) out "
		  "of range for output_size = %lu", offset, size, output_size);

#ifdef DEBUG
	if(debug & (1 << 12))
	    print_block_list();
	if(debug & (1 << 11))
	    print("output_flush(offset = %lu, size %lu)", offset, size);
#endif /* DEBUG */

	if(size == 0){
#ifdef DEBUG
	if(debug & (1 << 11))
	    print("\n");
#endif /* DEBUG */
	    return;
	}

	/*
	 * Search through the ordered output blocks to find the block before the
	 * new area and after the new area if any exist.
	 */
	before = NULL;
	after = NULL;
	p = &(output_blocks);
	while(*p){
	    block = *p;
	    if(offset < block->offset){
		after = block;
		break;
	    }
	    else{
		before = block;
	    }
	    p = &(block->next);
	}

	/*
	 * Check for overlap of the new area with the block before and after the
	 * new area if there are such blocks.
	 */
	if(before != NULL){
	    if(before->offset + before->size > offset){
		warning("internal error: output_flush(offset = %lu, size = %lu) "
		      "overlaps with flushed block(offset = %lu, size = %lu)",
		      offset, size, before->offset, before->size);
		printf("calling abort()\n");	
		abort();
	    }
	}
	if(after != NULL){
	    if(offset + size > after->offset){
		warning("internal error: output_flush(offset = %lu, size = %lu) "
		      "overlaps with flushed block(offset = %lu, size = %lu)",
		      offset, size, after->offset, after->size);
		printf("calling abort()\n");	
		abort();
	    }
	}

	/*
	 * Now see how the new area fits in with the blocks before and after it
	 * (that is does it touch both, one or the other or neither blocks).
	 * For each case first the offset and size to write (write_offset and
	 * write_size) are set for the area of full pages that can now be
	 * written from the block.  Then the area written in the block
	 * (->written_offset and ->written_size) are set to reflect the total
	 * area in the block now written.  Then offset and size the block
	 * refers to (->offset and ->size) are set to total area of the block.
	 * Finally the links to others blocks in the list are adjusted if a
	 * block is added or removed.
	 *
	 * See if there is a block before the new area and the new area
	 * starts at the end of that block.
	 */
	if(before != NULL && before->offset + before->size == offset){
	    /*
	     * See if there is also a block after the new area and the new area
	     * ends at the start of that block.
	     */
	    if(after != NULL && offset + size == after->offset){
		/*
		 * This is the case where the new area exactly fill the area
		 * between two existing blocks.  The total area is folded into
		 * the block before the new area and the block after the new
		 * area is removed from the list.
		 */
		if(before->offset == 0 && before->written_size == 0){
		    write_offset = 0;
		    before->written_offset = 0;
		}
		else
		    write_offset =before->written_offset + before->written_size;
		if(after->written_size == 0)
		    write_size = trnc(after->offset + after->size -
				      write_offset, host_pagesize);
		else
		    write_size = trnc(after->written_offset - write_offset,
				      host_pagesize);
		if(write_size != 0){
		    before->written_size += write_size;
		}
		if(after->written_size != 0)
		    before->written_size += after->written_size;
		before->size += size + after->size;

		/* remove the block after the new area */
		before->next = after->next;
		remove_block(after);
	    }
	    else{
		/*
		 * This is the case where the new area starts at the end of the
		 * block just before it but does not end where the block after
		 * it (if any) starts.  The new area is folded into the block
		 * before the new area.
		 */
		write_offset = before->written_offset + before->written_size;
		write_size = trnc(offset + size - write_offset, host_pagesize);
		if(write_size != 0)
		    before->written_size += write_size;
		before->size += size;
	    }
	}
	/*
	 * See if the new area and the new area ends at the start of the block
	 * after it (if any).
	 */
	else if(after != NULL && offset + size == after->offset){
	    /*
	     * This is the case where the new area ends at the begining of the
	     * block just after it but does not start where the block before it.
	     * (if any) ends.  The new area is folded into this block after the
	     * new area.
	     */
	    write_offset = rnd(offset, host_pagesize);
	    if(after->written_size == 0)
		write_size = trnc(after->offset + after->size - write_offset,
				  host_pagesize);
	    else
		write_size = trnc(after->written_offset - write_offset,
				  host_pagesize);
	    if(write_size != 0){
		after->written_offset = write_offset;
		after->written_size += write_size;
	    }
	    else if(write_offset != after->written_offset){
		after->written_offset = write_offset;
	    }
	    after->offset = offset;
	    after->size += size;
	}
	else{
	    /*
	     * This is the case where the new area neither starts at the end of
	     * the block just before it (if any) or ends where the block after
	     * it (if any) starts.  A new block is created and the new area is
	     * is placed in it.
	     */
	    write_offset = rnd(offset, host_pagesize);
	    write_size = trnc(offset + size - write_offset, host_pagesize);
	    block = get_block();
	    block->offset = offset;
	    block->size = size;
	    block->written_offset = write_offset;
	    block->written_size = write_size;
	    /*
	     * Insert this block in the ordered list in the correct place.
	     */
	    if(before != NULL){
		block->next = before->next;
		before->next = block;
	    }
	    else{
		block->next = output_blocks;
		output_blocks = block;
	    }
	}

	/*
	 * Now if there are full pages to write write them to the output file.
	 */
	if(write_size != 0){
#ifdef DEBUG
	if((debug & (1 << 11)) || (debug & (1 << 10)))
	    print(" writing (write_offset = %lu write_size = %lu)\n",
		   write_offset, write_size);
#endif /* DEBUG */
	    lseek(fd, write_offset, L_SET);
	    if(write(fd, output_addr + write_offset, write_size) !=
	       (int)write_size)
		system_fatal("can't write to output file");
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)(output_addr +
				  write_offset), write_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() buffer for output file");
	}
#ifdef DEBUG
	else{
	    if(debug & (1 << 11))
		print(" no write\n");
	}
#endif /* DEBUG */
}

/*
 * final_output_flush() flushes the last part of the last page of the object
 * file if it does not round out to exactly a page.
 */
static
void
final_output_flush(void)
{ 
    struct block *block;
    unsigned long write_offset, write_size;
    kern_return_t r;

#ifdef DEBUG
	/* The compiler "warning: `write_offset' may be used uninitialized in */
	/* this function" can safely be ignored */
	write_offset = 0;
	if((debug & (1 << 11)) || (debug & (1 << 10))){
	    print("final_output_flush block_list:\n");
	    print_block_list();
	}
#endif /* DEBUG */

	write_size = 0;
	block = output_blocks;
	if(block != NULL){
	    if(block->offset != 0)
		fatal("internal error: first block not at offset 0");
	    if(block->written_size != 0){
		if(block->written_offset != 0)
		    fatal("internal error: first block written_offset not 0");
		write_offset = block->written_size;
		write_size = block->size - block->written_size;
	    }
	    else{
		write_offset = block->offset;
		write_size = block->size;
	    }
	    if(block->next != NULL)
		fatal("internal error: more than one block in final list");
	}
	if(write_size != 0){
#ifdef DEBUG
	    if((debug & (1 << 11)) || (debug & (1 << 10)))
		print(" writing (write_offset = %lu write_size = %lu)\n",
		       write_offset, write_size);
#endif /* DEBUG */
	    lseek(fd, write_offset, L_SET);
	    if(write(fd, output_addr + write_offset, write_size) !=
	       (int)write_size)
		system_fatal("can't write to output file");
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)(output_addr +
				  write_offset), write_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() buffer for output file");
	}
}

#ifdef DEBUG
/*
 * print_block_list() prints the list of blocks.  Used for debugging.
 */
static
void
print_block_list(void)
{
    struct block **p, *block;

	p = &(output_blocks);
	if(*p == NULL)
	    print("Empty block list\n");
	while(*p){
	    block = *p;
	    print("block 0x%x\n", (unsigned int)block);
	    print("    offset %lu\n", block->offset);
	    print("    size %lu\n", block->size);
	    print("    written_offset %lu\n", block->written_offset);
	    print("    written_size %lu\n", block->written_size);
	    print("    next 0x%x\n", (unsigned int)(block->next));
	    p = &(block->next);
	}
}
#endif /* DEBUG */

/*
 * get_block() returns a pointer to a new block.  This could be done by
 * allocating block of these placing them on a free list and and handing them
 * out.  The maximum number of blocks needed would be one for each content
 * section, one for each section that has relocation entries (if saving them)
 * and one for the symbol and string table.  For the initial release of this
 * code this number is typicly around 8 it is not a big win so each block is
 * just allocated and free'ed.
 */
static
struct block *
get_block(void)
{
    struct block *block;

	block = allocate(sizeof(struct block));
	return(block);
}

/*
 * remove_block() throws away the block specified.  See comments in get_block().
 */
static
void
remove_block(
struct block *block)
{
	free(block);
}

/*
 * trnc() truncates the value 'v' to the power of two value 'r'.  If v is
 * less than zero it returns zero.
 */
static
unsigned long
trnc(
unsigned long v,
unsigned long r)
{
	if(((long)v) < 0)
	    return(0);
	return(v & ~(r - 1));
}
#endif /* !defined(RLD) */

/*
 * output_headers() copys the headers of the object file into the buffer for
 * the output file.
 */
static
void
output_headers(void)
{
    unsigned long header_offset;
    struct merged_segment **p, *msg;
    struct merged_section **content, **zerofill, *ms;
#ifndef RLD
    unsigned long i;
    struct merged_fvmlib **q, *mfl;
    struct dylinker_command *dyld;
    struct merged_dylib *mdl;
    struct dylib_command *dl;
    struct dynamic_library *dp;
    enum bool some_symbols_referenced, some_non_weak_refs;
#endif /* !defined(RLD) */
    struct mach_header *mh;
    struct load_command *lc;

	header_offset = 0;

	/* first the mach header */
	mh = (struct mach_header *)output_addr;
	memcpy(mh, &output_mach_header, sizeof(struct mach_header));
	header_offset += sizeof(struct mach_header);
	lc = (struct load_command *)(output_addr + header_offset);

	/* next the segment load commands (and section structures) */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    memcpy(output_addr + header_offset, &(msg->sg),
		   sizeof(struct segment_command));
	    header_offset += sizeof(struct segment_command);
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		memcpy(output_addr + header_offset, &(ms->s),
		       sizeof(struct section));
		header_offset += sizeof(struct section);
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		memcpy(output_addr + header_offset, &(ms->s),
		       sizeof(struct section));
		header_offset += sizeof(struct section);
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}

#ifndef RLD
	/* next the fixed VM shared library load commands */
	q = &merged_fvmlibs;
	while(*q){
	    mfl = *q;
	    memcpy(output_addr + header_offset, mfl->fl, mfl->fl->cmdsize);
	    header_offset += mfl->fl->cmdsize;
	    q = &(mfl->next);
	}

	/* next the dynamic linker load command */
	if(merged_dylinker != NULL){
	    memcpy(output_addr + header_offset, merged_dylinker->dyld,
		   merged_dylinker->dyld->cmdsize);
	    if(filetype != MH_DYLINKER){
		dyld = (struct dylinker_command *)(output_addr + header_offset);
		dyld->cmd = LC_LOAD_DYLINKER;
	    }
	    header_offset += merged_dylinker->dyld->cmdsize;
	}

	/* next the dynamicly linked shared library load commands */
	mdl = merged_dylibs;
	while(mdl != NULL){
	    memcpy(output_addr + header_offset, mdl->dl, mdl->dl->cmdsize);
	    if(mdl->output_id == FALSE){
		dl = (struct dylib_command *)(output_addr + header_offset);
		/*
		 * Propagate the some_non_weak_refs and some_non_weak_refs 
		 * booleans up through the sub images for this dylib.
		 */
		some_symbols_referenced =
		    mdl->dynamic_library->some_symbols_referenced;
		some_non_weak_refs = 
		    mdl->dynamic_library->some_non_weak_refs;
		for(i = 0; i < mdl->dynamic_library->nsub_images; i++){
		    if(mdl->dynamic_library->sub_images[i]->
		       some_symbols_referenced == TRUE){
			some_symbols_referenced = TRUE;
			if(mdl->dynamic_library->sub_images[i]->
			   some_non_weak_refs == TRUE)
			    some_non_weak_refs = TRUE;
		    }
		}
		if((some_symbols_referenced == TRUE &&
		    some_non_weak_refs == FALSE) ||
		    mdl->dynamic_library->force_weak_dylib == TRUE){
		    if(macosx_deployment_target.major >= 2){
			dl->cmd = LC_LOAD_WEAK_DYLIB;
		    }
		    else{
			warning("dynamic shared library: %s not made a weak "
				"library in output with "
				"MACOSX_DEPLOYMENT_TARGET environment variable "
				"set to: %s", mdl->definition_object->file_name,
				macosx_deployment_target.name);
			dl->cmd = LC_LOAD_DYLIB;
		    }
		}
		else
		    dl->cmd = LC_LOAD_DYLIB;
	    }
	    header_offset += mdl->dl->cmdsize;
	    mdl = mdl->next;
	}

	/* next the sub framework load command */
	if(merged_sub_framework != NULL){
	    memcpy(output_addr + header_offset, merged_sub_framework->sub,
		   merged_sub_framework->sub->cmdsize);
	    header_offset += merged_sub_framework->sub->cmdsize;
	}

	/* next the sub umbrella load commands */
	for(i = 0; i < nsub_umbrellas ; i++){
	    memcpy(output_addr + header_offset, merged_sub_umbrellas[i].sub,
		   merged_sub_umbrellas[i].sub->cmdsize);
	    header_offset += merged_sub_umbrellas[i].sub->cmdsize;
	}

	/* next the sub library load commands */
	for(i = 0; i < nsub_librarys ; i++){
	    memcpy(output_addr + header_offset, merged_sub_librarys[i].sub,
		   merged_sub_librarys[i].sub->cmdsize);
	    header_offset += merged_sub_librarys[i].sub->cmdsize;
	}

	/* next the sub client load commands */
	for(i = 0; i < nallowable_clients ; i++){
	    memcpy(output_addr + header_offset, merged_sub_clients[i].sub,
		   merged_sub_clients[i].sub->cmdsize);
	    header_offset += merged_sub_clients[i].sub->cmdsize;
	}

	/* next the prebound dynamic libraries load commands */
	if(filetype == MH_EXECUTE){
	    for(dp = dynamic_libs; dp != NULL; dp = dp->next){
		if(dp->type == DYLIB){
		    if(dp->pbdylib != NULL){
			memcpy(output_addr + header_offset, dp->pbdylib,
			       dp->pbdylib->cmdsize);
			header_offset += dp->pbdylib->cmdsize;
		    }
		}
	    }
	}
#endif /* !defined(RLD) */

	/* next the symbol table load command */
	memcpy(output_addr + header_offset,
	       &(output_symtab_info.symtab_command),
	       output_symtab_info.symtab_command.cmdsize);
	header_offset += output_symtab_info.symtab_command.cmdsize;

	/* next the dysymbol table load command */
	if(nindirectsyms != 0 || output_for_dyld){
	    memcpy(output_addr + header_offset,
		   &(output_dysymtab_info.dysymtab_command),
		   output_dysymtab_info.dysymtab_command.cmdsize);
	    header_offset += output_dysymtab_info.dysymtab_command.cmdsize;
	}

	/* next the two-level namespace hints load command */
	if(output_for_dyld && twolevel_namespace == TRUE &&
	   twolevel_namespace_hints == TRUE){
	    memcpy(output_addr + header_offset,
	    	   &(output_hints_info.twolevel_hints_command),
		   output_hints_info.twolevel_hints_command.cmdsize);
	    header_offset += output_hints_info.twolevel_hints_command.cmdsize;
	}

	/* next the prebind cksum load command */
	if(output_cksum_info.prebind_cksum_command.cmdsize != 0){
	    memcpy(output_addr + header_offset,
	    	   &(output_cksum_info.prebind_cksum_command),
		   output_cksum_info.prebind_cksum_command.cmdsize);
	    header_offset += output_cksum_info.prebind_cksum_command.cmdsize;
	}

	/* next the uuid load command */
	if(output_uuid_info.uuid_command.cmdsize != 0){
	    memcpy(output_addr + header_offset,
	    	   &(output_uuid_info.uuid_command),
		   output_uuid_info.uuid_command.cmdsize);
	    header_offset += output_uuid_info.uuid_command.cmdsize;
	}

	/* next the thread command if the output file has one */
	if(output_thread_info.thread_in_output == TRUE){
	    /* the thread command itself */
	    memcpy(output_addr + header_offset,
		   &(output_thread_info.thread_command),
		   sizeof(struct thread_command));
	    header_offset += sizeof(struct thread_command);
	    /* the flavor of the thread state */
	    memcpy(output_addr + header_offset, &(output_thread_info.flavor),
		   sizeof(long));
	    header_offset += sizeof(long);
	    /* the count of longs of the thread state */
	    memcpy(output_addr + header_offset, &(output_thread_info.count),
		   sizeof(long));
	    header_offset += sizeof(long);
	    /* the thread state */
	    memcpy(output_addr + header_offset, output_thread_info.state,
		   output_thread_info.count * sizeof(long));
	    header_offset += output_thread_info.count * sizeof(long);
	    /* the second thread state if any */
	    if(output_thread_info.second_count != 0){
		/* the flavor of the second thread state */
		memcpy(output_addr + header_offset,
		       &(output_thread_info.second_flavor),
		       sizeof(long));
		header_offset += sizeof(long);
		/* the count of longs of the second thread state */
		memcpy(output_addr + header_offset,
		       &(output_thread_info.second_count),
		       sizeof(long));
		header_offset += sizeof(long);
		/* the second thread state */
		memcpy(output_addr + header_offset,
		       output_thread_info.second_state,
		       output_thread_info.second_count * sizeof(long));
		header_offset += output_thread_info.second_count * sizeof(long);
	    }
	}
	/* next the routines command if the output file has one */
	if(output_routines_info.routines_in_output == TRUE){
	    memcpy(output_addr + header_offset,
		   &(output_routines_info.routines_command),
		   sizeof(struct routines_command));
	    header_offset += sizeof(struct routines_command);
	}
	if(host_byte_sex != target_byte_sex)
	    if(swap_object_headers(mh, lc) == FALSE)
		fatal("internal error: swap_object_headers() failed in "
		      "output_headers()");
#ifndef RLD
	output_flush(0, sizeof(struct mach_header) +
			output_mach_header.sizeofcmds);
#endif /* !defined(RLD) */
}
