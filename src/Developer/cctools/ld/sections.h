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
 * Global types, variables and routines declared in the file sections.c.
 *
 * The following include file need to be included before this file:
 * #include <sys/loader.h> 
 * #include "ld.h"
 * #include "objects.h"
 */

/*
 * The fields of the segment command in a merged segment are set and
 * maintained as follows:
 *	cmd		set in layout_segments() (layout)
 *	cmdsize		set in layout_segments() (layout)
 *	segname		set when the merged segment is created (pass1)
 *	vmaddr		set in process_segments() or layout_segments() (layout)
 *	vmsize		set in layout_segments() (layout)
 *	fileoff		set in layout_segments() (layout)
 *	filesize	set in layout_segments() (layout)
 *	maxprot		set in process_segments() or layout_segments() (layout)
 *	initprot	set in process_segments() or layout_segments() (layout)
 *	nsects		incremented as each section is merged (pass1)
 *	flags		set to 0 in process_segments and SG_NORELOC
 *			 conditionally or'ed in in pass2() (pass2)
 */
struct merged_segment {
    struct segment_command sg;	/* The output file's segment structure. */
    struct merged_section	/* The list of section that contain contents */
	*content_sections;	/*  that is non-zerofill sections. */
    struct merged_section	/* The list of zerofill sections. */
	*zerofill_sections;
    char *filename;		/* File this segment is in, the output file */
				/*  or a fixed VM shared library */
    enum bool addr_set;		/* TRUE when address of this segment is set */
    enum bool prot_set;		/* TRUE when protection of this segment is set*/
    enum bool split_dylib;	/* TRUE when this segment is from a dylib */
				/*  which is MH_SPLIT_SEGS */
    enum bool debug_only;	/* TRUE if segment contains sections with the */
				/*  S_ATTR_DEBUG attribute */
#ifdef RLD
    long set_num;		/* Object set this segment first appears in. */
#endif /* RLD */
    struct merged_segment *next;/* The next segment in the list. */
};

/*
 * The fields of the section structure in a merged section are set and
 * maintained as follows:
 *	sectname	set when the merged segment is created (pass1)
 *	segname		set when the merged segment is created (pass1)
 *	addr		set in layout_segments (layout)
 *	size		accumulated to the total size (pass1)
 *	offset		set in layout_segments (layout)
 *	align		merged to the maximum alignment (pass1)
 *	reloff		set in layout_segments (layout)
 *	nreloc		accumulated to the total count (pass1)
 *	flags		set when the merged segment is created (pass1)
 *	reserved1	zeroed when created in merged_section (pass1)
 *	reserved2	zeroed when created in merged_section (pass1)
 */
struct merged_section {
    struct section s;		/* The output file's section structure. */
    unsigned long output_sectnum;/* Section number in the output file. */
    unsigned long output_nrelocs;/* The current number of relocation entries */
				/*  written to the output file in pass2 for */
				/*  this section */
    /* These two fields are used to help set the SG_NORELOC flag */
    enum bool relocated;	/* This section was relocated */
    enum bool referenced;	/* This section was referenced by a relocation*/
				/*  entry (local or through a symbol). */
    /* The literal_* fields are used only if this section is a literal section*/
    void (*literal_merge)();	/* The routine to merge the literals. */
    void (*literal_output)();	/* The routine to write the literals. */
    void (*literal_free)();	/* The routine to free the literals. */
    void (*literal_order)();	/* The routine to order the literals. */
    void (*literal_reset_live)(); /* The routine to reset literal data before
				     only the live literals are re-merged */
    void *literal_data;		/* A pointer to a block of data to help merge */
				/*  and hold the literals. */
    /* These three fields are used only if this section is created from a file*/
    char *contents_filename;	/* File name for the contents of the section */
				/*  if it is created from a file, else NULL */
    char *file_addr;		/* address the above file is mapped at */
    unsigned long file_size;	/* size of above file as returned by stat(2) */

