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
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines to manage the structures that hold the
 * command line specifications about segment and sections.
 */
#if !(defined(KLD) && defined(__STATIC__))
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <string.h>
#include <stdarg.h>
#include <mach-o/loader.h>
#include "stuff/arch.h"

#include "ld.h"
#ifndef RLD
#include "specs.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"

/* The structures to hold the information specified about segments */
__private_extern__ struct segment_spec *segment_specs = NULL;
__private_extern__ unsigned long nsegment_specs = 0;

/*
 * create_segment_spec() looks through the segment_specs and returns a pointer
 * to the segment_spec for the specified segment name.  If none exist then one
 * is created for this segname, the segname is set, it is initialized to zero
 * and a pointer to it is returned.
 */ 
__private_extern__
struct segment_spec *
create_segment_spec(
char *segname)
{
    unsigned long i;

	for(i = 0; i < nsegment_specs; i++){
	    if(strcmp(segment_specs[i].segname, segname) == 0)
		return(&(segment_specs[i]));
	}
	segment_specs = reallocate(segment_specs, (nsegment_specs + 1) *
						  sizeof(struct segment_spec));
	memset(&(segment_specs[nsegment_specs]), '\0',
	       sizeof(struct segment_spec));
	segment_specs[nsegment_specs].segname = segname;
	return(&(segment_specs[nsegment_specs++]));
}

/*
 * create_section_spec() looks through the section_specs for the specificed and
 * segment_spec and returns a pointer to the section_spec for the specified
 * section name.  If none exist then one is created for this sectname, the
 * sectname is set, it is initialized to zero and a pointer to it is returned.
 */ 
__private_extern__
struct section_spec *
create_section_spec(
struct segment_spec *seg_spec,
char *sectname)
{
    unsigned long i;

	for(i = 0; i < seg_spec->nsection_specs; i++){
	    if(strcmp(seg_spec->section_specs[i].sectname, sectname) == 0)
		return(&(seg_spec->section_specs[i]));
	}
	seg_spec->section_specs = reallocate(seg_spec->section_specs,
					     (seg_spec->nsection_specs + 1) *
					     sizeof(struct section_spec));
	memset(&(seg_spec->section_specs[seg_spec->nsection_specs]), '\0',
	       sizeof(struct section_spec));
	seg_spec->section_specs[seg_spec->nsection_specs].sectname = sectname;
	return(&(seg_spec->section_specs[seg_spec->nsection_specs++]));
}

/*
 * lookup_segment_spec() returns a pointer to the segment_spec for the specified
 * segment name.  NULL is returned if none exists.
 */
__private_extern__
struct segment_spec *
lookup_segment_spec(
char *segname)
{
    unsigned long i;

	for(i = 0; i < nsegment_specs; i++){
	    if(strcmp(segment_specs[i].segname, segname) == 0)
		return(&(segment_specs[i]));
	}
	return(NULL);
}

/*
 * lookup_section_spec() returns a pointer to the section_spec for the specified
 * section name (segname,sectname).  NULL is returned if none exists.
 */
__private_extern__
struct section_spec *
lookup_section_spec(
char *segname,
char *sectname)
{
    unsigned long i, j;

	for(i = 0; i < nsegment_specs; i++){
	    if(strcmp(segment_specs[i].segname, segname) == 0){
		for(j = 0; j < segment_specs[i].nsection_specs; j++){
		    if(strcmp(segment_specs[i].section_specs[j].sectname,
			      sectname) == 0)
			return(&(segment_specs[i].section_specs[j]));
		}
	    }
	}
	return(NULL);
}

/*
 * process_section_specs() folds in the information specified by command line
 * options into the merged section table (called by layout()).  Currently three
 * things can be specified for segments alignment, creation from a file, and a
 * layout order.  If there is an alignment specified for a section that does not
 * exist it is an error.  If the alignment specified is less than the merged
 * alignment a warning is issued.  If a section is to be created and it already
 * exist from the object files it is an error.  Section layout order can only be
 * specified for regular sections (non-zero fill, non-literal, not created from
 * a file, etc).
 */
