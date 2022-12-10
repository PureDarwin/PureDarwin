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

#define __srr0 srr0
#define __r1 r1
#define __eip eip
#define __esp esp
#define __es es
#define __ds ds
#define __ss ss
#define __cs cs

#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines that drives the layout phase of the
 * link-editor.  In this phase the output file's addresses and offset are
 * set up.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else
#include <mach/mach.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <sys/param.h>
#include "stuff/openstep_mach.h"
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#import <mach/m68k/thread_status.h>
#undef MACHINE_THREAD_STATE	/* need to undef these to avoid warnings */
#undef MACHINE_THREAD_STATE_COUNT
#undef THREAD_STATE_NONE
#undef VALID_THREAD_STATE_FLAVOR
#import <mach/ppc/thread_status.h>
#undef MACHINE_THREAD_STATE	/* need to undef these to avoid warnings */
#undef MACHINE_THREAD_STATE_COUNT
#undef THREAD_STATE_NONE
#undef VALID_THREAD_STATE_FLAVOR
#import <mach/m88k/thread_status.h>
#import <mach/i860/thread_status.h>
#import <mach/i386/thread_status.h>
#import <mach/hppa/thread_status.h>
#import <mach/sparc/thread_status.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#if defined(RLD) && !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
#include <mach-o/rld.h>
#include <streams/streams.h>
#endif /* defined(RLD) && !defined(SA_RLD) &&
	  !(defined(KLD) && defined(__STATIC__)) */

#include "stuff/arch.h"
#include "stuff/macosx_deployment_target.h"

#include "ld.h"
#include "specs.h"
#include "fvmlibs.h"
#include "dylibs.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "pass2.h"
#include "sets.h"
#include "mach-o/sarld.h"
#include "indirect_sections.h"
#include "uuid.h"

#ifdef RLD
__private_extern__ long RLD_DEBUG_OUTPUT_FILENAME_flag;
#endif

/* The output file's mach header */
__private_extern__ struct mach_header output_mach_header = { 0 };

/*
 * The output file's symbol table load command and the offsets used in the
 * second pass to output the symbol table and string table.
 */
__private_extern__ struct symtab_info output_symtab_info = { {0} };

/*
 * The output file's dynamic symbol table load command.
 */
__private_extern__ struct dysymtab_info output_dysymtab_info = { {0} };

/*
 * The output file's two level hints load command.
 */
__private_extern__ struct hints_info output_hints_info = { { 0 } };

/*
 * The output file's prebind_cksum load command.
 */
__private_extern__ struct cksum_info output_cksum_info = { { 0 } };

/*
 * The output file's UUID load command.
 */
__private_extern__ struct uuid_info output_uuid_info = { 0 };

/*
 * The output file's thread load command and the machine specific information
 * for it.
 */
__private_extern__ struct thread_info output_thread_info = { {0} };

/*
 * The output file's routines load command and the specific information for it.
 */
__private_extern__ struct routines_info output_routines_info = { {0} };

/*
 * The thread states that are currently known by this link editor.
 * (for the specific cputypes)
 */
/* cputype == CPU_TYPE_MC680x0, all cpusubtype's */
static struct m68k_thread_state_regs mc680x0 = { {0} };
/* cputype == CPU_TYPE_POWERPC, all cpusubtype's */
static ppc_thread_state_t powerpc = { 0 };
/* cputype == CPU_TYPE_MC88000, all cpusubtype's */
static m88k_thread_state_grf_t mc88000 = { 0 };
/* cputype == CPU_TYPE_I860, all cpusubtype's */
static struct i860_thread_state_regs i860 = { {0} };
/* cputype == CPU_TYPE_I386, all cpusubtype's */
static i386_thread_state_t intel386 = { 0 };
/* cputype == CPU_TYPE_HPPA, all cpusubtypes */
static struct hp_pa_frame_thread_state hppa_frame_state = { 0 };
static struct hp_pa_integer_thread_state hppa_integer_state = { 0 };
/* cputype == CPU_TYPE_SPARC, all subtypes */
static struct sparc_thread_state_regs sparc_state = { {0} };
/* cputype == CPU_TYPE_ARM, all subtypes */
static arm_thread_state_t arm_state = { {0} };

static void layout_segments(void);
static unsigned long next_vmaddr(
    unsigned long vmaddr,
    unsigned long vmsize);
static int qsort_vmaddr(
    const struct merged_segment **msg1,
    const struct merged_segment **msg2);
#ifndef RLD
static enum bool check_reserved_segment(char *segname,
					char *reserved_error_string);
static void check_overlap(struct merged_segment *msg1,
			  struct merged_segment *msg2,
			  enum bool prebind_check,
    			  struct merged_segment *outputs_linkedit_segment);
static void check_for_overlapping_segments(
    struct merged_segment *outputs_linkedit_segment);
static void check_for_lazy_pointer_relocs_too_far(void);
static void print_load_map(void);
static void print_load_map_for_objects(struct merged_section *ms);
static uint32_t get_stack_size_from_flag(const struct arch_flag *flag);
static vm_prot_t get_segprot_from_flag(const struct arch_flag *flag);
static int get_stack_direction_from_flag(const struct arch_flag *flag);

#endif /* !defined(RLD) */

/*
 * layout() is called from main() and lays out the output file.
 */
__private_extern__
void
layout(void)
{
#ifdef RLD
	memset(&output_mach_header, '\0', sizeof(struct mach_header));
	memset(&output_symtab_info, '\0', sizeof(struct symtab_info));
	memset(&output_dysymtab_info, '\0', sizeof(struct dysymtab_info));
	memset(&output_hints_info, '\0', sizeof(struct hints_info));
	memset(&output_cksum_info, '\0', sizeof(struct cksum_info));
#ifndef KLD
	memset(&output_uuid_info, '\0', sizeof(struct uuid_info));
#endif
	memset(&output_thread_info, '\0', sizeof(struct thread_info));
	memset(&mc680x0, '\0', sizeof(struct m68k_thread_state_regs));
	memset(&powerpc,     '\0', sizeof(ppc_thread_state_t));
	memset(&mc88000, '\0', sizeof(m88k_thread_state_grf_t));
	memset(&intel386,'\0', sizeof(i386_thread_state_t));
	intel386.es = USER_DATA_SELECTOR;
	intel386.ds = USER_DATA_SELECTOR;
	intel386.ss = USER_DATA_SELECTOR;
	intel386.cs = USER_CODE_SELECTOR;
	memset(&hppa_frame_state, '\0',
		sizeof(struct hp_pa_frame_thread_state));
	memset(&hppa_integer_state, '\0',
		sizeof(struct hp_pa_integer_thread_state));
	memset(&sparc_state, '\0', sizeof(struct sparc_thread_state_regs));
#endif /* RLD */
	/*
	 * First finish creating all sections that will be in the final output
	 * file.  This involves defining common symbols which can create a
	 * (__DATA,__common) section and creating sections from files (via
	 * -sectcreate options).
	 */
	define_common_symbols();
	/*
	 * Process the command line specifications for the sections including
	 * creating sections from files.
	 */
#ifndef RLD
	process_section_specs();
#endif /* !defined(RLD) */

	/*
	 * So literal pointer sections can use indirect symbols these need to
	 * be resolved before the literal pointer section is merged.
	 */
	reduce_indr_symbols();
	if(errors)
	    return;

#ifndef RLD
	/*
	 * Setup the link editor defined symbols if the output file type could
	 * be output for dyld. This is needed because the symbol needs to be
	 * defined and set to a private extern so that file can be laid out
	 * even though we don't know it's address at this point.
	 */
	if(filetype == MH_EXECUTE ||
	   filetype == MH_BUNDLE ||
	   filetype == MH_DYLIB ||
	   filetype == MH_DYLINKER){
	    setup_link_editor_symbols();
	    if(undefined_flag == UNDEFINED_DEFINE_A_WAY)
		define_undefined_symbols_a_way();
	}
	if(filetype == MH_PRELOAD)
	    define_link_editor_preload_symbols(TRUE);
#endif /* !defined(RLD) */

	/*
	 * Now that the alignment of all the sections has been determined (from
	 * the command line and the object files themselves) the literal
	 * sections can be merged with the correct alignment and their sizes
	 * in the output file can be determined.
	 */
	save_lazy_symbol_pointer_relocs = prebinding;
	merge_literal_sections(FALSE);
	if(errors)
	    return;
#ifdef DEBUG
	if(debug & (1 << 21))
	    print_merged_section_stats();
#endif /* DEBUG */

	/*
	 * Segments with only debug sections do not appear in the output.
	 * So before dead code stripping and laying out the segments and
	 * sections for the output remove them from the merged segment and
	 * merged sections list.
	 */
	remove_debug_segments();

#ifndef RLD
	/*
	 * Layout any sections that have -sectorder options specified for them.
	 */
	layout_ordered_sections();

	/*
	 * If -dead_strip is specified mark the fine_relocs that are live and
	 * update all the merged counts, and resize everything to only contain
	 * live items.
	 */
	if(dead_strip == TRUE){
	    /*
	     * Mark the fine_relocs and symbols that are live.
	     */
	    live_marking();

	    /*
	     * If live_marking() encountered a relocation error just return now.
	     */
	    if(errors)
		return;

	    /*
	     * Now with all the live fine_relocs and live merged symbols marked
	     * merged tables, calculated sizes and counts for everything that is
	     * live.
	     */
	    count_live_symbols();

	    /*
	     * Now resize all sections using the live block sizes and adjust
	     * the number of relocation entries so the counts includes entries
	     * only for live blocks.
	     */
	    resize_live_sections();

	    /*
	     * Now re-merge all the literal sections so that only live literals
	     * end up in the output.
	     */
	    merge_literal_sections(TRUE);
	}
#endif /* RLD */

	/*
	 * Report undefined symbols and account for the merged symbols that will
	 * not be in the output file.
	 */
	process_undefineds();
	if(errors)
	    return;

#ifndef RLD
	/*
	 * Check to make sure symbols are not overridden in dependent dylibs
	 * when prebinding.
	 */
	prebinding_check_for_dylib_override_symbols();

	/*
	 * If the users wants to see warnings about unused multiply defined
	 * symbols when -twolevel_namespace is in effect then check for them
	 * and print out a warning.
	 */
	if(nowarnings == FALSE &&
	   twolevel_namespace == TRUE &&
	   multiply_defined_unused_flag != MULTIPLY_DEFINED_SUPPRESS)
	    twolevel_namespace_check_for_unused_dylib_symbols();
#endif /* RLD */

	/*
	 * Assign the symbol table indexes for the symbol table entries.
	 */
	assign_output_symbol_indexes();

#ifndef RLD
	/*
	 * Layout the dynamic shared library tables if the output is a MH_DYLIB
	 * file.
	 */
	if(filetype == MH_DYLIB)
	    layout_dylib_tables();

	/*
	 * If the output is intended for the dynamic link editor or -dead_strip
	 * is specified relayout the relocation entries for only the ones that
	 * will be in the output file.
	 */
	if(output_for_dyld == TRUE || dead_strip == TRUE)
	    relayout_relocs();

	/*
	 * If the segment alignment is not set, set it based on the target
	 * architecture.
	 */
	if(segalign_specified == FALSE)
#endif /* !defined(RLD) */
	    segalign = get_segalign_from_flag(&arch_flag);

	/*
	 * Set the segment addresses, protections and offsets into the file.
	 */
	layout_segments();

#ifndef RLD
	/*
	 * For symbol from dylibs reset the prebound symbols if not prebinding.
	 */
	reset_prebound_undefines();

	if(load_map)
	    print_load_map();
#endif /* !defined(RLD) */

#ifdef DEBUG
	if(debug & (1 << 7)){
	    print_mach_header();
	    print_merged_sections("after layout");
	    print_symtab_info();
	    print_thread_info();
	}
	if(debug & (1 << 8))
	    print_symbol_list("after layout", FALSE);
	if(debug & (1 << 20))
	    print_object_list();
#endif /* DEBUG */
}