    /* These three fields are used only if this section has a -sectorder file */
    char *order_filename;	/* File name that contains the order that */
				/*  symbols are to loaded in this section */
    char *order_addr;		/* address the above file is mapped at */
    unsigned long order_size;	/* size of above file as returned by stat(2) */
    struct order_load_map	/* the load map for printing with -M */
	*order_load_maps;
    unsigned long
	norder_load_maps;	/* size of the above map */
#ifdef RLD
    long set_num;		/* Object set this section first appears in. */
#endif /* RLD */
    /* These four are used for output_for_dyld only if this is a non-regular
       section that will have relocation entries */
    unsigned long iextrel;	/* index into output external reloc entries */
    unsigned long nextrel;	/* number of output external reloc entries */
    unsigned long ilocrel;	/* index into output local reloc entries */
    unsigned long nlocrel;	/* number of output local reloc entries */
    struct merged_section *next;/* The next section in the list. */
};

/*
 * This is the load map (-M) for sections that have had their sections orders
 * with -sectorder option.
 */
struct order_load_map {
    char *archive_name;		/* archive name */
    char *object_name;		/* object name */
    char *symbol_name;		/* symbol name */
    unsigned long value;	/* symbol's value */
    struct section_map
	*section_map;		/* section map to relocate symbol's value */
    unsigned long size;		/* size of symbol in the input file */
    unsigned long order;	/* order the symbol appears in the section */
    struct load_order *load_order; /* the load_order for this map entry */
};

/* the pointer to the head of the output file's section list */
__private_extern__ struct merged_segment *merged_segments;
#ifdef RLD
__private_extern__ struct merged_segment *original_merged_segments;
#endif /* RLD */

/* the total number relocation entries */
__private_extern__ unsigned long nreloc;

/*
 * This is set to TRUE if any of the input objects do not have the
 * MH_SUBSECTIONS_VIA_SYMBOLS bit set in the mach_header flags field.
 */
__private_extern__ enum bool some_non_subsection_via_symbols_objects;

__private_extern__ void merge_sections(
    void);
__private_extern__ void remove_debug_segments(
    void);
__private_extern__ void merge_literal_sections(
    enum bool redo_live);
__private_extern__ void layout_ordered_sections(
    void);
__private_extern__ enum bool is_literal_output_offset_live(
    struct merged_section *ms,
    unsigned long output_offset);
__private_extern__ void parse_order_line(
    char *line,
    char **archive_name,
    char **object_name,
    char **symbol_name,
    struct merged_section *ms,
    unsigned long line_number);
__private_extern__ void resize_live_sections(
    void);
__private_extern__ void relayout_relocs(
    void);
__private_extern__ void output_literal_sections(
    void);
__private_extern__ void output_sections_from_files(
    void);
__private_extern__ void output_section(
    struct section_map *map);
__private_extern__ unsigned long pass2_nsect_merged_symbol_section_type(
    struct merged_symbol *merged_symbol);
__private_extern__ void nop_pure_instruction_scattered_sections(
    void);
__private_extern__ void flush_scatter_copied_sections(
    void);
__private_extern__ void live_marking(
    void);
__private_extern__ struct fine_reloc *get_fine_reloc_for_merged_symbol(
    struct merged_symbol *merged_symbol,
    struct section_map **local_map);
__private_extern__ unsigned long r_symbolnum_from_r_value(
    unsigned long r_value,
    struct object_file *obj);
__private_extern__ struct merged_section *create_merged_section(
    struct section *s);
__private_extern__ struct merged_segment *lookup_merged_segment(
    char *segname);
__private_extern__ struct merged_section *lookup_merged_section(
    char *segname,
    char *sectname);
__private_extern__ enum bool is_merged_symbol_coalesced(
    struct merged_symbol *merged_symbol);
__private_extern__ int qsort_load_order_values(
    const struct load_order *load_order1,
    const struct load_order *load_order2);
__private_extern__ unsigned long align_to_input_mod(
    unsigned long output_offset,
    unsigned long input_offset,
    unsigned long align);
#ifdef RLD
__private_extern__ void reset_merged_sections(
    void);
__private_extern__ void zero_merged_sections_sizes(
    void);
__private_extern__ void remove_merged_sections(
    void);
#endif /* RLD */

#ifdef DEBUG
__private_extern__ void print_merged_sections(
    char *string);
__private_extern__ void print_merged_section_stats(
    void);
__private_extern__ void print_name_arrays(
    void);
__private_extern__ void print_load_order(
    struct load_order *load_order,
    unsigned long nload_order,
    struct merged_section *ms,
    struct object_file *object_file,
    char *string);
#endif /* DEBUG */
