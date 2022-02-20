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
 * Global types, variables and routines declared in the file objects.c.
 *
 * The following include file need to be included before this file:
 * #include "ld.h"
 * #include "sections.h"
 */

/*
 * The structure to hold the information for the object files actually loaded.
 */
struct object_file {
    char *file_name;		/* Name of the file the object is in. */
    char *obj_addr;		/* Address of the object file in memory. */
    unsigned long obj_size;	/* Size of the object file. */
    enum bool swapped;		/* TRUE if the bytesex of the object does not */
				/*  match the host bytesex. */
    enum bool fvmlib_stuff;	/* TRUE if any SG_FVMLIB segments or any */
				/*  LC_LOADFVMLIB or LC_IDFVMLIB load */
				/*  commands in the file. */
    enum bool dylib;		/* TRUE if mh->filetype == MH_DYLIB */
    enum bool dylib_stuff;	/* TRUE if any LC_LOAD_DYLIB or LC_ID_DYLIB */
				/*  load commands in the file. */
    enum bool bundle_loader;	/* TRUE this the -bundle_loader object */
    unsigned long		/* when -twolevel_name space is in effect */
	library_ordinal;	/*  this the library_ordinal for recording */
    unsigned long isub_image;   /* when -twolevel_name space is in effect */
				/*  this the sub_image index for recording */
    unsigned long nload_dylibs; /* Number of LC_LOAD_DYLIB commands */
    enum bool dylinker;		/* TRUE if mh->filetype == MH_DYLINKER */
    enum bool command_line;	/* TRUE if object is created for a symbol */
				/*  created on the command line. */
    struct ar_hdr *ar_hdr;	/* Archive header (NULL in not in archive). */
    char *ar_name;		/* Archive member name */
    unsigned long ar_name_size; /* Size of archive member name */
    struct dylib_module		/* Dynamic shared library module this object */
		*dylib_module;	/*  file struct refers to or NULL. */
    unsigned long nsection_maps;/* Total number of sections in this object. */
    struct section_map		/* A section map for each section in this. */
	*section_maps;		/*  object file. */
    struct symtab_command	/* The LC_SYMTAB load command which has */
	*symtab;		/*  offsets to the symbol and string tables. */
    struct dysymtab_command	/* The LC_DYSYMTAB load command which has */
	*dysymtab;		/*  offsets to the tables for dynamic linking.*/
    struct routines_command *rc;/* The LC_ROUTINES load command */
    unsigned long nundefineds;	/* Number of undefined symbols in the object */
    struct undefined_map	/* Map of undefined symbol indexes and */
	*undefined_maps;	/*  pointers to merged symbols (for external */
				/*  relocation) */
    unsigned long nextdefsym;	/* number of externally defined symbols */
    unsigned long iextdefsym;	/* output index to above symbols for MH_DYLIB */
    unsigned long nprivatesym;	/* number of private external symbols */
    unsigned long iprivatesym;	/* output index to above symbols */
    unsigned long cprivatesym;	/* current output index being assigned to */
				/*  private external symbols */
    unsigned long nlocalsym;	/* number of local symbols in output */
    unsigned long ilocalsym;	/* output index to above symbols */
    struct localsym_block	/* the list of blocks of local symbols that */
	*localsym_blocks;	/*  that are to be excluded from the output */
    struct section_map		/* Current map of symbols used for -sectorder */
	*cur_section_map;	/*  that is being processed from this object */
    char *resolved_path;	/* The full path name and length of the name */
    unsigned long		/*  for N_OSO stabs added with the -Sp option */
	resolved_path_len;
    const char * dwarf_name;	/* The name of the main source file, */
    const char * dwarf_comp_dir; /* and the current directory, */
				/*  from the DWARF information; NULL if none */
    size_t * dwarf_source_data; /* See read_dwarf_info in pass1.c for */
				/*  an explanation.  */
    const char * * dwarf_paths; /* Array of DWARF source file pathnames.  */
    size_t dwarf_num_paths;	/* Length of dwarf_paths.  */
  
#ifdef RLD
    long set_num;		/* The set number this object belongs to. */
    enum bool user_obj_addr;	/* TRUE if the user allocated the object */
				/*  file's memory (then rld doesn't */
				/*  deallocate it) */
    enum bool from_fat_file;	/* TRUE if the object is in a fat file (then */
				/*  the file's memory is deallocate by */
				/*  clean_archives_and_fats(). */
#endif
#ifndef RLD
    /* These are for MH_DYLIB files only */
    char *module_name;		/* module name */
    unsigned long nrefsym;	/* number of reference symbol table entries */
    unsigned long irefsym;	/* output index to reference symbol table */
    struct reference_map	/* Map of external symbols (pointers to merged*/
	*reference_maps;	/*  merged symbols and flag for reference type*/
    unsigned long iinit;	/* index into mod init section */
    unsigned long ninit;	/* number of mod init entries */
    unsigned long iterm;	/* index into mod term section */
    unsigned long nterm;	/* number of mod term entries */
    struct section_map		/* the section map for the (__OBJC, */
	*objc_module_info;	/*  __module_info) section if any. */
    enum bool init_module_loaded; /* has the module with the library */
				  /*  initialization routine been loaded */
#endif
    unsigned long imodtab;	/* output index to the module table */
    unsigned long iextrel;	/* index into output external reloc entries */
    unsigned long nextrel;	/* number of output external reloc entries */
    unsigned long ilocrel;	/* index into output local reloc entries */
    unsigned long nlocrel;	/* number of output local reloc entries */
};