#if defined(RLD) && !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/*
 * layout_rld_symfile() is called from rld_write_symfile() and lays out the
 * output file.  This contains only a mach_header, a symtab load command the
 * symbol and string table for the current set of merged symbols.
 */
__private_extern__
void
layout_rld_symfile(void)
{
    unsigned long offset;
    kern_return_t r;

	memset(&output_mach_header, '\0', sizeof(struct mach_header));
	memset(&output_symtab_info, '\0', sizeof(struct symtab_info));

	/*
	 * Create the symbol table load command.
	 */
	output_symtab_info.symtab_command.cmd = LC_SYMTAB;
	output_symtab_info.symtab_command.cmdsize =
						sizeof(struct symtab_command);
	output_symtab_info.symtab_command.nsyms = nmerged_symbols;
	output_symtab_info.symtab_command.strsize =
	    rnd(merged_string_size + STRING_SIZE_OFFSET,
		  sizeof(unsigned long));
	output_symtab_info.output_strpad =
	    output_symtab_info.symtab_command.strsize -
	    (merged_string_size + STRING_SIZE_OFFSET);
	output_symtab_info.output_merged_strsize = STRING_SIZE_OFFSET;
	output_symtab_info.output_local_strsize = STRING_SIZE_OFFSET +
						      merged_string_size;
	/*
	 * Fill in the mach_header for the output file.
	 */
	output_mach_header.magic = MH_MAGIC;
	output_mach_header.cputype = arch_flag.cputype;
	output_mach_header.cpusubtype = arch_flag.cpusubtype;
	output_mach_header.filetype = filetype;
	output_mach_header.ncmds = 1;
	output_mach_header.sizeofcmds =
	 			output_symtab_info.symtab_command.cmdsize;
	output_mach_header.flags = 0;

	/*
	 * Lay everything out setting the offsets.
	 */
	offset = sizeof(struct mach_header) + output_mach_header.sizeofcmds;
	output_symtab_info.symtab_command.symoff = offset;
	offset += output_symtab_info.symtab_command.nsyms *
		  sizeof(struct nlist);
	output_symtab_info.symtab_command.stroff = offset;
	offset += output_symtab_info.symtab_command.strsize;

	/*
	 * Allocate the buffer for the output file.
	 */
	output_size = offset;
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&output_addr,
			    output_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() memory for output of size "
		       "%lu", output_size);
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() vm_allocate: addr = 0x%0x size = 0x%x\n",
	      (unsigned int)output_addr, (unsigned int)output_size);
#endif /* RLD_VM_ALLOC_DEBUG */
}
#endif /* defined(RLD) && !defined(SA_RLD) &&
	  !(defined(KLD) && defined(__STATIC__)) */

/*
 * layout_segments() basicly lays out the addresses and file offsets of
 * everything in the ouput file (since everything can be in a segment).
 * It checks for the link editor reserved segment "__PAGEZERO" and "__LINKEDIT"
 * and prints an error message if they exist in the output.  It creates these
 * segments if this is the right file type and the right options are specified.
 * It processes all the segment specifications from the command line options.
 * Sets the addresses of all segments and sections in those segments and sets
 * the sizes of all segments.  Also sets the file offsets of all segments,
 * sections, relocation information and symbol table information.  It creates
 * the mach header that will go in the output file.  It numbers the merged
 * sections with their section number they will have in the output file.
 */
