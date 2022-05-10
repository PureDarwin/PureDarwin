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
 * The Apple, Inc. Mach-O (Mach object file format) link-editor.  This file
 * contains the main() routine and the global error handling routines and other
 * miscellaneous small global routines.  It also defines the global varaibles
 * that are set or changed by command line arguments.
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "stuff/arch.h"
#include "stuff/version_number.h"
#include "stuff/guess_short_name.h"
#include "stuff/macosx_deployment_target.h"
#include "stuff/execute.h"
#if !(defined(KLD))
#include <stdio.h>
#endif
#if !(defined(KLD) && defined(__STATIC__))
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include <ar.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include "stuff/seg_addr_table.h"
#ifndef RLD
#include "stuff/symbol_list.h"
#endif
#include <mach/mach_init.h>
#if defined(__OPENSTEP__) || defined(__GONZO_BUNSEN_BEAKER__)
#include <servers/netname.h>
#else
#include <servers/bootstrap.h>
#endif
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/mach.h>
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */

#include "ld.h"
#ifndef RLD
static char *mkstr(
	const char *args,
	...);
#endif /* !defined(RLD) */
#include "specs.h"
#include "pass1.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "fvmlibs.h"
#include "symbols.h"
#include "layout.h"
#include "pass2.h"

/* name of this program as executed (argv[0]) */
__private_extern__ char *progname = NULL;
/* indication of an error set in error(), for processing a number of errors
   and then exiting */
__private_extern__ unsigned long errors = 0;
/* the pagesize of the machine this program is running on, getpagesize() value*/
__private_extern__ unsigned long host_pagesize = 0;
/* the byte sex of the machine this program is running on */
__private_extern__ enum byte_sex host_byte_sex = UNKNOWN_BYTE_SEX;

/* name of output file */
__private_extern__ char *outputfile = NULL;
/* type of output file */
__private_extern__ unsigned long filetype = MH_EXECUTE;
/* multi or single module dylib output */
__private_extern__ enum bool multi_module_dylib = TRUE;
#ifndef RLD
static enum bool filetype_specified = FALSE;
static enum bool moduletype_specified = FALSE;
/* if the -A flag is specified use to set the object file type */
static enum bool Aflag_specified = FALSE;
#endif /* !defined(RLD) */
/*
 * The architecture of the output file as specified by -arch and the cputype
 * and cpusubtype of the object files being loaded which will be the output
 * cputype and cpusubtype.  specific_arch_flag is true if an -arch flag is
 * specified and the flag for a specific implementation of an architecture.
 */
__private_extern__ struct arch_flag arch_flag =
#if defined(KLD) && defined(__STATIC__)