/* The number of object_file structrures in object_list */
#ifndef RLD
#define NOBJECTS 120
#else
#define NOBJECTS 10
#endif

/*
 * The structure to hold a chunk the list of object files actually loaded.
 */
struct object_list {
    struct object_file
	object_files[NOBJECTS]; /* the object_file structures in this chunk */
    unsigned long used;		/* the number used in this chunk */
    struct object_list *next;	/* the next chunk (NULL in no more chunks) */
};

/*
 * The structure for information about each section of an object module.  This
 * gets allocated as an array and the sections in it are in the order that
 * they appear in the header so that section numbers (minus one) can be used as
 * indexes into this array.
 */
struct section_map {
    struct section *s;		/* Pointer to the section structure in the    */
				/*  object file. */
    struct merged_section	/* Pointer the output section to get the      */
	*output_section;	/*  section number this will be in the output */
				/*  file used in pass2 to rewrite relocation  */
				/*  entries. */
    unsigned long offset;	/* The offset from the start of the section   */
			        /* that this object file's section will start */
				/* at in the output file. */
				/*  the address the section will be at and is */
				/*  used in pass2 to do the relocation.	      */
    unsigned long flush_offset;	/* The offset from the start of the section   */
				/*  that is to be used by output_flush() that */
				/*  includes the area before the above offset */
				/*  for alignment. */
    struct fine_reloc		/* Map for relocation of literals or other    */
	*fine_relocs;		/*  items which are smaller than the section. */
    unsigned long nfine_relocs;	/* Number of structures in the map */
    enum bool no_load_order;	/* Not to be scattered, loaded normalily */
    unsigned long order;	/* order when no_load_order == TRUE */
    struct load_order		/* Map of symbols used for -sectorder */
	*load_orders;
    unsigned long nload_orders;	/* Number of structures in the map */
    enum bool start_section;	/* There is a symbol at the start of the */
				/*  section. */
    enum bool			/* For symbol stub sections if any indirect */
	absolute_indirect_defineds; /* symbol is defined as an absolute */
    enum bool			/* TRUE when the input relocation entries */
	input_relocs_already_swapped; /* have been already swapped */
    /*
     * These are set in count_reloc() and tested in output_section() for
     * internal error checking.
     */
    unsigned long nlocrel;	/* number of local reloc entries */
    unsigned long nextrel;	/* number of external reloc entries */
    /*
     * These next two fields along with the above two fields are used for
     * output_for_dyld only if this is a non-regular section that will have
     * external relocation entries (currently only coaleseced sections)
     */
    unsigned long iextrel;	/* index into output external reloc entries */
    unsigned long ilocrel;	/* index into output local reloc entries */
};

/*
 * This structure is used for relocating items in a section from an input file
 * when they are scatered in the output file (normally the section is copied 
 * as a whole).  The address in the input file and the resulting address of
 * the section in the output file are also needed to do the relocation.
 * The two fields indirect_defined and use_contents are used for indirect
 * sections to indicate if the indirect symbol for the item is defined and if
 * the item from the object file will be used in the output file.
 */
struct fine_reloc {
    unsigned long
	indirect_defined:1,	     /* TRUE if the indirect sym is defined */
	use_contents:1,		     /* TRUE if this item is used */
	local_symbol:1,		     /* TRUE if the indirect sym is local */
	live:1,			     /* TRUE if referenced (for -dead_strip) */
	refs_marked_live:1,	     /* TRUE when references marked live */
	searched_for_live_refs:1,    /* TRUE if searched for live refs */
	indirect_symbol_local:1,     /* TRUE if this is for an indirect */
				     /*  section with INDIRECT_SYMBOL_LOCAL */
	unused_bits:25;
    unsigned long input_offset;      /* offset in the input file for the item */
    unsigned long output_offset;     /* offset in the output file for the item*/
    struct merged_symbol	     /* the global merged_symbol for the item */
		  *merged_symbol;    /*  if any (else NULL) */
    struct ref *refs;		     /* if dead code stripping list of refs */
};