static
void
layout_segments(void)
{
    unsigned long i, ncmds, sizeofcmds, headers_size, offset;
    unsigned long addr, size, max_first_align, pad, max_align;
    struct merged_segment **p, *msg, *first_msg;
    struct merged_section **content, **zerofill, *ms;
#ifndef RLD
    struct merged_fvmlib **q, *mfl;
    unsigned long nfvmlibs;
    struct segment_spec *seg_spec;
    enum bool address_zero_specified;
    struct merged_dylib *mdl;
    struct dynamic_library *dp;
#endif /* !defined(RLD) */
#ifdef RLD
#ifndef SA_RLD
    kern_return_t r;
#endif /* !defined SA_RLD */
    unsigned long allocate_size;
#endif /* defined(RLD) */
    struct merged_symbol *merged_symbol;

    static struct merged_segment linkedit_segment = { {0} };
    static struct merged_segment pagezero_segment = { {0} };
    static struct merged_segment stack_segment = { {0} };
    static struct merged_segment object_segment = { {0} };

#ifdef RLD
	memset(&object_segment, '\0', sizeof(struct merged_segment));
	original_merged_segments = merged_segments;
#endif /* RLD */

	/*
	 * If the file type is MH_OBJECT then place all the sections in one
	 * unnamed segment.
	 */
	if(filetype == MH_OBJECT){
	    object_segment.filename = outputfile;
	    content = &(object_segment.content_sections);
	    zerofill = &(object_segment.zerofill_sections);
	    p = &merged_segments;
	    while(*p){
		msg = *p;
		object_segment.sg.nsects += msg->sg.nsects;
		*content = msg->content_sections;
		while(*content){
		    ms = *content;
		    content = &(ms->next);
		}
		*zerofill = msg->zerofill_sections;
		while(*zerofill){
		    ms = *zerofill;
		    zerofill = &(ms->next);
		}
		p = &(msg->next);
	    }
	    if(object_segment.sg.nsects != 0)
		merged_segments = &object_segment;
	}

#ifndef RLD
	/*
	 * Set thread_in_output in the output_thread_info if we are going to
	 * create a thread command.  It is created if this is filetype is not a
	 * shared library or bundle and we have seen an object file so we know
	 * what type of machine the thread is for.  Or if we haven't seen an
	 * object file but entry point symbol name was specified.
	 */
 	if(filetype != MH_FVMLIB &&
	   filetype != MH_DYLIB &&
	   filetype != MH_BUNDLE &&
	   arch_flag.cputype != 0 &&
 	   ((merged_segments != NULL &&
	     merged_segments->content_sections != NULL) ||
	     entry_point_name != NULL)){

	    output_thread_info.thread_in_output = TRUE;

	    if(filetype == MH_DYLINKER)
		output_thread_info.thread_command.cmd = LC_THREAD;
	    else
		output_thread_info.thread_command.cmd = LC_UNIXTHREAD;

	    /*
	     * If the stack address or size is set then create the stack segment
	     * for it.
	     */
	    if((stack_addr_specified == TRUE || stack_size_specified == TRUE) &&
	       output_thread_info.thread_command.cmd == LC_UNIXTHREAD){
		if(check_reserved_segment(SEG_UNIXSTACK, "segment "
			SEG_UNIXSTACK " reserved for the -stack_addr and "
			"-stack_size options")){
		    /*
		     * There shouldn't be any segment specifications for this
		     * segment except protection. Protection must be at least
		     * rw- and defaults to architecure's segment protection
		     * default for both initial and maximum protection.
		     */
		    seg_spec = lookup_segment_spec(SEG_UNIXSTACK);
		    if(seg_spec != NULL){
			if(seg_spec->addr_specified)
			    error("specified address for segment " SEG_UNIXSTACK
				  " not allowed (segment " SEG_UNIXSTACK
				  " reserved for unix stack, use -stack_addr)");
			if(seg_spec->prot_specified){
			    if((seg_spec->maxprot &
			        (VM_PROT_READ | VM_PROT_WRITE)) !=
				(VM_PROT_READ | VM_PROT_WRITE)){
				error("specified maximum protection for "
				  "segment " SEG_UNIXSTACK " must include read "
				  " and write");
				seg_spec->maxprot |= (VM_PROT_READ |
						      VM_PROT_WRITE);
			    }
			    if((seg_spec->initprot &
			        (VM_PROT_READ | VM_PROT_WRITE)) !=
				(VM_PROT_READ | VM_PROT_WRITE)){
				error("specified initial protection for "
				  "segment " SEG_UNIXSTACK " must include read "
				  " and write");
				seg_spec->initprot |= (VM_PROT_READ |
						       VM_PROT_WRITE);
			    }
			    stack_segment.sg.maxprot = seg_spec->maxprot;
			    stack_segment.sg.initprot = seg_spec->initprot;
			    /*
			     * Only if the protection of the static is specified
			     * to include execute permision do we also cause the
			     * MH_ALLOW_STACK_EXECUTION bit to get set.
			     */
			    if((stack_segment.sg.maxprot & VM_PROT_EXECUTE) ==
			       VM_PROT_EXECUTE)
				allow_stack_execute = TRUE;
			}
			seg_spec->processed = TRUE;
			stack_segment.prot_set = TRUE;
		    }
		    else{
			stack_segment.sg.maxprot =
			    get_segprot_from_flag(&arch_flag);
			stack_segment.sg.initprot = stack_segment.sg.maxprot;
			stack_segment.prot_set = TRUE;
		    }
		    if(stack_addr_specified == TRUE){
			if(stack_addr % segalign != 0)
			    fatal("-stack_addr: 0x%x not a multiple of the "
				  "segment alignment (0x%x)",
				  (unsigned int)stack_addr,
				  (unsigned int)segalign);
		    }
		    else{
			stack_addr = get_stack_addr_from_flag(&arch_flag);
			warning("no -stack_addr specified using the default "
				"addr: 0x%x", (unsigned int)stack_addr);
			stack_addr_specified = TRUE;
		    }
		    if(stack_size_specified == TRUE){
			if(stack_size % segalign != 0)
			    fatal("-stack_size: 0x%x not a multiple of the "
				  "segment alignment (0x%x)",
				  (unsigned int)stack_size,
				  (unsigned int)segalign);
		    }
		    else{
			stack_size = get_stack_size_from_flag(&arch_flag);
			warning("no -stack_size specified using the default "
				"size: 0x%x", (unsigned int)stack_size);
		    }
		    stack_segment.filename = outputfile;
		    strcpy(stack_segment.sg.segname, SEG_UNIXSTACK);
		    if(get_stack_direction_from_flag(&arch_flag) < 0)
			stack_segment.sg.vmaddr = stack_addr - stack_size;
		    else
			stack_segment.sg.vmaddr = stack_addr;
		    stack_segment.sg.vmsize = stack_size;
		    stack_segment.addr_set = TRUE;
		    /* place this last in the merged segment list */
		    p = &merged_segments;
		    while(*p){
			msg = *p;
			p = &(msg->next);
		    }
		    *p = &stack_segment;
		}
	    }
	}
	else{
	    output_thread_info.thread_in_output = FALSE;
	}

	/*
	 * Set routines_in_output in the output_routines_info if we are going to
	 * create a routines command.  It is created if this filetype is a
	 * shared library and an init name was specified.
	 */
 	if(filetype == MH_DYLIB && init_name != NULL){
	    output_routines_info.routines_in_output = TRUE;
	    output_routines_info.routines_command.cmd = LC_ROUTINES;
	    output_routines_info.routines_command.cmdsize =
		sizeof(struct routines_command);
	}
	else{
	    output_routines_info.routines_in_output = FALSE;
	}

	/*
	 * Create the link edit segment if specified and size it.
	 */
	if(filetype == MH_EXECUTE ||
	   filetype == MH_BUNDLE ||
	   filetype == MH_FVMLIB ||
	   filetype == MH_DYLIB ||
	   filetype == MH_DYLINKER){
	    if(check_reserved_segment(SEG_LINKEDIT, "segment " SEG_LINKEDIT
				   " reserved for the -seglinkedit option")){
		/*
		 * Now that the above check has been made and the segment is
		 * known not to exist create the link edit segment if specified.
		 */
		if(seglinkedit == TRUE){
		    /*
		     * Fill in the merged segment.  In this case only the
		     * segment name and filesize of the segment are not zero
		     * or NULL.  Note the link edit segment is unique in that
		     * it's filesize is not rounded to the segment alignment.
		     * This can only be done because this is the last segment
		     * in the file (right before end of file).
		     */
		    linkedit_segment.filename = outputfile;
		    strcpy(linkedit_segment.sg.segname, SEG_LINKEDIT);
		    if(save_reloc)
			linkedit_segment.sg.filesize += nreloc *
						sizeof(struct relocation_info);
		    if(output_for_dyld)
			linkedit_segment.sg.filesize +=
			    (output_dysymtab_info.dysymtab_command.nlocrel +
			     output_dysymtab_info.dysymtab_command.nextrel) *
			    sizeof(struct relocation_info);
		    if(filetype == MH_DYLIB)
			linkedit_segment.sg.filesize +=
			    output_dysymtab_info.dysymtab_command.ntoc *
				sizeof(struct dylib_table_of_contents) +
			    output_dysymtab_info.dysymtab_command.nmodtab *
				sizeof(struct dylib_module) +
			    output_dysymtab_info.dysymtab_command.nextrefsyms *
				sizeof(struct dylib_reference);
		    if(nindirectsyms != 0)
			linkedit_segment.sg.filesize +=
			    nindirectsyms * sizeof(unsigned long);
		    if(strip_level != STRIP_ALL)
			linkedit_segment.sg.filesize +=
			    (nmerged_symbols
			     - nstripped_merged_symbols
			     + nlocal_symbols
			     - nmerged_symbols_referenced_only_from_dylibs) *
			    sizeof(struct nlist) +
			    rnd(merged_string_size +
				  local_string_size +
				  STRING_SIZE_OFFSET,
				  sizeof(unsigned long));
		    else
			warning("segment created for -seglinkedit zero size "
			        "(output file stripped)");
		    if(output_for_dyld &&
		       twolevel_namespace == TRUE &&
		       twolevel_namespace_hints == TRUE)
			linkedit_segment.sg.filesize +=
			    output_hints_info.twolevel_hints_command.nhints *
			    sizeof(struct twolevel_hint);
		    linkedit_segment.sg.vmsize =
				rnd(linkedit_segment.sg.filesize, segalign);
		    /* place this last in the merged segment list */
		    p = &merged_segments;
		    while(*p){
			msg = *p;
			p = &(msg->next);
		    }
		    *p = &linkedit_segment;
		}
	    }
	}

	/*
	 * If the the file type is MH_EXECUTE and address zero has not been
	 * assigned to a segment create the "__PAGEZERO" segment.
	 */
	if(filetype == MH_EXECUTE){
	    if(check_reserved_segment(SEG_PAGEZERO, "segment " SEG_PAGEZERO
				      " reserved for address zero through "
				      "segment alignment")){
		/*
		 * There shouldn't be any segment specifications for this
		 * segment (address or protection).
		 */
		seg_spec = lookup_segment_spec(SEG_PAGEZERO);
		if(seg_spec != NULL){
		    if(seg_spec->addr_specified)
			error("specified address for segment " SEG_PAGEZERO
			      " not allowed (segment " SEG_PAGEZERO " reserved "
			      "for address zero through segment alignment)");
		    if(seg_spec->prot_specified)
			error("specified protection for segment " SEG_PAGEZERO
			      " not allowed (segment " SEG_PAGEZERO " reserved "
			      "for address zero through segment alignment and "
			      "has no assess protections)");
		    seg_spec->processed = TRUE;
		}
		address_zero_specified = FALSE;
		for(i = 0; i < nsegment_specs; i++){
		    if(segment_specs[i].addr_specified &&
		       segment_specs[i].addr == 0 &&
		       &(segment_specs[i]) != seg_spec){
			address_zero_specified = TRUE;
			break;
		    }
		}
		if(address_zero_specified == FALSE &&
		   (seg1addr_specified == FALSE || seg1addr != 0)){
		    pagezero_segment.filename = outputfile;
		    pagezero_segment.addr_set = TRUE;
		    pagezero_segment.prot_set = TRUE;
		    strcpy(pagezero_segment.sg.segname, SEG_PAGEZERO);
		    if(pagezero_size != 0)
			pagezero_segment.sg.vmsize = rnd(pagezero_size,
							   segalign);
		    else
			pagezero_segment.sg.vmsize = segalign;
		    /* place this first in the merged segment list */
		    pagezero_segment.next = merged_segments;
		    merged_segments = &pagezero_segment;
		}
	    }
	}

	/*
	 * Process the command line specifications for the segments setting the
	 * addresses and protections for the specified segments into the merged
	 * segments.
	 */
	process_segment_specs();
#endif /* !defined(RLD) */

#ifndef RLD
	/*
	 * If there is a "__TEXT" segment who's protection has not been set
	 * set it's inital protection to "r-x" and it's maximum protection
	 * to "rwx".
	 */
	msg = lookup_merged_segment(SEG_TEXT);
	if(msg != NULL && msg->prot_set == FALSE){
	    msg->sg.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
	    msg->sg.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	    msg->prot_set = TRUE;
	}
	/*
	 * If there is a "__IMPORT" segment who's protection has not been set
	 * set it's inital protection to "rwx" and it's maximum protection
	 * to "rwx".
	 */
	msg = lookup_merged_segment(SEG_IMPORT);
	if(msg != NULL && msg->prot_set == FALSE){
	    msg->sg.initprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	    msg->sg.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	    msg->prot_set = TRUE;
	}
	/*
	 * If the "__LINKEDIT" segment is created sets its inital protection to
	 * "r--" and it's maximum protection to the architecture's default.
	 */
	if(seglinkedit){
	    msg = lookup_merged_segment(SEG_LINKEDIT);
	    if(msg != NULL && msg->prot_set == FALSE){
		msg->sg.initprot = VM_PROT_READ;
		msg->sg.maxprot = get_segprot_from_flag(&arch_flag);
		msg->prot_set = TRUE;
	    }
	}
#endif /* !defined(RLD) */

	/*
	 * Set the protections of segments that have not had their protection
	 * set to the architecture's default protection.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    if(msg->prot_set == FALSE){
		/*
		 * Only turn on execute protection if any of the sections in
		 * the segment contain some instructions.  For pre-4.0 objects
		 * this is always in the (__TEXT,__text) section and is handled
		 * above anyway.
		 */
		msg->sg.initprot = VM_PROT_READ | VM_PROT_WRITE;
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    if((ms->s.flags & S_ATTR_SOME_INSTRUCTIONS) ==
			S_ATTR_SOME_INSTRUCTIONS){
			msg->sg.initprot |= VM_PROT_EXECUTE;
			break;
		    }
		    content = &(ms->next);
		}
		msg->sg.maxprot = get_segprot_from_flag(&arch_flag);
		if((msg->sg.initprot & VM_PROT_EXECUTE) == VM_PROT_EXECUTE)
		    msg->sg.maxprot |= VM_PROT_EXECUTE;
		msg->prot_set = TRUE;
	    }
	    p = &(msg->next);
	}

	/*
	 * Set the address of the first segment via the -seg1addr option or the
	 * -segs_read_only_addr/-segs_read_write_addr options or the default
	 * first address of the output format.
	 */
	if(segs_read_only_addr_specified){
	    if(segs_read_only_addr % segalign != 0)
		fatal("-segs_read_only_addr: 0x%x not a multiple of the segment"
		      " alignment (0x%x)", (unsigned int)segs_read_only_addr,
		      (unsigned int)segalign);
	    if(segs_read_write_addr_specified){
		if(segs_read_write_addr % segalign != 0)
		    fatal("-segs_read_write_addr: 0x%x not a multiple of the "
		          "segment alignment (0x%x)",
			  (unsigned int)segs_read_write_addr,
			  (unsigned int)segalign);
	    }
	    else{
		segs_read_write_addr = segs_read_only_addr + 
				   get_shared_region_size_from_flag(&arch_flag);
	    }
	}
	first_msg = merged_segments;
	if(first_msg == &pagezero_segment)
	    first_msg = first_msg->next;
	if(first_msg != NULL){
	    if(seg1addr_specified == TRUE){
		if(seg1addr % segalign != 0)
		    fatal("-seg1addr: 0x%x not a multiple of the segment "
			  "alignment (0x%x)", (unsigned int)seg1addr,
			  (unsigned int)segalign);
		if(first_msg->addr_set == TRUE)
		    fatal("address of first segment: %.16s set both by name "
			  "and with the -seg1addr option",
			  first_msg->sg.segname);
		first_msg->sg.vmaddr = seg1addr;
		first_msg->addr_set = TRUE;
	    }
	    else{
		if(first_msg->addr_set == FALSE){
		    if(filetype == MH_EXECUTE &&
		       pagezero_segment.addr_set == TRUE)
			first_msg->sg.vmaddr = pagezero_segment.sg.vmsize;
		    else{
			if(segs_read_only_addr_specified){
			    if((first_msg->sg.initprot & VM_PROT_WRITE) == 0)
				first_msg->sg.vmaddr = segs_read_only_addr;
			    else
				first_msg->sg.vmaddr = segs_read_write_addr;
			}
			else{
			    first_msg->sg.vmaddr = 0;
			}
		    }
		    first_msg->addr_set = TRUE;
		}
	    }
	}

	/*
	 * Size and count the load commands in the output file which include:
	 *   The segment load commands
	 *   The load fvmlib commands
	 *   A symtab command
	 *   A dysymtab command
	 *   A thread command (if the file type is NOT MH_DYLIB or MH_FVMLIB)
	 */
	ncmds = 0;
	sizeofcmds = 0;
	/*
	 * Size the segment commands and accumulate the number of commands and
	 * size of them.
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    msg->sg.cmd = LC_SEGMENT;
	    msg->sg.cmdsize = sizeof(struct segment_command) +
			      msg->sg.nsects * sizeof(struct section);
	    ncmds++;
	    sizeofcmds += msg->sg.cmdsize;
	    p = &(msg->next);
	}
#ifndef RLD
	/*
	 * Accumulate the number of commands for the fixed VM shared libraries
	 * and their size.  Since the commands themselves come from the input
	 * object files the 'cmd' and 'cmdsize' fields are already set.
	 */
	nfvmlibs = 0;
	q = &merged_fvmlibs;
	while(*q){
	    mfl = *q;
	    nfvmlibs++;
	    sizeofcmds += mfl->fl->cmdsize;
	    q = &(mfl->next);
	}
	if(filetype == MH_FVMLIB && nfvmlibs != 1){
	    if(nfvmlibs == 0)
		error("no LC_IDFVMLIB command in the linked object files");
	    else
		error("more than one LC_IDFVMLIB command in the linked object "
		      "files");
	}
	ncmds += nfvmlibs;

	/*
	 * If the output file is a dynamicly linked shared library (MH_DYLIB)
	 * then create the library identifing information load command.  And
	 * if -sub_framework was specified then create the sub_framework
	 * command.
	 */
	if(filetype == MH_DYLIB){
	    create_dylib_id_command();
	    if(sub_framework == TRUE){
		create_sub_framework_command();
		sizeofcmds += merged_sub_framework->sub->cmdsize;
		ncmds++;
	    }
	    if(nsub_umbrellas != 0){
		sizeofcmds += create_sub_umbrella_commands();
		ncmds += nsub_umbrellas;
	    }
	    if(nsub_librarys != 0){
		sizeofcmds += create_sub_library_commands();
		ncmds += nsub_librarys;
	    }
	    if(nallowable_clients != 0){
		sizeofcmds += create_sub_client_commands();
		ncmds += nallowable_clients;
	    }
	}
	/*
	 * If the output file is a dynamic linker (MH_DYLINKER) then create the
	 * the dynamic linker identifing information load command.
	 */
	if(filetype == MH_DYLINKER)
	    create_dylinker_id_command();

	/*
	 * If there is a dynamic linkers then account for the load commands and
	 * it's size.  The command already has the 'cmd' and 'cmdsize' fields
	 * set.
	 */
	if(merged_dylinker != NULL){
	    sizeofcmds += merged_dylinker->dyld->cmdsize;
	    ncmds++;
	}

	/*
	 * Accumulate the number of commands for the dynamicly linked shared
	 * libraries and their size.  The commands already have their 'cmd' and
	 * 'cmdsize' fields set.
	 */
	mdl = merged_dylibs;
	while(mdl != NULL){
	    sizeofcmds += mdl->dl->cmdsize;
	    ncmds++;
	    /*
	     * If -headerpad_max_install_names is specified make sure headerpad
	     * is big enough to change all the install name of the dylibs in
	     * the output to MAXPATHLEN.
	     */
	    if(headerpad_max_install_names == TRUE){
		if(mdl->dl->cmdsize - sizeof(struct dylib_command) < MAXPATHLEN)
		    headerpad += MAXPATHLEN -
				 (mdl->dl->cmdsize -
				  sizeof(struct dylib_command));
	    }
	    mdl = mdl->next;
	}

	/*
	 * Accumulate the number of commands for the prebound dynamic libraries
	 * and their size.
	 */
	if(filetype == MH_EXECUTE){
	    for(dp = dynamic_libs; dp != NULL; dp = dp->next){
		if(dp->type == DYLIB){
		    if(dp->pbdylib != NULL){
			sizeofcmds += dp->pbdylib->cmdsize;
			ncmds++;
			/*
			 * If -headerpad_max_install_names is specified make
			 * sure headerpad is big enough to change all the
			 * install name of the dylibs in the output to
			 * MAXPATHLEN.
			 */
			if(headerpad_max_install_names == TRUE){
			    if(dp->pbdylib->cmdsize -
			       sizeof(struct prebound_dylib_command) <
				MAXPATHLEN)
				headerpad += MAXPATHLEN -
				     (dp->pbdylib->cmdsize -
				      sizeof(struct prebound_dylib_command));
			}
			/*
			 * Since we are building this executable prebound we
			 * want to have some header padding in case their are
			 * more indirectly referenced dylibs that will need to
			 * be added when redoing the prebinding.  We have found
			 * that in the 10.2 release that 3 times the size of
			 * the initial LC_PREBOUND_DYLIB commands seems to work
			 * for most but not all things.
			 */
			headerpad += dp->pbdylib->cmdsize * 3 * indirect_library_ratio;
		    }
		}
	    }
	}