#ifdef __ppc__
    { "ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL };
#elif __i386__
    { "i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL };
#elif __arm__
    { "arm",	CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_ALL };
#else
#error "unsupported architecture for static KLD"
#endif

#else /* !(defined(KLD) && defined(__STATIC__)) */
    { 0 };
#endif /* defined(KLD) && defined(__STATIC__) */
__private_extern__ enum bool specific_arch_flag = FALSE;

/*
 * The -force_cpusubtype_ALL flag.
 */
__private_extern__ enum bool force_cpusubtype_ALL = FALSE;

/* the byte sex of the output file */
__private_extern__ enum byte_sex target_byte_sex = UNKNOWN_BYTE_SEX;
static enum bool arch_multiple = FALSE;	/* print one arch message before error*/

__private_extern__
enum bool trace = FALSE;		/* print stages of link-editing */
__private_extern__
enum bool save_reloc = FALSE;		/* save relocation information */
__private_extern__
enum bool output_for_dyld = FALSE;	/* produce output for use with dyld */
__private_extern__
enum bool bind_at_load = FALSE;		/* mark the output for dyld to be bound
					   when loaded */
__private_extern__
enum bool no_fix_prebinding = FALSE;	/* mark the output for dyld to never
					   run fix_prebinding */
__private_extern__
enum bool load_map = FALSE;		/* print a load map */
__private_extern__
enum bool define_comldsyms = TRUE;	/* define common and link-editor defined
					   symbol reguardless of file type */
#ifndef RLD
static enum bool
    dflag_specified = FALSE;		/* the -d flag has been specified */
#endif /* !defined(RLD) */
__private_extern__
enum bool seglinkedit = FALSE;		/* create the link edit segment */
#ifndef RLD
static enum bool
    seglinkedit_specified = FALSE;	/* if either -seglinkedit or */
					/*  -noseglinkedit was specified */
#endif /* !defined(RLD) */
__private_extern__
enum bool whyload = FALSE;		/* print why archive members are
					   loaded */
#ifndef RLD
static enum bool whatsloaded = FALSE;	/* print which object files are loaded*/
#endif /* !defined(RLD) */
__private_extern__
enum bool flush = TRUE;			/* Use the output_flush routine to flush
					   output file by pages */
__private_extern__
enum bool sectorder_detail = FALSE;	/* print sectorder warnings in detail */
__private_extern__
enum bool nowarnings = FALSE;		/* suppress warnings */
__private_extern__
enum bool no_arch_warnings = FALSE;	/* suppress wrong arch warnings */
__private_extern__
enum bool arch_errors_fatal = FALSE;	/* cause wrong arch errors to be fatal*/
__private_extern__
enum bool archive_ObjC = FALSE;		/* objective-C archive semantics */
__private_extern__
enum bool archive_all = FALSE;		/* always load everything in archives */
__private_extern__
enum bool keep_private_externs = FALSE;	/* don't turn private externs into
					   non-external symbols */
/* TRUE if -dynamic is specified, FALSE if -static is specified */
__private_extern__
enum bool dynamic = TRUE;
#ifndef RLD
static enum bool dynamic_specified = FALSE;
static enum bool static_specified = FALSE;
#endif

/* The level of symbol table stripping */
__private_extern__ enum strip_levels strip_level = STRIP_DUP_INCLS;
/* Strip the base file symbols (the -A argument's symbols) */
__private_extern__ enum bool strip_base_symbols = FALSE;

/* strip dead blocks */
__private_extern__ enum bool dead_strip = FALSE;
/* don't strip module init and term sections */
__private_extern__ enum bool no_dead_strip_inits_and_terms = FALSE;
/* print timings for dead striping code */
__private_extern__ enum bool dead_strip_times = FALSE;

#ifndef RLD
/*
 * Data structures to perform selective exporting of global symbols.
 * save_symbols is the names of the symbols from -exported_symbols_list
 * remove_symbols is the names of the symbols from -unexported_symbols_list
 */
__private_extern__ struct symbol_list *save_symbols = NULL;
__private_extern__ uint32_t nsave_symbols = 0;
__private_extern__ struct symbol_list *remove_symbols = NULL;
__private_extern__ uint32_t nremove_symbols = 0;

/*
 * -executable_path option's argument, executable_path is used to replace
 * @executable_path for dependent libraries.
 */
__private_extern__ char *executable_path = NULL;
#endif /* RLD */


/* The list of symbols to be traced */
__private_extern__ char **trace_syms = NULL;
__private_extern__ unsigned long ntrace_syms = 0;

/* The number of references of undefined symbols to print */
__private_extern__ unsigned long Yflag = 0;

/* The list of allowed undefined symbols */
__private_extern__ char **undef_syms = NULL;
__private_extern__ unsigned long nundef_syms = 0;

/* The list of -dylib_file arguments */
__private_extern__ char **dylib_files = NULL;
__private_extern__ unsigned long ndylib_files = 0;

/* The checking for undefined symbols */
__private_extern__ enum undefined_check_level undefined_flag = UNDEFINED_ERROR;
#ifndef RLD
static enum bool undefined_flag_specified = FALSE;
#endif

/* The checking for (twolevel namespace) multiply defined symbols */
__private_extern__ enum multiply_defined_check_level
    multiply_defined_flag = MULTIPLY_DEFINED_WARNING;
__private_extern__ enum multiply_defined_check_level
    multiply_defined_unused_flag = MULTIPLY_DEFINED_SUPPRESS;
/* the -nomultidefs option */
__private_extern__ enum bool nomultidefs = FALSE;
#ifndef RLD
static enum bool multiply_defined_flag_specified = FALSE;
static enum bool multiply_defined_unused_flag_specified = FALSE;
#endif

/* The checking for read only relocs */
__private_extern__ enum read_only_reloc_check_level
    read_only_reloc_flag = READ_ONLY_RELOC_ERROR;

/* The checking for section difference relocs */
__private_extern__ enum sect_diff_reloc_check_level
    sect_diff_reloc_flag = SECT_DIFF_RELOC_SUPPRESS;

/* The handling for weak reference mismatches */
__private_extern__ enum weak_reference_mismatches_handling
    weak_reference_mismatches = WEAK_REFS_MISMATCH_ERROR;

/* The Mac OS X deployment target */
__private_extern__ struct macosx_deployment_target
	macosx_deployment_target = { 0 };

/* The prebinding optimization */
#ifndef RLD
static enum bool prebinding_flag_specified = FALSE;
#endif
__private_extern__ enum bool prebinding = FALSE;
__private_extern__ enum bool prebind_allow_overlap = FALSE;
__private_extern__ enum bool prebind_all_twolevel_modules = FALSE;
#ifndef RLD
static enum bool read_only_reloc_flag_specified = FALSE;
static enum bool sect_diff_reloc_flag_specified = FALSE;
static enum bool weak_reference_mismatches_specified = FALSE;
static enum bool prebind_all_twolevel_modules_specified = FALSE;
static enum bool unprebound_library(
    char *dylib_install_name,
    char *seg_addr_table_filename);
#endif

/* True if -m is specified to allow multiply symbols, as a warning */
__private_extern__ enum bool allow_multiply_defined_symbols = FALSE;

/* The segment alignment and pagezero_size, note the segalign is reset in
 * layout() by get_segalign_from_flag() based on the target architecture.
 */
__private_extern__ unsigned long segalign = 0x2000;
#ifndef RLD
__private_extern__ enum bool segalign_specified = FALSE;
#endif /* !defined(RLD) */
__private_extern__ unsigned long pagezero_size = 0;

/* The default section alignment */
__private_extern__ unsigned long defaultsectalign = DEFAULTSECTALIGN;

/* The first segment address */
__private_extern__ unsigned long seg1addr = 0;
__private_extern__ enum bool seg1addr_specified = FALSE;

/* read-only and read-write segment addresses */
__private_extern__ unsigned long segs_read_only_addr = 0;
__private_extern__ enum bool segs_read_only_addr_specified = FALSE;
__private_extern__ unsigned long segs_read_write_addr = 0;
__private_extern__ enum bool segs_read_write_addr_specified = FALSE;

#ifndef RLD
/* file name of the segment address table */
static char *seg_addr_table_name = NULL;
/* the file system path name to use instead of the install name */
static char *seg_addr_table_filename = NULL;
#endif /* !defined(RLD) */

/* The stack address and size */
__private_extern__ unsigned long stack_addr = 0;
__private_extern__ enum bool stack_addr_specified = FALSE;
__private_extern__ unsigned long stack_size = 0;
__private_extern__ enum bool stack_size_specified = FALSE;

/* TRUE if -allow_stack_execute is specified */
__private_extern__ enum bool allow_stack_execute = FALSE;

#ifndef RLD
/* A -segaddr option was specified */
static enum bool segaddr_specified = FALSE;
#endif /* !defined(RLD) */

/*
 * The header pad, the default is set to the size of a section strcuture so
 * that if /bin/objcunique is run on the result and up to two sections can be
 * added.
 */
__private_extern__ unsigned long headerpad = sizeof(struct section) * 2;
#ifndef RLD
static enum bool headerpad_specified = FALSE;
#endif /* !defined(RLD) */
/*
 * If specified makes sure the header pad is big enough to change all the
 * install name of the dylibs in the output to MAXPATHLEN.
 */
__private_extern__ enum bool headerpad_max_install_names = FALSE;

/* The name of the specified entry point */
__private_extern__ char *entry_point_name = NULL;

/* The name of the specified library initialization routine */
__private_extern__ char *init_name = NULL;

/* The dylib information */
__private_extern__ char *dylib_install_name = NULL;
__private_extern__ uint32_t dylib_current_version = 0;
__private_extern__ uint32_t dylib_compatibility_version = 0;

/* the umbrella/sub/client framework information */
__private_extern__ enum bool sub_framework = FALSE;
__private_extern__ enum bool umbrella_framework = FALSE;
__private_extern__ char *sub_framework_name = NULL;
__private_extern__ char *umbrella_framework_name = NULL;
__private_extern__ char *client_name = NULL;
__private_extern__ char **allowable_clients = NULL;
__private_extern__ unsigned long nallowable_clients = 0;

/* The list of sub_umbrella frameworks */
__private_extern__ char **sub_umbrellas = NULL;
__private_extern__ unsigned long nsub_umbrellas = 0;

/* The list of sub_library dynamic libraries */
__private_extern__ char **sub_librarys = NULL;
__private_extern__ unsigned long nsub_librarys = 0;

/* The dylinker information */
__private_extern__ char *dylinker_install_name = NULL;

#ifndef RLD
/* set to the -bundle_loader argument if specified */
static char *bundle_loader = NULL;
#endif

/* set to TRUE if -private_bundle is specified */
__private_extern__ enum bool private_bundle = FALSE;

/* The value of the environment variable NEXT_ROOT or the -syslibroot argument*/
__private_extern__ char *next_root = NULL;
#ifndef RLD
static enum bool syslibroot_specified = FALSE;
#endif

/* TRUE if the environment variable LD_TRACE_ARCHIVES
   (or temporarily RC_TRACE_ARCHIVES) is set */
__private_extern__ enum bool ld_trace_archives = FALSE;

/* TRUE if the environment variable LD_TRACE_DYLIBS
   (or temporarily RC_TRACE_DYLIBS) is set */
__private_extern__ enum bool ld_trace_dylibs = FALSE;

/* TRUE if the environment variable LD_TRACE_PREBINDING_DISABLED
   (or temporarily LD_TRACE_PREBINDING_DISABLED) is set */
__private_extern__ enum bool ld_trace_prebinding_disabled = FALSE;

#ifndef KLD
/* The file LD_TRACE_FILE references, or NULL if none is set */
static const char *trace_file_path = NULL;
#endif

/* the argument to -final_output if any */
__private_extern__ char *final_output = NULL;

/* The variables to support namespace options */
__private_extern__ enum bool namespace_specified = FALSE;
__private_extern__ enum bool twolevel_namespace = TRUE;
__private_extern__ enum bool force_flat_namespace = FALSE;

#ifndef RLD
/* Variable to support options logging.  */
static enum bool ld_print_options = FALSE;
#endif

/*
 * Because the MacOS X 10.0 code in libSystem for the NSObjectFileImage*() APIs
 * does not ignore unknown load commands if MH_BUNDLE files are built with
 * two-level namespace hints the LC_TWOLEVEL_HINTS load command will produce a
 * "malformed object" errors.  So to make the MacOS X 10.1 ld(1) produce
 * MH_BUNDLE files that will work on MacOS X 10.0 the hints table is not
 * produced by default for MH_BUNDLE files.
 */
#ifndef RLD
static enum bool twolevel_namespace_hints_specified = FALSE;
#endif
__private_extern__ enum bool twolevel_namespace_hints = TRUE;

#ifdef DEBUG
__private_extern__ unsigned long debug = 0;	/* link-editor debugging */
#endif /* DEBUG */

#ifdef RLD
/* the cleanup routine for fatal errors to remove the output file */
__private_extern__ void cleanup(void);
#else /* !defined(RLD) */
static void cleanup(void);
static void ld_exit(int exit_value);

/* The signal hander routine for SIGINT, SIGTERM, SIGBUS & SIGSEGV */
static void handler(int sig);

/* Static routines to help parse arguments */
static enum bool ispoweroftwo(unsigned long x);
static vm_prot_t getprot(char *prot, char **endp);
static enum bool check_max_init_prot(vm_prot_t maxprot, vm_prot_t initprot);

/* apple_version is created by the libstuff/Makefile */
extern char apple_version[];

/*
 * main() parses the command line arguments and drives the link-edit process.
 */
int
main(
int argc,
char *argv[],
char *envp[])
{
    int i;
    unsigned long j, symbols_created, objects_specified, sections_created;
    uint32_t table_size;
    int fd;
    char *p, *symbol_name, *indr_symbol_name, *endp, *file_name;
    char *filelist, *dirname, *addr, *env_seg_addr_table_name;
    struct seg_addr_table *seg_addr_table, *seg_addr_table_entry;
    struct segment_spec *seg_spec;
    struct section_spec *sect_spec;
    unsigned long align, tmp;
    struct stat stat_buf;
    kern_return_t r;
    const struct arch_flag *family_arch_flag;
    enum undefined_check_level new_undefined_flag;
    enum multiply_defined_check_level new_multiply_defined_flag,
	new_multiply_defined_unused_flag;
    enum read_only_reloc_check_level new_read_only_reloc_flag;
    enum sect_diff_reloc_check_level new_sect_diff_reloc_flag;
    enum weak_reference_mismatches_handling new_weak_reference_mismatches;
    enum bool is_framework;
    char *has_suffix;
    struct symbol_list *sp;
    char *exported_symbols_list, *unexported_symbols_list;
    enum bool missing_syms;
    enum bool vflag;
    enum bool prebinding_via_LD_PREBIND;
    enum bool hash_instrument_specified;
    char *ld_library_path;

#ifdef __MWERKS__
    char **dummy;
        dummy = envp;
#endif

	vflag = FALSE;
	exported_symbols_list = NULL;
	unexported_symbols_list = NULL;
	seg_addr_table_entry = NULL;
	hash_instrument_specified = FALSE;

	progname = argv[0];
#ifndef BINARY_COMPARE
	host_pagesize = 0x2000;
#else
	host_pagesize = getpagesize();
#endif
	host_byte_sex = get_host_byte_sex();

	if(argc == 1)
	    fatal("Usage: %s [options] file [...]", progname);

	/*
	 * If interrupt and termination signal are not being ignored catch
	 * them so things can be cleaned up.
	 */
	if(signal(SIGINT, SIG_IGN) != SIG_IGN)
	    signal(SIGINT, handler);
	if(signal(SIGTERM, SIG_IGN) != SIG_IGN)
	    signal(SIGTERM, handler);
	if(signal(SIGBUS, SIG_IGN) != SIG_IGN)
	    signal(SIGBUS, handler);
	if(signal(SIGSEGV, SIG_IGN) != SIG_IGN)
	    signal(SIGSEGV, handler);

	/* This needs to be here so that we test the environment variable before
	   the rest of options parsing.  */
	if (getenv("LD_PRINT_OPTIONS") != NULL)
	  ld_print_options = TRUE;

	/*
	 * Parse the command line options in this pass and skip the object files
	 * and symbol creation flags in this pass.  This will make sure optionsd
	 * like -Ldir are not position dependent relative to -lx options (the
	 * same for -ysymbol relative to object files, etc).
	 */
	for(i = 1 ; i < argc ; i++){
	    if(*argv[i] != '-'){
		/* object file argv[i] processed in the next pass of
		   parsing arguments */
		continue;
	    }
	    else{
	        if (ld_print_options == TRUE)
		  print("[Logging ld options]\t%s\n", argv[i]);

	        p = &(argv[i][1]);
		switch(*p){
		case 'l':
		    if(p[1] == '\0')
			fatal("-l: argument missing");
		    /* path searched abbrevated file name, processed in the
		       next pass of parsing arguments */
		    break;

		/* Flags effecting search path of -lx arguments */
		case 'L':
		    if(p[1] == '\0')
			fatal("-L: directory name missing");
		    /* add a pathname to the list of search paths */
		    search_dirs = reallocate(search_dirs,
					   (nsearch_dirs + 1) * sizeof(char *));
		    search_dirs[nsearch_dirs++] = &(p[1]);
		    if(stat(&(p[1]), &stat_buf) == -1)
			warning("-L: directory name (%s) does not exist",
				&(p[1]));
		    break;
		case 'Z':
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* do not use the standard search path */
		    standard_dirs[0] = NULL;
		    standard_framework_dirs[0] = NULL;
		    break;

		/* File format flags */
		case 'M':
		    if(strcmp(p, "Mach") == 0){
			if(filetype_specified == TRUE && filetype != MH_EXECUTE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_EXECUTE;
		    }
		    else if(strcmp(p, "M") == 0){
			/* produce load map */
			load_map = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'p':
		    if(strcmp(p, "preload") == 0 || p[1] == '\0'){
			if(filetype_specified == TRUE && filetype != MH_PRELOAD)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_PRELOAD;
		    }
		    else if(strcmp(p, "pagezero_size") == 0){
			if(i + 1 >= argc)
			    fatal("-pagezero_size: argument missing");
			if(pagezero_size != 0)
			    fatal("-pagezero_size: multiply specified");
			pagezero_size = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("size for -pagezero_size %s not a proper "
				  "hexadecimal number", argv[i+1]);
			if(pagezero_size == 0)
			    fatal("size for -pagezero_size %s must not be zero",
				  argv[i+1]);
			i += 1;
		    }
		    else if(strcmp(p, "prebind") == 0){
			if(prebinding_flag_specified == TRUE &&
			   prebinding == FALSE)
			    fatal("both -prebind and -noprebind can't "
				  "be specified");
			prebinding_flag_specified = TRUE;
			prebinding = TRUE;
		    }
		    else if(strcmp(p, "prebind_allow_overlap") == 0){
			prebind_allow_overlap = TRUE;
		    }
		    else if(strcmp(p, "prebind_all_twolevel_modules") == 0){
			if(prebind_all_twolevel_modules_specified == TRUE &&
			   prebind_all_twolevel_modules == FALSE)
			    fatal("both -prebind_all_twolevel_modules and "
				  "-noprebind_all_twolevel_modules can't be "
				  "specified");
			prebind_all_twolevel_modules = TRUE;
			prebind_all_twolevel_modules_specified = TRUE;
		    }
		    else if(strcmp(p, "private_bundle") == 0){
			private_bundle = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'f':
		    if(p[1] == '\0')
			fatal("use of old flag -f (old version of mkshlib(1) "
			      "will not work with this version of ld(1))");
		    else if(strcmp(p, "fvmlib") == 0){
			if(filetype_specified == TRUE && filetype != MH_FVMLIB)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_FVMLIB;
		    }
		    else if(strcmp(p, "force_cpusubtype_ALL") == 0){
			force_cpusubtype_ALL = TRUE;
		    }
		    else if(strcmp(p, "framework") == 0){
			if(i + 1 >= argc)
			    fatal("-framework: argument missing");
			/* path searched abbrevated framework name, processed
			   in the next pass of parsing arguments */
			i += 1;
		    }
		    else if(strcmp(p, "filelist") == 0){
			if(i + 1 >= argc)
			    fatal("-filelist: argument missing");
			/* filelist of object names, processed
			   in the next pass of parsing arguments */
			i += 1;
		    }
		    else if(strcmp(p, "flat_namespace") == 0){
			if(namespace_specified == TRUE &&
			   twolevel_namespace == TRUE)
			    fatal("can't specify both -flat_namespace and "
				  "-twolevel_namespace");
			namespace_specified = TRUE;
			twolevel_namespace = FALSE;
		    }
		    else if(strcmp(p, "force_flat_namespace") == 0){
			if(namespace_specified == TRUE &&
			   twolevel_namespace == TRUE)
			    fatal("can't specify both -force_flat_namespace "
				  "and -twolevel_namespace");
			force_flat_namespace = TRUE;
		        twolevel_namespace = FALSE;
		    }
		    else if(strcmp(p, "final_output") == 0){
			if(i + 1 >= argc)
			    fatal("-final_output: argument missing");
			if(final_output != NULL)
			    fatal("-final_output multiply specified");
			final_output = argv[i+1];
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'F':
		    if(p[1] == '\0')
			fatal("-F: directory name missing");
		    /* add a pathname to the list of framework search paths */
		    framework_dirs = reallocate(framework_dirs,
				       (nframework_dirs + 1) * sizeof(char *));
		    framework_dirs[nframework_dirs++] = &(p[1]);
		    if(stat(&(p[1]), &stat_buf) == -1)
			warning("-F: directory name (%s) does not exist",
				&(p[1]));
		    break;

		case 'r':
		    if(strcmp(p, "read_only_relocs") == 0){
			if(++i >= argc)
			    fatal("-read_only_relocs: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_read_only_reloc_flag = READ_ONLY_RELOC_SUPPRESS;
			else{
			    fatal("-read_only_relocs: unknown argument: %s",
				  argv[i]);
			    new_read_only_reloc_flag = READ_ONLY_RELOC_ERROR;
			}
			if(read_only_reloc_flag_specified == TRUE &&
			   new_read_only_reloc_flag != read_only_reloc_flag)
			    fatal("more than one value specified for "
				  "-read_only_relocs");
			read_only_reloc_flag_specified = TRUE;
			read_only_reloc_flag = new_read_only_reloc_flag;
			break;
		    }
		    else if(strcmp(p, "run_init_lazily") == 0){
			warning("-run_init_lazily is obsolete");
			break;
		    }
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* save relocation information, and produce a relocatable
		       object */
		    save_reloc = TRUE;
		    if(filetype_specified == FALSE)
			filetype = MH_OBJECT;
		    if(dflag_specified == FALSE)
			define_comldsyms = FALSE;
		    break;
		case 'A':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(++i >= argc)
			fatal("-A: argument missing");
		    /* object file argv[i] processed in the next pass of
		       parsing arguments */
		    Aflag_specified = TRUE;
		    break;
		case 'd':
		    if(strcmp(p, "d") == 0){
			/* define common symbols and loader defined symbols
			   reguardless of file format */
			dflag_specified = TRUE;
			define_comldsyms = TRUE;
		    }
		    else if(strcmp(p, "dynamic") == 0){
			if(static_specified)
			    fatal("only one of -dynamic or -static can be "
				  "specified");

			dynamic = TRUE;
			dynamic_specified = TRUE;
		    }
		    else if(strcmp(p, "dylib") == 0){
			if(filetype_specified == TRUE && filetype != MH_DYLIB)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_DYLIB;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "dylib_install_name") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_install_name: argument missing");
			dylib_install_name = argv[i + 1];
			i += 1;
		    }
  		    else if(strcmp(p, "dylib_current_version") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_current_version: argument missing");
			if(get_version_number("-dylib_current_version",
			    argv[i+1], &dylib_current_version) == FALSE)
			    cleanup();
			if(dylib_current_version == 0)
			    fatal("-dylib_current_version must be greater than "
				  "zero");
			i += 1;
		    }
		    else if(strcmp(p, "dylib_compatibility_version") == 0){
			if(i + 1 >= argc)
			    fatal("-dylib_compatibility_version: argument "
				  "missing");
			if(get_version_number("-dylib_compatibility_version",
			    argv[i+1], &dylib_compatibility_version) == FALSE)
			    cleanup();
			if(dylib_compatibility_version == 0)
			    fatal("-dylib_compatibility_version must be "
				  "greater than zero");
			i += 1;
		    }
		    else if(strcmp(p, "dylib_file") == 0){
			if(++i >= argc)
			    fatal("-dylib_file: argument missing");
			file_name = strchr(argv[i], ':');
			if(file_name == NULL ||
			   file_name[1] == '\0' || argv[i][0] == ':')
			    fatal("-dylib_file argument: %s must have a ':' "
				  "between its file names", argv[i]);
			dylib_files = reallocate(dylib_files,
					(ndylib_files + 1) * sizeof(char *));
			dylib_files[ndylib_files++] = argv[i];
		    }
		    else if(strcmp(p, "dylinker") == 0){
			if(filetype_specified == TRUE &&
			   filetype != MH_DYLINKER)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_DYLINKER;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "dylinker_install_name") == 0){
			if(i + 1 >= argc)
			    fatal("-dylinker_install_name: argument missing");
			if(dylinker_install_name != NULL)
			    fatal("-dylinker_install_name multiply specified");
			dylinker_install_name = argv[i + 1];
			i += 1;
		    }
		    else if(strcmp(p, "dead_strip") == 0){
			dead_strip = TRUE;
		    }
		    else if(strcmp(p, "dead_strip_times") == 0){
			dead_strip_times = TRUE;
		    }
#ifdef DEBUG
		    else if(strcmp(p, "debug") == 0){
			if(++i >= argc)
			    fatal("-debug: argument missing");
			debug |= 1 << strtoul(argv[i], &endp, 10);
			if(*endp != '\0' || strtoul(argv[i], &endp, 10) > 32)
			    fatal("argument for -debug %s not a proper "
				  "decimal number less than 32", argv[i]);
		    }
#endif /* DEBUG */
		    else
			goto unknown_flag;
		    break;

		case 'n':
		    if(strcmp(p, "noflush") == 0){
			flush = FALSE;
		    }
		    else if(strcmp(p, "nofixprebinding") == 0){
			no_fix_prebinding = TRUE;
		    }
		    else if(strcmp(p, "no_arch_warnings") == 0){
			no_arch_warnings = TRUE;
		    }
		    else if(strcmp(p, "noseglinkedit") == 0){
			if(seglinkedit_specified && seglinkedit == TRUE)
			    fatal("both -seglinkedit and -noseglinkedit can't "
				  "be specified");
			seglinkedit = FALSE;
			seglinkedit_specified = TRUE;
		    }
		    else if(strcmp(p, "noprebind") == 0){
			if(prebinding_flag_specified == TRUE &&
			   prebinding == TRUE)
			    fatal("both -prebind and -noprebind can't "
				  "be specified");
			prebinding_flag_specified = TRUE;
			prebinding = FALSE;
		    }
		    else if(strcmp(p, "nomultidefs") == 0){
			nomultidefs = TRUE;
		    }
		    else if(strcmp(p, "noprebind_all_twolevel_modules") == 0){
			if(prebind_all_twolevel_modules_specified == TRUE &&
			   prebind_all_twolevel_modules == TRUE)
			    fatal("both -prebind_all_twolevel_modules and "
				  "-noprebind_all_twolevel_modules can't be "
				  "specified");
			prebind_all_twolevel_modules = FALSE;
			prebind_all_twolevel_modules_specified = TRUE;
		    }
		    else if(strcmp(p, "no_dead_strip_inits_and_terms") == 0){
			no_dead_strip_inits_and_terms = TRUE;
		    }
		    else if(strcmp(p, "no_uuid") == 0){
			 output_uuid_info.suppress = TRUE;
		    }
		    else if(strcmp(p, "noall_load") == 0){
		      /* Ignore the flag.  */
		      ;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'b':
		    if(strcmp(p, "bundle") == 0){
			if(filetype_specified == TRUE && filetype != MH_BUNDLE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_BUNDLE;
			output_for_dyld = TRUE;
		    }
		    else if(strcmp(p, "bind_at_load") == 0){
			bind_at_load = TRUE;
		    }
		    else if(strcmp(p, "bundle_loader") == 0){
			if(i + 1 >= argc)
			    fatal("-bundle_loader: argument missing");
			if(bundle_loader != NULL)
			    fatal("-bundle_loader multiply specified");
			bundle_loader = argv[i + 1];
			i += 1;
		    }
		    /* Strip the base file symbols (the -A argument's symbols)*/
		    else if(p[1] == '\0')
			strip_base_symbols = TRUE;
		    else
			goto unknown_flag;
		    break;

		/*
		 * Stripping level flags, in increasing level of stripping.  The
		 * level of stripping is set to the maximum level specified.
		 */
		case 'X':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(strip_level < STRIP_L_SYMBOLS)
			strip_level = STRIP_L_SYMBOLS;
		    break;
		case 'S':
		    if(strcmp(p, "Sn") == 0){
			strip_level = STRIP_NONE;
		    }
		    else if(strcmp(p, "Si") == 0){
			if(strip_level < STRIP_DUP_INCLS)
			    strip_level = STRIP_DUP_INCLS;
		    }
		    else if(strcmp(p, "Sp") == 0){
			if(strip_level < STRIP_MIN_DEBUG)
			    strip_level = STRIP_MIN_DEBUG;
		    }
		    else if(p[1] == '\0'){
			if(strip_level < STRIP_DEBUG)
			    strip_level = STRIP_DEBUG;
		    }
		    else{
			goto unknown_flag;
		    }
		    break;
		case 'x':
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(strip_level < STRIP_NONGLOBALS)
			strip_level = STRIP_NONGLOBALS;
		    break;
		case 's':
		    if(strcmp(p, "s") == 0){
			strip_level = STRIP_ALL;
		    }
		    else if(strcmp(p, "static") == 0){
			if(dynamic_specified)
			    fatal("only one of -static or -dynamic can be "
				  "specified");
			dynamic = FALSE;
			static_specified = TRUE;
		        twolevel_namespace = FALSE;
		    }
		    else if(strcmp(p, "search_paths_first") == 0){
			search_paths_first = TRUE;
		    }
		    /*
		     * Flags for specifing information about sections.
		     */
		    /* create a section from the contents of a file
		       -sectcreate <segname> <sectname> <filename> */
		    else if(strcmp(p, "sectcreate") == 0 ||
		    	    strcmp(p, "segcreate") == 0){ /* the old name */
			if(i + 3 >= argc)
			    fatal("%s: arguments missing", argv[i]);
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->contents_filename != NULL)
			     fatal("section (%s,%s) multiply specified with a "
				   "%s option", argv[i+1], argv[i+2], argv[i]);
			if((fd = open(argv[i+3], O_RDONLY, 0)) == -1)
			    system_fatal("Can't open: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("Can't stat file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(sect_spec->file_addr),
				(boolean_t)TRUE, (vm_size_t)stat_buf.st_size)
				) != KERN_SUCCESS)
				mach_fatal(r, "can't map file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			}
			else{
			    sect_spec->file_addr = NULL;
			}
			close(fd);
			sect_spec->file_size = stat_buf.st_size;
			sect_spec->contents_filename = argv[i+3];
			i += 3;
		    }
		    /* specify the alignment of a section as a hexadecimal
		       power of 2
		       -sectalign <segname> <sectname> <number> */
		    else if(strcmp(p, "sectalign") == 0){
			if(i + 3 >= argc)
			    fatal("-sectalign arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->align_specified)
			     fatal("alignment for section (%s,%s) multiply "
				   "specified", argv[i+1], argv[i+2]);
			sect_spec->align_specified = TRUE;
			align = strtoul(argv[i+3], &endp, 16);
			if(*endp != '\0')
			    fatal("argument for -sectalign %s %s: %s not a "
				  "proper hexadecimal number", argv[i+1],
				  argv[i+2], argv[i+3]);
			if(!ispoweroftwo(align))
			    fatal("argument to -sectalign %s %s: %lx (hex) must"
				  " be a power of two", argv[i+1], argv[i+2],
				  align);
			if(align != 0)
			    for(tmp = align; (tmp & 1) == 0; tmp >>= 1)
				sect_spec->align++;
			if(sect_spec->align > MAXSECTALIGN)
			    fatal("argument to -sectalign %s %s: %lx (hex) must"
				  " equal to or less than %x (hex)", argv[i+1],
				  argv[i+2], align,
				  (unsigned int)(1 << MAXSECTALIGN));
			i += 3;
		    }
		    /* specify that section object symbols are to be created
		       for the specified section
		       -sectobjectsymbols <segname> <sectname> */
		    else if(strcmp(p, "sectobjectsymbols") == 0){
			if(i + 2 >= argc)
			    fatal("-sectobjectsymbols arguments missing");
			if(sect_object_symbols.specified &&
			   (strcmp(sect_object_symbols.segname,
				   argv[i+1]) != 0 ||
			    strcmp(sect_object_symbols.sectname,
				   argv[i+2]) != 0) )
			     fatal("-sectobjectsymbols multiply specified (it "
				   "can only be specified for one section)");
			sect_object_symbols.specified = TRUE;
			sect_object_symbols.segname = argv[i+1];
			sect_object_symbols.sectname = argv[i+2];
			i += 2;
		    }
		    /* layout a section in the order the symbols appear in file
		       -sectorder <segname> <sectname> <filename> */
		    else if(strcmp(p, "sectorder") == 0){
			if(i + 3 >= argc)
			    fatal("%s: arguments missing", argv[i]);
			seg_spec = create_segment_spec(argv[i+1]);
			sect_spec = create_section_spec(seg_spec, argv[i+2]);
			if(sect_spec->order_filename != NULL)
			     fatal("section (%s,%s) multiply specified with a "
				   "%s option", argv[i+1], argv[i+2], argv[i]);
			if((fd = open(argv[i+3], O_RDONLY, 0)) == -1)
			    system_fatal("Can't open: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("Can't stat file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(sect_spec->order_addr),
				(boolean_t)TRUE, (vm_size_t)stat_buf.st_size)
				) != KERN_SUCCESS)
				mach_fatal(r, "can't map file: %s for %s %s %s",
				    argv[i+3], argv[i], argv[i+1], argv[i+2]);
			}
			else{
			    sect_spec->order_addr = NULL;
			}
			close(fd);
			sect_spec->order_size = stat_buf.st_size;
			sect_spec->order_filename = argv[i+3];
			i += 3;
		    }
		    else if(strcmp(p, "sectorder_detail") == 0){
			sectorder_detail = TRUE;
		    }
		    else if(strcmp(p, "sect_diff_relocs") == 0){
			if(++i >= argc)
			    fatal("-sect_diff_relocs: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_sect_diff_reloc_flag = SECT_DIFF_RELOC_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_sect_diff_reloc_flag = SECT_DIFF_RELOC_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_sect_diff_reloc_flag = SECT_DIFF_RELOC_SUPPRESS;
			else{
			    fatal("-sect_diff_relocs: unknown argument: %s",
				  argv[i]);
			    new_sect_diff_reloc_flag = SECT_DIFF_RELOC_SUPPRESS;
			}
			if(sect_diff_reloc_flag_specified == TRUE &&
			   new_sect_diff_reloc_flag != sect_diff_reloc_flag)
			    fatal("more than one value specified for "
				  "-sect_diff_relocs");
			sect_diff_reloc_flag_specified = TRUE;
			sect_diff_reloc_flag = new_sect_diff_reloc_flag;
			break;
		    }
		    /*
		     * Flags for specifing information about segments.
		     */
		    /* specify the address (in hex) of a segment
		       -segaddr <segname> <address> */
		    else if(strcmp(p, "segaddr") == 0){
			if(i + 2 >= argc)
			    fatal("-segaddr: arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			if(seg_spec->addr_specified == TRUE)
			    fatal("address of segment %s multiply specified",
				  argv[i+1]);
			segaddr_specified = TRUE;
			seg_spec->addr_specified = TRUE;
			seg_spec->addr = strtoul(argv[i+2], &endp, 16);
			if(*endp != '\0')
			    fatal("address for -segaddr %s %s not a proper "
				  "hexadecimal number", argv[i+1], argv[i+2]);
			i += 2;
		    }
		    /* specify the protection for a segment
		       -segprot <segname> <maxprot> <initprot>
		       where the protections are specified with "rwx" with a
		       "-" for no protection. */
		    else if(strcmp(p, "segprot") == 0){
			if(i + 3 >= argc)
			    fatal("-segprot: arguments missing");
			seg_spec = create_segment_spec(argv[i+1]);
			if(seg_spec->prot_specified == TRUE)
			    fatal("protection of segment %s multiply "
				  "specified", argv[i]);
			seg_spec->maxprot = getprot(argv[i+2], &endp);
			if(*endp != '\0')
			    fatal("bad character: '%c' in maximum protection: "
				  "%s for segment %s", *endp, argv[i+2],
				  argv[i+1]);
			seg_spec->initprot = getprot(argv[i+3], &endp);
			if(*endp != '\0')
			    fatal("bad character: '%c' in initial protection: "
				  "%s for segment %s", *endp, argv[i+3],
				  argv[i+1]);
			if(check_max_init_prot(seg_spec->maxprot,
					       seg_spec->initprot) == FALSE)
			    fatal("maximum protection: %s for segment: %s "
				  "doesn't include all initial protections: %s",
				  argv[i+2], argv[i+1], argv[i+3]);
			seg_spec->prot_specified = TRUE;
			i += 3;
		    }
		    /* specify the address (in hex) of the first segment
		       -seg1addr <address> */
		    else if(strcmp(p, "seg1addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg1addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			seg1addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			seg1addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the address (in hex) of the read-only segments
		       -segs_read_only_addr <address> */
		    else if(strcmp(p, "segs_read_only_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(segs_read_only_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			segs_read_only_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			segs_read_only_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the address (in hex) of the read-write segments
		       -segs_read_write_addr <address> */
		    else if(strcmp(p, "segs_read_write_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(segs_read_write_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			segs_read_write_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			segs_read_write_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the name of the segment address table */
		    else if(strcmp(p, "seg_addr_table") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg_addr_table_name != NULL)
			    fatal("%s: multiply specified", argv[i]);
			seg_addr_table_name = argv[i+1];
			i += 1;
		    }
		    /* specify the file system path name to be used instead of
		       the install name in the segment address table */
		    else if(strcmp(p, "seg_addr_table_filename") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg_addr_table_filename != NULL)
			    fatal("%s: multiply specified", argv[i]);
			seg_addr_table_filename = argv[i+1];
			i += 1;
		    }
		    /* specify the segment alignment as a hexadecimal power of 2
		       -segalign <number> */
		    else if(strcmp(p, "segalign") == 0){
			if(segalign_specified)
			    fatal("-segalign: multiply specified");
			if(++i >= argc)
			    fatal("-segalign: argument missing");
			segalign = strtoul(argv[i], &endp, 16);
			if(*endp != '\0')
			    fatal("argument for -segalign %s not a proper "
				  "hexadecimal number", argv[i]);
			if(!ispoweroftwo(segalign) || segalign == 0)
			    fatal("argument to -segalign: %lx (hex) must be a "
				  "non-zero power of two", segalign);
			if(segalign > MAXSEGALIGN)
			    fatal("argument to -segalign: %lx (hex) must equal "
				  "to or less than %x (hex)", segalign,
				  (unsigned int)MAXSEGALIGN);
			segalign_specified = TRUE;
			if(segalign < (1 << DEFAULTSECTALIGN)){
			    defaultsectalign = 0;
			    align = segalign;
			    while((align & 0x1) != 1){
				defaultsectalign++;
				align >>= 1;
			    }
			}
		    }
		    else if(strcmp(p, "seglinkedit") == 0){
			if(seglinkedit_specified && seglinkedit == FALSE)
			    fatal("both -seglinkedit and -noseglinkedit can't "
				  "be specified");
			seglinkedit = TRUE;
			seglinkedit_specified = TRUE;
		    }
		    /* specify the stack address as a hexadecimal number
		       -stack_addr <address> */
		    else if(strcmp(p, "stack_addr") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(stack_addr_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			stack_addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			stack_addr_specified = TRUE;
			i += 1;
		    }
		    /* specify the stack size as a hexadecimal number
		       -stack_size <address> */
		    else if(strcmp(p, "stack_size") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(stack_size_specified == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			stack_size = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			stack_size_specified = TRUE;
			i += 1;
		    }
		    /* specify a sub_umbrella
		       -sub_umbrella <name> */
		    else if(strcmp(p, "sub_umbrella") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			sub_umbrellas = reallocate(sub_umbrellas,
					(nsub_umbrellas + 1) * sizeof(char *));
			sub_umbrellas[nsub_umbrellas++] = argv[i+1];
			i += 1;
		    }
		    /* specify a sub_library
		       -sub_library <name> */
		    else if(strcmp(p, "sub_library") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			sub_librarys = reallocate(sub_librarys,
					(nsub_librarys + 1) * sizeof(char *));
			sub_librarys[nsub_librarys++] = argv[i+1];
			i += 1;
		    }
		    /* -single_module for MH_DYLIB output */
		    else if(strcmp(p, "single_module") == 0){
			if(moduletype_specified == TRUE &&
			   multi_module_dylib == TRUE)
			    fatal("can't specify both -single_module and "
				  "-multi_module");
			moduletype_specified = TRUE;
			multi_module_dylib = FALSE;
		    }
		    else if(strcmp(p, "syslibroot") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(syslibroot_specified == TRUE && strcmp(next_root, argv[i+1]) != 0)
			    fatal("%s: multiply specified", argv[i]);
			next_root = argv[i+1];
			syslibroot_specified = TRUE;
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 't':
		    /* trace flag */
		    if(strcmp(p, "twolevel_namespace") == 0){
			if(namespace_specified == TRUE &&
			   twolevel_namespace == FALSE)
			    fatal("can't specify both -twolevel_namespace and "
				  "-flat_namespace");
			namespace_specified = TRUE;
			twolevel_namespace = TRUE;
		    }
		    else if(strcmp(p, "twolevel_namespace_hints") == 0){
			if(namespace_specified == TRUE &&
			   twolevel_namespace == FALSE)
			    fatal("can't specify both -twolevel_namespace_hints"
				  " and -flat_namespace");
			twolevel_namespace_hints_specified = TRUE;
		    }
		    else if(p[1] == '\0')
			trace = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'o':
		    if(strcmp(p, "object") == 0){
			if(filetype_specified == TRUE && filetype != MH_OBJECT)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_OBJECT;
			break;
		    }
		    /* specify the output file name */
		    if(p[1] != '\0')
			goto unknown_flag;
		    if(outputfile != NULL)
			fatal("-o: multiply specified");
		    if(++i >= argc)
			fatal("-o: argument missing");
		    outputfile = argv[i];
		    break;

		case 'a':
		    if(strcmp(p, "all_load") == 0)
			archive_all = TRUE;
		    else if(strcmp(p, "arch_multiple") == 0)
			arch_multiple = TRUE;
		    else if(strcmp(p, "arch_errors_fatal") == 0)
			arch_errors_fatal = TRUE;
		    else if(strcmp(p, "allow_stack_execute") == 0)
			allow_stack_execute = TRUE;
		    else if(strcmp(p, "arch") == 0){
			if(++i >= argc)
			    fatal("-arch: argument missing");
			if(arch_flag.name != NULL &&
			   strcmp(arch_flag.name, argv[i]) != 0)
			    fatal("-arch: multiply specified");
			if(get_arch_from_flag(argv[i], &arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "-arch %s", argv[i]);
			    fatal("Usage: %s [options] file [...]", progname);
			}
			/* Default to -single_module on ARM. */
			if(arch_flag.cputype == CPU_TYPE_ARM){
			    multi_module_dylib = FALSE;
			}
			target_byte_sex = get_byte_sex_from_flag(&arch_flag);
		    }
		    /* specify an allowable client of this subframework
		       -allowable_client client_name */
		    else if(strcmp(p, "allowable_client") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			allowable_clients = reallocate(allowable_clients,
				    (nallowable_clients + 1) * sizeof(char *));
			allowable_clients[nallowable_clients++] = argv[i+1];
			i += 1;
			break;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'c':
		    /* specify this client's name which is using a subframework
		       -client_name client_name */
		    if(strcmp(p, "client_name") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(client_name != NULL)
			    fatal("%s: multiply specified", argv[i]);
			client_name = argv[i+1];
			i += 1;
			break;
		    }
		    else if(strcmp(p, "compatibility_version") == 0){
		        if(i + 1 >= argc)
			    fatal("-compatibility_version: argument "
				  "missing");
			if(get_version_number("-compatibility_version",
			    argv[i+1], &dylib_compatibility_version) == FALSE)
			    cleanup();
			if(dylib_compatibility_version == 0)
			    fatal("-compatibility_version must be "
				  "greater than zero");
			i += 1;
			break;
		    }
		    else if(strcmp(p, "current_version") == 0){
		        if(i + 1 >= argc)
			    fatal("-current_version: argument missing");
			if(get_version_number("-current_version",
			    argv[i+1], &dylib_current_version) == FALSE)
			    cleanup();
			if(dylib_current_version == 0)
			    fatal("-current_version must be greater than "
				  "zero");
			i += 1;
			break;
		    }
		    if(p[1] != '\0')
			goto unknown_flag;
		    break;


		/* Flags dealing with symbols */
		case 'i':
		    if(strcmp(p, "image_base") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(seg1addr_specified == TRUE)
			    fatal("%s: argument missing", argv[i]);
			seg1addr = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for %s %s not a proper "
				  "hexadecimal number", argv[i], argv[i+1]);
			seg1addr_specified = TRUE;
			i += 1;
		    }
		    else if(strcmp(p, "init") == 0){
			/* check to see if the pointer is not already set */
			if(init_name != NULL)
			    fatal("-init: multiply specified");
			if(++i >= argc)
			    fatal("-init: argument missing");
			init_name = argv[i];
		    }
		    else if(strcmp(p, "install_name") == 0){
		        if(i + 1 >= argc)
			    fatal("-install_name: argument missing");
			dylib_install_name = argv[i + 1];
			i += 1;
		    }
		    else{
			/* create an indirect symbol, symbol_name, to be an
			   indirect symbol for indr_symbol_name */
			symbol_name = p + 1;
			indr_symbol_name = strchr(p + 1, ':');
			if(indr_symbol_name == NULL ||
			   indr_symbol_name[1] == '\0' || *symbol_name == ':')
			    fatal("-i argument: %s must have a ':' between "
				  "its symbol names", p + 1);
			/* the creating of the symbol is done in the next pass
			   of parsing arguments */
		    }
		    break;

		case 'm':
		    if(strcmp(p, "multiply_defined") == 0){
			if(++i >= argc)
			    fatal("-multiply_defined: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_multiply_defined_flag = MULTIPLY_DEFINED_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_multiply_defined_flag =MULTIPLY_DEFINED_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_multiply_defined_flag=MULTIPLY_DEFINED_SUPPRESS;
			else{
			    fatal("-multiply_defined: unknown argument: %s",
				  argv[i]);
			    new_multiply_defined_flag =MULTIPLY_DEFINED_WARNING;
			}
			if(multiply_defined_flag_specified == TRUE &&
			   new_multiply_defined_flag != multiply_defined_flag)
			    fatal("more than one value specified for "
				  "-multiply_defined");
			multiply_defined_flag_specified = TRUE;
			multiply_defined_flag = new_multiply_defined_flag;
			break;
		    }
		    else if(strcmp(p, "multiply_defined_unused") == 0){
			if(++i >= argc)
			    fatal("-multiply_defined_unused: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_multiply_defined_unused_flag =
				MULTIPLY_DEFINED_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_multiply_defined_unused_flag =
				MULTIPLY_DEFINED_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_multiply_defined_unused_flag =
				MULTIPLY_DEFINED_SUPPRESS;
			else{
			    fatal("-multiply_defined_unused: unknown argument: "
				  "%s", argv[i]);
			    new_multiply_defined_unused_flag =
				MULTIPLY_DEFINED_SUPPRESS;
			}
			if(multiply_defined_unused_flag_specified == TRUE &&
			   new_multiply_defined_unused_flag !=
				multiply_defined_unused_flag)
			    fatal("more than one value specified for "
				  "-multiply_defined_unused");
			multiply_defined_unused_flag_specified = TRUE;
			multiply_defined_unused_flag =
			    new_multiply_defined_unused_flag;
			break;
		    }
		    /* -multi_module for MH_DYLIB output */
		    else if(strcmp(p, "multi_module") == 0){
			if(moduletype_specified == TRUE &&
			   multi_module_dylib == FALSE)
			    fatal("can't specify both -single_module and "
				  "-multi_module");
			moduletype_specified = TRUE;
			multi_module_dylib = TRUE;
			break;
		    }
		    /* -macosx_version_min for overriding
		       MACOSX_DEPLOYMENT_TARGET on command line */
		    else if(strcmp (p, "macosx_version_min") == 0){
			if(++i >= argc)
			    fatal("-macosx_version_min: argument missing");
			put_macosx_deployment_target (argv[i]);
			break;
		    }
		    /* treat multiply defined symbols as a warning not a
		       hard error */
		    if(p[1] != '\0')
			goto unknown_flag;
		    allow_multiply_defined_symbols = TRUE;
		    break;

		case 'u':
		    if(strcmp(p, "undefined") == 0){
			if(++i >= argc)
			    fatal("-undefined: argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_undefined_flag = UNDEFINED_ERROR;
			else if(strcmp(argv[i], "warning") == 0)
			    new_undefined_flag = UNDEFINED_WARNING;
			else if(strcmp(argv[i], "suppress") == 0)
			    new_undefined_flag = UNDEFINED_SUPPRESS;
			else if(strcmp(argv[i], "dynamic_lookup") == 0)
			    new_undefined_flag = UNDEFINED_DYNAMIC_LOOKUP;
			else if(strcmp(argv[i], "define_a_way") == 0){
			    new_undefined_flag = UNDEFINED_DEFINE_A_WAY;
			    warning("suggest the use of -dead_strip instead of "
				    "-undefined define_a_way");
			}
			else{
			    fatal("-undefined: unknown argument: %s", argv[i]);
			    new_undefined_flag = UNDEFINED_ERROR;
			}
			if(undefined_flag_specified == TRUE &&
			   new_undefined_flag != undefined_flag)
			    fatal("more than one value specified for "
				  "-undefined");
			undefined_flag_specified = TRUE;
			undefined_flag = new_undefined_flag;
			break;
		    }
		    /* specify this dynamic library as a subframework
		       -umbrella umbrella_framework_name */
		    else if(strcmp(p, "umbrella") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(sub_framework == TRUE)
			    fatal("%s: multiply specified", argv[i]);
			umbrella_framework_name = argv[i+1];
		        sub_framework = TRUE;
			i += 1;
			break;
		    }
		    else if(strcmp(p, "unexported_symbols_list") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(remove_symbols != NULL)
			    fatal("%s: multiply specified", argv[i]);
			setup_symbol_list(argv[i+1], &remove_symbols,
					  &nremove_symbols);
			unexported_symbols_list = argv[i+1];
			i += 1;
			break;
		    }
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* cause the specified symbol name to be undefined */
		    if(++i >= argc)
			fatal("-u: argument missing");
		    /* the creating of the symbol is done in the next pass of
		       parsing arguments */
		    break;

		case 'e':
		    if(strcmp(p, "execute") == 0){
			if(filetype_specified == TRUE && filetype != MH_EXECUTE)
			    fatal("more than one output filetype specified");
			filetype_specified = TRUE;
			filetype = MH_EXECUTE;
			break;
		    }
		    else if(strcmp(p, "exported_symbols_list") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(save_symbols != NULL)
			    fatal("%s: multiply specified", argv[i]);
			setup_symbol_list(argv[i+1], &save_symbols,
					  &nsave_symbols);
			exported_symbols_list = argv[i+1];
			i += 1;
			break;
		    }
		    else if(strcmp(p, "executable_path") == 0){
			if(i + 1 >= argc)
			    fatal("%s: argument missing", argv[i]);
			if(executable_path != NULL)
			    fatal("%s: multiply specified", argv[i]);
			executable_path = argv[i+1];
			i += 1;
			break;
		    }
		    /* specify the entry point, the symbol who's value to be
		       used as the program counter in the unix thread */
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* check to see if the pointer is not already set */
		    if(entry_point_name != NULL)
			fatal("-e: multiply specified");
		    if(++i >= argc)
			fatal("-e: argument missing");
		    entry_point_name = argv[i];
		    break;

		case 'U':
		    if(p[1] != '\0')
			goto unknown_flag;
		    /* allow the specified symbol name to be undefined */
		    if(++i >= argc)
			fatal("-U: argument missing");
		    undef_syms = reallocate(undef_syms,
					    (nundef_syms + 1) * sizeof(char *));
		    undef_syms[nundef_syms++] = argv[i];
		    break;

		case 'w':
		    if(strcmp(p, "w") == 0)
			nowarnings = TRUE;
		    else if(strcmp(p, "whyload") == 0)
			whyload = TRUE;
		    else if(strcmp(p, "whatsloaded") == 0)
			whatsloaded = TRUE;
		    else if(strcmp(p, "weak_reference_mismatches") == 0){
			if(++i >= argc)
			    fatal("-weak_reference_mismatches: "
				  "argument missing");
			if(strcmp(argv[i], "error") == 0)
			    new_weak_reference_mismatches =
				WEAK_REFS_MISMATCH_ERROR;
			else if(strcmp(argv[i], "weak") == 0)
			    new_weak_reference_mismatches =
				WEAK_REFS_MISMATCH_WEAK;
			else if(strcmp(argv[i], "non-weak") == 0)
			    new_weak_reference_mismatches =
				WEAK_REFS_MISMATCH_NON_WEAK;
			else{
			    fatal("-weak_reference_mismatches: unknown "
				  "argument: %s", argv[i]);
			    new_weak_reference_mismatches =
				WEAK_REFS_MISMATCH_ERROR;
			}
			if(weak_reference_mismatches_specified == TRUE &&
			   new_weak_reference_mismatches !=
				weak_reference_mismatches)
			    fatal("more than one value specified for "
				  "-weak_reference_mismatches");
			weak_reference_mismatches_specified = TRUE;
			weak_reference_mismatches =
			    new_weak_reference_mismatches;
			break;
		    }
		    else if(strcmp(p, "weak_library") == 0){
			if(i + 1 >= argc)
			    fatal("-weak_library: argument missing");
			/* object file argv[i] processed in the next pass of
			   parsing arguments */
			i += 1;
		    }
		    else if(strncmp(p, "weak-l", sizeof("weak-l") - 1) == 0){
			if(p[sizeof("weak-l") - 1] == '\0')
			    fatal("-weak-l: argument missing");
			/* path searched abbrevated file name, processed in the
			   next pass of parsing arguments */
		    }
		    else if(strcmp(p, "weak_framework") == 0){
			if(i + 1 >= argc)
			    fatal("-weak_framework: argument missing");
			/* path searched abbrevated framework name, processed
			   in the next pass of parsing arguments */
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'O':
		    if(strcmp(p, "ObjC") == 0)
			archive_ObjC = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'y':
		    /* symbol tracing */
		    if(p[1] == '\0')
			fatal("-y: symbol name missing");
		    trace_syms = reallocate(trace_syms,
					    (ntrace_syms + 1) * sizeof(char *));
		    trace_syms[ntrace_syms++] = &(p[1]);
		    break;

		case 'Y':
		    /* undefined reference symbol tracing */
		    if(strcmp(p, "Y") == 0){
			if(i + 1 >= argc)
			    fatal("-Y: argument missing");
			Yflag = strtoul(argv[i+1], &endp, 10);
			if(*endp != '\0')
			    fatal("reference count for -Y %s not a proper "
				  "decimal number", argv[i+1]);
		    }
		    else
			goto unknown_flag;
		    break;

		case 'h':
		    /* specify the header pad (in hex)
		       -headerpad <value> */
		    if(strcmp(p, "headerpad") == 0){
			if(i + 1 >= argc)
			    fatal("-headerpad: argument missing");
			if(headerpad_specified == TRUE)
			    fatal("-headerpad: multiply specified");
			headerpad = strtoul(argv[i+1], &endp, 16);
			if(*endp != '\0')
			    fatal("address for -headerpad %s not a proper "
				  "hexadecimal number", argv[i+1]);
			headerpad_specified = TRUE;
			i += 1;
		    }
		    else if(strcmp(p, "headerpad_max_install_names") == 0){
			headerpad_max_install_names = TRUE;
		    }
		    else if(strcmp(p, "hash_instrument") == 0){
			hash_instrument_specified = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'k':
		    if(strcmp(p, "keep_private_externs") == 0)
			keep_private_externs = TRUE;
		    else if(strcmp(p, "k") == 0)
			dynamic = TRUE;
		    else
			goto unknown_flag;
		    break;

		case 'N':
		    if(strcmp(p, "NEXTSTEP-deployment-target") == 0){
			if(i + 1 >= argc)
			    fatal("-NEXTSTEP-deployment-target: argument "
				  "missing");
			if(dynamic_specified == TRUE ||
			   static_specified == TRUE)
			    fatal("-NEXTSTEP-deployment-target, -dynamic or "
				  "-static : multiply specified");
			if(strcmp(argv[i+1], "3.3") == 0){
			    if(static_specified)
				fatal("only one of -NEXTSTEP-deployment-target "
				      "3.3 or -static can be specified");
			    dynamic = TRUE;
			    dynamic_specified = TRUE;
			}
			else if(strcmp(argv[i+1], "3.2") == 0){
			    if(dynamic_specified)
				fatal("only one of -NEXTSTEP-deployment-target "
				      "3.2 or -dynamic can be specified");
			    dynamic = FALSE;
			    static_specified = TRUE;
			}
			else
			    fatal("unknown deployment release flag: "
				"-NEXTSTEP-deployment-target %s", argv[i+1]);
			i += 1;
		    }
		    else
			goto unknown_flag;
		    break;

		case 'v':
		    if(strcmp(p, "v") == 0){
			vflag = TRUE;
			printf("Apple Inc. version %s\n", apple_version);
		    }
		    else
			goto unknown_flag;
		    break;

		default:
unknown_flag:
		    fatal("unknown flag: %s", argv[i]);
		}
	    }
	}

	/*
	 * -sub_umbrella and -sub_library are not supported on ARM.
	 * See <rdar://problem/4771657>.
	 */
	if(arch_flag.cputype == CPU_TYPE_ARM){
	    if(sub_umbrellas != NULL){
	        fatal("-sub_umbrella is not supported on ARM");
	    }
	    if(sub_librarys != NULL){
	        fatal("-sub_library is not supported on ARM");
	    }
	}

	/*
	 * If either -syslibroot or the environment variable NEXT_ROOT is set
	 * prepend it to the standard paths for library searches.  This was
	 * added to ease cross build environments.
	 */
	p = getenv("NEXT_ROOT");
	if(syslibroot_specified == TRUE){
	    if(p != NULL && strcmp(p, next_root) != 0)
		warning("NEXT_ROOT environment variable ignored because "
			"-syslibroot specified");
	}
	else{
	    next_root = p;
	}
	if(next_root != NULL){
	    for(i = 0; standard_dirs[i] != NULL; i++){
		p = allocate(strlen(next_root) +
			     strlen(standard_dirs[i]) + 1);
		strcpy(p, next_root);
		strcat(p, standard_dirs[i]);
		standard_dirs[i] = p;
	    }
	    for(i = 0; standard_framework_dirs[i] != NULL; i++){
		p = allocate(strlen(next_root) +
			     strlen(standard_framework_dirs[i]) + 1);
		strcpy(p, next_root);
		strcat(p, standard_framework_dirs[i]);
		standard_framework_dirs[i] = p;
	    }
	}
 	/*
	 * If -syslibroot is specified, prepend it to the user-specified
	 * paths *if* the prepended version exists.
	 */
	if(syslibroot_specified == TRUE){
	    for(i = 0; i < nsearch_dirs; i++){
		if(search_dirs[i][0] == '/'){
		    p = mkstr(next_root, search_dirs[i], NULL);
		    if(stat(p, &stat_buf) == 0)
			search_dirs[i] = p;
		    else
			free(p);
		}
	    }
	    for(i = 0; i < nframework_dirs; i++){
		if(framework_dirs[i][0] == '/'){
		    p = mkstr(next_root, framework_dirs[i], NULL);
		    if(stat(p, &stat_buf) == 0)
			framework_dirs[i] = p;
		    else
			free(p);
		}
	    }
	}

	/*
         * Test to see if the various RC_* or XBS_* environment variables
	 * are set.
         */
        if((getenv("LD_TRACE_ARCHIVES") != NULL) ||
	   getenv("RC_TRACE_ARCHIVES") != NULL)
	  ld_trace_archives = TRUE;
        if((getenv("LD_TRACE_DYLIBS") != NULL) ||
	   (getenv("RC_TRACE_DYLIBS") != NULL))
	  ld_trace_dylibs = TRUE;
        if((getenv("LD_TRACE_PREBINDING_DISABLED") != NULL) ||
	   getenv("RC_TRACE_PREBINDING_DISABLED") != NULL)
	  ld_trace_prebinding_disabled = TRUE;
	if(ld_trace_archives || ld_trace_dylibs)
	  trace_file_path = getenv("LD_TRACE_FILE");
        if(getenv("LD_TRACE_BUNDLE_LOADER") != NULL &&
	   bundle_loader != NULL)
	    print("[Logging for XBS] Referenced bundle loader: %s\n",
		  bundle_loader);

	if(save_reloc == FALSE){
	    if(getenv("LD_DEAD_STRIP") != NULL)
		dead_strip = TRUE;
	    if(getenv("LD_NO_DEAD_STRIP_INITS_AND_TERMS") != NULL)
		no_dead_strip_inits_and_terms = TRUE;
	}
	if(getenv("LD_DEAD_STRIP_DYLIB") != NULL && filetype == MH_DYLIB)
	    dead_strip = TRUE;

	prebinding_via_LD_PREBIND = FALSE;
	/*
	 * The LD_FORCE_NO_PREBIND environment variable overrides the command
	 * line and the LD_PREBIND environment variable.
	 */
	if(getenv("LD_FORCE_NO_PREBIND") != NULL){
	    if(prebinding_flag_specified == TRUE &&
	       prebinding == TRUE){
		warning("-prebind ignored because LD_FORCE_NO_PREBIND "
			"environment variable specified");
		prebinding_flag_specified = TRUE;
		prebinding = FALSE;
	    }
	}
	/*
	 * The -prebind flag can also be specified with the LD_PREBIND
	 * environment variable.  We quitely ignore this when -r is on or
	 * if this is a fixed shared library output.
	 */
	else if(getenv("LD_PREBIND") != NULL &&
	   save_reloc == FALSE &&
	   filetype != MH_FVMLIB){
	    if(prebinding_flag_specified == TRUE &&
	       prebinding == FALSE){
		warning("LD_PREBIND environment variable ignored because "
			"-noprebind specified");
	    }
	    else{
		if(prebinding_flag_specified == FALSE)
		    prebinding_via_LD_PREBIND = TRUE;
		prebinding_flag_specified = TRUE;
		prebinding = TRUE;
	    }
	}
	if(getenv("LD_PREBIND_ALLOW_OVERLAP") != NULL)
	    prebind_allow_overlap = TRUE;
	if(prebind_all_twolevel_modules_specified == FALSE &&
	   getenv("LD_PREBIND_ALL_TWOLEVEL_MODULES") != NULL)
	    prebind_all_twolevel_modules = TRUE;

	/*
	 * The -twolevel_namespace flag can also be specified with the
	 * LD_TWOLEVEL_NAMESPACE environment variable.  We quitely ignore this
	 * when -flat_namespace or -static is specified.
	 */
	if(getenv("LD_TWOLEVEL_NAMESPACE") != NULL &&
	   namespace_specified == FALSE &&
	   static_specified == FALSE){
		namespace_specified = TRUE;
		twolevel_namespace = TRUE;
	}

	/*
	 * See if LD_LIBRARY_PATH is set.  And if so parse out the colon
	 * separated set of paths.
	 */
	ld_library_path = getenv("LD_LIBRARY_PATH");
	if(ld_library_path != NULL){
	    nld_library_paths = 1;
	    for(i = 0; ld_library_path[i] != '\0'; i++){
		if(ld_library_path[i] == ':')
		     nld_library_paths++;
	    }
	    ld_library_paths = allocate(sizeof(char *) * nld_library_paths);
	    j = 0;
	    ld_library_paths[j] = ld_library_path;
	    j++;
	    for(i = 0; ld_library_path[i] != '\0'; i++){
		if(ld_library_path[i] == ':'){
		    ld_library_path[i] = '\0';
		    ld_library_paths[j] = ld_library_path + i + 1;
		    j++;
		}
	    }
	}

	/*
	 * If there was a -arch flag two things needed to be done in reguard to
	 * the handling of the cpusubtypes.
	 */
	if(arch_flag.name != NULL){

	    /*
	     * 64-bit architectures are an error.
	     */
	    if(arch_flag.cputype & CPU_ARCH_ABI64)
		fatal("does not support 64-bit architectures");

	    family_arch_flag = get_arch_family_from_cputype(arch_flag.cputype);
	    if(family_arch_flag == NULL)
		fatal("internal error: unknown cputype (%d) for -arch %s (this "
		      "program out of sync with get_arch_family_from_cputype())"
		      ,arch_flag.cputype, arch_flag.name);
	    /*
	     * Pick up the Mac OS X deployment target.
	     */
	    get_macosx_deployment_target(&macosx_deployment_target);
	    /*
	     * If for this cputype we are to always output the ALL cpusubtype
	     * then set force_cpusubtype_ALL.
	     */
	    if(force_cpusubtype_ALL_for_cputype(arch_flag.cputype) == TRUE)
		force_cpusubtype_ALL = TRUE;
	    /*
	     * First, if -force_cpusubtype_ALL is set and an -arch flag was
	     * specified set the cpusubtype to the _ALL type for that cputype
	     * since the specified flag may not have the _ALL type and the
	     * -force_cpusubtype_ALL has precedence over an -arch flags for a
	     * specific implementation of an architecture.
	     */
	    if(force_cpusubtype_ALL == TRUE){
		arch_flag.cpusubtype = family_arch_flag->cpusubtype;
	    }
	    else{
		/*
		 * Second, if no -force_cpusubtype_ALL is specified and an -arch
		 * flag for a specific implementation of an architecture was
		 * specified then the resulting cpusubtype will be for that
		 * specific implementation of that architecture and all
		 * cpusubtypes must combine with the cpusubtype for the -arch
		 * flag to the cpusubtype for the -arch flag else an error must
		 * be flaged.  This is done check_cur_obj() where cpusubtypes
		 * are combined.  What needs to be done here is to determine if
		 * the -arch flag is for a specific implementation of an
		 * architecture.
		 */
		if(arch_flag.cpusubtype != family_arch_flag->cpusubtype)
		    specific_arch_flag = TRUE;
	    }
	}
	else{
	    /*
	     * We need to pick up the Mac OS X deployment target even if the
	     * target architecture is not yet known so we can check to see if
	     * the flags specified are valid.
	     */
	    if(macosx_deployment_target.major == 0)
		get_macosx_deployment_target(&macosx_deployment_target);
	}

	/*
	 * If the -sect_diff_relocs is specified check to see it can be used
	 * else pick up the LD_SECT_DIFF_RELOC if that can be used.
	 */
	if(sect_diff_reloc_flag_specified == TRUE){
	    if(filetype != MH_EXECUTE || dynamic == FALSE)
		fatal("can't use -sect_diff_relocs unless both -execute and "
		      "-dynamic are in effect");
	}
	else{
	    /*
	     * The -sect_diff_relocs flag was not specified on the command
	     * line, so if both -execute and -dynamic are in effect see if
	     * LD_SECT_DIFF_RELOCS is specified as an environment variable and
	     * use that value.
	     */
	    if(filetype == MH_EXECUTE && dynamic == TRUE){
		p = getenv("LD_SECT_DIFF_RELOCS");
		if(p != NULL){
		    if(strcmp(p, "error") == 0)
			sect_diff_reloc_flag = SECT_DIFF_RELOC_ERROR;
		    else if(strcmp(p, "warning") == 0)
			sect_diff_reloc_flag = SECT_DIFF_RELOC_WARNING;
		    else if(strcmp(p, "suppress") == 0)
			sect_diff_reloc_flag = SECT_DIFF_RELOC_SUPPRESS;
		    else{
			fatal("Unknown LD_SECT_DIFF_RELOCS environment variable"
			      " %s value", p);
		    }
		}
	    }
	}

	/*
	 * Check for flag combinations that would result in a bad output file.
	 */
	if(save_reloc && strip_level == STRIP_ALL)
	    fatal("can't use -s with -r (resulting file would not be "
		  "relocatable)");
	if(save_reloc && strip_level == STRIP_MIN_DEBUG)
	    fatal("can't use -Sp with -r (only allowed for fully linked "
		  "images)");
	if(save_reloc && strip_base_symbols == TRUE)
	    fatal("can't use -b with -r (resulting file would not be "
		  "relocatable)");
	if(save_reloc && dead_strip == TRUE)
	    fatal("can't use -dead_strip with -r (only allowed for fully "
		  "linked images)");
	if(keep_private_externs == TRUE){
	    if(save_symbols != NULL)
		fatal("can't use both -keep_private_externs and "
		      "-exported_symbols_list");
	    if(remove_symbols != NULL)
		fatal("can't use both -keep_private_externs and "
		      "-unexported_symbols_list");
	}
	if(save_symbols != NULL && remove_symbols != NULL){
	    for(j = 0; j < nremove_symbols ; j++){
		sp = bsearch(remove_symbols[j].name,
			     save_symbols, nsave_symbols,
			     sizeof(struct symbol_list),
			     (int (*)(const void *, const void *))
				symbol_list_bsearch);
		if(sp != NULL){
		    error("symbol name: %s is listed in both "
			  "-exported_symbols_list and -unexported_symbols_list "
			  "(can't be both exported and unexported)",
			  remove_symbols[j].name);
		}
	    }
	    if(errors != 0)
		ld_exit(1);
	}
	if(filetype_specified == TRUE && filetype == MH_OBJECT){
	    if(dynamic == TRUE)
		fatal("incompatible to specifiy -object when -dynamic is used "
		      "(use -execute (the default) with -dynamic or -static "
		      "with -object)");
	}
	if(filetype == MH_DYLINKER){
	    if(dynamic == FALSE)
		fatal("incompatible flag -dylinker used (must specify "
		      "\"-dynamic\" to be used)");
	}
	if(filetype == MH_DYLIB){
	    if(dynamic == FALSE)
		fatal("incompatible flag -dylib used (must specify "
		      "\"-dynamic\" to be used)");
	    if(save_reloc)
		fatal("can't use -r and -dylib (file format produced with "
		      "-dylib is not a relocatable format)");
	    if(strip_level == STRIP_ALL)
		fatal("can't use -s with -dylib (file must contain at least "
		      "global symbols, for maximum stripping use -x)");
	    if(Aflag_specified)
		fatal("can't use -A and -dylib");
	    if(keep_private_externs == TRUE)
		fatal("can't use -keep_private_externs and -dylib");
	    if(segaddr_specified)
		fatal("can't use -segaddr options with -dylib (use seg1addr to "
		      "specify the starting address)");
	    if(seg1addr_specified && segs_read_only_addr_specified)
		fatal("can't use both the -seg1addr option and "
		      "-segs_read_only_addr option");
	    if(seg1addr_specified && segs_read_write_addr_specified)
		fatal("can't use both the -seg1addr option and "
		      "-segs_read_write_addr option");
	    if(seg1addr_specified && seg_addr_table_name != NULL)
		fatal("can't use both the -seg1addr option and "
		      "-seg_addr_table option");
	    if(seg_addr_table_name != NULL && segs_read_only_addr_specified)
		fatal("can't use both the -seg_addr_table option and "
		      "-segs_read_only_addr option");
	    if(seg_addr_table_name != NULL && segs_read_write_addr_specified)
		fatal("can't use both the -seg_addr_table option and "
		      "-segs_read_only_addr option");
	    if(seg_addr_table_name != NULL && dylib_install_name == NULL)
		fatal("must also specify -dylib_install_name when using "
		      "-seg_addr_table");
	    if(segs_read_only_addr_specified &&
	       read_only_reloc_flag != READ_ONLY_RELOC_ERROR)
		fatal("can't used -read_only_relocs %s with format produced "
		      "with the -segs_read_only_addr option\n",
		      read_only_reloc_flag == READ_ONLY_RELOC_WARNING ?
		      "warning" : "suppress");
	    if(segs_read_write_addr_specified &&
	       !segs_read_only_addr_specified)
		fatal("must also specify -segs_read_only_addr when using "
		      "-segs_read_write_addr");
	    if(seglinkedit_specified && seglinkedit == FALSE)
		fatal("can't use -noseglinkedit with -dylib (resulting file "
		      "must have a link edit segment to access symbols)");
	    if(bind_at_load == TRUE){
		warning("-bind_at_load is meaningless with -dylib");
		bind_at_load = FALSE;
	    }
	    /* use a segment address table if specified */
	    env_seg_addr_table_name = getenv("LD_SEG_ADDR_TABLE");
	    if(seg_addr_table_name != NULL ||
	       (env_seg_addr_table_name != NULL && dylib_install_name != NULL)){
		if(seg_addr_table_name != NULL &&
		   env_seg_addr_table_name != NULL &&
		   strcmp(seg_addr_table_name, env_seg_addr_table_name) != 0){
		    warning("-seg_addr_table %s ignored, LD_SEG_ADDR_TABLE "
			    "environment variable: %s used instead",
			    seg_addr_table_name, env_seg_addr_table_name);
		}
		if(env_seg_addr_table_name != NULL){
		    seg_addr_table_name = env_seg_addr_table_name;
		    seg_addr_table = parse_seg_addr_table(seg_addr_table_name,
			"LD_SEG_ADDR_TABLE", "environment variable",
			&table_size);
		}
		else
		    seg_addr_table = parse_seg_addr_table(seg_addr_table_name,
			"-seg_addr_table", seg_addr_table_name, &table_size);
		if(seg_addr_table_filename != NULL)
		    seg_addr_table_entry = search_seg_addr_table(seg_addr_table,
						     seg_addr_table_filename);
		else
		    seg_addr_table_entry = search_seg_addr_table(seg_addr_table,
						     dylib_install_name);
		if(seg_addr_table_entry != NULL){
		    if(seg_addr_table_entry->split == TRUE){
	       		if(read_only_reloc_flag != READ_ONLY_RELOC_ERROR){
			    warning("-read_only_relocs %s ignored, when using "
				    "with format produced with the "
				    "-segs_read_only_addr option (via the "
				    "segment address table: %s %s line %u)",
		      		    read_only_reloc_flag ==
					READ_ONLY_RELOC_WARNING ?
				    "warning" : "suppress",
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			    read_only_reloc_flag = READ_ONLY_RELOC_ERROR;
			}
			if(seg1addr_specified){
			    warning("-seg1addr 0x%x ignored, using "
				    "-segs_read_only_addr 0x%x and "
				    "-segs_read_write_addr 0x%x from segment "
				    "address table: %s %s line %u",
				    (unsigned int)seg1addr,
				    (unsigned int)seg_addr_table_entry->
					    segs_read_only_addr,
				    (unsigned int)seg_addr_table_entry->
					    segs_read_write_addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			if(segs_read_only_addr_specified &&
			   segs_read_only_addr !=
				    seg_addr_table_entry->segs_read_only_addr){
			    warning("-segs_read_only_addr 0x%x ignored, using "
				    "-segs_read_only_addr 0x%x from segment "
				    "address table: %s %s line %u",
				    (unsigned int)segs_read_only_addr,
				    (unsigned int)seg_addr_table_entry->
					    segs_read_only_addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			if(segs_read_write_addr_specified &&
			   segs_read_write_addr !=
				    seg_addr_table_entry->segs_read_write_addr){
			    warning("-segs_read_write_addr 0x%x ignored, using "
				    "-segs_read_write_addr 0x%x from segment "
				    "address table: %s %s line %u",
				    (unsigned int)segs_read_write_addr,
				    (unsigned int)seg_addr_table_entry->
					    segs_read_write_addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			seg1addr_specified = FALSE;
			seg1addr = 0;
			segs_read_only_addr_specified = TRUE;
			segs_read_only_addr =
				seg_addr_table_entry->segs_read_only_addr;
			segs_read_write_addr_specified = TRUE;
			segs_read_write_addr =
				seg_addr_table_entry->segs_read_write_addr;
			if(segs_read_only_addr == 0 &&
			   segs_read_write_addr == 0){
			    segs_read_write_addr = get_shared_region_size_from_flag(&arch_flag);
			    warning("-segs_read_write_addr 0x0 ignored from "
				    "segment address table: %s %s line %u "
				    "using -segs_read_write_addr 0x%x",
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line,
				    (unsigned int)segs_read_write_addr);
			}
		    }
		    else{
			if(seg1addr_specified &&
			   seg1addr != seg_addr_table_entry->seg1addr){
			    warning("-seg1addr 0x%x ignored, using "
				    "-seg1addr 0x%x from segment address "
				    "table: %s %s line %u",
				    (unsigned int)seg1addr,
				    (unsigned int)seg_addr_table_entry->
					    seg1addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			if(segs_read_only_addr_specified){
			    warning("-segs_read_only_addr 0x%x ignored, using "
				    "-seg1addr 0x%x from segment address "
				    "table: %s %s line %u",
				    (unsigned int)segs_read_only_addr,
				    (unsigned int)seg_addr_table_entry->
					    seg1addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			if(segs_read_write_addr_specified){
			    warning("-segs_read_write_addr 0x%x ignored, using "
				    "-seg1addr 0x%x from segment address "
				    "table: %s %s line %u",
				    (unsigned int)segs_read_write_addr,
				    (unsigned int)seg_addr_table_entry->
					    seg1addr,
				    env_seg_addr_table_name != NULL ?
				    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
				    seg_addr_table_name,
				    seg_addr_table_entry->line);
			}
			seg1addr_specified = TRUE;
			seg1addr = seg_addr_table_entry->seg1addr;
			segs_read_only_addr_specified = FALSE;
			segs_read_only_addr = 0;
			segs_read_write_addr_specified = FALSE;
			segs_read_write_addr = 0;
		    }
		}
		else{
		    warning("%s %s not found in segment address table %s %s",
			    seg_addr_table_filename != NULL ?
			    "-seg_addr_table_filename" : "-dylib_install_name",
			    seg_addr_table_filename != NULL ?
			    seg_addr_table_filename : dylib_install_name,
			    env_seg_addr_table_name != NULL ?
			    "LD_SEG_ADDR_TABLE" : "-seg_addr_table",
			    seg_addr_table_name);
		}
	    }
	    /*
	     * If this is not a subframework then if it has an install name
	     * then guess its implied umbrella framework name from the
	     * install name.  Then if its install name is a framework name use
	     * that as the umbrella framework name.  Otherwise it is not
	     * considered an umbrella framework.
	     */
	    if(sub_framework == FALSE && dylib_install_name != NULL){
		umbrella_framework_name = guess_short_name(dylib_install_name,
			&is_framework, &has_suffix);
		if(umbrella_framework_name != NULL && is_framework == TRUE)
		    umbrella_framework = TRUE;
		else
		    umbrella_framework_name = NULL;
	    }
	    if(nallowable_clients != 0 && sub_framework == FALSE)
		fatal("-allowable_client flags can only be used when -umbrella "
		      "is also specified");
	}
	else{
	    if(segs_read_only_addr_specified)
		fatal("-segs_read_only_addr can only be used when -dylib "
		      "is also specified");
	    if(segs_read_write_addr_specified)
		fatal("-segs_read_write_addr can only be used when -dylib "
		      "is also specified");
	    if(seg_addr_table_name != NULL)
		fatal("-seg_addr_table can only be used when -dylib "
		      "is also specified");
	    if(sub_framework == TRUE)
		fatal("-umbrella %s can only be used when -dylib "
		      "is also specified", umbrella_framework_name);
	    if(nsub_umbrellas != 0)
		fatal("-sub_umbrella flags can only be used when -dylib "
		      "is also specified");
	    if(nsub_librarys != 0)
		fatal("-sub_library flags can only be used when -dylib "
		      "is also specified");
	    if(nallowable_clients != 0)
		fatal("-allowable_client flags can only be used when -dylib "
		      "is also specified");
	    if(moduletype_specified == TRUE)
		fatal("-single_module or -multi_module flags can only be used "
		      "when -dylib is also specified");
	}

	/*
	 * For Mac OS X 10.4 and later, prebinding will be limited to split
	 * shared libraries. So if this is not a split library then turn off
	 * prebinding.
	 */
	if(macosx_deployment_target.major >= 4){
	    if(filetype != MH_DYLIB){
		/* 
		 * If this is arm* or xscale, we want to prebind executables
		 * too, not just dylibs and frameworks. 
		 */
		if (!((arch_flag.name != NULL) && 
		      ((strncmp(arch_flag.name, "arm", 3) == 0) ||
		       (strcmp(arch_flag.name, "xscale") == 0))))
		{
		    if(prebinding_via_LD_PREBIND == FALSE &&
		       prebinding_flag_specified == TRUE &&
		       prebinding == TRUE){
			warning("-prebind ignored because MACOSX_DEPLOYMENT_TARGET "
				"environment variable greater or equal to 10.4");
		    }
		    prebinding = FALSE;
		}
	    }
	    /*
	     * This is an MH_DYLIB.  First see if it is on the list of libraries
	     * not to be prebound.  Then see if was specified to be built as a
	     * split, if not check LD_SPLITSEGS_NEW_LIBRARIES to see if we are
	     * forcing it to be a split library.
	     */
	    else{
		/*
		 * If this library was not in the seg_addr_table see if it is
		 * on the list of libraries not to be prebound. And if so turn
		 * off prebinding.  Note this list is only ever used when
		 * macosx_deployment_target.major >= 4 .
		 */
		if(seg_addr_table_entry == NULL &&
		   unprebound_library(dylib_install_name,
				      seg_addr_table_filename) == TRUE){
		    if(prebinding_flag_specified == TRUE &&
		       prebinding == TRUE){
			warning("-prebind ignored because -install_name %s "
				"listed in LD_UNPREBOUND_LIBRARIES environment "
				"variable file: %s", dylib_install_name,
				getenv("LD_UNPREBOUND_LIBRARIES"));
		    }
		    prebinding = FALSE;
		}
		else{
		    /*
		     * This is not on the list of libraries not to be prebound,
		     * and if there was no seg_addr_table entry for this then
		     * force this to be a split library.  Note even if
		     * prebinding was not specified we will still force this to
		     * be a split library.
		     */
		    if(seg_addr_table_entry == NULL &&
		       getenv("LD_SPLITSEGS_NEW_LIBRARIES") != NULL){
		    unsigned long arch_rw_addr =
			get_shared_region_size_from_flag(&arch_flag);

			if(seg1addr_specified){
			    warning("-seg1addr 0x%x ignored, using "
				    "-segs_read_only_addr 0x%x and "
				    "-segs_read_write_addr 0x%x because "
				    "LD_SPLITSEGS_NEW_LIBRARIES environment is "
				    "set",(unsigned int)seg1addr, 0,
				    (unsigned int)arch_rw_addr);
			}
			seg1addr_specified = FALSE;
			seg1addr = 0;
			segs_read_only_addr_specified = TRUE;
			segs_read_only_addr = 0;
			segs_read_write_addr = arch_rw_addr;
		    }
		    /*
		     * Finally if this is not a split library then turn off
		     * prebinding.
		     */
		    if(segs_read_only_addr_specified == FALSE){
			if(prebinding_via_LD_PREBIND == FALSE &&
			   prebinding_flag_specified == TRUE &&
			   prebinding == TRUE){
			    warning("-prebind ignored because "
				    "MACOSX_DEPLOYMENT_TARGET environment "
				    "variable greater or equal to 10.4");
			}
			prebinding = FALSE;
		    }
		}
	    }
	}

	if(filetype == MH_BUNDLE){
	    if(dynamic == FALSE)
		fatal("incompatible flag -bundle used (must specify "
		      "\"-dynamic\" to be used)");
	    if(save_reloc)
		fatal("can't use -r and -bundle (flags are mutually "
		      "exclusive, only one or the other can be used)");
	    if(strip_level == STRIP_ALL)
		fatal("can't use -s with -bundle (file must contain "
		      "at least global symbols, for maximum stripping use -x)");
	    if(Aflag_specified)
		fatal("can't use -A and -bundle");
	    if(keep_private_externs == TRUE)
		fatal("can't use -keep_private_externs and -bundle");
	    if(segaddr_specified)
		fatal("can't use -segaddr options with -bundle (use "
		      "seg1addr to specify the starting address)");
	    if(seglinkedit_specified && seglinkedit == FALSE)
		fatal("can't use -noseglinkedit with -bundle "
		      "(resulting file must have a link edit segment to access "
		      "symbols)");
	    if(prebinding == TRUE){
		if(prebinding_flag_specified == TRUE)
		    warning("-prebind has no effect with -bundle");
		prebinding = FALSE;
	    }
	    if(private_bundle == TRUE && twolevel_namespace == TRUE)
		warning("-private_bundle has no effect when "
			"-twolevel_namespace is in effect");
	    if(twolevel_namespace_hints_specified != TRUE)
		twolevel_namespace_hints = FALSE;
	}
	else{
	    if(client_name != NULL)
		fatal("-client_name %s flag can only be used with -bundle",
		      client_name);
	    if(bundle_loader != NULL)
		fatal("-bundle_loader %s flag can only be used with -bundle",
		      bundle_loader);
	    if(private_bundle == TRUE)
		fatal("-private_bundle flag can only be used with -bundle");
	}
	if(filetype != MH_DYLINKER){
	    if(dylinker_install_name != NULL)
		warning("flag: -dylinker_install_name %s ignored (-dylinker "
			"was not specified", dylinker_install_name);
	}
	if(filetype != MH_DYLIB){
	    if(dylib_install_name != NULL)
		warning("flag: -dylib_install_name %s ignored (-dylib "
			"was not specified", dylib_install_name);
	    if(dylib_current_version != 0)
		warning("flag: -dylib_current_version %u ignored (-dylib "
			"was not specified", dylib_current_version);
	    if(dylib_compatibility_version != 0)
		warning("flag: -dylib_compatibility_version %u ignored (-dylib"
			" was not specified", dylib_compatibility_version);
	    if(init_name != NULL)
		warning("flag: -init %s ignored (-dylib was not specified",
			init_name);
	}
	if(twolevel_namespace == TRUE &&
	   undefined_flag != UNDEFINED_ERROR &&
	   undefined_flag != UNDEFINED_DYNAMIC_LOOKUP &&
	   undefined_flag != UNDEFINED_DEFINE_A_WAY){
	    if(macosx_deployment_target.major >= 3)
		fatal("-undefined error, -undefined dynamic_lookup or "
		      "-undefined define_a_way must be used when "
		      "-twolevel_namespace is in effect");
	    else
		fatal("-undefined error or -undefined define_a_way must be "
		      "used when -twolevel_namespace is in effect");
	}
	if(undefined_flag == UNDEFINED_DYNAMIC_LOOKUP){
	    if(dynamic == FALSE)
		fatal("incompatible flag -undefined dynamic_lookup used (must "
		      "specify \"-dynamic\" to be used)");
	    if(macosx_deployment_target.major < 3)
		fatal("flag: -undefined dynamic_lookup can't be used with "
		      "MACOSX_DEPLOYMENT_TARGET environment variable set to: "
		      "%s", macosx_deployment_target.name);
	}
	if(twolevel_namespace == TRUE && nundef_syms != 0){
	    fatal("can't use -U flags when -twolevel_namespace is in effect");
	}
	if(nomultidefs == TRUE){
	    if(multiply_defined_flag_specified == TRUE &&
	       multiply_defined_flag != MULTIPLY_DEFINED_ERROR)
		fatal("-multiply_defined error must be used when -nomultidefs "
		      "is specified");
	   multiply_defined_flag = MULTIPLY_DEFINED_ERROR;
	}
	if(prebinding == TRUE && undefined_flag == UNDEFINED_SUPPRESS){
	    if(prebinding_flag_specified == TRUE)
		warning("-undefined suppress disables -prebind");
	    prebinding = FALSE;
	}
	if(prebinding == TRUE && save_reloc){
	    if(prebinding_flag_specified == TRUE)
		warning("-r disables -prebind");
	    prebinding = FALSE;
	}
	if(prebinding == TRUE && dynamic == FALSE){
	    prebinding = FALSE;
	}

	/*
	 * If the output file name as not specified set it to the default name
	 * "a.out".  This needs to be done before any segments are merged
	 * because this is used when merging them (the 'filename' field in a
	 * merged_segment is set to it).
	 */
	if(outputfile == NULL)
	    outputfile = "a.out";
	/*
	 * If the -A flag is specified and the file type has not been specified
	 * then make the output file type MH_OBJECT.
	 */
	if(Aflag_specified == TRUE && filetype_specified == FALSE)
	    filetype = MH_OBJECT;

	/*
	 * If neither the -seglinkedit or -noseglinkedit has been specified then
	 * set creation of this segment if the output file type can have one.
	 * If -seglinkedit has been specified make sure the output file type
	 * can have one.
	 */
	if(seglinkedit_specified == FALSE){
	    if(filetype == MH_EXECUTE || filetype == MH_BUNDLE ||
	       filetype == MH_FVMLIB ||
	       filetype == MH_DYLIB || filetype == MH_DYLINKER)
		seglinkedit = TRUE;
	    else
		seglinkedit = FALSE;
	}
	else{
	    if(seglinkedit &&
	       (filetype != MH_EXECUTE && filetype != MH_BUNDLE &&
		filetype != MH_FVMLIB &&
		filetype != MH_DYLIB && filetype != MH_DYLINKER))
		fatal("link edit segment can't be created (wrong output file "
		      "type, file type must be MH_EXECUTE, MH_BUNDLE, "
		      "MH_DYLIB, MH_DYLINKER or MH_FVMLIB)");
	}
	if(allow_stack_execute == TRUE && filetype != MH_EXECUTE)
	    fatal("-allow_stack_execute can only be used when output file type "
		  "is MH_EXECUTE");

	if(trace)
	    print("%s: Pass 1\n", progname);
	/*
	 * This pass of parsing arguments only processes object files
	 * and creation of symbols now that all the options are set.
	 * This are order dependent and must be processed as they appear
	 * on the command line.
	 */
	symbols_created = 0;
	objects_specified = 0;
	sections_created = 0;
	/*
	 * If a -bundle_loader is specified and this is a flat_namespace
	 * output force the bundle_loader to be loaded first.
	 */
	if(bundle_loader != NULL && twolevel_namespace == FALSE){
	    pass1(bundle_loader, FALSE, FALSE, FALSE, TRUE, FALSE);
	}
	for(i = 1 ; i < argc ; i++){
	    if(*argv[i] != '-'){
		/* just a normal object file name */
		pass1(argv[i], FALSE, FALSE, FALSE, FALSE, FALSE);
		objects_specified++;
	    }
	    else{
		p = &(argv[i][1]);
		switch(*p){
		case 'b':
		    if(strcmp(p, "bundle_loader") == 0){
			/*
			 * If a -bundle_loader was specified and this is a
			 * flat_namespace output force the bundle_loader was
			 * loaded first above.
			 */
			if(twolevel_namespace == TRUE)
			    pass1(argv[i+1], FALSE, FALSE, FALSE, TRUE, FALSE);
			i++;
			break;
		    }
		    break;
		case 'l':
		    /* path searched abbrevated file name */
		    pass1(argv[i], TRUE, FALSE, FALSE, FALSE, FALSE);
		    objects_specified++;
		    break;
		case 'A':
		    if(base_obj != NULL)
			fatal("only one -A argument can be specified");
		    pass1(argv[++i], FALSE, TRUE, FALSE, FALSE, FALSE);
		    objects_specified++;
		    break;
		case 'f':
		    if(strcmp(p, "framework") == 0){
			if(dynamic == FALSE)
			    fatal("incompatible flag -framework used (must "
				  "specify \"-dynamic\" to be used)");
			pass1(argv[++i], FALSE, FALSE, TRUE, FALSE, FALSE);
			objects_specified++;
		    }
		    if(strcmp(p, "filelist") == 0){
			filelist = argv[++i];
			dirname = strrchr(filelist, ',');
			if(dirname != NULL){
			    *dirname = '\0';
			    dirname++;
			}
			else
			    dirname = "";
			if((fd = open(filelist, O_RDONLY, 0)) == -1)
			    system_fatal("can't open file list file: %s",
				         filelist);
			if(fstat(fd, &stat_buf) == -1)
			    system_fatal("can't stat file list file: %s",
					 filelist);
			/*
			 * For some reason mapping files with zero size fails
			 * so it has to be handled specially.
			 */
			if(stat_buf.st_size != 0){
			    if((r = map_fd((int)fd, (vm_offset_t)0,
				(vm_offset_t *)&(addr), (boolean_t)TRUE,
				(vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
				mach_fatal(r, "can't map file list file: %s",
				    filelist);
			}
			else{
			    fatal("file list file: %s is empty", filelist);
			}
			close(fd);
			file_name = addr;
			for(j = 0; j < stat_buf.st_size; j++){
			    if(addr[j] == '\n'){
				addr[j] = '\0';
				if(*dirname != '\0'){
				    file_name = mkstr(dirname, "/",
						      file_name, NULL);
				}
				pass1(file_name, FALSE, FALSE, FALSE, FALSE,
				      FALSE);
				objects_specified++;
				file_name = addr + j + 1;
			    }
			}
		    }
		    if(strcmp(p, "final_output") == 0)
			i++;
		    break;
		case 'm':
		    if(strcmp(p, "multiply_defined") == 0 ||
		       strcmp(p, "multiply_defined_unused") == 0 ||
		       strcmp(p, "macosx_version_min") == 0){
			i++;
			break;
		    }
		    break;
		case 'u':
		    if(strcmp(p, "undefined") == 0 ||
		       strcmp(p, "umbrella") == 0 ||
		       strcmp(p, "unexported_symbols_list") == 0){
			i++;
			break;
		    }
		    /* cause the specified symbol name to be undefined */
		    (void)command_line_symbol(argv[++i]);
		    symbols_created++;
		    break;
		case 'i':
		    if(strcmp(p, "image_base") == 0){
			i++;
			break;
		    }
		    else if(strcmp(p, "init") == 0){
			i++;
			break;
		    }
		    else if(strcmp(p, "install_name") == 0){
		        i++;
			break;
		    }
		    /* create an indirect symbol, symbol_name, to be an indirect
		       symbol for indr_symbol_name */
		    symbol_name = p + 1;
	  	    indr_symbol_name = strchr(p + 1, ':');
		    *indr_symbol_name = '\0';
		    indr_symbol_name++;
		    command_line_indr_symbol(symbol_name, indr_symbol_name);
		    symbols_created++;
		    break;

		/* multi argument flags */
		case 'a':
		    if(strcmp(p, "all_load") == 0 ||
		       strcmp(p, "arch_multiple") == 0 ||
		       strcmp(p, "arch_errors_fatal") == 0 ||
		       strcmp(p, "allow_stack_execute") == 0)
			break;
		    i++;
		    break;
		case 'c':
		    i++;
		    break;
		case 'p':
		    if(strcmp(p, "pagezero_size") == 0){
			i++;
			break;
		    }
		    break;
		case 's':
		    if(strcmp(p, "sectcreate") == 0 ||
		       strcmp(p, "segcreate") == 0){
			sections_created++;
			i += 3;
		    }
		    else if(strcmp(p, "sectalign") == 0 ||
		            strcmp(p, "segprot") == 0 ||
		            strcmp(p, "sectorder") == 0)
			i += 3;
		    else if(strcmp(p, "segaddr") == 0 ||
			    strcmp(p, "sect_diff_relocs") == 0 ||
		            strcmp(p, "sectobjectsymbols") == 0)
			i += 2;
		    else if(strcmp(p, "seg1addr") == 0 ||
		            strcmp(p, "stack_addr") == 0 ||
		            strcmp(p, "stack_size") == 0 ||
		            strcmp(p, "segalign") == 0 ||
			    strcmp(p, "segs_read_only_addr") == 0 ||
			    strcmp(p, "segs_read_write_addr") == 0 ||
			    strcmp(p, "seg_addr_table") == 0 ||
			    strcmp(p, "seg_addr_table_filename") == 0 ||
			    strcmp(p, "sub_umbrella") == 0 ||
			    strcmp(p, "sub_library") == 0 ||
			    strcmp(p, "syslibroot") == 0)
			i++;
		    break;
		case 'r':
		    if(strcmp(p, "r") == 0 ||
		       strcmp(p, "run_init_lazily") == 0)
			break;
		    i++;
		    break;
		case 'o':
		    if(strcmp(p, "object") == 0)
			break;
		    i++;
		    break;
		case 'e':
		    if(strcmp(p, "execute") == 0)
			break;
		    i++;
		    break;
		case 'd':
		    if(strcmp(p, "d") == 0 ||
		       strcmp(p, "dylib") == 0 ||
		       strcmp(p, "dylinker") == 0 ||
		       strcmp(p, "dynamic") == 0 ||
		       strcmp(p, "dead_strip") == 0)
			break;
		    i++;
		    break;
		case 'h':
		    if(strcmp(p, "headerpad_max_install_names") == 0)
			break;
		    i++;
		    break;
		case 'U':
		case 'N':
		case 'Y':
		    i++;
		    break;
		case 'w':
		    if(strcmp(p, "weak_reference_mismatches") == 0)
			i++;
		    else if(strcmp(p, "weak_library") == 0){
			pass1(argv[++i], FALSE, FALSE, FALSE, FALSE, TRUE);
			objects_specified++;
		    }
		    else if(strncmp(p, "weak-l", sizeof("weak-l") - 1) == 0){
			/* path searched abbrevated file name */
			pass1(argv[i] + sizeof("weak"), TRUE, FALSE, FALSE,
			      FALSE, TRUE);
			objects_specified++;
		    }
		    else if(strcmp(p, "weak_framework") == 0){
			if(dynamic == FALSE)
			    fatal("incompatible flag -weak_framework used (must"
				  " specify \"-dynamic\" to be used)");
			pass1(argv[++i], FALSE, FALSE, TRUE, FALSE, TRUE);
			objects_specified++;
		    }
		    break;
		}
	    }
	}

 	/*
	 * If the architecture was not specified, and was inferred
	 * from the object files, if it is a 64-bit architecture it is an error.
	 */
	if(arch_flag.cputype != 0 &&
	    arch_flag.cputype & CPU_ARCH_ABI64){
	    fatal("does not support 64-bit architectures");
	}

	/*
	 * Now search the libraries on the dynamic shared libraries search list
	 */
	search_dynamic_libs();

	/*
	 * Check to see that the output file will have something in it.
	 */
	if(objects_specified == 0){
	    if(symbols_created != 0 || sections_created != 0){
		warning("no object files specified, only command line created "
			"symbols and/or sections created from files will "
			"appear in the output file");
		if(arch_flag.name == NULL)
		    target_byte_sex = host_byte_sex;
		segalign = host_pagesize;
	    }
	    else{
		if(vflag == TRUE)
		    ld_exit(0);
		fatal("no object files specified");
	    }
	}
	else if(base_obj != NULL && nobjects == 1){
	    if(symbols_created != 0 || sections_created != 0)
		warning("no object files loaded other than base file, only "
			"additional command line created symbols and/or "
			"sections created from files will appear in the output "
			"file");
	    else{
		if(vflag == TRUE)
		    ld_exit(0);
		fatal("no object files loaded other than base file");
	    }
	}
	else if(nobjects == 0){
	    if(symbols_created != 0 || sections_created != 0)
		warning("no object files loaded, only command line created "
			"symbols and/or sections created from files will "
			"appear in the output file");
	    else{
		if(vflag == TRUE)
		    ld_exit(0);
		fatal("no object files loaded");
	    }
	}

#ifdef DEBUG
	if(debug & (1 < 0))
	    print_object_list();
	if(debug & (1 << 1))
	    print_merged_sections("after pass1");
	if(debug & (1 << 2))
	    print_symbol_list("after pass1", TRUE);
	if(debug & (1 << 3))
	    print_undefined_list();
	if(debug & (1 << 4))
	    print_segment_specs();
	if(debug & (1 << 5))
	    print_load_fvmlibs_list();
	if(debug & (1 << 6))
	    print_fvmlib_segments();
	if(debug & (1 << 9)){
	    print("Number of objects loaded = %lu\n", nobjects);
	    print("Number of merged symbols = %lu\n", nmerged_symbols);
	}
#endif /* DEBUG */

	/*
	 * If there were any errors from pass1() then don't continue.
	 */
	if(errors != 0)
	    ld_exit(1);

	/*
	 * Print which files are loaded if requested.
	 */
	if(whatsloaded == TRUE)
	    print_whatsloaded();

	/*
	 * Clean up any data structures not need for layout() or pass2().
	 */
	if(nsearch_dirs != 0){
	    free(search_dirs);
	    nsearch_dirs = 0;
	}

	/*
	 * Layout the output object file.
	 */
	layout();

	/*
	 * Check to that the exported or unexported symbols listed were seen.
	 */
	if(save_symbols != NULL){
	    missing_syms = FALSE;
	    for(j = 0; j < nsave_symbols ; j++){
		if(save_symbols[j].seen == FALSE){
		    if(missing_syms == FALSE){
			error("symbols names listed in "
			      "-exported_symbols_list: %s not in linked "
			      "objects", exported_symbols_list);
			missing_syms = TRUE;
		    }
		    printf("%s\n", save_symbols[j].name);
		}
	    }
	}

	/*
	 * If there were any errors from layout() then don't continue.
	 */
	if(errors != 0)
	    ld_exit(1);

	/*
	 * Clean up any data structures not need for pass2().
	 */
	free_pass1_symbol_data();
	if(ntrace_syms != 0){
	    free(trace_syms);
	    ntrace_syms = 0;
	}
	if(nundef_syms != 0){
	    free(undef_syms);
	    nundef_syms = 0;
	}

	/*
	 * Write the output object file doing relocation on the sections.
	 */
	if(trace)
	    print("%s: Pass 2\n", progname);
	pass2();
	/*
	 * If there were any errors from pass2() make sure the output file is
	 * removed and exit non-zero.
	 */
	if(errors != 0)
	    cleanup();

	if(hash_instrument_specified == TRUE)
	    hash_instrument();

	ld_exit(0);

	/* this is to remove the compiler warning, it never gets here */
	return(0);
}

/*
 * unprebound_library() checks the file for the environment variable
 * LD_UNPREBOUND_LIBRARIES to see if the dynamic library is one listed as to
 * not be prebound.  The dynamic library is specified with the
 * dylib_install_name unless seg_addr_table_filename is not NULL then
 * seg_addr_table_filename is used.  If it is found on the list then TRUE is
 * returned.  If not FALSE is returned.
 */
static
enum bool
unprebound_library(
char *dylib_install_name,
char *seg_addr_table_filename)
{
    int fd;
    kern_return_t r;
    struct stat stat_buf;
    unsigned long j, file_size, line;
    char *file_name, *library_name, *file_addr, *name, *end;

	/*
	 * If there is no file name then it is not on the list and return FALSE.
	 */
	file_name = getenv("LD_UNPREBOUND_LIBRARIES");
	if(file_name == NULL)
	    return(FALSE);

	/*
	 * If there is no library name then it is not on the list and return
	 * FALSE.
	 */
	if(seg_addr_table_filename != NULL)
	    library_name = dylib_install_name;
	else if(dylib_install_name != NULL)
	    library_name = dylib_install_name;
	else
	    return(FALSE);


	if((fd = open(file_name, O_RDONLY, 0)) == -1)
	    system_fatal("Can't open: %s for LD_UNPREBOUND_LIBRARIES "
			 "environment variable", file_name);
	if(fstat(fd, &stat_buf) == -1)
	    system_fatal("Can't stat file: %s for LD_UNPREBOUND_LIBRARIES "
		    	 "environment variable", file_name);
	/*
	 * For some reason mapping files with zero size fails
	 * so it has to be handled specially.
	 */
	if(stat_buf.st_size != 0){
	    if((r = map_fd((int)fd, (vm_offset_t)0,
		(vm_offset_t *)&file_addr, (boolean_t)TRUE,
		(vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't map file: %s for LD_UNPREBOUND_LIBRARIES "
			   "environment variable", file_name);
	}
	else
	    fatal("Empty file: %s for LD_UNPREBOUND_LIBRARIES environment "
		  "variable", file_name);
	close(fd);
	file_size = stat_buf.st_size;

	/*
	 * Got the file mapped now parse it.
	 */
	if(file_addr[file_size - 1] != '\n')
	    fatal("file: %s for LD_UNPREBOUND_LIBRARIES environment variable "
		  "does not end in new line", file_name);

	line = 1;
	for(j = 0; j < file_size; /* no increment expression */ ){
	    /* Skip lines that start with '#' */
	    if(file_addr[j] == '#'){
		j++;
		while(file_addr[j] != '\n')
		    j++;
		continue;
	    }
	    /* Skip blank lines and leading white space */
	    while(file_addr[j] == ' ' || file_addr[j] == '\t')
		j++;
	    if(file_addr[j] == '\n'){
		j++;
		line++;
		continue;
	    }
	    if(j == file_size)
		fatal("missing library install name on line %lu in file: "
		      "%s for LD_UNPREBOUND_LIBRARIES environment variable",
		      line, file_name);

	    name = file_addr + j;
	    while(file_addr[j] != '\n')
		j++;
	    file_addr[j] = '\0';
	    end = file_addr + j;
	    line++;
	    j++;

	    /* Trim trailing spaces */
	    end--;
	    while(end > name && (*end == ' ' || *end == '\t')){
		*end = '\0';
		end--;
	    }

	    /* finally compare the name on this line with the library name */
	    if(strcmp(library_name, name) == 0)
		return(TRUE);
	}

	return(FALSE);
}

/*
 * ispoweroftwo() returns TRUE or FALSE depending if x is a power of two.
 */
static
enum
bool
ispoweroftwo(
unsigned long x)
{
	if(x == 0)
	    return(TRUE);
	while((x & 0x1) != 0x1){
	    x >>= 1;
	}
	if((x & ~0x1) != 0)
	    return(FALSE);
	else
	    return(TRUE);
}

/*
 * getprot() returns the vm_prot for the specified string passed to it.  The
 * string may contain any of the following characters: 'r', 'w', 'x' and '-'
 * representing read, write, execute and no protections.  The pointer pointed
 * to by endp is set to the first character that is not one of the above
 * characters.
 */
static
vm_prot_t
getprot(
char *prot,
char **endp)
{
    vm_prot_t vm_prot;

	vm_prot = VM_PROT_NONE;
	while(*prot){
	    switch(*prot){
	    case 'r':
	    case 'R':
		vm_prot |= VM_PROT_READ;
		break;
	    case 'w':
	    case 'W':
		vm_prot |= VM_PROT_WRITE;
		break;
	    case 'x':
	    case 'X':
		vm_prot |= VM_PROT_EXECUTE;
		break;
	    case '-':
		break;
	    default:
		*endp = prot;
		return(vm_prot);
	    }
	    prot++;
	}
	*endp = prot;
	return(vm_prot);
}

/*
 * check_max_init_prot() checks to make sure that all protections in the initial
 * protection are also in the maximum protection.
 */
static
enum bool
check_max_init_prot(
vm_prot_t maxprot,
vm_prot_t initprot)
{
	if(((initprot & VM_PROT_READ)    && !(maxprot & VM_PROT_READ)) ||
	   ((initprot & VM_PROT_WRITE)   && !(maxprot & VM_PROT_WRITE)) ||
	   ((initprot & VM_PROT_EXECUTE) && !(maxprot & VM_PROT_EXECUTE)) )
	return(FALSE);
	return(TRUE);
}

/*
 * ld_exit() is use for all exit()s from the link editor.
 */
static
void
ld_exit(
int exit_value)
{
	exit(exit_value);
}

/*
 * cleanup() is called by all routines handling fatal errors to remove the
 * output file if it had been created by the link editor and exit non-zero.
 */
static
void
cleanup(void)
{
	if(output_addr != NULL)
	    unlink(outputfile);
	ld_exit(1);
}

/*
 * handler() is the routine for catching SIGINT, SIGTERM, SIGBUG & SIGSEGV
 *  signals. It cleans up and exit()'s non-zero.
 */
static
void
handler(
int sig)
{
#ifdef __MWERKS__
    int dummy;
        dummy = sig;
#endif
	if(output_addr != NULL)
	    unlink(outputfile);
	_exit(1);
}

/*
 * allocate() is just a wrapper around malloc that prints and error message and
 * exits if the malloc fails.
 */
__private_extern__
void *
allocate(
unsigned long size)
{
    void *p;

	if(size == 0)
	    return(NULL);
	if((p = malloc(size)) == NULL)
	    system_fatal("virtual memory exhausted (malloc failed)");
	return(p);
}

/*
 * reallocate() is just a wrapper around realloc that prints and error message
 * and exits if the realloc fails.
 */
__private_extern__
void *
reallocate(
void *p,
unsigned long size)
{
	if(p == NULL)
	    return(allocate(size));
	if((p = realloc(p, size)) == NULL)
	    system_fatal("virtual memory exhausted (realloc failed)");
	return(p);
}

/*
 * savestr() malloc's space for the string passed to it, copys the string into
 * the space and returns a pointer to that space.
 */
__private_extern__
char *
savestr(
const char *s)
{
    long len;
    char *r;

	len = strlen(s) + 1;
	r = (char *)allocate(len);
	strcpy(r, s);
	return(r);
}

/*
 * Mkstr() creates a string that is the concatenation of a variable number of
 * strings.  It is pass a variable number of pointers to strings and the last
 * pointer is NULL.  It returns the pointer to the string it created.  The
 * storage for the string is malloc()'ed can be free()'ed when nolonger needed.
 */
static
char *
mkstr(
const char *args,
...)
{
    va_list ap;
    char *s, *p;
    unsigned long size;

	size = 0;
	if(args != NULL){
	    size += strlen(args);
	    va_start(ap, args);
	    p = (char *)va_arg(ap, char *);
	    while(p != NULL){
		size += strlen(p);
		p = (char *)va_arg(ap, char *);
	    }
	}
	s = allocate(size + 1);
	*s = '\0';

	if(args != NULL){
	    (void)strcat(s, args);
	    va_start(ap, args);
	    p = (char *)va_arg(ap, char *);
	    while(p != NULL){
		(void)strcat(s, p);
		p = (char *)va_arg(ap, char *);
	    }
	    va_end(ap);
	}
	return(s);
}
#endif /* !defined(RLD) */

/*
 * rnd() rounds v to a multiple of r.
 */
__private_extern__
unsigned long
rnd(
unsigned long v,
unsigned long r)
{
	r--;
	v += r;
	v &= ~(long)r;
	return(v);
}

#ifndef RLD
#include "stuff/unix_standard_mode.h"
/*
 * All printing of all messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
	if(get_unix_standard_mode() == TRUE)
	    vfprintf(stderr, format, ap);
	else
	    vprintf(format, ap);
}
#endif /* !defined(RLD) */

/*
 * The print function that just calls the above vprint() function.
 */
__private_extern__
void
print(
const char *format,
...)
{
    va_list ap;

	va_start(ap, format);
	vprint(format, ap);
	va_end(ap);
}

/*
 * The ld_trace function that logs things for B&I.
 */
__private_extern__
void
ld_trace(
const char *format,
...)
{
#ifdef KLD
    va_list ap;

	va_start(ap, format);
	vprint(format, ap);
	va_end(ap);
#else
	static int trace_file = -1;
	char trace_buffer[MAXPATHLEN * 2];
	char *buffer_ptr;
	int length;
	ssize_t amount_written;

	if(trace_file == -1){
		if(trace_file_path != NULL){
			trace_file = open(trace_file_path, O_WRONLY | O_APPEND | O_CREAT, 0666);
			if(trace_file == -1)
				fatal("Could not open or create trace file: %s\n", trace_file_path);
		}
		else{
			trace_file = fileno(stderr);
		}
	}
    va_list ap;

	va_start(ap, format);
	length = vsnprintf(trace_buffer, sizeof(trace_buffer), format, ap);
	va_end(ap);
	buffer_ptr = trace_buffer;
	while(length > 0){
		amount_written = write(trace_file, buffer_ptr, length);
		if(amount_written == -1)
			/* Failure to write shouldn't fail the build. */
			return;
		buffer_ptr += amount_written;
		length -= amount_written;
	}
#endif
}

static
void
print_architecture_banner(void)
{
    static enum bool printed = FALSE;

	if(arch_multiple == TRUE && printed == FALSE && arch_flag.name != NULL){
	    print("%s: for architecture %s\n", progname, arch_flag.name);
	    printed = TRUE;
	}
}

/*
 * Print the warning message.  This is non-fatal and does not set 'errors'.
 */
__private_extern__
void
warning(
const char *format,
...)
{
    va_list ap;

	if(nowarnings == TRUE)
	    return;
	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: warning ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
}

/*
 * Print the error message and set the 'error' indication.
 */
__private_extern__
void
error(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors = 1;
}

/*
 * Print the fatal error message, and exit non-zero.
 */
__private_extern__
void
fatal(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	cleanup();
}

/*
 * Print the current object file name and warning message.
 */
__private_extern__
void
warning_with_cur_obj(
const char *format,
...)
{
    va_list ap;

	if(nowarnings == TRUE)
	    return;
	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: warning ", progname);
	print_obj_name(cur_obj);
	vprint(format, ap);
        print("\n");
	va_end(ap);
}

/*
 * Print the current object file name and error message, set the non-fatal
 * error indication.
 */
__private_extern__
void
error_with_cur_obj(
const char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	print_obj_name(cur_obj);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors = 1;
}

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/*
 * Print the warning message along with the system error message.
 */
__private_extern__
void
system_warning(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: warning ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
}

/*
 * Print the error message along with the system error message, set the
 * non-fatal error indication.
 */
__private_extern__
void
system_error(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
	errors = 1;
}

/*
 * Print the fatal message along with the system error message, and exit
 * non-zero.
 */
__private_extern__
void
system_fatal(
const char *format,
...)
{
    va_list ap;
    int errnum;

	if(arch_multiple)
	    print_architecture_banner();
	errnum = errno;
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s, errno = %d)\n", strerror(errnum), errnum);
	va_end(ap);
	cleanup();
}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

/*
 * Print the fatal error message along with the mach error string, and exit
 * non-zero.
 */
__private_extern__
void
mach_fatal(
kern_return_t r,
char *format,
...)
{
    va_list ap;

	if(arch_multiple)
	    print_architecture_banner();
	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s)\n", mach_error_string(r));
	va_end(ap);
	cleanup();
}