__private_extern__
void
process_section_specs(void)
{
    unsigned long i, j, k;
    struct merged_section *ms;
    struct section_spec *sect_spec;
    struct section s = { {0} };

	/* check for long non-unique names in the specifications */
	for(i = 0; i < nsegment_specs; i++){
	    for(j = i + 1; j < nsegment_specs; j++){
		if(strncmp(segment_specs[i].segname, segment_specs[j].segname,
			   sizeof(s.segname)) == 0)
		    error("segment names: %s and %s not unique to %lu "
			  "characters\n", segment_specs[i].segname,
			  segment_specs[j].segname, sizeof(s.segname));
	    }
	    for(j = 0; j < segment_specs[i].nsection_specs; j++){
		for(k = j + 1; k < segment_specs[i].nsection_specs; k++){
		if(strncmp(segment_specs[i].section_specs[j].sectname,
			   segment_specs[i].section_specs[k].sectname,
			   sizeof(s.sectname)) == 0)
		    error("section names: %s and %s (in segment %s) not unique "
			  "to %lu characters\n",
			  segment_specs[i].section_specs[j].sectname,
			  segment_specs[i].section_specs[k].sectname,
			  segment_specs[i].segname, sizeof(s.segname));
		}
	    }
	}

	/* fold in the specifications about sections into the merged sections */
	for(i = 0; i < nsegment_specs; i++){
	    for(j = 0; j < segment_specs[i].nsection_specs; j++){
		/*
		 * The (__DATA,__common) section specification has already been
		 * processed if it it exists by define_common_symbols() and will
		 * be marked as processed so it doesn't have to be special cased
		 * here.
		 */
		if(segment_specs[i].section_specs[j].processed)
		    continue;
		/* lookup this section to see it there is a merged section */
		ms = lookup_merged_section(segment_specs[i].segname,
			  segment_specs[i].section_specs[j].sectname);
		sect_spec = &(segment_specs[i].section_specs[j]);
		if(ms == NULL){
		    /*
		     * There is no merged section so if this section is not to
		     * be created from a file issue a warning for it's
		     * specifications.
		     */
		    if(sect_spec->contents_filename == NULL){
			if(sect_spec->align_specified)
			    warning("no (%s,%s) section in output file, "
				    "specified alignment not used",
				    segment_specs[i].segname,
				    sect_spec->sectname);
			if(sect_spec->order_filename)
			    warning("no (%s,%s) section in output file, "
				    "-sectorder file: %s not used",
				    segment_specs[i].segname,
				    sect_spec->sectname,
				    sect_spec->order_filename);
		    }
		    else{
			if(sect_spec->order_filename)
			    warning("section (%s,%s) is to be created from "
				    "file: %s and is meaningless to have a "
				    "-sectorder file: %s for it",
				    segment_specs[i].segname,
				    sect_spec->sectname,
				    sect_spec->contents_filename,
				    sect_spec->order_filename);
			/*
			 * This section doesn't exist and is to be created from
			 * a file so create a merged section for it and apply
			 * all the specifications to it.
			 */
			strncpy(s.segname, segment_specs[i].segname,
				sizeof(s.segname));
			strncpy(s.sectname, sect_spec->sectname,
				sizeof(s.segname));
			ms = create_merged_section(&s);
			ms->contents_filename = sect_spec->contents_filename;
			ms->file_addr = sect_spec->file_addr;
			ms->file_size = sect_spec->file_size;
			if(sect_spec->align_specified)
			    ms->s.align = sect_spec->align;
			else
			    ms->s.align = defaultsectalign;
			ms->s.size = rnd(sect_spec->file_size,
					 1 << ms->s.align);
			/*
			 * Sections created from files don't have symbols and
			 * can't be referenced via a relocation entry.  So to
			 * avoid having their contents removed when -dead_strip
			 * is specified on later links we set this section
			 * attribute.
			 */
			ms->s.flags |= S_ATTR_NO_DEAD_STRIP;
		    }
		    sect_spec->processed = TRUE;
		    continue;
		}
		/*
		 * There is a merged section for this section specification
		 * so if this section is to be created from a file it is an
		 * error.
		 */
		if(sect_spec->contents_filename != NULL)
		    error("section (%.16s,%.16s) exist in the loaded object "
			  "files and can't be created from the file: %s",
			  ms->s.segname, ms->s.sectname,
			  sect_spec->contents_filename);
		else{
		    /* Increase the alignment to the specified alignment */
		    if(sect_spec->align_specified){
			if(ms->s.align > sect_spec->align)
			    warning("specified alignment (0x%x) for section (%s"
				    ",%s) not used (less than the required "
				    "alignment in the input files (0x%x))",
				    (unsigned int)(1 << sect_spec->align),
				    segment_specs[i].segname,
				    sect_spec->sectname,
				    (unsigned int)(1 << ms->s.align));
			else
			    ms->s.align = sect_spec->align;
		    }
		    if(sect_spec->order_filename != NULL){
			if(arch_flag.cputype == CPU_TYPE_I860)
			    warning("sections can't be ordered in I860 cputype "
				    "objects (-sectorder %s %s %s ignored)",
				    segment_specs[i].segname,
				    sect_spec->sectname,
				    sect_spec->order_filename);
			else if(filetype == MH_DYLIB &&
				strcmp(ms->s.segname, SEG_OBJC) == 0 &&
				strcmp(ms->s.sectname, SECT_OBJC_MODULES) == 0){
			    warning("for MH_DYLIB output files section ("
				SEG_OBJC "," SECT_OBJC_MODULES ") can't be "
				"ordered (-sectorder %s %s %s ignored)",
				segment_specs[i].segname, sect_spec->sectname,
				sect_spec->order_filename);
			}
			else if((ms->s.flags & S_ATTR_DEBUG) == S_ATTR_DEBUG){
			    warning("debug sections can't be ordered as they "
				    "won't appear in the output (-sectorder %s "
				    "%s %s ignored)", segment_specs[i].segname,
				    sect_spec->sectname,
				    sect_spec->order_filename);
			}
			else{
			    ms->order_filename = sect_spec->order_filename;
			    ms->order_addr = sect_spec->order_addr;
			    ms->order_size = sect_spec->order_size;
			}
		    }
		}
		sect_spec->processed = TRUE;
	    }
	}
}
/*
 * process_segment_specs() folds in the information specified by command line
 * options into the merged segment table (called by layout_segments()).  If
 * there is a specification for a segment that does not exist a warning is
 * issued.  If the address is not a multiple of the segment alignment it is an
 * error.
 */