#endif /* !defined(RLD) */
	/*
	 * Create the symbol table load command.
	 */
	output_symtab_info.symtab_command.cmd = LC_SYMTAB;
	output_symtab_info.symtab_command.cmdsize =
						sizeof(struct symtab_command);
	if(strip_level != STRIP_ALL){
	    output_symtab_info.symtab_command.nsyms =
		nmerged_symbols
		- nstripped_merged_symbols
		+ nlocal_symbols
		- nmerged_symbols_referenced_only_from_dylibs;
	    output_symtab_info.symtab_command.strsize =
		rnd(merged_string_size +
		      local_string_size +
		      STRING_SIZE_OFFSET,
		      sizeof(unsigned long));
	    output_symtab_info.output_strpad =
		output_symtab_info.symtab_command.strsize -
		(merged_string_size + local_string_size + STRING_SIZE_OFFSET);
	    output_symtab_info.output_merged_strsize = STRING_SIZE_OFFSET;
	    output_symtab_info.output_local_strsize = STRING_SIZE_OFFSET +
						      merged_string_size;
	}
	ncmds++;
	sizeofcmds += output_symtab_info.symtab_command.cmdsize;
	/*
	 * Create the dynamic symbol table load command.
	 */
	if(nindirectsyms != 0 || output_for_dyld){
	    output_dysymtab_info.dysymtab_command.cmd = LC_DYSYMTAB;
	    output_dysymtab_info.dysymtab_command.cmdsize =
						sizeof(struct dysymtab_command);
	    output_dysymtab_info.dysymtab_command.nindirectsyms = nindirectsyms;
	    ncmds++;
	    sizeofcmds += output_dysymtab_info.dysymtab_command.cmdsize;
	}
	/*
	 * Create the two-level namespace hints load command.
	 */
	if(output_for_dyld && twolevel_namespace == TRUE &&
	   twolevel_namespace_hints == TRUE){
	    output_hints_info.twolevel_hints_command.cmd = LC_TWOLEVEL_HINTS;
	    output_hints_info.twolevel_hints_command.cmdsize =
					sizeof(struct twolevel_hints_command);
	    ncmds++;
	    sizeofcmds += output_hints_info.twolevel_hints_command.cmdsize;
	}
	/*
	 * Create the prebind cksum load command.
	 */
	if(prebinding == TRUE && macosx_deployment_target.major >= 2){
	    output_cksum_info.prebind_cksum_command.cmd = LC_PREBIND_CKSUM;
	    output_cksum_info.prebind_cksum_command.cmdsize =
					sizeof(struct prebind_cksum_command);
	    output_cksum_info.prebind_cksum_command.cksum = 0;
	    ncmds++;
	    sizeofcmds += output_cksum_info.prebind_cksum_command.cmdsize;
	}
	/*
	 * Create the uuid load command.
	 */
#ifndef KLD
	if(output_uuid_info.suppress != TRUE &&
	   (output_uuid_info.emit == TRUE ||
	    arch_flag.cputype == CPU_TYPE_ARM)){
	    output_uuid_info.uuid_command.cmd = LC_UUID;
	    output_uuid_info.uuid_command.cmdsize = sizeof(struct uuid_command);
	    uuid(&(output_uuid_info.uuid_command.uuid[0]));
	    ncmds++;
	    sizeofcmds += output_uuid_info.uuid_command.cmdsize;
	}
#else
	if(output_uuid_info.uuid_command.cmdsize != 0){
	    ncmds++;
	    sizeofcmds += output_uuid_info.uuid_command.cmdsize;
	}