/*
 * When dead code stripping a linked list of these ref structures are created
 * from the relocation entries for each fine_reloc.  There are fields if this
 * is for an external reference and fields for a local references.  One set of
 * fields are set and the other are set to NULL.
 */
struct ref {
    /* if this reference came from an external symbol this is set */
    struct merged_symbol *merged_symbol;

    /* if this reference is to a local block these are set */
    struct fine_reloc *fine_reloc;
    struct section_map *map;
    struct object_file *obj;

    /* next reference in the list if any, else NULL */
    struct ref *next;
};

/*
 * This structure is used when a section has a -sectorder order file and is used
 * to set the orders and then build fine_reloc structures for it so it can be
 * scatter loaded.
 */
struct load_order {
    char *name;			/* symbol's name */
    unsigned long value;	/* symbol's value */
    unsigned long index;	/* symbol's index in symbol table */
    unsigned long order;	/* order in output, 0 if not assigned yet */
    unsigned long line_number;  /* line number if specified or zero */
    unsigned long input_offset;	/* offset in the input file for the item */
    unsigned long input_size;	/* size of symbol in the input file */
    unsigned long output_offset;/* offset in the output file for the item */
    struct fine_reloc *fine_reloc; /* the fine_reloc for this load_order */
    /* the following is only used in coalesced_section_merge() to know if this
       load_order struct is for global coalesced symbol */
    enum bool global_coalesced_symbol;
};

/*
 * This structure holds pairs of indexes into an object files symbol table and
 * pointers to merged symbol table structures for each symbol that is an
 * undefined symbol in an object file.
 */
struct undefined_map {
    unsigned long index; /* index of symbol in the object file's symbol table */
    struct merged_symbol /* pointer to the merged symbol */
	*merged_symbol;
};

/*
 * This structure holds reference type flags and pointers to merged symbol table
 * structures for each exteranl symbol that is referenced in an object file that
 * will be part of a MH_DYLIB file.  These structures are used to create the
 * reference table for a MH_DYLIB file.
 */
struct reference_map {
    unsigned long flags; /* type of reference made to this symbol */
    struct merged_symbol /* pointer to the merged symbol being referenced */
	*merged_symbol;
};

/*
 * The head of the object file list and the total count of all object files
 * in the list.
 */
__private_extern__ struct object_list *objects;
__private_extern__ unsigned long nobjects;

/*
 * A pointer to the current object being processed in pass1 or pass2.
 */
__private_extern__ struct object_file *cur_obj;

/*
 * A pointer to the base object for an incremental link if not NULL.
 */
__private_extern__ struct object_file *base_obj;

__private_extern__ struct object_file *new_object_file(
    void);
#ifndef RLD
__private_extern__ unsigned long object_index(
    struct object_file *obj);
#endif
__private_extern__ struct object_file *add_last_object_file(
    struct object_file *new_object);
__private_extern__ void remove_last_object_file(
    struct object_file *last_object);
__private_extern__ void print_obj_name(
    struct object_file *obj);
__private_extern__ unsigned long size_ar_name(
    struct ar_hdr *ar_hdr);
__private_extern__ void set_obj_resolved_path(
    struct object_file *obj);
__private_extern__ void print_whatsloaded(
    void);
__private_extern__ enum bool is_dylib_module_loaded(
    struct dylib_module *dylib_module);
__private_extern__ unsigned long fine_reloc_output_offset(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ unsigned long fine_reloc_output_address(
    struct section_map *map,
    unsigned long input_offset,
    unsigned long output_base_address);
__private_extern__ void fine_reloc_output_ref(
    struct section_map *map,
    unsigned long input_offset,
    struct live_ref *ref);
__private_extern__ enum bool fine_reloc_offset_in_output(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ enum bool fine_reloc_offset_in_output_for_output_offset(
    struct section_map *map,
    unsigned long output_offset);
__private_extern__ unsigned long fine_reloc_output_sectnum(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ enum bool fine_reloc_arm(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ enum bool fine_reloc_thumb(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ enum bool fine_reloc_local(
    struct section_map *map,
    unsigned long input_offset);
__private_extern__ struct fine_reloc *fine_reloc_for_input_offset(
    struct section_map *map,
    unsigned long input_offset);
#ifdef RLD
__private_extern__ void clean_objects(
    void);
__private_extern__ void remove_objects(
    void);
#endif /* RLD */

#ifdef DEBUG
__private_extern__ void print_object_list(
    void);
__private_extern__ void print_fine_relocs(
    struct fine_reloc *fine_relocs,
    unsigned long nfine_relocs,
    char *string);
#endif /* DEBUG */
