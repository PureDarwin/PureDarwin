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
 * Global types, variables and routines declared in the file specs.c.
 *
 * The following include file need to be included before this file:
 * #include <mach.h>
 * #include "ld.h"
 */

/* Type to hold the information specified on the command line about segments */
struct segment_spec {
    char *segname;		/* full segment name from command line */
    enum bool addr_specified;	/* TRUE if address has been specified */
    enum bool prot_specified;	/* TRUE if protection has been specified */
    unsigned long addr;		/* specified address */
    vm_prot_t maxprot;		/* specified maximum protection */
    vm_prot_t initprot;		/* specified initial protection */
    unsigned long
	nsection_specs;		/* count of section_spec structures below */
    struct section_spec		/* list of section_spec structures for */
	  *section_specs;	/*  -segcreate options */
    enum bool processed;	/* TRUE after this has been processed */
};

/* Type to hold the information about sections specified on the command line */
struct section_spec {
    char *sectname;		/* full section name from command line */
    enum bool align_specified;	/* TRUE if alignment has been specified */
    unsigned long align;	/* the alignment (as a power of two) */
    char *contents_filename;	/* file name for the contents of the section */
    char *file_addr;		/* address the above file is mapped at */
    unsigned long file_size;	/* size of above file as returned by stat(2) */
    char *order_filename;	/* file name that contains the order that */
				/*  symbols are to loaded in this section */
    char *order_addr;		/* address the above file is mapped at */
    unsigned long order_size;	/* size of above file as returned by stat(2) */
    enum bool processed;	/* TRUE after this has been processed */
};

/* The structures to hold the information specified about segments */
__private_extern__ struct segment_spec *segment_specs;
__private_extern__ unsigned long nsegment_specs;

__private_extern__ struct segment_spec *create_segment_spec(
    char *segname);
__private_extern__ struct section_spec *create_section_spec(
    struct segment_spec *seg_spec,
    char *sectname);
__private_extern__ struct segment_spec * lookup_segment_spec(
    char *segname);
__private_extern__ struct section_spec *lookup_section_spec(
    char *segname,
    char *sectname);
__private_extern__ void process_section_specs(
    void);
__private_extern__ void process_segment_specs(
    void);
#ifdef DEBUG
__private_extern__ void print_segment_specs(
    void);
__private_extern__ void print_prot(
    vm_prot_t prot);
#endif /* DEBUG */