#endif /* KLD */

	/*
	 * Create the thread command if this is filetype is to have one.
	 */
	if(output_thread_info.thread_in_output == TRUE){
	    output_thread_info.thread_command.cmdsize =
						sizeof(struct thread_command) +
						2 * sizeof(long);
	    if(arch_flag.cputype == CPU_TYPE_MC680x0){
		output_thread_info.flavor = M68K_THREAD_STATE_REGS;
		output_thread_info.count = M68K_THREAD_STATE_REGS_COUNT;
		output_thread_info.entry_point = &(mc680x0.pc);
		output_thread_info.stack_pointer = &(mc680x0.areg[7]);
		output_thread_info.state = &mc680x0;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					    M68K_THREAD_STATE_REGS_COUNT;
	    }
	    else if(arch_flag.cputype == CPU_TYPE_POWERPC ||
		    arch_flag.cputype == CPU_TYPE_VEO){
		output_thread_info.flavor = PPC_THREAD_STATE;
		output_thread_info.count = PPC_THREAD_STATE_COUNT;
		output_thread_info.entry_point = (int *)&(powerpc.srr0);
		output_thread_info.stack_pointer = (int *)&(powerpc.r1);
		output_thread_info.state = &powerpc;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					    PPC_THREAD_STATE_COUNT;
	    }
	    else if(arch_flag.cputype == CPU_TYPE_MC88000){
		output_thread_info.flavor = M88K_THREAD_STATE_GRF;
		output_thread_info.count = M88K_THREAD_STATE_GRF_COUNT;
		output_thread_info.entry_point = (int *)&(mc88000.xip);
		output_thread_info.stack_pointer = (int *)&(mc88000.r31);
		output_thread_info.state = &mc88000;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					    M88K_THREAD_STATE_GRF_COUNT;
	    }
	    else if(arch_flag.cputype == CPU_TYPE_I860){
		output_thread_info.flavor = I860_THREAD_STATE_REGS;
		output_thread_info.count = I860_THREAD_STATE_REGS_COUNT;
		output_thread_info.entry_point = &(i860.pc);
		output_thread_info.stack_pointer = &(i860.ireg[0]);
		output_thread_info.state = &i860;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					  I860_THREAD_STATE_REGS_COUNT;
	    }
	    else if(arch_flag.cputype == CPU_TYPE_I386){
		output_thread_info.flavor = i386_THREAD_STATE;
		output_thread_info.count = i386_THREAD_STATE_COUNT;
		output_thread_info.entry_point = (int *)&(intel386.eip);
		output_thread_info.stack_pointer = (int *)&(intel386.esp);
		intel386.es = USER_DATA_SELECTOR;
		intel386.ds = USER_DATA_SELECTOR;
		intel386.ss = USER_DATA_SELECTOR;
		intel386.cs = USER_CODE_SELECTOR;
		output_thread_info.state = &intel386;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					    i386_THREAD_STATE_COUNT;
	    }
	    else if(arch_flag.cputype == CPU_TYPE_HPPA){
		output_thread_info.flavor = HPPA_FRAME_THREAD_STATE;
		output_thread_info.count = HPPA_FRAME_THREAD_STATE_COUNT;
		output_thread_info.entry_point =
				(int *)&(hppa_frame_state.ts_pcoq_front);
		output_thread_info.state = &hppa_frame_state;
		output_thread_info.thread_command.cmdsize += sizeof(long) *
					    HPPA_FRAME_THREAD_STATE_COUNT;
		if(stack_addr_specified == TRUE){
		    output_thread_info.second_flavor =
						HPPA_INTEGER_THREAD_STATE;
		    output_thread_info.second_count =
						HPPA_INTEGER_THREAD_STATE_COUNT;
		    output_thread_info.stack_pointer =
					(int *)&(hppa_integer_state.ts_gr30);
		    output_thread_info.second_state = &hppa_integer_state;
		    output_thread_info.thread_command.cmdsize +=
				sizeof(long) * HPPA_INTEGER_THREAD_STATE_COUNT +
				2 * sizeof(long);
		}
	    }
	    else if (arch_flag.cputype == CPU_TYPE_SPARC) {
	      output_thread_info.flavor = SPARC_THREAD_STATE_REGS;
	      output_thread_info.count = SPARC_THREAD_STATE_REGS_COUNT;
	      output_thread_info.entry_point = &(sparc_state.regs.r_pc);
	      output_thread_info.stack_pointer = &(sparc_state.regs.r_sp);
	      output_thread_info.state = &sparc_state;
	      output_thread_info.thread_command.cmdsize += sizeof(long) *
		SPARC_THREAD_STATE_REGS_COUNT;
	    }
	    else if (arch_flag.cputype == CPU_TYPE_ARM) {
	      output_thread_info.flavor = ARM_THREAD_STATE;
	      output_thread_info.count = ARM_THREAD_STATE_COUNT;
	      output_thread_info.entry_point = (int *)&(arm_state.__pc);
	      output_thread_info.stack_pointer = (int *)&(arm_state.__sp);
	      output_thread_info.state = &arm_state;
	      output_thread_info.thread_command.cmdsize += sizeof(long) *
		ARM_THREAD_STATE_COUNT;
	    }
	    else{
		fatal("internal error: layout_segments() called with unknown "
		      "cputype (%d) set", arch_flag.cputype);
	    }
	    sizeofcmds += output_thread_info.thread_command.cmdsize;
	    ncmds++;
	}

	/*
	 * Create the routines command if this is filetype is to have one.
	 */
	if(output_routines_info.routines_in_output == TRUE){
	    sizeofcmds += output_routines_info.routines_command.cmdsize;
	    ncmds++;
	}

	/*
	 * Fill in the mach_header for the output file.
	 */
	output_mach_header.magic = MH_MAGIC;
	output_mach_header.cputype = arch_flag.cputype;
	output_mach_header.cpusubtype = arch_flag.cpusubtype;
	output_mach_header.filetype = filetype;
	output_mach_header.ncmds = ncmds;
	output_mach_header.sizeofcmds = sizeofcmds;
	output_mach_header.flags = 0;
	if(base_obj != NULL)
	    output_mach_header.flags |= MH_INCRLINK;
	if(output_for_dyld){
	    output_mach_header.flags |= MH_DYLDLINK;
	    if(bind_at_load)
		output_mach_header.flags |= MH_BINDATLOAD;
	    if(segs_read_only_addr_specified)
		output_mach_header.flags |= MH_SPLIT_SEGS;
	    if(twolevel_namespace)
		output_mach_header.flags |= MH_TWOLEVEL;
	    if(force_flat_namespace)
		output_mach_header.flags |= MH_FORCE_FLAT;
	    if(nomultidefs)
		output_mach_header.flags |= MH_NOMULTIDEFS;
	    if(no_fix_prebinding)
		output_mach_header.flags |= MH_NOFIXPREBINDING;
	}
	if(some_non_subsection_via_symbols_objects == FALSE)
	    output_mach_header.flags |= MH_SUBSECTIONS_VIA_SYMBOLS;
	if(allow_stack_execute == TRUE)
	    output_mach_header.flags |= MH_ALLOW_STACK_EXECUTION;

	/*
	 * The total headers size needs to be known in the case of MH_EXECUTE,
	 * MH_BUNDLE, MH_FVMLIB, MH_DYLIB and MH_DYLINKER format file types
	 * because their headers get loaded as part of of the first segment.
	 * For the MH_FVMLIB and MH_DYLINKER file types the headers are placed
	 * on their own page or pages (the size of the segment alignment).
	 */
	headers_size = sizeof(struct mach_header) + sizeofcmds;
	if(filetype == MH_FVMLIB){
	    if(headers_size > segalign)
		fatal("size of headers (0x%x) exceeds the segment alignment "
		      "(0x%x) (would cause the addresses not to be fixed)",
		      (unsigned int)headers_size, (unsigned int)segalign);
	    headers_size = segalign;
	}
	else if(filetype == MH_DYLINKER){
	    headers_size = rnd(headers_size, segalign);
	}

	/*
	 * For MH_EXECUTE, MH_BUNDLE, and MH_DYLIB formats the as much of the
	 * segment padding that can be is moved to the begining of the segment
	 * just after the headers.  This is done so that the headers could
	 * added to by a smart program like segedit(1) some day.
	 */
	if(filetype == MH_EXECUTE ||
	   filetype == MH_BUNDLE ||
	   filetype == MH_DYLIB){
	    if(first_msg != NULL){
		size = 0;
		content = &(first_msg->content_sections);
		if(*content){
		    max_first_align = 1 << (*content)->s.align;
		    while(*content){
			ms = *content;
			if((unsigned long)(1 << ms->s.align) > segalign)
			    error("alignment (0x%x) of section (%.16s,%.16s) "
				  "greater than the segment alignment (0x%x)",
				  (unsigned int)(1 << ms->s.align),
				  ms->s.segname, ms->s.sectname,
				  (unsigned int)segalign);
			size = rnd(size, 1 << ms->s.align);
			if((unsigned long)(1 << ms->s.align) > max_first_align)
			    max_first_align = 1 << ms->s.align;
			size += ms->s.size;
			content = &(ms->next);
		    }
		    if(errors == 0){
			pad = ((rnd(size + rnd(headers_size,
				      max_first_align), segalign) -
			       (size + rnd(headers_size, max_first_align))) /
				 max_first_align) * max_first_align;
			if(pad > headerpad)
			    headerpad = pad;
			headers_size += headerpad;
		    }
		}
	    }
	}

	/*
	 * Assign the section addresses relitive to the start of their segment
	 * to accumulate the file and vm sizes of the segments.
	 */
	max_align = 1;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    if(msg != &pagezero_segment &&
	       msg != &linkedit_segment &&
	       msg != &stack_segment){
		if(msg == first_msg &&
		   (filetype == MH_EXECUTE ||
		    filetype == MH_BUNDLE ||
		    filetype == MH_FVMLIB ||
		    filetype == MH_DYLIB ||
		    filetype == MH_DYLINKER))
		    addr = headers_size;
		else
		    addr = 0;
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    if((unsigned long)(1 << ms->s.align) > segalign)
			error("alignment (0x%x) of section (%.16s,%.16s) "
			      "greater than the segment alignment (0x%x)",
			      (unsigned int)(1 << ms->s.align), ms->s.segname,
			      ms->s.sectname, (unsigned int)segalign);
		    if((unsigned long)(1 << ms->s.align) > max_align)
			max_align = 1 << ms->s.align;
		    addr = rnd(addr, 1 << ms->s.align);
		    ms->s.addr = addr;
		    addr += ms->s.size;
		    content = &(ms->next);
		}
		if(msg == &object_segment)
		    msg->sg.filesize = addr;
		else
		    msg->sg.filesize = rnd(addr, segalign);
		zerofill = &(msg->zerofill_sections);
		while(*zerofill){
		    ms = *zerofill;
		    if((unsigned long)(1 << ms->s.align) > segalign)
			error("alignment (0x%x) of section (%.16s,%.16s) "
			      "greater than the segment alignment (0x%x)",
			      (unsigned int)(1 << ms->s.align), ms->s.segname,
			      ms->s.sectname, (unsigned int)segalign);
		    if((unsigned long)(1 << ms->s.align) > max_align)
			max_align = 1 << ms->s.align;
		    addr = rnd(addr, 1 << ms->s.align);
		    ms->s.addr = addr;
		    addr += ms->s.size;
		    zerofill = &(ms->next);
		}
		if(msg == &object_segment)
		    msg->sg.vmsize = addr;
		else
		    msg->sg.vmsize = rnd(addr, segalign);
	    }
	    p = &(msg->next);
	}

#ifdef RLD
	/*
	 * For rld() the output format is MH_OBJECT and the contents of the
	 * first segment (the entire vmsize not just the filesize), if it exists,
	 * plus headers are allocated and the address the segment is linked to
	 * is the address of this memory.
	 */
	output_size = 0;

	headers_size = rnd(headers_size, max_align);
	output_size = headers_size;
	if(first_msg != NULL)
	    output_size += first_msg->sg.vmsize;
	allocate_size = output_size;
	if(strip_level != STRIP_ALL)
	    allocate_size += output_symtab_info.symtab_command.nsyms *
				sizeof(struct nlist) +
				output_symtab_info.symtab_command.strsize;

#ifdef SA_RLD
	if(allocate_size > sa_rld_output_size)
	    fatal("not enough memory for output of size %lu (memory "
		    "available %lu)", allocate_size, sa_rld_output_size);
	output_addr = sa_rld_output_addr;
#else /* !defined(SA_RLD) */
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&output_addr,
			    allocate_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() memory for output of size "
			"%lu", allocate_size);
	/*
	 * The default initial protection for vm_allocate()'ed memory
	 * may not include VM_PROT_EXECUTE so we need to raise the
	 * the protection to VM_PROT_ALL which include this.
	 */
	if((r = vm_protect(mach_task_self(), (vm_address_t)output_addr,
	    allocate_size, FALSE, VM_PROT_ALL)) != KERN_SUCCESS)
	    mach_fatal(r, "can't set vm_protection on memory for output");
#endif /* defined(SA_RLD) */
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() vm_allocate: addr = 0x%0x size = 0x%x\n",
		(unsigned int)output_addr, (unsigned int)allocate_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	sets[cur_set].output_addr = output_addr;
	sets[cur_set].output_size = output_size;

	if(first_msg != NULL){
	    if(address_func != NULL){
	        if(RLD_DEBUG_OUTPUT_FILENAME_flag)
		    first_msg->sg.vmaddr =
		      (*address_func)(allocate_size, headers_size)+headers_size;
		else
		    first_msg->sg.vmaddr =
		      (*address_func)(output_size, headers_size) + headers_size;
	    }
	    else
		first_msg->sg.vmaddr = (long)output_addr + headers_size;
	}
#endif /* RLD */
	/*
	 * Set the addresses of segments that have not had their addresses set
	 * and set the addresses of all sections (previously set relitive to the
	 * start of their section and here just moved by the segment address).
	 * The addresses of segments that are not set are set to the next
	 * available address after the first segment that the vmsize will fit
	 * (note that the address of the first segment has been set above).
	 */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    /*
	     * The first segment has had it's address set previouly above so
	     * this test will always fail for it and there is no problem of
	     * trying to use first_msg for the first segment.
	     */
	    if(msg->addr_set == FALSE){
		if(segs_read_only_addr_specified){
		    if((msg->sg.initprot & VM_PROT_WRITE) == 0)
			msg->sg.vmaddr = next_vmaddr(segs_read_only_addr,
						     msg->sg.vmsize);
		    else
			msg->sg.vmaddr = next_vmaddr(segs_read_write_addr,
						     msg->sg.vmsize);
		}
		else{
		    msg->sg.vmaddr = next_vmaddr(first_msg->sg.vmaddr,
						 msg->sg.vmsize);
		}
		msg->addr_set = TRUE;
	    }
	    if(msg != &pagezero_segment &&
	       msg != &linkedit_segment &&
	       msg != &stack_segment){
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    ms->s.addr += msg->sg.vmaddr;
		    content = &(ms->next);
		}
		zerofill = &(msg->zerofill_sections);
		while(*zerofill){
		    ms = *zerofill;
		    ms->s.addr += msg->sg.vmaddr;
		    zerofill = &(ms->next);
		}
	    }
	    p = &(msg->next);
	}

#ifndef RLD
	/*
	 * Check for overlapping segments (including fvmlib segments).
	 */
	check_for_overlapping_segments(&linkedit_segment);

	/*
	 * If prebinding check to see the that the lazy pointer relocation
	 * entries are not too far away to fit into the 24-bit r_adderess field
	 * of a scattered relocation entry.
	 */
	if(prebinding)
	    check_for_lazy_pointer_relocs_too_far();

	if(prebinding)
	    output_mach_header.flags |= MH_PREBOUND;