__private_extern__
void
process_segment_specs(void)
{
    unsigned long i;
    struct merged_segment *msg;

	/* fold in the specifications about sections into the merged sections */
	for(i = 0; i < nsegment_specs; i++){
	    if(segment_specs[i].processed)
		continue;
	    /* lookup this segment to see it there is a merged segment */
	    msg = lookup_merged_segment(segment_specs[i].segname);
	    if(msg != NULL){
		if(segment_specs[i].addr_specified){
		    if(segment_specs[i].addr % segalign != 0){
			error("address: 0x%x specified for segment: %s not a "
			      "multiple of the segment alignment (0x%x)",
			      (unsigned int)(segment_specs[i].addr),
			      segment_specs[i].segname,
			      (unsigned int)(segalign));
		    }
		    else{
			msg->sg.vmaddr = segment_specs[i].addr;
			msg->addr_set = TRUE;
		    }
		}
		if(segment_specs[i].prot_specified){
		    msg->sg.maxprot = segment_specs[i].maxprot;
		    msg->sg.initprot = segment_specs[i].initprot;
		    msg->prot_set = TRUE;
		}
	    }
	    else{
		/*
		 * If the output format is MH_OBJECT just issue one warning
		 * and return if there are any specifications.
		 */
		if(filetype == MH_OBJECT &&
		   (segment_specs[i].addr_specified ||
		    segment_specs[i].prot_specified)){
		    warning("all named segment specifications ignored with "
			    "MH_OBJECT output file format");
		    return;
		}
		/*
		 * There is no merged segment so issue a warnings for it's
		 * specifications.
		 */
		if(segment_specs[i].addr_specified)
		    warning("segment: %s not in output file, specified address "
			    "not used", segment_specs[i].segname);
		if(segment_specs[i].prot_specified)
		    warning("segment: %s not in output file, specified "
			    "protection not used", segment_specs[i].segname);
	    }
	}
}
#endif /* !defined(RLD) */

#ifdef DEBUG
#ifndef RLD
/*
 * print_segment_specs() prints the segment specs.  Used for debugging.
 */
__private_extern__
void
print_segment_specs(void)
{
    unsigned long i, j;

	print("Segment specs\n");
	for(i = 0; i < nsegment_specs; i++){
	    print("    segname = %s\n", segment_specs[i].segname);
	    if(segment_specs[i].addr_specified)
		print("\taddr_specified TRUE\n"
		      "\taddr 0x%08x\n", (unsigned int)(segment_specs[i].addr));
	    else
		print("\taddr_specified FALSE\n");
	    if(segment_specs[i].prot_specified){
		print("\tprot_specified TRUE\n");
		print("\tmaxprot ");
		print_prot(segment_specs[i].maxprot);
		print("\n");
		print("\tinitprot ");
		print_prot(segment_specs[i].initprot);
		print("\n");
	    }
	    else{
		print("\tprot_specified FALSE\n");
	    }
	    print("\tnsection_specs %lu\n", segment_specs[i].nsection_specs);
	    print("\tSection specs\n");
	    for(j = 0; j < segment_specs[i].nsection_specs; j++){
		print("\t    sectname %s\n",
		       segment_specs[i].section_specs[j].sectname);
		if(segment_specs[i].section_specs[j].contents_filename != NULL){
		    print("\t    contents_filename %s\n",
			   segment_specs[i].section_specs[j].contents_filename);
		    print("\t    file_addr 0x%x\n", (unsigned int)
			   (segment_specs[i].section_specs[j].file_addr));
		    print("\t    file_size %lu\n",
			   segment_specs[i].section_specs[j].file_size);
		}
		if(segment_specs[i].section_specs[j].order_filename != NULL){
		    print("\t    order_filename %s\n",
			   segment_specs[i].section_specs[j].order_filename);
		    print("\t    order_addr 0x%x\n", (unsigned int)
			   (segment_specs[i].section_specs[j].order_addr));
		    print("\t    order_size %lu\n",
			   segment_specs[i].section_specs[j].order_size);
		}
		if(segment_specs[i].section_specs[j].align_specified)
		    print("\t    align_specified TRUE\n"
		           "\t    align %lu\n",
			   segment_specs[i].section_specs[j].align);
		else
		    print("\t    align_specified FALSE\n");
	    }
	}
}
#endif /* !defined(RLD) */

__private_extern__
void
print_prot(
vm_prot_t prot)
{
	if(prot & VM_PROT_READ)
	    print("r");
	else
	    print("-");
	if(prot & VM_PROT_WRITE)
	    print("w");
	else
	    print("-");
	if(prot & VM_PROT_EXECUTE)
	    print("x");
	else
	    print("-");
}
#endif /* DEBUG */