#endif /* RLD */

	/*
	 * Assign all file offsets.  Things with offsets appear in the following
	 * two possible orders in the file:
	 *
	 *   For relocatable objects (not output for dyld)
	 *	The segments (and their content sections)
	 *	The relocation entries for the content sections
	 *	The symbol table
	 *	The string table.
	 *
	 *   For objects output for dyld
	 *	The segments (and their content sections)
	 *	The local relocation entries
	 *	The symbol table
	 *		local symbols
	 *		external defined symbols
	 *		undefined symbols
	 *	The two-level namespace hints table
	 *	The external relocation entries
	 *	The indirect symbol table
	 *	The table of contents (MH_DYLIB only)
	 *	The module table (MH_DYLIB only)
	 *	The reference table (MH_DYLIB only)
	 *	The string table.
	 *		external strings
	 *		local strings
	 */
	offset = headers_size;
	/* set the offset to the segments and sections */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    if(msg != &pagezero_segment &&
	       msg != &linkedit_segment &&
	       msg != &stack_segment){
		if(msg == first_msg &&
		   (filetype == MH_EXECUTE ||
		    filetype == MH_BUNDLE ||
		    filetype == MH_FVMLIB ||
		    filetype == MH_DYLIB ||
		    filetype == MH_DYLINKER)){
		    msg->sg.fileoff = 0;
		    content = &(msg->content_sections);
		    if(*content){
			ms = *content;
			offset = ms->s.addr - msg->sg.vmaddr;
		    }
		}
		else
		    msg->sg.fileoff = offset;
		content = &(msg->content_sections);
		while(*content){
		    ms = *content;
		    ms->s.offset = offset;
		    if(ms->next != NULL)
			offset += (ms->next->s.addr - ms->s.addr);
		    content = &(ms->next);
		}
		if(msg->sg.filesize == 0)
		    msg->sg.fileoff = 0;
		if(msg == first_msg &&
		   (filetype == MH_EXECUTE ||
		    filetype == MH_BUNDLE ||
		    filetype == MH_FVMLIB ||
		    filetype == MH_DYLIB ||
		    filetype == MH_DYLINKER))
		    offset = msg->sg.filesize;
		else
		    if(msg->sg.filesize != 0)
			offset = msg->sg.fileoff + msg->sg.filesize;
	    }
	    p = &(msg->next);
	}

	/*
	 * The offsets to all the link edit structures in the file must be on
	 * boundaries that they can be mapped into memory and then used as is.
	 * The maximum alignment of all structures in a Mach-O file is
	 * sizeof(long) so the offset must be rounded to this as the sections
	 * and segments may not be rounded to this.
	 */
	offset = rnd(offset, sizeof(long));
#ifdef RLD
	/*
	 * For RLD if there is any symbol table it is written past the size
	 * of the output_size.  Room has been allocated for it above if the
	 * strip_level != STRIP_ALL.
	 */
	offset = output_size;
#endif /* RLD */

	/* the linkedit segment will start here */
	linkedit_segment.sg.fileoff = offset;

	/* set the offset to the relocation entries (if in the output file) */
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(save_reloc && ms->s.nreloc != 0){
		    ms->s.reloff = offset;
		    offset += ms->s.nreloc * sizeof(struct relocation_info);
		}
		else{
		    ms->s.reloff = 0;
		    ms->s.nreloc = 0;
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
	if(output_for_dyld){
	    if(output_dysymtab_info.dysymtab_command.nlocrel != 0){
		output_dysymtab_info.dysymtab_command.locreloff = offset;
		offset += output_dysymtab_info.dysymtab_command.nlocrel *
			  sizeof(struct relocation_info);
	    }
	}
	/* set the offset to the symbol table (output for dyld case) */
	if(output_for_dyld){
	    if(strip_level != STRIP_ALL){
		output_symtab_info.symtab_command.symoff = offset;
		offset += output_symtab_info.symtab_command.nsyms *
			  sizeof(struct nlist);
	    }
	}
	/* set the offset to the two-level namespace hints */
	if(output_for_dyld && twolevel_namespace == TRUE &&
	   twolevel_namespace_hints == TRUE){
	    output_hints_info.twolevel_hints_command.offset = offset;
	    offset += output_hints_info.twolevel_hints_command.nhints *
		      sizeof(struct twolevel_hint);
	}
	if(output_for_dyld){
	    if(output_dysymtab_info.dysymtab_command.nextrel != 0){
		output_dysymtab_info.dysymtab_command.extreloff = offset;
		offset += output_dysymtab_info.dysymtab_command.nextrel *
			  sizeof(struct relocation_info);
	    }
	}
	/* set the offset to the indirect symbol table */
	if(nindirectsyms != 0){
	    output_dysymtab_info.dysymtab_command.indirectsymoff = offset;
	    offset += nindirectsyms * sizeof(unsigned long);
	}
#ifndef RLD
	/* set the offset to the dylib tables */
	if(filetype == MH_DYLIB){
	    output_dysymtab_info.dysymtab_command.tocoff = offset;
	    offset += output_dysymtab_info.dysymtab_command.ntoc *
		      sizeof(struct dylib_table_of_contents);
	    output_dysymtab_info.dysymtab_command.modtaboff = offset;
	    offset += output_dysymtab_info.dysymtab_command.nmodtab *
		      sizeof(struct dylib_module);
	    output_dysymtab_info.dysymtab_command.extrefsymoff = offset;
	    offset += output_dysymtab_info.dysymtab_command.nextrefsyms *
		      sizeof(struct dylib_reference);
	}
#endif /* !defined(RLD) */
	/* set the offset to the symbol table (output not for dyld case) */
	if(output_for_dyld == FALSE){
	    if(strip_level != STRIP_ALL){
		output_symtab_info.symtab_command.symoff = offset;
		offset += output_symtab_info.symtab_command.nsyms *
			  sizeof(struct nlist);
	    }
	}
	/* set the offset to the string table */
	if(strip_level != STRIP_ALL){
	    output_symtab_info.symtab_command.stroff = offset;
	    offset += output_symtab_info.symtab_command.strsize;
	}
#ifndef RLD
	/* set the size of the output file */
	output_size = offset;
#endif /* !defined(RLD) */

	/*
	 * Set the output section number in to each merged section to be used
	 * to set symbol's section numbers and local relocation section indexes.
	 */
	i = 1;
	p = &merged_segments;
	while(*p){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		ms->output_sectnum = i++;
		content = &(ms->next);
	    }
	    zerofill = &(msg->zerofill_sections);
	    while(*zerofill){
		ms = *zerofill;
		ms->output_sectnum = i++;
		zerofill = &(ms->next);
	    }
	    p = &(msg->next);
	}
	if(i > MAX_SECT)
	    fatal("too many sections used, maximum is: %d", MAX_SECT);

#ifndef RLD
	/*
	 * Define the loader defined symbols.  This is done here because the
	 * address of the headers is needed to defined the symbol for
	 * MH_EXECUTE, MH_BUNDLE, MH_DYLIB and MH_DYLINKER filetypes.
	 */
	if(filetype == MH_EXECUTE &&
	   first_msg != NULL && first_msg != &linkedit_segment)
	    define_link_editor_execute_symbols(first_msg->sg.vmaddr);
	if((filetype == MH_BUNDLE ||
	    filetype == MH_DYLIB || filetype == MH_DYLINKER) &&
	   first_msg != NULL && first_msg != &linkedit_segment)
	    define_link_editor_dylib_symbols(first_msg->sg.vmaddr);
	if(filetype == MH_PRELOAD)
	    define_link_editor_preload_symbols(FALSE);
#endif /* !defined(RLD) */

	/*
	 * Now with the addresses of the segments and sections set and the
	 * sections numbered for the output file set the values and section
	 * numbers of the merged symbols.
	 */
	layout_merged_symbols();

	/*
	 * Set the entry point to either the specified symbol name's value or
	 * the address of the first section.
	 */
	if(output_thread_info.thread_in_output == TRUE){
	    if(entry_point_name != NULL){
		merged_symbol = lookup_symbol(entry_point_name);
		/*
		 * If the symbol is not found, undefined or common the
		 * entry point can't be set.
		 */
		if(merged_symbol->name_len == 0 ||
		   merged_symbol->nlist.n_type == (N_EXT | N_UNDF))
		    fatal("entry point symbol name: %s not defined",
			  entry_point_name);
		*output_thread_info.entry_point = merged_symbol->nlist.n_value;
	    }
	    else{
		*output_thread_info.entry_point =
					    first_msg->content_sections->s.addr;
	    }
	    /*
	     * Set up wierd hppa instruction address offset queue.
	     * iioq_head is the entry point.  iioq_tail is the next instruction.
	     */
	    if(arch_flag.cputype == CPU_TYPE_HPPA)
		hppa_frame_state.ts_pcoq_back =
			hppa_frame_state.ts_pcoq_front + 4;
	    /*
	     * If the stack address is specified set this in the stack pointer.
	     */
	    if(stack_addr_specified == TRUE &&
	       output_thread_info.thread_command.cmd == LC_UNIXTHREAD)
		*output_thread_info.stack_pointer = stack_addr;
	}
	else{
	    if(entry_point_name != NULL)
		warning("specified entry point symbol name ignored, output "
 			"file type has no entry point or no non-zerofill "
 			"sections");
	}

	/*
	 * Set the addresses and module indexes in the routine command if that
	 * is to appear in the output.
	 */
	if(output_routines_info.routines_in_output == TRUE){
	    if(init_name != NULL){
		merged_symbol = lookup_symbol(init_name);
		/*
		 * If the symbol is not found, undefined or common the
		 * initialization routine address can't be set.
		 */
		if(merged_symbol->name_len == 0 ||
		   merged_symbol->nlist.n_type == (N_EXT | N_UNDF))
		    fatal("initialization routine symbol name: %s not defined",
			  init_name);
		if(arch_flag.cputype == CPU_TYPE_ARM &&
		   (merged_symbol->nlist.n_desc & N_ARM_THUMB_DEF))
		    /* Have to set the low-order bit if symbol is Thumb */
		    output_routines_info.routines_command.init_address =
			merged_symbol->nlist.n_value | 1;
		else
		    output_routines_info.routines_command.init_address =
			merged_symbol->nlist.n_value;
		output_routines_info.routines_command.init_module =
		    merged_symbol->definition_object->imodtab;
	    }
	}
	else{
	    if(init_name != NULL)
		warning("specified initialization routine symbol name ignored, "
			"output file type has no initialization routine");
	}
}

/*
 * next_vmaddr() is passed in a vmaddr and vmsize and returns then next highest
 * vmaddr that will fit the vmsize in the merged segments which have addresses
 * assigned.
 */
static
unsigned long
next_vmaddr(
unsigned long vmaddr,
unsigned long vmsize)
{
    unsigned long i, n;
    struct merged_segment *msg, **sorted_merged_segments;

	/*
	 * Count the of merged segments with addresses set of non-zero size.
	 */
	n = 0;
	for(msg = merged_segments; msg != NULL ; msg = msg->next){
	    if(msg->addr_set == TRUE && msg->sg.vmsize != 0)
		n++;
	}
	/*
	 * If no merged segment of non-zero size has an address set return the
	 * vmaddr passed in.
	 */
	if(n == 0)
	    return(vmaddr);

	/*
	 * Create a list of merged segments sorted by vmaddr for the merged
	 * segments with addresses set of non-zero size.
	 */
	sorted_merged_segments = (struct merged_segment **)
				 allocate(n * sizeof(struct merged_segment *));
	i = 0;
	for(msg = merged_segments; msg != NULL ; msg = msg->next){
	    if(msg->addr_set == TRUE && msg->sg.vmsize != 0){
		sorted_merged_segments[i] = msg;
		i++;
	    }
	}
	qsort(sorted_merged_segments, n, sizeof(struct merged_segment *),
	      (int (*)(const void *, const void *))qsort_vmaddr);

	/*
	 * Find the next highest address from the vmaddr passed in that will fit
	 * the vmsize passed in.  This may wrap around to lower addresses.  Also
	 * if all address space is taken this will return a address that
	 * overlaps.
	 */
	for(i = 0; i < n; i++){
	    if(vmaddr < sorted_merged_segments[i]->sg.vmaddr){
		if(vmaddr + vmsize <= sorted_merged_segments[i]->sg.vmaddr)
		    goto done;
		vmaddr = sorted_merged_segments[i]->sg.vmaddr +
			 sorted_merged_segments[i]->sg.vmsize;
	    }
	    if(vmaddr < sorted_merged_segments[i]->sg.vmaddr +
			sorted_merged_segments[i]->sg.vmsize){
		vmaddr = sorted_merged_segments[i]->sg.vmaddr +
			 sorted_merged_segments[i]->sg.vmsize;
	    }
	}
done:
	free(sorted_merged_segments);
	return(vmaddr);
}

/*
 * Function for qsort to sort merged_segments by their vmaddr.
 */
static
int
qsort_vmaddr(
const struct merged_segment **msg1,
const struct merged_segment **msg2)
{
	if((*msg1)->sg.vmaddr < (*msg2)->sg.vmaddr)
	    return(-1);
	if((*msg1)->sg.vmaddr == (*msg2)->sg.vmaddr)
	    return(0);
	/* (*msg1)->sg.vmaddr > (*msg2)->sg.vmaddr */
	    return(1);
}

#ifndef RLD
/*
 * check_reserved_segment() checks to that the reserved segment is NOT in the
 * merged segment list and returns TRUE if so.  If it the segment is found in
 * the merged segment list it prints an error message stating the segment exists
 * and prints the string passed to it as to why this segment is reserved.  Then
 * It prints all sections created from files in that segment and all object
 * files that contain that segment.  Finally it returns FALSE in this case.
 */
static
enum bool
check_reserved_segment(
char *segname,
char *reserved_error_string)
{
    struct merged_segment *msg;
    struct merged_section **content, *ms;

    unsigned long i, j;
    struct object_list *object_list, **q;
    struct object_file *object_file;
    struct section *s;

	msg = lookup_merged_segment(segname);
	if(msg != NULL){
	    error("segment %s exist in the output file (%s)", segname,
		  reserved_error_string);
	    /*
	     * Loop through the content sections and report any sections
	     * created from files.
	     */
	    content = &(msg->content_sections);
	    while(*content){
		ms = *content;
		if(ms->contents_filename != NULL)
		    print("section (%.16s,%.16s) created from file "
			   "%s\n", ms->s.segname, ms->s.sectname,
			   ms->contents_filename);
		content = &(ms->next);
	    }
	    /*
	     * Loop through all the objects and report those that have
	     * this segment.
	     */
	    for(q = &objects; *q; q = &(object_list->next)){
		object_list = *q;
		for(i = 0; i < object_list->used; i++){
		    object_file = &(object_list->object_files[i]);
		    if(object_file == base_obj)
			continue;
		    if(object_file->dylib)
			continue;
		    if(object_file->bundle_loader)
			continue;
		    if(object_file->dylinker)
			continue;
		    for(j = 0; j < object_file->nsection_maps; j++){
			s = object_file->section_maps[j].s;
			if(strcmp(s->segname, segname) == 0){
			    print_obj_name(object_file);
			    print("contains section (%.16s,%.16s)\n",
				   s->segname, s->sectname);
			}
		    }
		}
	    }
	    return(FALSE);
	}
	return(TRUE);
}

/*
 * check_for_overlapping_segments() checks for overlapping segments in the
 * output file and in the segments from the fixed VM shared libraries it uses.
 */
static
void
check_for_overlapping_segments(
struct merged_segment *outputs_linkedit_segment)
{
    struct merged_segment **p1, **p2, **last_merged, **last_fvmseg, **last_base,
			  *msg1, *msg2;

	/*
	 * To make the checking loops below clean the fvmlib segment list is
	 * attached to the end of the merged segment list and then detached
	 * before we return.
	 */
	last_merged = &merged_segments;
	while(*last_merged){
	    msg1 = *last_merged;
	    last_merged = &(msg1->next);
	}
	if(fvmlib_segments != NULL){
	    *last_merged = fvmlib_segments;

	    last_fvmseg = &fvmlib_segments;
	    while(*last_fvmseg){
		msg1 = *last_fvmseg;
		last_fvmseg = &(msg1->next);
	    }
	}
	else
	    last_fvmseg = last_merged;
	*last_fvmseg = base_obj_segments;

	p1 = &merged_segments;
	while(*p1){
	    msg1 = *p1;
	    p2 = &(msg1->next);
	    while(*p2){
		msg2 = *p2;
		check_overlap(msg1, msg2, FALSE, outputs_linkedit_segment);
		p2 = &(msg2->next);
	    }
	    p1 = &(msg1->next);
	}

	/*
	 * If we are doing prebinding add the segments from the dylibs and
	 * then check for overlap with these segments.
	 */
	if(prebinding && dylib_segments != NULL){
	    /* first add on the dylib segments */
	    last_base = last_fvmseg;
	    while(*last_base){
		msg1 = *last_base;
		last_base = &(msg1->next);
	    }
	    *last_base = dylib_segments;
	    /* now check overlap for prebinding */
	    p1 = &merged_segments;
	    while(*p1){
		msg1 = *p1;
		p2 = &(msg1->next);
		while(*p2){
		    msg2 = *p2;
		    check_overlap(msg1, msg2, TRUE, outputs_linkedit_segment);
		    p2 = &(msg2->next);
		}
		p1 = &(msg1->next);
	    }
	    /* take off the dylib segments */
	    *last_base = NULL;
	}

	/* take off the fvmlib segments */
	*last_merged = NULL;

	/* take off the base_obj segments */
	*last_fvmseg = NULL;
}

/*
 * check_overlap() checks if the two segments passed to it overlap and if so
 * prints an error message if prebind_check is FALSE.  If prebind_check is
 * TRUE it prints a warning that prebind is will be disabled.  In either case
 * prebinding is disabled if there is an overlap.
 */
static
void
check_overlap(
struct merged_segment *msg1,
struct merged_segment *msg2,
enum bool prebind_check,
struct merged_segment *outputs_linkedit_segment)
{
    char *not;

	if(msg1->sg.vmsize == 0 || msg2->sg.vmsize == 0)
	    return;

	if(msg1->sg.vmaddr > msg2->sg.vmaddr){
	    if(msg2->sg.vmaddr + msg2->sg.vmsize <= msg1->sg.vmaddr)
		return;
	}
	else{
	    if(msg1->sg.vmaddr + msg1->sg.vmsize <= msg2->sg.vmaddr)
		return;
	}
	if(prebind_check == FALSE)
	    error("%.16s segment (address = 0x%x size = 0x%x) of %s overlaps "
		  "with %.16s segment (address = 0x%x size = 0x%x) of %s",
		  msg1->sg.segname, (unsigned int)(msg1->sg.vmaddr),
		  (unsigned int)(msg1->sg.vmsize), msg1->filename,
		  msg2->sg.segname, (unsigned int)(msg2->sg.vmaddr),
		  (unsigned int)(msg2->sg.vmsize), msg2->filename);
	else{
	    if((segs_read_only_addr_specified &&
		((msg1 == outputs_linkedit_segment &&
			msg2->split_dylib == TRUE)||
	         (msg2 == outputs_linkedit_segment &&
			msg1->split_dylib == TRUE))) ||
		 (msg1->split_dylib == TRUE &&
			strcmp(msg1->sg.segname, SEG_LINKEDIT) == 0) ||
		 (msg2->split_dylib == TRUE &&
			strcmp(msg2->sg.segname, SEG_LINKEDIT) == 0)){
		warning("prebinding not disabled even though (%.16s segment "
			"(address = 0x%x size = 0x%x) of %s overlaps with "
			"%.16s segment (address = 0x%x size = 0x%x) of %s on "
			"the assumption that the stripped output will not "
			"overlap",
			msg1->sg.segname, (unsigned int)(msg1->sg.vmaddr),
			(unsigned int)(msg1->sg.vmsize), msg1->filename,
			msg2->sg.segname, (unsigned int)(msg2->sg.vmaddr),
			(unsigned int)(msg2->sg.vmsize), msg2->filename);
		return;
	    }
	    if(prebind_allow_overlap == TRUE)
		not = " not";
	    else
		not = "";
	    warning("prebinding%s disabled because (%.16s segment (address = "
		    "0x%x size = 0x%x) of %s overlaps with %.16s segment "
		    "(address = 0x%x size = 0x%x) of %s", not,
		    msg1->sg.segname, (unsigned int)(msg1->sg.vmaddr),
		    (unsigned int)(msg1->sg.vmsize), msg1->filename,
		    msg2->sg.segname, (unsigned int)(msg2->sg.vmaddr),
		    (unsigned int)(msg2->sg.vmsize), msg2->filename);
	    if(prebind_allow_overlap == TRUE)
		return;
	    if(ld_trace_prebinding_disabled == TRUE)
	      ld_trace("[Logging for XBS] prebinding disabled "
		       "for %s because (%.16s segment (address = 0x%x size = "
		       "0x%x) of %s overlaps with %.16s segment (address = 0x%x "
		       "size = 0x%x) of %s\n", final_output != NULL ?
		       final_output : outputfile,
		       msg1->sg.segname, (unsigned int)(msg1->sg.vmaddr),
		       (unsigned int)(msg1->sg.vmsize), msg1->filename,
		       msg2->sg.segname, (unsigned int)(msg2->sg.vmaddr),
		       (unsigned int)(msg2->sg.vmsize), msg2->filename);
	}
	prebinding = FALSE;
}

/*
 * check_for_lazy_pointer_relocs_too_far() is call when prebinding is TRUE and
 * checks to see that the lazy pointer's will not be too far away to overflow
 * the the 24-bit r_address field of a scattered relocation entry.  If so
 * then prebinding will be disabled.
 */
static
void
check_for_lazy_pointer_relocs_too_far(
void)
{
    struct merged_segment **p, *msg;
    struct merged_section **content, *ms;
    unsigned long base_addr;

	if(segs_read_only_addr_specified == TRUE)
	    base_addr = segs_read_write_addr;
	else
	    base_addr = merged_segments->sg.vmaddr;

	p = &merged_segments;
	while(*p && prebinding){
	    msg = *p;
	    content = &(msg->content_sections);
	    while(*content && prebinding){
		ms = *content;
		if((ms->s.flags & SECTION_TYPE) ==
		   S_LAZY_SYMBOL_POINTERS){
		    if(((ms->s.addr + ms->s.size) - base_addr) & 0xff000000){
			warning("prebinding disabled because output is too "
				"large (limitation of the 24-bit r_address "
				"field of scattered relocation entries)");
			prebinding = FALSE;
		    }
		}
		content = &(ms->next);
	    }
	    p = &(msg->next);
	}
}

/*
 * print_load_map() is called from layout() if a load map is requested.
 */
static
void
print_load_map(void)
{
    unsigned long i;
    struct merged_segment *msg;
    struct merged_section *ms;
    struct common_symbol *common_symbol;

	print("Load map for: %s\n", outputfile);
	print("Segment name     Section name     Address    Size\n");
	for(msg = merged_segments; msg ; msg = msg->next){
	    print("%-16.16s %-16.16s 0x%08x 0x%08x\n",
		   msg->sg.segname, "", (unsigned int)(msg->sg.vmaddr),
		   (unsigned int)(msg->sg.vmsize));
	    for(ms = msg->content_sections; ms ; ms = ms->next){
		print("%-16.16s %-16.16s 0x%08x 0x%08x",
		       ms->s.segname, ms->s.sectname,
		       (unsigned int)(ms->s.addr), (unsigned int)(ms->s.size));
		if(ms->contents_filename)
		    print(" from the file: %s\n", ms->contents_filename);
		else{
		    if(ms->order_load_maps){
			print("\n");
			for(i = 0; i < ms->norder_load_maps; i++){
			    if(dead_strip == FALSE ||
			       ms->order_load_maps[i].load_order->
			       fine_reloc->live == TRUE){
				print("\t\t\t\t  0x%08x 0x%08x ",
				  (unsigned int)(fine_reloc_output_offset(
				    ms->order_load_maps[i].section_map,
				    ms->order_load_maps[i].value -
				    ms->order_load_maps[i].section_map->
					s->addr) +
				    ms->order_load_maps[i].section_map->
					output_section->s.addr),
				    (unsigned int)
					(ms->order_load_maps[i].size));
			    }
			    else{
				print("\t\t\t\t  (dead stripped)       ");
			    }
			    if(ms->order_load_maps[i].archive_name != NULL)
				print("%s:",
					   ms->order_load_maps[i].archive_name);
			    if(ms->order_load_maps[i].symbol_name != NULL)
				print("%s:%s\n",
				      ms->order_load_maps[i].object_name,
				      ms->order_load_maps[i].symbol_name);
			    else
				print("%s\n",
				      ms->order_load_maps[i].object_name);
			}
		    }
		    else{
			print("\n");
			print_load_map_for_objects(ms);
		    }
		}
	    }
	    for(ms = msg->zerofill_sections; ms ; ms = ms->next){
		print("%-16.16s %-16.16s 0x%08x 0x%08x\n",
		       ms->s.segname, ms->s.sectname,
		       (unsigned int)(ms->s.addr), (unsigned int)(ms->s.size));
		if(ms->order_load_maps){
		    for(i = 0; i < ms->norder_load_maps; i++){
			if(dead_strip == FALSE ||
			   ms->order_load_maps[i].load_order->
			   fine_reloc->live == TRUE){
			    print("\t\t\t\t  0x%08x 0x%08x ",
				(unsigned int)(fine_reloc_output_offset(
				    ms->order_load_maps[i].section_map,
				    ms->order_load_maps[i].value -
				    ms->order_load_maps[i].section_map->
					s->addr) +
				    ms->order_load_maps[i].section_map->
					output_section->s.addr),
				    (unsigned int)
					(ms->order_load_maps[i].size));
			}
			else{
			    print("\t\t\t\t  (dead stripped)       ");
			}

/* old
			print("\t\t\t\t  0x%08x 0x%08x ",
			    (unsigned int)(fine_reloc_output_offset(
				ms->order_load_maps[i].section_map,
				ms->order_load_maps[i].value -
				ms->order_load_maps[i].section_map->s->addr) +
				ms->order_load_maps[i].section_map->
							output_section->s.addr),
				(unsigned int)(ms->order_load_maps[i].size));
*/
			if(ms->order_load_maps[i].archive_name != NULL)
			    print("%s:", ms->order_load_maps[i].archive_name);
			print("%s:%s\n",
			      ms->order_load_maps[i].object_name,
			      ms->order_load_maps[i].symbol_name);
		    }
		}
		else{
		    print_load_map_for_objects(ms);
		    if(common_load_map.common_ms == ms){
			common_symbol = common_load_map.common_symbols;
			for(i = 0; i < common_load_map.ncommon_symbols; i++){
			    print("\t\t\t\t  0x%08x 0x%08x symbol: %s\n",
			       (unsigned int)
				(common_symbol->merged_symbol->nlist.n_value),
			       (unsigned int)(common_symbol->common_size),
			       common_symbol->merged_symbol->nlist.n_un.n_name);
			    common_symbol++;
			}
			common_load_map.common_ms = NULL;
			common_load_map.ncommon_symbols = 0;
			free(common_load_map.common_symbols);
		    }
		}
	    }
	    if(msg->next != NULL)
		print("\n");
	}

	if(base_obj){
	    print("\nLoad map for base file: %s\n", base_obj->file_name);
	    print("Segment name     Section name     Address    Size\n");
	    for(msg = base_obj_segments; msg ; msg = msg->next){
		print("%-16.16s %-16.16s 0x%08x 0x%08x\n",
		      msg->sg.segname, "", (unsigned int)(msg->sg.vmaddr),
		      (unsigned int)(msg->sg.vmsize));
	    }
	}

	if(fvmlib_segments != NULL){
	    print("\nLoad map for fixed VM shared libraries\n");
	    print("Segment name     Section name     Address    Size\n");
	    for(msg = fvmlib_segments; msg ; msg = msg->next){
		print("%-16.16s %-16.16s 0x%08x 0x%08x %s\n",
		      msg->sg.segname, "", (unsigned int)(msg->sg.vmaddr),
		      (unsigned int)(msg->sg.vmsize), msg->filename);
	    }
	}
}

/*
 * print_load_map_for_objects() prints the load map for each object that has
 * a non-zero size in the specified merge section.
 */
static
void
print_load_map_for_objects(
struct merged_section *ms)
{
    unsigned long i, j, k;
    struct object_list *object_list, **p;
    struct object_file *object_file;
    struct fine_reloc *fine_relocs;

	for(p = &objects; *p; p = &(object_list->next)){
	    object_list = *p;
	    for(i = 0; i < object_list->used; i++){
		object_file = &(object_list->object_files[i]);
		if(object_file == base_obj)
		    continue;
		if(object_file->dylib)
		    continue;
		if(object_file->bundle_loader)
		    continue;
		if(object_file->dylinker)
		    continue;
		for(j = 0; j < object_file->nsection_maps; j++){
		    if(object_file->section_maps[j].output_section == ms &&
		       object_file->section_maps[j].s->size != 0){

		        if(object_file->section_maps[j].nfine_relocs != 0){
			    fine_relocs =
				object_file->section_maps[j].fine_relocs;
			    for(k = 0;
				k < object_file->section_maps[j].nfine_relocs;
				k++){
				print("  (input address 0x%08x) ",
				      (unsigned int)
				      (object_file->section_maps[j].s->addr +
					fine_relocs[k].input_offset));
				if((object_file->section_maps[j].s->flags &
				    SECTION_TYPE) == S_SYMBOL_STUBS ||
				   (object_file->section_maps[j].s->flags &
				    SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
				   (object_file->section_maps[j].s->flags &
				    SECTION_TYPE) ==
				    S_NON_LAZY_SYMBOL_POINTERS ||
				   (object_file->section_maps[j].s->flags &
				    SECTION_TYPE) == S_COALESCED){
				    if(fine_relocs[k].use_contents == FALSE)
					print("(eliminated)    ");
				    else if(dead_strip == TRUE &&
				       fine_relocs[k].live == FALSE)
					print("(dead stripped) ");
				    else
					print("     0x%08x ",
					      (unsigned int)(ms->s.addr +
						fine_relocs[k].output_offset));

				}
				else if(dead_strip == TRUE &&
				   fine_relocs[k].live == FALSE)
				    print("(dead stripped) ");
				else
				    print("     0x%08x ",
					  (unsigned int)(ms->s.addr +
					    fine_relocs[k].output_offset));
				print("0x%08x ",
				      (unsigned int)
				      (k == (unsigned int)
					((object_file->section_maps[j].
							    nfine_relocs) -
					(unsigned int)1) ?
				      (unsigned int)
				      (object_file->section_maps[j].s->size) -
					(unsigned int)(fine_relocs[k].
						input_offset) :
				      (unsigned int)(fine_relocs[k + 1].
					input_offset) -
					(unsigned int)(fine_relocs[k].
					input_offset)));
				print_obj_name(object_file);
				print("\n");
			    }
			}
			else{
			    print("\t\t\t\t  0x%08x 0x%08x ",
				   (unsigned int)(ms->s.addr +
				   object_file->section_maps[j].offset),
				   (unsigned int)
					(object_file->section_maps[j].s->size));
			    print_obj_name(object_file);
			    print("\n");
			}
		    }
		}
	    }
	}
}
#endif /* !defined(RLD) */

#ifdef DEBUG
/*
 * print_mach_header() prints the output file's mach header.  For debugging.
 */
__private_extern__
void
print_mach_header(void)
{
	print("Mach header for output file\n");
	print("    magic = 0x%x\n", (unsigned int)(output_mach_header.magic));
	print("    cputype = %d\n", output_mach_header.cputype);
	print("    cpusubtype = %d\n", output_mach_header.cpusubtype);
	print("    filetype = %u\n", output_mach_header.filetype);
	print("    ncmds = %u\n", output_mach_header.ncmds);
	print("    sizeofcmds = %u\n", output_mach_header.sizeofcmds);
	print("    flags = %u\n", output_mach_header.flags);
}

/*
 * print_symtab_info() prints the output file's symtab command.  For
 * debugging.
 */
__private_extern__
void
print_symtab_info(void)
{
	print("Symtab info for output file\n");
	print("    cmd = %u\n", output_symtab_info.symtab_command.cmd);
	print("    cmdsize = %u\n", output_symtab_info.symtab_command.cmdsize);
	print("    nsyms = %u\n", output_symtab_info.symtab_command.nsyms);
	print("    symoff = %u\n", output_symtab_info.symtab_command.symoff);
	print("    strsize = %u\n", output_symtab_info.symtab_command.strsize);
	print("    stroff = %u\n", output_symtab_info.symtab_command.stroff);
}

/*
 * print_thread_info() prints the output file's thread information.  For
 * debugging.
 */
__private_extern__
void
print_thread_info(void)
{
	print("Thread info for output file\n");
	print("    flavor = %lu\n", output_thread_info.flavor);
	print("    count = %lu\n", output_thread_info.count);
	print("    entry_point = 0x%x",
	      (unsigned int)(output_thread_info.entry_point));
	if(output_thread_info.entry_point != NULL)
	    print(" (0x%x)\n", (unsigned int)(*output_thread_info.entry_point));
	else
	    print("\n");
}
#endif /* DEBUG */

#ifndef RLD

/*
 * get_stack_size_from_flag() returns the default size of the userstack.  This
 * should be in the header file <bsd/XXX/vmparam.h> as MAXSSIZ. Since some
 * architectures have come and gone and come back, you can't include all of
 * these headers in one source and some of the constants covered the whole
 * address space the common value of 64meg was chosen.
 */
static
uint32_t
get_stack_size_from_flag(
			 const struct arch_flag *flag)
{
#ifdef __MWERKS__
    const struct arch_flag *dummy;
    dummy = flag;
#endif
    
    return(64*1024*1024);
}

/*
 * get_segprot_from_flag() returns the default segment protection.
 */
static
vm_prot_t
get_segprot_from_flag(
		      const struct arch_flag *flag)
{
    if(flag->cputype == CPU_TYPE_I386)
	return(VM_PROT_READ | VM_PROT_WRITE);
    else
	return(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
}

/*
 * get_stack_direction_from_flag() returns the direction the stack grows as
 * either positive (+1) or negative (-1) of the architecture for the
 * specified cputype and cpusubtype if known.  If unknown it returns 0.
 */
static
int
get_stack_direction_from_flag(
			      const struct arch_flag *flag)
{
    if(flag->cputype == CPU_TYPE_MC680x0 ||
       flag->cputype == CPU_TYPE_MC88000 ||
       flag->cputype == CPU_TYPE_POWERPC ||
       flag->cputype == CPU_TYPE_I386 ||
       flag->cputype == CPU_TYPE_SPARC ||
       flag->cputype == CPU_TYPE_I860 ||
       flag->cputype == CPU_TYPE_VEO ||
       flag->cputype == CPU_TYPE_ARM)
	return(-1);
    else if(flag->cputype == CPU_TYPE_HPPA)
	return(+1);
    else
	return(0);
}

#endif /* !defined(RLD) */
