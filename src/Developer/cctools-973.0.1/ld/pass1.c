/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
 * This file contains the routines that drives pass1 of the link-editor.  In
 * pass1 the objects needed from archives are selected for loading and all of
 * the things that need to be merged from the input objects are merged into
 * tables (for output and relocation on the second pass).
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/openstep_mach.h"
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ldsyms.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <libc.h>
#include <stdio.h>
#include <mach/mach.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <ar.h>
#ifndef AR_EFMT1
#define	AR_EFMT1	"#1/"		/* extended format #1 */
#endif
#include <mach-o/ranlib.h>
#include "stuff/arch.h"
#include "stuff/best_arch.h"
#include "stuff/guess_short_name.h"
#include "stuff/macosx_deployment_target.h"

#include "ld.h"
#include "pass1.h"
#include "live_refs.h"
#include "objects.h"
#include "fvmlibs.h"
#include "dylibs.h"
#include "sections.h"
#include "symbols.h"
#include "sets.h"
#include "layout.h"
#ifndef KLD
#include "debugcompunit.h"
#include "debugline.h"
#endif /* ! KLD */

#ifndef RLD
/* TRUE if -search_paths_first was specified */
__private_extern__ enum bool search_paths_first = FALSE;

/* the user specified directories to search for -lx names, and the number
   of them */
__private_extern__ char **search_dirs = NULL;
__private_extern__ unsigned long nsearch_dirs = 0;

/*
 * The user specified directories to search via the environment variable
 * LD_LIBRARY_PATH.
 */
__private_extern__ char **ld_library_paths = NULL;
__private_extern__ unsigned long nld_library_paths = 0;

/* the standard directories to search for -lx names */
__private_extern__ char *standard_dirs[] = {
    "/lib/",
    "/usr/lib/",
    "/usr/local/lib/",
    NULL
};

/*
 * The user specified directories to search for "-framework Foo" names, and the
 * number of them.  These are specified with -F options.
 */
__private_extern__ char **framework_dirs = NULL;
__private_extern__ unsigned long nframework_dirs = 0;

/* the standard framework directories to search for "-framework Foo" names */
__private_extern__ char *standard_framework_dirs[] = {
#ifdef __OPENSTEP__
    "/LocalLibrary/Frameworks/",
    "/NextLibrary/Frameworks/",
#else /* !defined(__OPENSTEP__) */

#ifdef __GONZO_BUNSEN_BEAKER__
    "/Local/Library/Frameworks/",
#else /* !defined(__BUNSEN_BEAKER__) */
    "/Library/Frameworks/",
#endif /* __BUNSEN_BEAKER__ */

    "/Network/Library/Frameworks/",
    "/System/Library/Frameworks/",
#endif /* __OPENSTEP__ */
    NULL
};

/* The pointer to the head of the base object file's segments */
__private_extern__ struct merged_segment *base_obj_segments = NULL;
#endif /* !defined(RLD) */

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/*
 * The stat buffer for input files.  This is needed both by pass1() and
 * pass1_archive() to check the times.
 */
static struct stat stat_buf = { 0 };

/*
 * These are pointers to strings and symbols used to search of the table of
 * contents of a library.  These have to be can not be local so that
 * pass1_archive() and search_dylibs() can set them and that ranlib_bsearch()
 * and dylib_bsearch() can use them.  This is done instead of assigning to
 * ran_name so that the library can be mapped read only and thus not get dirty
 * and maybe written to the swap area by the kernel.
 */
__private_extern__ char *bsearch_strings = NULL;
#ifndef RLD
__private_extern__ struct nlist *bsearch_symbols = NULL;

/*
 * The list of dynamic libraries to search.  The list of specified libraries
 * can contain archive libraries when archive libraries appear after dynamic
 * libraries on the link line.
 */
__private_extern__ struct dynamic_library *dynamic_libs = NULL;

/*
 * When building two-level-namespace, indirect libraries are not kept
 * in the dynamic_libs list.  The pre-10.4 prebinding may need to grow
 * the load commands in an executable proportional to the number of
 * direct and indirect libraries.  The variable indirect_library_ratio
 * gives us a scaling factor to multiple by the number of libraries
 * in dynamic_libs as an estimate of the total number of libraries.
 */
__private_extern__ unsigned int indirect_library_ratio = 1;

/*
 * The variable indirect_dylib is used for search_dynamic_libs() to communicate
 * with check_cur_obj() to prevent the error message that is generated when
 * a subframework is directly linked against.  When an umbrella framework is
 * linked against it subframeworks are indirectly used which are not an error.
 */
static enum bool indirect_dylib = FALSE;

static void search_for_file(
    char *base_name,
    char **file_name,
    int *fd);
static void search_for_framework(
    char *name,
    char **file_name,
    int *fd);
static void search_paths_for_lname(
    const char *lname_argument,
    char **file_name,
    int *fd);
static void search_path_for_lname(
    const char *dir,
    const char *lname_argument,
    char **file_name,
    int *fd);
#endif /* !defined(RLD) */

static void pass1_fat(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool dylib_only,
    enum bool bundle_loader,
    enum bool force_weak);

static void pass1_archive(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool from_fat_file,
    enum bool bundle_loader,
    enum bool force_weak);

static enum bool check_archive_arch(
    char *file_name,
    char *file_addr,
    unsigned long file_size);

static enum bool check_extend_format_1(
    char *name,
    struct ar_hdr *ar_hdr,
    unsigned long size_left,
    unsigned long *member_name_size);

static void pass1_object(
    char *file_name,
    char *file_addr,
    unsigned long file_size,
    enum bool base_name,
    enum bool from_fat_file,
    enum bool dylib_only,
    enum bool bundle_loader,
    enum bool force_weak);

#ifndef RLD
static void load_init_dylib_module(
    struct dynamic_library *q);
static enum bool setup_sub_images(
    struct dynamic_library *p);
static void check_dylibs_for_definition(
    struct merged_symbol *merged_symbol,
    enum bool prebind_check,
    enum bool twolevel_namespace_check);
static enum bool check_dylibs_for_reference(
    struct merged_symbol *merged_symbol);

static enum bool open_dylib(
    struct dynamic_library *p);

static enum bool set_sub_frameworks_ordinals(
    struct dynamic_library *umbrella);

static enum bool set_sub_umbrella_sub_library_ordinal(
    struct dynamic_library *sub);

static void set_isub_image(
    struct dynamic_library *p,
    struct dynamic_library *sub);

static int nlist_bsearch(
    const char *symbol_name,
    const struct nlist *symbol);
#endif /* !defined(RLD) */

static int ranlib_bsearch(
    const char *symbol_name,
    const struct ranlib *ran);

#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

static void check_cur_obj(
    enum bool dylib_only,
    enum bool bundle_loader);

#ifndef KLD
static void read_dwarf_info(void);
#endif

static void check_size_offset(
    unsigned long size,
    unsigned long offset,
    unsigned long align,
    char *size_str,
    char *offset_str,
    unsigned long cmd);

static void check_size_offset_sect(
    unsigned long size,
    unsigned long offset,
    unsigned long align,
    char *size_str,
    char *offset_str,
    unsigned long cmd,
    unsigned long sect,
    char *segname,
    char *sectname);

#ifndef RLD
static void collect_base_obj_segments(
	void);

static void add_base_obj_segment(
	struct segment_command *sg,
	char *filename);

static char *mkstr(
	const char *args,
	...);
#endif /* !defined(RLD) */

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/*
 * pass1() is called from main() and is passed the name of a file and a flag
 * indicating if it is a path searched abbrevated file name (a -lx argument).
 *
 * If the name is a path searched abbrevated file name (of the form "-lx")
 * then it is searched for in the search_dirs list (created from the -Ldir
 * arguments) and then in the list of standard_dirs.  The string "x" of the
 * "-lx" argument will be converted to "libx.a" if the string does not end in
 * ".o", in which case it is left alone.
 *
 * If the file is the object file for a the base of an incremental load then
 * base_name is TRUE and the pointer to the object_file structure for it,
 * base_obj, is set when it is allocated.
 *
 * If the file turns out to be just a plain object then merge() is called to
 * merge its symbolic information and it will be unconditionally loaded.
 *
 * Object files in archives are loaded only if they resolve currently undefined
 * references or the -ObjC flag is set and their are symbols with the ".objc"
 * prefix defined.
 */
__private_extern__
void
pass1(
char *name,
enum bool lname,
enum bool base_name,
enum bool framework_name,
enum bool bundle_loader,
enum bool force_weak)
{
    int fd;
    char *file_name;
#ifndef RLD
    char *p, *type;
#endif /* !defined(RLD) */
    kern_return_t r;
    unsigned long file_size;
    char *file_addr;
    struct fat_header *fat_header;
#ifdef __MWERKS__
    enum bool dummy;
        dummy = lname;
        dummy = base_name;
        dummy = framework_name;
#endif

#ifdef DEBUG
	/* The compiler "warning: `file_name' may be used uninitialized in */
	/* this function" can safely be ignored */
	file_name = NULL;
#endif /* DEBUG */

	fd = -1;
#ifndef RLD
	if(lname){
	    if(name[0] != '-' || name[1] != 'l')
		fatal("Internal error: pass1() called with name of: %s and "
		      "lname == TRUE", name);
	    p = &name[2];
	    p = strrchr(p, '.');
	    if(p != NULL && strcmp(p, ".o") == 0){
		p = &name[2];
		search_for_file(p, &file_name, &fd);
		if(fd == -1)
		    fatal("can't locate file for: %s", name);
	    }
	    else{
		p = NULL;
		if(dynamic == TRUE){
		    if(search_paths_first == TRUE){
			search_paths_for_lname(&name[2], &file_name, &fd);
		    }
		    else{
			p = mkstr("lib", &name[2], ".dylib", NULL);
			search_for_file(p, &file_name, &fd);
			if(fd == -1){
			    p = mkstr("lib", &name[2], ".a", NULL);
			    search_for_file(p, &file_name, &fd);
			}
		    }
		}
		else{
		    p = mkstr("lib", &name[2], ".a", NULL);
		    search_for_file(p, &file_name, &fd);
		}
		if(fd == -1)
		    fatal("can't locate file for: %s", name);
		if(p != NULL)
		    free(p);
	    }
	}
	else if(framework_name){
	    type = strrchr(name, ',');
	    if(type != NULL && type[1] != '\0'){
		*type = '\0';
		type++;
		p = mkstr(name, ".framework/", name, type, NULL);
		search_for_framework(p, &file_name, &fd);
		if(fd == -1)
		    warning("can't locate framework for: -framework %s,%s "
			    "using suffix %s", name, type, type);
	    }
	    else
		type = NULL;
	    if(fd == -1){
		p = mkstr(name, ".framework/", name, NULL);
		search_for_framework(p, &file_name, &fd);
	    }
	    if(fd == -1){
		if(type != NULL)
		    fatal("can't locate framework for: -framework %s,%s",
			  name, type);
		else
		    fatal("can't locate framework for: -framework %s", name);
	    }
	}
	else
#endif /* !defined(RLD) */
	{
	    if((fd = open(name, O_RDONLY, 0)) == -1){
		system_error("can't open: %s", name);
		return;
	    }
	    file_name = name;
	}

	/*
	 * Now that the file_name has been determined and opened get it into
	 * memory by mapping it.
	 */
	if(fstat(fd, &stat_buf) == -1){
	    system_fatal("can't stat file: %s", file_name);
	    close(fd);
	    return;
	}
	file_size = stat_buf.st_size;
	/*
	 * For some reason mapping files with zero size fails so it has to
	 * be handled specially.
	 */
	if(file_size == 0){
	    error("file: %s is empty (not an object or archive)", file_name);
	    close(fd);
	    return;
	}
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&file_addr,
	    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
	    close(fd);
	    mach_fatal(r, "can't map file: %s", file_name);
	}
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() map_fd: addr = 0x%0x size = 0x%x\n",
	      (unsigned int)file_addr, (unsigned int)file_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	/*
	 * The mapped file can't be made read-only because even in the case of
	 * errors where a wrong bytesex file is attempted to be loaded it must
	 * be writeable to detect the error.
	 *
	 *  if((r = vm_protect(mach_task_self(), (vm_address_t)file_addr,
	 * 		file_size, FALSE, VM_PROT_READ)) != KERN_SUCCESS){
	 *      close(fd);
	 *      mach_fatal(r, "can't make memory for mapped file: %s read-only",
	 *      	   file_name);
	 *  }
	 */
	close(fd);

	/*
	 * Determine what type of file it is (fat, archive or thin object file).
	 */
	if(sizeof(struct fat_header) > file_size){
	    error("truncated or malformed file: %s (file size too small to be "
		  "any kind of object or library)", file_name);
	    return;
	}
	fat_header = (struct fat_header *)file_addr;
#ifdef __BIG_ENDIAN__
	if(fat_header->magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	{
#ifdef RLD
	    new_archive_or_fat(file_name, file_addr, file_size);
#endif /* RLD */
	    pass1_fat(file_name, file_addr, file_size, base_name, FALSE,
		      bundle_loader, force_weak);
	}
	else if(file_size >= SARMAG && strncmp(file_addr, ARMAG, SARMAG) == 0){
	    pass1_archive(file_name, file_addr, file_size, base_name, FALSE,
			  bundle_loader, force_weak);
	}
	else{
	    pass1_object(file_name, file_addr, file_size, base_name, FALSE,
			 FALSE, bundle_loader, force_weak);
	}
#ifdef VM_SYNC_DEACTIVATE
	vm_msync(mach_task_self(), (vm_address_t)file_addr,
		 (vm_size_t)file_size, VM_SYNC_DEACTIVATE);
#endif /* VM_SYNC_DEACTIVATE */
}

#ifndef RLD
/*
 * search_for_file() takes base_name and trys to open a file with that base name
 * in the -L search directories and in the standard directories.  If it is
 * sucessful it returns a pointer to the file name indirectly through file_name
 * and the open file descriptor indirectly through fd.
 */
static
void
search_for_file(
char *base_name,
char **file_name,
int *fd)
{
    unsigned long i;

	*fd = -1;
	for(i = 0; i < nsearch_dirs ; i++){
	    *file_name = mkstr(search_dirs[i], "/", base_name, NULL);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		break;
	    free(*file_name);
	}
	if(*fd == -1){
	    for(i = 0; i < nld_library_paths ; i++){
		*file_name = mkstr(ld_library_paths[i], "/", base_name, NULL);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		    break;
		free(*file_name);
	    }
	}
	if(*fd == -1){
	    for(i = 0; standard_dirs[i] != NULL ; i++){
		*file_name = mkstr(standard_dirs[i], base_name, NULL);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		    break;
		free(*file_name);
	    }
	}
}

/*
 * search_for_framework() takes name and trys to open a file with that name
 * in the -F search directories and in the standard framework directories.  If
 * it is sucessful it returns a pointer to the file name indirectly through
 * file_name and the open file descriptor indirectly through fd.
 */
static
void
search_for_framework(
char *name,
char **file_name,
int *fd)
{
    unsigned long i;

	*fd = -1;
	for(i = 0; i < nframework_dirs ; i++){
	    *file_name = mkstr(framework_dirs[i], "/", name, NULL);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		break;
	    free(*file_name);
	}
	if(*fd == -1){
	    for(i = 0; standard_framework_dirs[i] != NULL ; i++){
		*file_name = mkstr(standard_framework_dirs[i], name, NULL);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		    break;
		free(*file_name);
	    }
	}
}

/*
 * search_paths_for_lname() takes the argument to a -lx option and and trys to
 * open a file with the name libx.dylib or libx.a.  This routine is only used
 * when the -search_paths_first option is specified and -dynamic is in effect.
 * And looks for a file name ending in .dylib then .a in each directory before
 * looking in the next directory.  The list of the -L search directories and in
 * the standard directories are searched in that order.  If this is sucessful
 * it returns a pointer to the file name indirectly through file_name and the
 * open file descriptor indirectly through fd.
 */
static
void
search_paths_for_lname(
const char *lname_argument,
char **file_name,
int *fd)
{
    unsigned long i;

	*fd = -1;
	for(i = 0; i < nsearch_dirs ; i++){
	    search_path_for_lname(search_dirs[i], lname_argument, file_name,fd);
	    if(*fd != -1)
		return;
	}
	for(i = 0; i < nld_library_paths ; i++){
	    search_path_for_lname(ld_library_paths[i], lname_argument,
				  file_name, fd);
	    if(*fd != -1)
		return;
	}
	for(i = 0; standard_dirs[i] != NULL ; i++){
	    search_path_for_lname(standard_dirs[i],lname_argument,file_name,fd);
	    if(*fd != -1)
		return;
	}
}

/*
 * search_path_for_lname() takes the argument to a -lx option and and trys to
 * open a file with the name libx.dylib then libx.a in the specified directory
 * name.  This routine is only used when the -search_paths_first option is
 * specified and -dynamic is in effect.  If this is sucessful it returns a
 * pointer to the file name indirectly through file_name and the open file
 * descriptor indirectly through fd.
 */
static
void
search_path_for_lname(
const char *dir,
const char *lname_argument,
char **file_name,
int *fd)
{
	*file_name = mkstr(dir, "/", "lib", lname_argument, ".dylib", NULL);
	if((*fd = open(*file_name, O_RDONLY)) != -1)
	    return;
	free(*file_name);

	*file_name = mkstr(dir, "/", "lib", lname_argument, ".a", NULL);
	if((*fd = open(*file_name, O_RDONLY)) != -1)
	    return;
	free(*file_name);
}
#endif /* !defined(RLD) */

/*
 * pass1_fat() is passed a fat file to process.  The reason the swapping of
 * the fat headers is not done in place is so that when running native on a
 * little endian machine and the output is also little endian we don't want
 * cause the memory for the input files to be written just because of the
 * fat headers.
 */
static
void
pass1_fat(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool dylib_only,
enum bool bundle_loader,
enum bool force_weak)
{
    struct fat_header *fat_header;
#ifdef __LITTLE_ENDIAN__
    struct fat_header struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    struct fat_arch *fat_archs, *best_fat_arch;
    unsigned long previous_errors;
    char *arch_addr;
    unsigned long arch_size;
#if !(defined(KLD) && defined(__STATIC__))
    struct arch_flag host_arch_flag;
#endif /* !(defined(KLD) && defined(__STATIC__)) */
    const char *prev_arch;

	fat_header = (struct fat_header *)file_addr;
#ifdef __LITTLE_ENDIAN__
	struct_fat_header = *fat_header;
	swap_fat_header(&struct_fat_header, host_byte_sex);
	fat_header = &struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */

	if(sizeof(struct fat_header) + fat_header->nfat_arch *
	   sizeof(struct fat_arch) > file_size){
	    error("fat file: %s truncated or malformed (fat_arch structs "
		  "would extend past the end of the file)", file_name);
	    return;
	}

#ifdef __BIG_ENDIAN__
	fat_archs = (struct fat_arch *)(file_addr + sizeof(struct fat_header));
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	fat_archs = allocate(fat_header->nfat_arch * sizeof(struct fat_arch));
	memcpy(fat_archs, file_addr + sizeof(struct fat_header),
	       fat_header->nfat_arch * sizeof(struct fat_arch));
	swap_fat_arch(fat_archs, fat_header->nfat_arch, host_byte_sex);
#endif /* __LITTLE_ENDIAN__ */

	/*
	 * save the previous errors and only return out of here if something
	 * we do in here gets an error.
	 */
	previous_errors = errors;
	errors = 0;

	/* check the fat file */
	check_fat(file_name, file_size, fat_header, fat_archs, NULL, 0);
	if(errors)
	    goto pass1_fat_return;

	/* Now select an architecture out of the fat file to use if any */

	/*
	 * If the output file's cputype has been set then load the best fat_arch
	 * from it else it's an error.
	 */
	if(arch_flag.cputype != 0){
	    best_fat_arch = cpusubtype_findbestarch(
					arch_flag.cputype, arch_flag.cpusubtype,
					fat_archs, fat_header->nfat_arch);
	    if(best_fat_arch == NULL){
		if(no_arch_warnings == TRUE)
		    goto pass1_fat_return;
		if(arch_flag.name != NULL){
		    if(arch_errors_fatal == TRUE){
			error("fat file: %s does not contain an architecture "
			      "that matches the specified -arch flag: %s ",
			      file_name, arch_flag.name);
		    }
		    else
			warning("fat file: %s does not contain an architecture "
				"that matches the specified -arch flag: %s "
				"(file ignored)", file_name, arch_flag.name);
		}
		else{
		    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						         arch_flag.cpusubtype);
		    if(arch_errors_fatal == TRUE){
			error("fat file: %s does not contain an architecture "
			    "that matches the objects files (architecture %s) "
			    "previously loaded", file_name, prev_arch);
		    }
		    else
			warning("fat file: %s does not contain an architecture "
			    "that matches the objects files (architecture %s) "
			    "previously loaded (file ignored)", file_name,
			    prev_arch);
		}
		goto pass1_fat_return;
	    }
	    arch_addr = file_addr + best_fat_arch->offset;
	    arch_size = best_fat_arch->size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    if(arch_flag.name != NULL){
			error("fat file: %s (for architecture %s) is not a "
			      "dynamic shared library", file_name,
			      arch_flag.name);
		    }
		    else{
			prev_arch = get_arch_name_from_types(arch_flag.cputype,
							 arch_flag.cpusubtype);
			error("fat file: %s (for architecture %s) is not a "
			      "dynamic shared library", file_name, prev_arch);
		    }
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size,
			      base_name, TRUE, bundle_loader, force_weak);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only, bundle_loader, force_weak);
	    }
	    goto pass1_fat_return;
	}

#if !(defined(KLD) && defined(__STATIC__))
	/*
	 * If the output file's cputype has not been set so if this fat file
	 * has exactly one type in it then load that type.
	 */
	if(fat_header->nfat_arch == 1){
	    arch_addr = file_addr + fat_archs[0].offset;
	    arch_size = fat_archs[0].size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    error("fat file: %s (for architecture %s) is not a dynamic "
			  "shared library", file_name, get_arch_name_from_types(
			   fat_archs[0].cputype, fat_archs[0].cpusubtype));
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size, base_name, TRUE,
			      bundle_loader, force_weak);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only, bundle_loader, force_weak);
	    }
	    goto pass1_fat_return;
	}

	/*
	 * The output file's cputype has not been set and if this fat file has
	 * a best arch for the host's specific architecture type then load that
	 * type and set the output file's cputype to that.
	 */
	if(get_arch_from_host(NULL, &host_arch_flag) == 0)
	    fatal("can't determine the host architecture (specify an "
		  "-arch flag or fix get_arch_from_host() )");
	best_fat_arch = cpusubtype_findbestarch(
			    host_arch_flag.cputype, host_arch_flag.cpusubtype,
			    fat_archs, fat_header->nfat_arch);
	if(best_fat_arch != NULL){
	    arch_addr = file_addr + best_fat_arch->offset;
	    arch_size = best_fat_arch->size;
	    if(arch_size >= SARMAG &&
	       strncmp(arch_addr, ARMAG, SARMAG) == 0){
		if(dylib_only == TRUE){
		    error("fat file: %s (for architecture %s) is not a dynamic "
			  "shared library", file_name, get_arch_name_from_types(
			   best_fat_arch->cputype, best_fat_arch->cpusubtype));
		    goto pass1_fat_return;
		}
		pass1_archive(file_name, arch_addr, arch_size,
			      base_name, TRUE, bundle_loader, force_weak);
	    }
	    else{
		pass1_object(file_name, arch_addr, arch_size, base_name, TRUE,
			     dylib_only, bundle_loader, force_weak);
	    }
	    goto pass1_fat_return;
	}

	/*
	 * The output file's cputype has not been set and this fat file does not
	 * have only one architecture or has the host's family architecture so
	 * we are stuck not knowing what to load if anything from it.
	 */
	fatal("-arch flag must be specified (fat file: %s does not contain the "
	      "host architecture or just one architecture)", file_name);
#endif /* !(defined(KLD) && defined(__STATIC__)) */

pass1_fat_return:
	errors += previous_errors;
#ifdef __LITTLE_ENDIAN__
	free(fat_archs);
#endif /* __LITTLE_ENDIAN__ */
	return;
}

__private_extern__
void
check_fat(
char *file_name,
unsigned long file_size,
struct fat_header *fat_header,
struct fat_arch *fat_archs,
char *ar_name,
unsigned long ar_name_size)
{
    unsigned long i, j;

	if(fat_header->nfat_arch == 0){
	    if(ar_name != NULL)
		error("fat file: %s(%.*s) malformed (contains zero "
		      "architecture types)", file_name, (int)ar_name_size,
		      ar_name);
	    else
		error("fat file: %s malformed (contains zero architecture "
		      "types)", file_name);
	    return;
	}
	for(i = 0; i < fat_header->nfat_arch; i++){
	    if(fat_archs[i].offset + fat_archs[i].size > file_size){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) truncated or malformed (offset "
			  "plus size of cputype (%d) cpusubtype (%d) extends "
			  "past the end of the file)", file_name,
			  (int)ar_name_size, ar_name, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype);
		else
		    error("fat file: %s truncated or malformed (offset plus "
			  "size of cputype (%d) cpusubtype (%d) extends past "
			  "the end of the file)", file_name,
			  fat_archs[i].cputype, fat_archs[i].cpusubtype);
		return;
	    }
	    if(fat_archs[i].cputype == 0){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) fat_archs %lu cputype is zero (a "
			  "reserved value)", file_name, (int)ar_name_size,
			  ar_name, i);
		else
		    error("fat file: %s fat_archs %lu cputype is zero (a "
			  "reserved value)", file_name, i);
		return;
	    }
	    if(fat_archs[i].align > MAXSECTALIGN){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) align (2^%u) too large for "
			  "cputype (%d) cpusubtype (%d) (maximum 2^%d)",
			  file_name, (int)ar_name_size, ar_name,
			  fat_archs[i].align, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, MAXSECTALIGN);
		else
		    error("fat file: %s align (2^%u) too large for cputype "
			  "(%d) cpusubtype (%d) (maximum 2^%d)", file_name,
			  fat_archs[i].align, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, MAXSECTALIGN);
		return;
	    }
	    if(fat_archs[i].offset %
	       (1 << fat_archs[i].align) != 0){
		if(ar_name != NULL)
		    error("fat file: %s(%.*s) offset: %u for cputype (%d) "
			  "cpusubtype (%d)) not aligned on it's alignment "
			  "(2^%u)", file_name, (int)ar_name_size, ar_name,
			  fat_archs[i].offset, fat_archs[i].cputype,
			  fat_archs[i].cpusubtype, fat_archs[i].align);
		else
		    error("fat file: %s offset: %u for cputype (%d) "
			  "cpusubtype (%d)) not aligned on it's alignment "
			  "(2^%u)", file_name, fat_archs[i].offset,
			  fat_archs[i].cputype, fat_archs[i].cpusubtype,
			  fat_archs[i].align);
		return;
	    }
	}
	for(i = 0; i < fat_header->nfat_arch; i++){
	    for(j = i + 1; j < fat_header->nfat_arch; j++){
		if(fat_archs[i].cputype == fat_archs[j].cputype &&
		   fat_archs[i].cpusubtype == fat_archs[j].cpusubtype){
		    if(ar_name != NULL)
			error("fat file: %s(%.*s) contains two of the same "
			      "architecture (cputype (%d) cpusubtype (%d))",
			      file_name, (int)ar_name_size, ar_name,
			      fat_archs[i].cputype, fat_archs[i].cpusubtype);
		    else
			error("fat file: %s contains two of the same "
			      "architecture (cputype (%d) cpusubtype (%d))",
			      file_name, fat_archs[i].cputype,
			      fat_archs[i].cpusubtype);
		    return;
		}
	    }
	}
	return;
}

/*
 * This is an archive so conditionally load those objects that defined
 * currently undefined symbols and process archives with respect to the
 * -ObjC and -load_all flags if set.
 */
static
void
pass1_archive(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool from_fat_file,
enum bool bundle_loader,
enum bool force_weak)
{
    unsigned long i, j, offset;
#ifndef RLD
    unsigned long *loaded_offsets, nloaded_offsets;
    enum bool loaded_offset;
    struct ar_hdr *ar_hdr;
    unsigned long length;
    struct dynamic_library *p;
#endif /* !defined(RLD) */
    struct ar_hdr *symdef_ar_hdr;
    char *symdef_ar_name, *ar_name;
    unsigned long symdef_length, nranlibs, string_size, ar_name_size;
    struct ranlib *ranlibs, *ranlib;
    struct undefined_list *undefined;
    struct merged_symbol *merged_symbol;
    enum bool member_loaded;
    enum byte_sex toc_byte_sex;
    enum bool ld_trace_archive_printed;

#ifndef RLD
    unsigned long ar_size;
    struct fat_header *fat_header;
#ifdef __LITTLE_ENDIAN__
    struct fat_header struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    struct fat_arch *fat_archs, *best_fat_arch;
    char *arch_addr;
    unsigned long arch_size;
    struct arch_flag host_arch_flag;
    const char *prev_arch;

	arch_addr = NULL;
	arch_size = 0;
#endif /* !defined(RLD) */
#ifdef __MWERKS__
	{
	    enum bool dummy;
		dummy = base_name;
		dummy = from_fat_file;
	}
#endif

	ld_trace_archive_printed = FALSE;
	if(check_archive_arch(file_name, file_addr, file_size) == FALSE)
	    return;

	offset = SARMAG;
#ifndef RLD
	if(base_name)
	    fatal("base file of incremental link (argument of -A): %s should't "
		  "be an archive", file_name);
	if(bundle_loader)
	    fatal("-bundle_loader argument: %s should't be an archive",
		  file_name);

	/*
	 * If the flag to specifiy that all the archive members are to be
	 * loaded then load them all.
	 */
	if(archive_all == TRUE){
	    if(offset + sizeof(struct ar_hdr) > file_size){
		error("truncated or malformed archive: %s (archive header of "
		      "first member extends past the end of the file, can't "
		      "load from it)", file_name);
		return;
	    }
	    symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	    if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1,
		       sizeof(AR_EFMT1)-1) == 0)
		ar_name = file_addr + offset + sizeof(struct ar_hdr);
	    else
		ar_name = symdef_ar_hdr->ar_name;
	    if(strncmp(ar_name, SYMDEF, sizeof(SYMDEF)-1) == 0){
		offset += sizeof(struct ar_hdr);
		symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10);
		if(offset + symdef_length > file_size){
		    error("truncated or malformed archive: %s (table of "
			  "contents extends past the end of the file, can't "
			  "load from it)", file_name);
		    return;
		}
		offset += rnd(symdef_length, sizeof(short));
	    }
	    if(ld_trace_archives == TRUE && ld_trace_archive_printed == FALSE){
		char resolvedname[MAXPATHLEN];
		if(realpath(file_name, resolvedname) !=
		   NULL)
		    ld_trace("[Logging for XBS] Used static "
			     "archive: %s\n", resolvedname);
		else
		    ld_trace("[Logging for XBS] Used static "
			     "archive: %s\n", file_name);
		ld_trace_archive_printed = TRUE;
	    }
	    while(offset < file_size){
		if(offset + sizeof(struct ar_hdr) > file_size){
		    error("truncated or malformed archive: %s at offset %lu "
			  "(archive header of next member extends past the end "
			  "of the file, can't load from it)", file_name,offset);
		    return;
		}
		ar_hdr = (struct ar_hdr *)(file_addr + offset);
		if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
		    ar_name = file_addr + offset + sizeof(struct ar_hdr);
		    ar_name_size = strtoul(ar_hdr->ar_name +
					   sizeof(AR_EFMT1) - 1, NULL, 10);
		    i = ar_name_size;
		}
		else{
		    ar_name = ar_hdr->ar_name;
		    ar_name_size = 0;
		    i = size_ar_name(ar_hdr);
		}
		offset += sizeof(struct ar_hdr) + ar_name_size;
		ar_size = strtol(ar_hdr->ar_size, NULL, 10) - ar_name_size;
		if(offset + ar_size > file_size){
		    error("truncated or malformed archive: %s (member %.*s "
			  "extends past the end of the file, can't load from "
			  "it)", file_name, (int)i, ar_name);
		    return;
		}
		/*
		 * For -all_load we allow fat files as archive members, so if
		 * this is a fat file load the right architecture.
		 */
		fat_header = (struct fat_header *)(file_addr + offset);
		if(ar_size >= sizeof(struct fat_header) &&
#ifdef __BIG_ENDIAN__
		   fat_header->magic == FAT_MAGIC)
#endif
#ifdef __LITTLE_ENDIAN__
		   fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif
		{
#ifdef __LITTLE_ENDIAN__
		    struct_fat_header = *fat_header;
		    swap_fat_header(&struct_fat_header, host_byte_sex);
		    fat_header = &struct_fat_header;
#endif
		    if(sizeof(struct fat_header) + fat_header->nfat_arch *
		       sizeof(struct fat_arch) > ar_size){
			error("fat file: %s(%.*s) truncated or malformed "
			    "(fat_arch structs would extend past the end of "
			    "the file)", file_name, (int)i, ar_name);
			return;
		    }
#ifdef __BIG_ENDIAN__
		    fat_archs = (struct fat_arch *)(file_addr + offset +
				sizeof(struct fat_header));
#endif
#ifdef __LITTLE_ENDIAN__
		    fat_archs = allocate(fat_header->nfat_arch *
					 sizeof(struct fat_arch));
		    memcpy(fat_archs, file_addr + offset +
				      sizeof(struct fat_header),
			   fat_header->nfat_arch * sizeof(struct fat_arch));
		    swap_fat_arch(fat_archs, fat_header->nfat_arch,
				  host_byte_sex);
#endif
		    /* check the fat file */
		    check_fat(file_name, ar_size, fat_header, fat_archs,
			      ar_name, i);
		    if(errors)
			return;

		    /* Now select an architecture out of the fat file to use */

		    /*
		     * If the output file's cputype has been set then load the
		     * best fat_arch from it else it's an error.
		     */
		    if(arch_flag.cputype != 0){
			best_fat_arch = cpusubtype_findbestarch(
					arch_flag.cputype, arch_flag.cpusubtype,
					fat_archs, fat_header->nfat_arch);
			if(best_fat_arch == NULL){
			    if(no_arch_warnings == TRUE)
				return;
			    if(arch_flag.name != NULL){
				if(arch_errors_fatal == TRUE){
				    error("fat file: %s(%.*s) does not contain "
					"an architecture that matches the "
					"specified -arch flag: %s", file_name,
					(int)i, ar_name, arch_flag.name);
				}
				else
				    warning("fat file: %s(%.*s) does not "
					"contain an architecture that matches "
					"the specified -arch flag: %s (file "
					"ignored)", file_name, (int)i,
					ar_name, arch_flag.name);
			    }
			    else{
				prev_arch = get_arch_name_from_types(
				    arch_flag.cputype, arch_flag.cpusubtype);
				if(arch_errors_fatal == TRUE){
				    error("fat file: %s(%.*s) does not contain "
					"an architecture that matches the "
					"objects files (architecture %s) "
					"previously loaded", file_name,
					(int)i, ar_name, prev_arch);
				}
				else
				    warning("fat file: %s(%.*s) does not "
					"contain an architecture that matches "
					"the objects files (architecture %s) "
					"previously loaded (file ignored)",
					file_name, (int)i, ar_name, prev_arch);
			    }
			    return;
			}
			arch_addr = file_addr + offset + best_fat_arch->offset;
			arch_size = best_fat_arch->size;
		    }
		    /*
		     * The output file's cputype has not been set so if this
		     * fat file has exactly one type in it then load that type.
		     */
		    else if(fat_header->nfat_arch == 1){
			arch_addr = file_addr + offset + fat_archs[0].offset;
			arch_size = fat_archs[0].size;
		    }
		    /*
		     * The output file's cputype has not been set and if this
		     * fat file has a best arch for the host's specific
		     * architecture type then load that type and set the output
		     * file's cputype to that.
		     */
		    else{
			if(get_arch_from_host(NULL, &host_arch_flag) == 0)
			    fatal("can't determine the host architecture "
				"(specify an -arch flag or fix "
				"get_arch_from_host() )");
			best_fat_arch = cpusubtype_findbestarch(
			    host_arch_flag.cputype, host_arch_flag.cpusubtype,
			    fat_archs, fat_header->nfat_arch);
			if(best_fat_arch != NULL){
			    arch_addr = file_addr + offset +
					best_fat_arch->offset;
			    arch_size = best_fat_arch->size;
			}
			/*
			 * The output file's cputype has not been set and this
			 * fat file does not have only one architecture or has
			 * the host's family architecture so we are stuck not
			 * knowing what to load if anything from it.
			 */
			else
			    fatal("-arch flag must be specified (fat file: "
				"%s(%.*s) does not contain the host "
				"architecture or just one architecture)",
				file_name, (int)i, ar_name);
		    }
		    cur_obj = new_object_file();
		    cur_obj->file_name = file_name;
		    cur_obj->obj_addr = arch_addr;
		    cur_obj->obj_size = arch_size;
		    cur_obj->ar_hdr = ar_hdr;
		    cur_obj->ar_name = ar_name;
		    cur_obj->ar_name_size = i;
#ifdef __LITTLE_ENDIAN__
		    free(fat_archs);
#endif
		    goto down;
		}
		cur_obj = new_object_file();
		cur_obj->file_name = file_name;
		cur_obj->obj_addr = file_addr + offset;
		cur_obj->ar_hdr = ar_hdr;
		cur_obj->ar_name = ar_name;
		cur_obj->ar_name_size = i;
		cur_obj->obj_size = ar_size;
down:
		if(whyload){
		    print_obj_name(cur_obj);
		    print("loaded because of -all_load flag\n");
		}
		merge(FALSE, FALSE, force_weak);
		length = rnd(ar_size + ar_name_size, sizeof(short));
		offset = (offset - ar_name_size) + length;
	    }
	    return;
	}
#endif /* !defined(RLD) */

	/*
	 * If there are no undefined symbols then the archive doesn't have
	 * to be searched because archive members are only loaded to resolve
	 * undefined references unless the -ObjC flag is set.
	 */
	if(undefined_list.next == &undefined_list && archive_ObjC == FALSE)
	    return;

#ifdef RLD
	if(from_fat_file == FALSE)
	    new_archive_or_fat(file_name, file_addr, file_size);
#endif /* RLD */
	/*
	 * The file is an archive so get the symdef file
	 */
	if(offset == file_size){
	    warning("empty archive: %s (can't load from it)", file_name);
	    return;
	}
	if(offset + sizeof(struct ar_hdr) > file_size){
	    error("truncated or malformed archive: %s (archive header of first "
		  "member extends past the end of the file, can't load from "
		  " it)", file_name);
	    return;
	}
	symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
	    symdef_ar_name = file_addr + offset + sizeof(struct ar_hdr);
	    ar_name_size = strtoul(symdef_ar_hdr->ar_name +
				   sizeof(AR_EFMT1) - 1, NULL, 10);
	}
	else{
	    symdef_ar_name = symdef_ar_hdr->ar_name;
	    ar_name_size = 0;
	}
	offset += sizeof(struct ar_hdr) + ar_name_size;
	if(strncmp(symdef_ar_name, SYMDEF, sizeof(SYMDEF) - 1) != 0){
	    error("archive: %s has no table of contents, add one with "
		  "ranlib(1) (can't load from it)", file_name);
	    return;
	}
	symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10) - ar_name_size;
	/*
	 * The contents of a __.SYMDEF file is begins with a word giving the
	 * size in bytes of ranlib structures which immediately follow, and
	 * then continues with a string table consisting of a word giving the
	 * number of bytes of strings which follow and then the strings
	 * themselves.  So the smallest valid size is two words long.
	 */
	if(symdef_length < 2 * sizeof(long)){
	    error("size of table of contents for archive: %s too small to be "
		  "a valid table of contents (can't load from it)", file_name);
	    return;
	}
	if(offset + symdef_length > file_size){
	    error("truncated or malformed archive: %s (table of contents "
		  "extends past the end of the file, can't load from it)",
		  file_name);
	    return;
	}
	toc_byte_sex = get_toc_byte_sex(file_addr, file_size);
	nranlibs = *((long *)(file_addr + offset));
	if(toc_byte_sex != host_byte_sex)
	    nranlibs = SWAP_LONG(nranlibs);
	nranlibs = nranlibs / sizeof(struct ranlib);
	offset += sizeof(long);
	ranlibs = (struct ranlib *)(file_addr + offset);
	offset += sizeof(struct ranlib) * nranlibs;
	if(nranlibs == 0){
	    warning("empty table of contents: %s (can't load from it)",
		    file_name);
	    return;
	}
	if(offset - (2 * sizeof(long) + ar_name_size + sizeof(struct ar_hdr) +
		     SARMAG) > symdef_length){
	    error("truncated or malformed archive: %s (ranlib structures in "
		  "table of contents extends past the end of the table of "
		  "contents, can't load from it)", file_name);
	    return;
	}
	string_size = *((long *)(file_addr + offset));
	if(toc_byte_sex != host_byte_sex)
	    string_size = SWAP_LONG(string_size);
	offset += sizeof(long);
	bsearch_strings = file_addr + offset;
	offset += string_size;
	if(offset - (2 * sizeof(long) + ar_name_size + sizeof(struct ar_hdr) +
		     SARMAG) > symdef_length){
	    error("truncated or malformed archive: %s (ranlib strings in "
		  "table of contents extends past the end of the table of "
		  "contents, can't load from it)", file_name);
	    return;
	}
	if(symdef_length == 2 * sizeof(long)){
	    warning("empty table of contents for archive: %s (can't load from "
		    "it)", file_name);
	    return;
	}

	/*
	 * Check the string offset and the member offsets of the ranlib structs.
	 */
	if(toc_byte_sex != host_byte_sex)
	    swap_ranlib(ranlibs, nranlibs, host_byte_sex);
	for(i = 0; i < nranlibs; i++){
	    if(ranlibs[i].ran_un.ran_strx >= string_size){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "has bad string index, can't load from it)", file_name,i);
		return;
	    }
	    if(ranlibs[i].ran_off + sizeof(struct ar_hdr) >= file_size){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "has bad library member offset, can't load from it)",
		      file_name, i);
		return;
	    }
	    /*
	     * These should be on 4 byte boundaries because the maximum
	     * alignment of the header structures and relocation are 4 bytes.
	     * But this is has to be 2 bytes because that's the way ar(1) has
	     * worked historicly in the past.  Fortunately this works on the
	     * 68k machines but will have to change when this is on a real
	     * machine.
	     */
#if defined(mc68000) || defined(__i386__)
	    if(ranlibs[i].ran_off % sizeof(short) != 0){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "library member offset not a multiple of %lu bytes, can't"
		      " load from it)", file_name, i, sizeof(short));
		return;
	    }
#else
	    if(ranlibs[i].ran_off % sizeof(long) != 0){
		error("malformed table of contents in: %s (ranlib struct %lu "
		      "library member offset not a multiple of %lu bytes, can't"
		      " load from it)", file_name, i, sizeof(long));
		return;
	    }
#endif
	}

#ifndef RLD
	/*
	 * If the objective-C flag is set then load every thing in this archive
	 * that defines a symbol that starts with ".objc_class_name" or
	 * ".objc_category_name".
 	 */
	if(archive_ObjC == TRUE){
	    loaded_offsets = allocate(nranlibs * sizeof(unsigned long));
	    nloaded_offsets = 0;
	    for(i = 0; i < nranlibs; i++){
		/* See if this symbol is an objective-C symbol */
		if(strncmp(bsearch_strings + ranlibs[i].ran_un.ran_strx,
		           ".objc_class_name",
			   sizeof(".objc_class_name") - 1) != 0 &&
		   strncmp(bsearch_strings + ranlibs[i].ran_un.ran_strx,
		           ".objc_category_name",
			   sizeof(".objc_category_name") - 1) != 0)
		    continue;

		/* See if the object at this offset has already been loaded */
		loaded_offset = FALSE;
		for(j = 0; j < nloaded_offsets; j++){
		    if(loaded_offsets[j] == ranlibs[i].ran_off){
			loaded_offset = TRUE;
			break;
		    }
		}
		if(loaded_offset == TRUE)
		    continue;
		loaded_offsets[nloaded_offsets++] = ranlibs[i].ran_off;

		if(ld_trace_archives == TRUE &&
		   ld_trace_archive_printed == FALSE){
		    char resolvedname[MAXPATHLEN];
		    if(realpath(file_name, resolvedname) !=
		       NULL)
			ld_trace("[Logging for XBS] Used static "
				 "archive: %s\n", resolvedname);
		    else
			ld_trace("[Logging for XBS] Used static "
				 "archive: %s\n", file_name);
		    ld_trace_archive_printed = TRUE;
		}
		/*
		 * This is an objective-C symbol and the object file at this
		 * offset has not been loaded so load it.
		 */
		cur_obj = new_object_file();
		cur_obj->file_name = file_name;
		cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
					    ranlibs[i].ran_off);
		if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
			   sizeof(AR_EFMT1) - 1) == 0){
		    ar_name = file_addr + ranlibs[i].ran_off +
			      sizeof(struct ar_hdr);
		    ar_name_size = strtoul(cur_obj->ar_hdr->ar_name +
					   sizeof(AR_EFMT1) - 1, NULL, 10);
		    j = ar_name_size;
		}
		else{
		    ar_name = cur_obj->ar_hdr->ar_name;
		    ar_name_size = 0;
		    j = size_ar_name(cur_obj->ar_hdr);
		}
		cur_obj->ar_name = ar_name;
		cur_obj->ar_name_size = j;
		cur_obj->obj_addr = file_addr + ranlibs[i].ran_off +
				    sizeof(struct ar_hdr) + ar_name_size;
		cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
					   NULL, 10) - ar_name_size;
		if(ranlibs[i].ran_off + sizeof(struct ar_hdr) + ar_name_size +
				    cur_obj->obj_size > file_size){
		    error("malformed library: %s (member %.*s "
			  "extends past the end of the file, can't "
			  "load from it)", file_name, (int)j, ar_name);
		    return;
		}
		if(whyload){
		    print_obj_name(cur_obj);
		    print("loaded because of -ObjC flag to get symbol: %s\n",
			  bsearch_strings + ranlibs[i].ran_un.ran_strx);
		}
		merge(FALSE, FALSE, force_weak);
	    }
	    free(loaded_offsets);
	}

	/*
	 * If a dynamic library has been referenced then this archive library
	 * is put on the dynamic library search list and it will be loaded
	 * from with dynamic library search semantics.
	 */
	if(dynamic_libs != NULL){
	    p = add_dynamic_lib(SORTED_ARCHIVE, NULL, NULL);
	    if(strncmp(symdef_ar_name, SYMDEF_SORTED,
		       sizeof(SYMDEF_SORTED) - 1) == 0){
		p->type = SORTED_ARCHIVE;
/*
 * With the 4.1mach patch and 4.2mach release, we are putting the libgcc
 * functions into a static archive (libcc_dynamic.a) which we will link into
 * every image.  So this obscure warning message would then be seen on nearly
 * every link.  So the decision is to just remove the warning message.
 */
#ifdef notdef
		warning("archive library: %s appears after reference to "
			"dynamic shared library and will be searched as a "
			"dynamic shared library which only the first member "
			"that defines a symbol will ever be loaded", file_name);
#endif /* notdef */
	    }
	    else{
		p->type = UNSORTED_ARCHIVE;
#ifdef notdef
		warning("table of contents of library: %s not sorted slower "
			"link editing will result (use the ranlib(1) -s "
			"option), also library appears after reference to "
			"dynamic shared library and will be searched as a "
			"dynamic shared library which only the first member "
			"that defines a symbol will ever be loaded", file_name);
#endif /* notdef */
	    }
	    p->file_name = file_name;
	    p->file_addr = file_addr;
	    p->file_size = file_size;
	    p->nranlibs = nranlibs;
	    p->ranlibs = ranlibs;
	    p->ranlib_strings = bsearch_strings;
	    return;
	}
#endif /* !defined(RLD) */

	/*
	 * Two possible algorithms are used to determine which members from the
	 * archive are to be loaded.  The first is faster and requires the
	 * ranlib structures to be in sorted order (as produced by the ranlib(1)
	 * -s option).  The only case this can't be done is when more than one
	 * library member in the same archive defines the same symbol.  In this
	 * case ranlib(1) will never sort the ranlib structures but will leave
	 * them in the order of the archive so that the proper member that
	 * defines a symbol that is defined in more that one object is loaded.
	 */
	if(strncmp(symdef_ar_name, SYMDEF_SORTED,
		   sizeof(SYMDEF_SORTED) - 1) == 0){
	    /*
	     * Now go through the undefined symbol list and look up each symbol
	     * in the sorted ranlib structures looking to see it their is a
	     * library member that satisfies this undefined symbol.  If so that
	     * member is loaded and merge() is called.
	     */
	    for(undefined = undefined_list.next;
		undefined != &undefined_list;
		/* no increment expression */){
		/* If this symbol is no longer undefined delete it and move on*/
		if(undefined->merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
		   undefined->merged_symbol->nlist.n_value != 0){
		    undefined = undefined->next;
		    delete_from_undefined_list(undefined->prev);
		    continue;
		}
		ranlib = bsearch(undefined->merged_symbol->nlist.n_un.n_name,
			   ranlibs, nranlibs, sizeof(struct ranlib),
			   (int (*)(const void *, const void *))ranlib_bsearch);
		if(ranlib != NULL){

		    if(ld_trace_archives == TRUE &&
		       ld_trace_archive_printed == FALSE){
			char resolvedname[MAXPATHLEN];
			if(realpath(file_name, resolvedname) != NULL)
			    ld_trace("[Logging for XBS] Used "
				     "static archive: %s\n", resolvedname);
			else
			    ld_trace("[Logging for XBS] Used "
				     "static archive: %s\n", file_name);
			ld_trace_archive_printed = TRUE;
		    }

		    /* there is a member that defineds this symbol so load it */
		    cur_obj = new_object_file();
#ifdef RLD
		    cur_obj->file_name = allocate(strlen(file_name) + 1);
		    strcpy(cur_obj->file_name, file_name);
		    cur_obj->from_fat_file = from_fat_file;
#else
		    cur_obj->file_name = file_name;
#endif /* RLD */
		    cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
							ranlib->ran_off);
		    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
			       sizeof(AR_EFMT1)-1) == 0){
			ar_name = file_addr + ranlib->ran_off +
				  sizeof(struct ar_hdr);
			ar_name_size = strtoul(cur_obj->ar_hdr->ar_name +
					       sizeof(AR_EFMT1) - 1, NULL, 10);
			j = ar_name_size;
		    }
		    else{
			ar_name = cur_obj->ar_hdr->ar_name;
			ar_name_size = 0;
			j = size_ar_name(cur_obj->ar_hdr);
		    }
		    cur_obj->ar_name = ar_name;
		    cur_obj->ar_name_size = j;
		    cur_obj->obj_addr = file_addr + ranlib->ran_off +
					sizeof(struct ar_hdr) + ar_name_size;
		    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size, NULL,
					       10) - ar_name_size;
		    if(ranlib->ran_off + sizeof(struct ar_hdr) + ar_name_size +
						cur_obj->obj_size > file_size){
			error("malformed library: %s (member %.*s extends past "
			      "the end of the file, can't load from it)",
			      file_name, (int)j, ar_name);
			return;
		    }
		    if(whyload){
			print_obj_name(cur_obj);
			print("loaded to resolve symbol: %s\n",
			       undefined->merged_symbol->nlist.n_un.n_name);
		    }

		    merge(FALSE, FALSE, force_weak);

		    /* make sure this symbol got defined */
		    if(errors == 0 &&
		       undefined->merged_symbol->nlist.n_type == (N_UNDF|N_EXT)
		       && undefined->merged_symbol->nlist.n_value == 0){
			error("malformed table of contents in library: %s "
			      "(member %.*s did not define symbol %s)",
			      file_name, (int)j, ar_name,
			      undefined->merged_symbol->nlist.n_un.n_name);
		    }
		    undefined = undefined->next;
		    delete_from_undefined_list(undefined->prev);
		    continue;
		}
		undefined = undefined->next;
	    }
	}
	else{
	    /*
	     * The slower algorithm.  Lookup each symbol in the table of
	     * contents to see if is undefined.  If so that member is loaded
	     * and merge() is called.  A complete pass over the table of
	     * contents without loading a member terminates searching
	     * the library.  This could be made faster if this wrote on the
	     * ran_off to indicate the member at that offset was loaded and
	     * then it's symbol would be not be looked up on later passes.
	     * But this is not done because it would dirty the table of contents
	     * and cause the possibility of more swapping and if fast linking is
	     * wanted then the table of contents can be sorted.
	     */
	    member_loaded = TRUE;
	    while(member_loaded == TRUE && errors == 0){
		member_loaded = FALSE;
		for(i = 0; i < nranlibs; i++){
		    merged_symbol = lookup_symbol(bsearch_strings +
						   ranlibs[i].ran_un.ran_strx);
		    if(merged_symbol->name_len != 0){
			if(merged_symbol->nlist.n_type == (N_UNDF | N_EXT) &&
			   merged_symbol->nlist.n_value == 0){

			    if(ld_trace_archives == TRUE &&
			       ld_trace_archive_printed == FALSE){
				char resolvedname[MAXPATHLEN];
				if(realpath(file_name, resolvedname) != NULL)
				    ld_trace("[Logging for XBS] "
					     "Used static archive: %s\n",
					     resolvedname);
				else
				    ld_trace("[Logging for XBS] "
					     "Used static archive: %s\n",
					     file_name);
				ld_trace_archive_printed = TRUE;
			    }

			    /*
			     * This symbol is defined in this member so load it.
			     */
			    cur_obj = new_object_file();
#ifdef RLD
			    cur_obj->file_name = allocate(strlen(file_name) +1);
			    strcpy(cur_obj->file_name, file_name);
			    cur_obj->from_fat_file = from_fat_file;
#else
			    cur_obj->file_name = file_name;
#endif /* RLD */
			    cur_obj->ar_hdr = (struct ar_hdr *)(file_addr +
							ranlibs[i].ran_off);
			    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				       sizeof(AR_EFMT1)-1) == 0){
				ar_name = file_addr + ranlibs[i].ran_off +
					  sizeof(struct ar_hdr);
				ar_name_size =
					strtoul(cur_obj->ar_hdr->ar_name +
					        sizeof(AR_EFMT1) - 1, NULL, 10);
				j = ar_name_size;
			    }
			    else{
				ar_name = cur_obj->ar_hdr->ar_name;
				ar_name_size = 0;
				j = size_ar_name(cur_obj->ar_hdr);
			    }
			    cur_obj->ar_name = ar_name;
			    cur_obj->ar_name_size = j;
			    cur_obj->obj_addr = file_addr + ranlibs[i].ran_off +
					sizeof(struct ar_hdr) + ar_name_size;
			    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						       NULL, 10) - ar_name_size;
			    if(ranlibs[i].ran_off + sizeof(struct ar_hdr) +
			       ar_name_size + cur_obj->obj_size > file_size){
				error("malformed library: %s (member %.*s "
				      "extends past the end of the file, can't "
				      "load from it)", file_name, (int)j,
				      ar_name);
				return;
			    }
			    if(whyload){
				print_obj_name(cur_obj);
				print("loaded to resolve symbol: %s\n",
				       merged_symbol->nlist.n_un.n_name);
			    }

			    merge(FALSE, FALSE, force_weak);

			    /* make sure this symbol got defined */
			    if(errors == 0 &&
			       merged_symbol->nlist.n_type == (N_UNDF | N_EXT)
			       && merged_symbol->nlist.n_value == 0){
				error("malformed table of contents in library: "
				      "%s (member %.*s did not defined "
				      "symbol %s)", file_name, (int)j, ar_name,
				      merged_symbol->nlist.n_un.n_name);
			    }
			    /*
			     * Skip any other symbols that are defined in this
			     * member since it has just been loaded.
			     */
			    for(j = i; j + 1 < nranlibs; j++){
				if(ranlibs[i].ran_off != ranlibs[j + 1].ran_off)
				    break;
			    }
			    i = j;
			    member_loaded = TRUE;
			}
		    }
		}
	    }
	}
}

/*
 * get_toc_byte_sex() guesses the byte sex of the table of contents of the
 * library mapped in at the address, addr, of size, size based on the first
 * object file's bytesex.  If it can't figure it out, because the library has
 * no object file members or is malformed it will return UNKNOWN_BYTE_SEX.
 */
__private_extern__
enum byte_sex
get_toc_byte_sex(
char *addr,
uint32_t size)
{
     uint32_t magic;
     uint32_t ar_name_size;
     struct ar_hdr *ar_hdr;
     char *p;

	ar_hdr = (struct ar_hdr *)(addr + SARMAG);

	p = addr + SARMAG + sizeof(struct ar_hdr) +
	    rnd(strtoul(ar_hdr->ar_size, NULL, 10), sizeof(short));
	while(p + sizeof(struct ar_hdr) + sizeof(uint32_t) < addr + size){
	    ar_hdr = (struct ar_hdr *)p;
	    if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0)
		ar_name_size = strtoul(ar_hdr->ar_name + sizeof(AR_EFMT1) - 1,
				       NULL, 10);
	    else
		ar_name_size = 0;
	    p += sizeof(struct ar_hdr);
	    memcpy(&magic, p + ar_name_size, sizeof(uint32_t));
	    if(magic == MH_MAGIC || magic == MH_MAGIC_64)
		return(get_host_byte_sex());
	    else if(magic == SWAP_INT(MH_MAGIC) ||
		    magic == SWAP_INT(MH_MAGIC_64))
		return(get_host_byte_sex() == BIG_ENDIAN_BYTE_SEX ?
		       LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX);
	    p += rnd(strtoul(ar_hdr->ar_size, NULL, 10), sizeof(short));
	}
	return(UNKNOWN_BYTE_SEX);
}

/*
 * check_archive_arch() check the archive specified to see if it's architecture
 * does not match that of whats being loaded and if so returns FALSE.  Else it
 * returns TRUE and the archive should be attemped to be loaded from.  This is
 * done so that the obvious case of an archive that is the wrong architecture
 * is not reported an object file at a time but rather one error message is
 * printed.
 */
static
enum bool
check_archive_arch(
char *file_name,
char *file_addr,
unsigned long file_size)
{
    unsigned long offset, obj_size, length;
    struct ar_hdr *symdef_ar_hdr, *ar_hdr;
    unsigned long symdef_length, ar_name_size;
    char *obj_addr, *ar_name;
    struct mach_header mh;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    enum bool mixed_types;
    const char *new_arch, *prev_arch;

	cputype = 0;
	cpusubtype = 0;
	mixed_types = FALSE;

	offset = SARMAG;
	if(offset == file_size){
	    warning("empty archive: %s (can't load from it)", file_name);
	    return(FALSE);
	}
	if(offset + sizeof(struct ar_hdr) > file_size){
	    error("truncated or malformed archive: %s (archive header of "
		  "first member extends past the end of the file, can't "
		  "load from it)", file_name);
	    return(FALSE);
	}
	symdef_ar_hdr = (struct ar_hdr *)(file_addr + offset);
	if(strncmp(symdef_ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1)-1) == 0){
	    if(check_extend_format_1(file_name, symdef_ar_hdr,
				    file_size - (offset +sizeof(struct ar_hdr)),
				    &ar_name_size) == FALSE)
		return(FALSE);
	    ar_name = file_addr + offset + sizeof(struct ar_hdr);
	}
	else{
	    ar_name = symdef_ar_hdr->ar_name;
	    ar_name_size = 0;
	}
	if(strncmp(ar_name, SYMDEF, sizeof(SYMDEF) - 1) == 0){
	    offset += sizeof(struct ar_hdr);
	    symdef_length = strtol(symdef_ar_hdr->ar_size, NULL, 10);
	    if(offset + symdef_length > file_size){
		error("truncated or malformed archive: %s (table of "
		      "contents extends past the end of the file, can't "
		      "load from it)", file_name);
		return(FALSE);
	    }
	    offset += rnd(symdef_length, sizeof(short));
	}
	while(offset < file_size){
	    if(offset + sizeof(struct ar_hdr) > file_size){
		error("truncated or malformed archive: %s at offset %lu "
		      "(archive header of next member extends past the end "
		      "of the file, can't load from it)", file_name, offset);
		return(FALSE);
	    }
	    ar_hdr = (struct ar_hdr *)(file_addr + offset);
	    offset += sizeof(struct ar_hdr);
	    if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0){
		if(check_extend_format_1(file_name, ar_hdr, file_size - offset,
					&ar_name_size) == FALSE)
		    return(FALSE);
		ar_name = file_addr + offset;
	    }
	    else{
		ar_name = ar_hdr->ar_name;
		ar_name_size = 0;
	    }
	    offset += ar_name_size;
	    obj_addr = file_addr + offset;
	    obj_size = strtol(ar_hdr->ar_size, NULL, 10);
	    obj_size -= ar_name_size;
	    if(offset + obj_size > file_size){
		error("truncated or malformed archive: %s at offset %lu "
		      "(member extends past the end of the file, can't load "
		      "from it)", file_name, offset);
		return(FALSE);
	    }
	    if(obj_size >= sizeof(struct mach_header)){
		memcpy(&mh, obj_addr, sizeof(struct mach_header));
		if(mh.magic == SWAP_LONG(MH_MAGIC))
		    swap_mach_header(&mh, host_byte_sex);
		if(mh.magic == MH_MAGIC){
		    if(cputype == 0){
			cputype = mh.cputype;
			cpusubtype = mh.cpusubtype;
		    }
		    else if(cputype != mh.cputype){
			mixed_types = TRUE;
		    }
		}
	    }
	    length = rnd(obj_size, sizeof(short));
	    offset += length;
	}
	if(arch_flag.cputype != 0 && mixed_types == FALSE &&
	   arch_flag.cputype != cputype && cputype != 0){
	    if(no_arch_warnings == TRUE)
		return(FALSE);
	    new_arch = get_arch_name_from_types(cputype, cpusubtype);
	    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						 arch_flag.cpusubtype);
	    if(arch_flag.name != NULL)
		warning("%s archive's cputype (%d, architecture %s) does "
		    "not match cputype (%d) for specified -arch flag: "
		    "%s (can't load from it)", file_name, cputype, new_arch,
		    arch_flag.cputype, arch_flag.name);
	    else
		warning("%s archive's cputype (%d, architecture %s) does "
		    "not match cputype (%d architecture %s) of objects "
		    "files previously loaded (can't load from it)", file_name,
		    cputype, new_arch, arch_flag.cputype, prev_arch);
	    return(FALSE);
	}
	return(TRUE);
}

/*
 * check_extend_format_1() checks the archive header for extended format #1.
 */
static
enum bool
check_extend_format_1(
char *name,
struct ar_hdr *ar_hdr,
unsigned long size_left,
unsigned long *member_name_size)
{
    char *p, *endp, buf[sizeof(ar_hdr->ar_name)+1];
    unsigned long ar_name_size;

	*member_name_size = 0;

	buf[sizeof(ar_hdr->ar_name)] = '\0';
	memcpy(buf, ar_hdr->ar_name, sizeof(ar_hdr->ar_name));
	p = buf + sizeof(AR_EFMT1) - 1;
	if(isdigit(*p) == 0){
	    error("truncated or malformed archive: %s (ar_name: %.*s for "
		  "archive extend format #1 starts with non-digit)", name,
		  (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	    return(FALSE);
	}
	ar_name_size = strtoul(p, &endp, 10);
	if(ar_name_size == ULONG_MAX && errno == ERANGE){
	    error("truncated or malformed archive: %s (size in ar_name: %.*s "
		  "for archive extend format #1 overflows unsigned long)",
		  name, (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	    return(FALSE);
	}
	while(*endp == ' ' && *endp != '\0')
	    endp++;
	if(*endp != '\0'){
	    error("truncated or malformed archive: %s (size in ar_name: %.*s "
		  "for archive extend format #1 contains non-digit and "
		  "non-space characters)", name, (int)sizeof(ar_hdr->ar_name),
		  ar_hdr->ar_name);
	    return(FALSE);
	}
	if(ar_name_size > size_left){
	    error("truncated or malformed archive: %s (archive name of member "
		  "extends past the end of the file)", name);
	    return(FALSE);
	}
	*member_name_size = ar_name_size;
	return(TRUE);
}


/* This is an object file so it is unconditionally loaded */
static
void
pass1_object(
char *file_name,
char *file_addr,
unsigned long file_size,
enum bool base_name,
enum bool from_fat_file,
enum bool dylib_only,
enum bool bundle_loader,
enum bool force_weak)
{
#ifdef __MWERKS__
    enum bool dummy;
        dummy = base_name;
        dummy = from_fat_file;
#endif
	cur_obj = new_object_file();
#ifdef RLD
	cur_obj->file_name = allocate(strlen(file_name) + 1);
	strcpy(cur_obj->file_name, file_name);
	cur_obj->from_fat_file = from_fat_file;
#else
	cur_obj->file_name = file_name;
#endif /* RLD */
	cur_obj->obj_addr = file_addr;
	cur_obj->obj_size = file_size;
#ifndef RLD
	/*
	 * If this is the base file of an incremental link then set the
	 * pointer to the object file.
	 */
	if(base_name == TRUE)
	    base_obj = cur_obj;
#endif /* !defined(RLD) */

	merge(dylib_only, bundle_loader, force_weak);

#ifndef RLD
	/*
	 * If this is the base file of an incremental link then collect it's
	 * segments for overlap checking.
	 */
	if(base_name == TRUE)
	    collect_base_obj_segments();
#endif /* !defined(RLD) */
	return;
}

#ifndef RLD
/*
 * search_dynamic_libs() searches the libraries on the list of dynamic libraries
 * to resolve undefined symbols.  This is mostly done for static checking of
 * undefined symbols.  If an archive library appears after a dynamic library
 * on the static link line then it will be on the list of dynamic libraries
 * to search and be searched with dynamic library search semantics.  Dynamic
 * library search semantics here mimic what happens in the dynamic link editor.
 * For each undefined symbol a module that defines this symbol is searched for
 * throught the list of libraries to be searched.  That is for each symbol the
 * search starts at the begining of the list of libraries to be searched.  This
 * is unlike archive library search semantic when each library is search once
 * when encountered.
 */
__private_extern__
void
search_dynamic_libs(
void)
{
    struct dynamic_library *p, *q, *dep, *prev;
    unsigned long i, j, nmodules, size, ar_name_size;
    enum bool removed, some_images_setup;
    char *ar_name;

    struct mach_header *mh;
    struct load_command *lc;
    struct dylib_command *dl;
    struct segment_command *sg;

    struct undefined_list *undefined;
    enum bool found;
    struct dylib_table_of_contents *toc;
    struct ranlib *ranlib;

    enum bool bind_at_load_warning;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

    unsigned long library_ordinal;
    char *umbrella_install_name, *short_name, *has_suffix;
    enum bool is_framework, set_some_ordinals, non_set_ordinals;

    struct nlist *extdef_symbols, *extdef;

	/*
	 * If -twolevel_namespace is in effect assign the library ordinals to
	 * all of the dynamic libraries specified on the command line and
	 * already entered on the dynamic_libs list.  These are the primary
	 * libraries and only these ordinals will be recoded in the symbols.
	 * any dependent library added will use the library ordinal from the
	 * primary library that depended on it.  Then when an undefined symbol
	 * is referenced from a dynamic library then its library_ordinal is
	 * recorded in the symbol table nlist struct in the n_desc field with
	 * the macro SET_LIBRARY_ORDINAL(nlist.n_desc, library_ordinal).
	 */
	library_ordinal = 1;
	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB && p->dl->cmd == LC_ID_DYLIB){
		if(twolevel_namespace == TRUE){
		    if(library_ordinal > MAX_LIBRARY_ORDINAL)
			fatal("too many dynamic libraries used, maximum is: %d "
			      "when -twolevel_namespace is in effect",
			      MAX_LIBRARY_ORDINAL);
		}
		p->definition_obj->library_ordinal = library_ordinal;
		library_ordinal++;
		p->dependent_images =
		    allocate(p->definition_obj->nload_dylibs *
			     sizeof(struct dynamic_library *));
	    }
	    else if(p->type == BUNDLE_LOADER){
		p->definition_obj->library_ordinal = EXECUTABLE_ORDINAL;
	    }
	}

	/*
	 * The code in the following loop adds dynamic libraries to the search
	 * list and this ordering matches the library search order dyld uses
	 * for flat-level namespace images and lookups.
	 *
	 * But for two-level namespace lookups ld(1) and dyld lookup symbols in
	 * all the sub_images of a dynamic library when it is encountered in the
	 * search list.  So this can get different symbols in the flat and
	 * two-level namespace cases if there are multiple definitions of the
	 * same symbol in a framework's sub-images.  Or if there are multiple
	 * umbrella frameworks where their sub-frameworks have multiple
	 * definitions of the same symbol.
	 *
	 * Also to record two-level namespace hints ld(1) must exactly match the
	 * list of sub-images created for each library so it can assign the
	 * sub-image indexes and then record them.
	 */

	/*
	 * For dynamic libraries on the dynamic library search list that are
	 * from LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB or LC_REEXPORT_DYLIB
	 * references convert them into using a dylib file so it can be
	 * searched.  Or remove them from the search list if it can't be
	 * converted.  Then add all the dependent libraries for that library to 
	 * the search list.
	 */
	indirect_dylib = TRUE;
	prev = NULL;
	for(p = dynamic_libs; p != NULL; p = p->next){
	    removed = FALSE;
	    /*
	     * If this element on the dynamic library list comes from a
	     * LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB or LC_REEXPORT_DYLIB reference
	     * try to convert them into using a dylib file so it can be
	     * searched.  If not take it off the list.
	     */
	    if(p->type == DYLIB &&
	       (p->dl->cmd == LC_LOAD_DYLIB ||
		p->dl->cmd == LC_LOAD_WEAK_DYLIB ||
		p->dl->cmd == LC_REEXPORT_DYLIB)){
		if(open_dylib(p) == FALSE){
		    if(prebinding == TRUE){
			warning("prebinding disabled because dependent "
			    "library: %s can't be searched", p->dylib_file !=
			    NULL ? p->file_name : p->dylib_name);
			if(ld_trace_prebinding_disabled == TRUE)
			    ld_trace("[Logging for XBS] prebinding"
				     " disabled for %s because dependent library: "
				     "%s can't be searched\n", final_output !=
				     NULL ? final_output : outputfile,
				     p->dylib_file != NULL ? p->file_name :
				     p->dylib_name);
			prebinding = FALSE;
		    }
		    /* remove this dynamic library from the search list */
		    if(prev == NULL)
			dynamic_libs = p->next;
		    else
			prev->next = p->next;
		    removed = TRUE;
		}
		else{
		    p->dependent_images =
			allocate(p->definition_obj->nload_dylibs *
			         sizeof(struct dynamic_library *));
		}
	    }
	    /*
	     * If this element on the dynamic library list is a dylib file
	     * add all of it's dependent libraries to the list.
	     */
	    if(removed == FALSE &&
	       p->type == DYLIB && p->dl->cmd == LC_ID_DYLIB){
		mh = (struct mach_header *)p->definition_obj->obj_addr;
		if(prebinding == TRUE &&
		   (mh->flags & MH_PREBOUND) != MH_PREBOUND){
		    warning("prebinding disabled because dependent library: %s "
			"is not prebound", p->dylib_file != NULL ?
			p->file_name : p->dylib_name);
		    if(ld_trace_prebinding_disabled == TRUE)
			ld_trace("[Logging for XBS] prebinding "
				 "disabled for %s because dependent library: %s "
				 "is not prebound\n", final_output != NULL ?
				 final_output : outputfile, p->dylib_file !=
				 NULL ? p->file_name : p->dylib_name);
		    prebinding = FALSE;
		}
		lc = (struct load_command *)
			((char *)p->definition_obj->obj_addr +
			 sizeof(struct mach_header));
		for(i = 0; i < mh->ncmds; i++){
		    if(lc->cmd == LC_LOAD_DYLIB ||
		       lc->cmd == LC_LOAD_WEAK_DYLIB ||
		       lc->cmd == LC_REEXPORT_DYLIB){
			dl = (struct dylib_command *)lc;
			dep = add_dynamic_lib(DYLIB, dl, p->definition_obj);
			p->dependent_images[p->ndependent_images++] = dep;
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	    if(removed == FALSE)
		prev = p;
	}
	indirect_dylib = FALSE;

#ifdef DEBUG
	if(debug & (1 << 22)){
	    print("dynamic library search list and ordinals before sub "
		  "assignments:\n");
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    if(p->dylib_file != NULL)
			printf("  %s ordinal %lu (using file %s)\n",
			   p->dylib_name, p->definition_obj->library_ordinal,
			   p->file_name);
		    else{
			short_name = guess_short_name(p->dylib_name,
						&is_framework, &has_suffix);
			printf("  %s oridinal %lu\n", short_name != NULL ?
			       short_name : p->dylib_name,
			       p->definition_obj->library_ordinal);
		    }
		}
		else
		    printf("  %s (archive)\n", p->file_name);
	    }
	}
#endif /* DEBUG */

	/*
	 * If we are creating or using two-level namespace images now that all
	 * the indirect libraries have been found and opened go through and set
	 * the library_ordinal for those that are used indirectly via a
	 * sub-umbrella, sub-library or sub-framework.
	 */

	/*
	 * Set up the umbrella and library names (if any) for all dynamic
	 * libraries.
	 */
	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type != DYLIB)
		continue;
	    if(p->umbrella_name == NULL){
		umbrella_install_name = (char *)p->dl +
					p->dl->dylib.name.offset;
		short_name = guess_short_name(umbrella_install_name,
					      &is_framework, &has_suffix);
		if(short_name != NULL && is_framework == TRUE)
		    p->umbrella_name = short_name;
		else if(short_name != NULL && is_framework == FALSE)
		    p->library_name = short_name;
	    }
	}
	/*
	 * Now with all the indirect libraries loaded and the
	 * dependent_images set up set up the sub_images for any dynamic
	 * library that does not have this set up yet.  Since sub_images
	 * include sub_umbrellas and sub_librarys any dynamic library that
	 * has sub_umbrellas or sub_librarys must have their sub_umbrella
	 * and sub_librarys images set up first. To do this
	 * setup_sub_images() will return FALSE for a dynamic library that
	 * needed one of its sub_umbrellas or sub_libraries set up and we
	 * will loop here until we get a clean pass with no more dynamic
	 * libraries needing setup.
	 */
	do{
	    some_images_setup = FALSE;
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type != DYLIB)
		    continue;
		if(p->sub_images_setup == FALSE)
		    some_images_setup |= setup_sub_images(p);
	    }
	}while(some_images_setup == TRUE);
	/*
	 * Set the library ordinals for libraries that are not set.
	 */
	do{
	    /*
	     * Set the library ordinals of sub-frameworks who's umbrella
	     * framework has its library ordinal set.
	     */
	    set_some_ordinals = FALSE;
	    non_set_ordinals = FALSE;
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type != DYLIB)
		    continue;
		if(p->definition_obj->library_ordinal != 0 &&
		   p->umbrella_name != NULL)
		    set_some_ordinals |= set_sub_frameworks_ordinals(p);
		else
		    non_set_ordinals = TRUE;
	    }
	    /*
	     * If there are still some not set ordinals then set the dylibs
	     * that are sub-umbrella's or sub-libraries that are not set.
	     */
	    if(non_set_ordinals == TRUE){
		for(p = dynamic_libs; p != NULL; p = p->next){
		    if(p->type != DYLIB)
			continue;
		    if(p->definition_obj->library_ordinal == 0 &&
		       (p->umbrella_name != NULL ||
			p->library_name != NULL)){
			set_some_ordinals |=
			    set_sub_umbrella_sub_library_ordinal(p);
		    }
		}
	    }
	}while(set_some_ordinals == TRUE);

#ifdef DEBUG
	if(debug & (1 << 22)){
	    print("dynamic library search list and ordinals after sub "
		  "assignments:\n");
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    if(p->dylib_file != NULL)
			printf("  %s ordinal %lu isub_image %lu "
			       "(using file %s)\n", p->dylib_name,
			       p->definition_obj->library_ordinal,
			       p->definition_obj->isub_image, p->file_name);
		    else{
			short_name = guess_short_name(p->dylib_name,
						&is_framework, &has_suffix);
			printf("  %s oridinal %lu isub_image %lu\n",
			       short_name != NULL ?  short_name : p->dylib_name,
			       p->definition_obj->library_ordinal,
			       p->definition_obj->isub_image);
		    }
		    print("    ndependent_images = %lu\n",p->ndependent_images);
		    for(i = 0; i < p->ndependent_images; i++){
			dep = p->dependent_images[i];
			short_name = guess_short_name(dep->dylib_name,
						&is_framework, &has_suffix);
			printf("      [%lu] %s\n", i, short_name != NULL ?
			       short_name : dep->dylib_name);

		    }
		    print("    nsub_images = %lu\n",p->nsub_images);
		    for(i = 0; i < p->nsub_images; i++){
			dep = p->sub_images[i];
			short_name = guess_short_name(dep->dylib_name,
						&is_framework, &has_suffix);
			printf("      [%lu] %s\n", i, short_name != NULL ?
			       short_name : dep->dylib_name);

		    }
		}
		else
		    printf("  %s (archive)\n", p->file_name);
	    }
	}
#endif /* DEBUG */

	/*
	 * When building for two-level-namespace, remove from the search path
	 * indirect libraries that cannot be encoded in a library ordinal.
	 *
	 * It is unclear what the reason for this part of the logic:
	 *  && (force_flat_namespace == FALSE) && (filetype != MH_OBJECT)
	 * is here for and is likely wrong.
	 */
	if((twolevel_namespace == TRUE) &&
	   (force_flat_namespace == FALSE) &&
	   (filetype != MH_OBJECT)){
	    struct dynamic_library* last_library;
	    unsigned int total_library_count;
	    unsigned int direct_library_count;

	    last_library = NULL;
	    total_library_count = 0;
	    direct_library_count = 0;
	    for(p = dynamic_libs; p != NULL; p = p->next){
		total_library_count += 1;
		if((p->type == DYLIB) &&
		   (p->definition_obj->library_ordinal == 0)){
		    if(last_library == NULL)
			dynamic_libs = p->next;
		    else
			last_library->next = p->next;
		}
		else{
		    last_library = p;
		    direct_library_count += 1;
		}
	    }
	    if(direct_library_count != 0)
		indirect_library_ratio =
		    total_library_count / direct_library_count;
	}

	/*
	 * Go through the specified dynamic libraries setting up their table of
	 * contents data.
	 */
	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		if(p->dl->cmd == LC_ID_DYLIB){
		    p->tocs = (struct dylib_table_of_contents *)(
			       p->definition_obj->obj_addr +
			       p->definition_obj->dysymtab->tocoff);
		    p->strings = p->definition_obj->obj_addr +
				 p->definition_obj->symtab->stroff;
		    p->symbols = (struct nlist *)(
				  p->definition_obj->obj_addr +
				  p->definition_obj->symtab->symoff);
		    if(p->definition_obj->swapped)
	    		swap_nlist(p->symbols,
				   p->definition_obj->symtab->nsyms,
				   host_byte_sex);
		    p->mods = (struct dylib_module *)(
			       p->definition_obj->obj_addr +
			       p->definition_obj->dysymtab->modtaboff);
		    /*
		     * If prebinding an executable create a LC_PREBOUND_DYLIB
		     * load command for each dynamic library.  To allow the
		     * prebinding to be redone when the library has more
		     * modules the bit vector for the linked modules is padded
		     * to 125% of the number of modules with a minimum of 64
		     * modules.
		     */
		    if(prebinding == TRUE && filetype == MH_EXECUTE){
			nmodules = p->definition_obj->dysymtab->nmodtab +
				   (p->definition_obj->dysymtab->nmodtab >> 2);
			if(nmodules < 64)
			    nmodules = 64;
			size = sizeof(struct prebound_dylib_command) +
			       rnd(strlen(p->dylib_name) + 1, sizeof(long)) +
			       rnd(nmodules / 8, sizeof(long));
			p->pbdylib = allocate(size);
			memset(p->pbdylib, '\0', size);
			p->pbdylib->cmd = LC_PREBOUND_DYLIB;
			p->pbdylib->cmdsize = size;
			p->pbdylib->name.offset =
				sizeof(struct prebound_dylib_command);
			strcpy(((char *)p->pbdylib) +
				sizeof(struct prebound_dylib_command),
				p->dylib_name);
			p->pbdylib->nmodules =
				p->definition_obj->dysymtab->nmodtab;
			p->pbdylib->linked_modules.offset =
				sizeof(struct prebound_dylib_command) +
				rnd(strlen(p->dylib_name) + 1, sizeof(long));
			p->linked_modules = ((char *)p->pbdylib) +
				sizeof(struct prebound_dylib_command) +
				rnd(strlen(p->dylib_name) + 1, sizeof(long));
		    }
		}
	    }
	    if(p->type == BUNDLE_LOADER){
		p->strings = p->definition_obj->obj_addr +
			     p->definition_obj->symtab->stroff;
		p->symbols = (struct nlist *)(
			      p->definition_obj->obj_addr +
			      p->definition_obj->symtab->symoff);
		if(p->definition_obj->swapped)
		    swap_nlist(p->symbols,
			       p->definition_obj->symtab->nsyms,
			       host_byte_sex);
	    }
	}

	/*
	 * If we are going to attempt to prebind we save away the segments of
	 * the dylibs so they can be checked for overlap after layout.
	 */
	if(prebinding == TRUE){
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB && p->dl->cmd == LC_ID_DYLIB){
		    mh = (struct mach_header *)p->definition_obj->obj_addr;
		    lc = (struct load_command *)
			    ((char *)p->definition_obj->obj_addr +
			     sizeof(struct mach_header));
		    for(i = 0; i < mh->ncmds; i++){
			if(lc->cmd == LC_SEGMENT){
		    	    sg = (struct segment_command *)lc;
			    add_dylib_segment(sg, p->dylib_name,
				(mh->flags & MH_SPLIT_SEGS) == MH_SPLIT_SEGS);
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
	    }
	}

#ifdef DEBUG
	if(debug & (1 << 22)){
	    print("dynamic library search list:\n");
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    if(p->dylib_file != NULL)
			printf("\t%s (using file %s)\n", p->dylib_name,
			       p->file_name);
		    else{
			short_name = guess_short_name(p->dylib_name,
						&is_framework, &has_suffix);
			printf("\t%s\n", short_name != NULL ?
			       short_name : p->dylib_name);
		    }
		}
		else
		    printf("\t%s (archive)\n", p->file_name);
	    }
	}
#endif
	if(ld_trace_dylibs == TRUE){
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB){
		    char resolvedname[MAXPATHLEN];
		    if(realpath(p->definition_obj->file_name, resolvedname) !=
		       NULL)
			ld_trace("[Logging for XBS] Used dynamic "
				 "library: %s\n", resolvedname);
		    else
		        ld_trace("[Logging for XBS] Used dynamic "
				 "library: %s\n", p->definition_obj->file_name);
		}
	    }
	}

	/*
	 * Now go through the undefined symbol list and look up each symbol
	 * in the table of contents of each library on the dynamic library
	 * search list.
	 */
	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    /* no increment expression */){

	    /*
	     * If this symbol is a twolevel_reference placed on the undefined
	     * list by merge_dylib_module_symbols() in symbols.c then we
	     * determine which dylib module needs to be loaded and if it is not
	     * yet loaded we load it.  Note the merged symbol pointed to by
	     * this undefined entry is a fake and not entered into the symbol
	     * symbol table.  This merged symbol's nlist is a copy of the nlist
	     * from the referencing_library.
	     */
	    if(undefined->merged_symbol->twolevel_reference == TRUE){
		library_ordinal = GET_LIBRARY_ORDINAL(
				    undefined->merged_symbol->nlist.n_desc);
		if(library_ordinal == SELF_LIBRARY_ORDINAL)
		    p = undefined->merged_symbol->referencing_library;
		/*
		 * Note that if library_ordinal was DYNAMIC_LOOKUP_ORDINAL then
		 * merge_dylib_module_symbols() in symbols.c would not have
		 * set the twolevel_reference field to TRUE in the merged_symbol
		 * and if we get here it with this it is an internal error.
		 */
		else if(library_ordinal == DYNAMIC_LOOKUP_ORDINAL)
		    fatal("internal error: search_dynamic_libs() with a "
			  "merged_symbol (%s) on the undefined list with "
			  "twolevel_reference == TRUE and library_ordinal == "
			  "DYNAMIC_LOOKUP_ORDINAL", undefined->merged_symbol->
			  nlist.n_un.n_name);
		else
		    p = undefined->merged_symbol->referencing_library->
			    dependent_images[library_ordinal - 1];
		q = p;
		/*
		 * This could be a dylib that was missing so its dynamic_library
		 * struct will be just an LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB,
		 * LC_REEXPORT_DYLIB command and a name with no strings,
		 * symbols, sub_images, etc.
	 	 */
		if(p->dl->cmd == LC_LOAD_DYLIB ||
		   p->dl->cmd == LC_LOAD_WEAK_DYLIB ||
		   p->dl->cmd == LC_REEXPORT_DYLIB)
		    goto undefined_twolevel_reference;
		bsearch_strings = q->strings;
		bsearch_symbols = q->symbols;
		toc = bsearch(undefined->merged_symbol->nlist.n_un.n_name,
			      q->tocs, q->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		if(toc == NULL){
		    for(i = 0; toc == NULL && i < p->nsub_images; i++){
			q = p->sub_images[i];
			if(q->dl->cmd == LC_LOAD_DYLIB ||
			   q->dl->cmd == LC_LOAD_WEAK_DYLIB ||
			   q->dl->cmd == LC_REEXPORT_DYLIB)
			    break;
			bsearch_strings = q->strings;
			bsearch_symbols = q->symbols;
			toc = bsearch(undefined->merged_symbol->
				  nlist.n_un.n_name, q->tocs,
				  q->definition_obj->dysymtab->ntoc,
				  sizeof(struct dylib_table_of_contents),
				  (int (*)(const void *, const void *))
				    dylib_bsearch);
		    }
		}
		if(toc != NULL){
		    /*
		     * There is a module that defineds this symbol so see if it
		     * has been loaded and if not load it.
		     */
		    if(is_dylib_module_loaded(q->mods + toc->module_index) ==
		       FALSE){
			cur_obj = new_object_file();
			*cur_obj = *(q->definition_obj);
			cur_obj->dylib_module = q->mods + toc->module_index;
			if(q->linked_modules != NULL)
			    q->linked_modules[toc->module_index / 8] |=
				1 << toc->module_index % 8;
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s ",
			       undefined->merged_symbol->nlist.n_un.n_name);
			    dep = undefined->merged_symbol->referencing_library;
			    if(dep->umbrella_name != NULL)
				short_name = dep->umbrella_name;
			    else if(dep->library_name != NULL)
				short_name = dep->library_name;
			    else
				short_name = dep->dylib_name;
			    print("referenced from %s\n", short_name);
			}
			merge_dylib_module_symbols(q);
			/*
			 * We would like to make sure this symbol got defined
			 * from this dylib module.  But since this is a
			 * two-level reference the symbol in this dylib module
			 * may or may not be used by the output file being
			 * created (the symbol entered in the merged symbol
			 * table not this fake one).
			 */

			/*
			 * Since something from this dynamic library is being
			 * used, if there is a library initialization routine
			 * make sure that the module that defines it is loaded.
			 */
			load_init_dylib_module(q);
		    }
		    /*
		     * If the -Y flag is not set free the memory for this fake
		     * merged_symbol which was for a two-level reference, move
		     * to the next undefined, and take this off the undefined
		     * list.
		     */
		    if(!Yflag)
			free(undefined->merged_symbol);
		    undefined = undefined->next;
		    delete_from_undefined_list(undefined->prev);
		}
		else{
		    /*
		     * The library expected to define this symbol did not define
		     * it so it must remain undefined and should result in an
		     * undefined symbol error.  In process_undefineds() in
		     * symbols.c it loops over the undefined list looking for
		     * these fake merged_symbols.
		     */
undefined_twolevel_reference:
		    undefined = undefined->next;
		}
		continue;
	    }

	    /* If this symbol is no longer undefined delete it and move on*/
	    if(undefined->merged_symbol->nlist.n_type != (N_UNDF | N_EXT) ||
	       undefined->merged_symbol->nlist.n_value != 0){
		undefined = undefined->next;
		delete_from_undefined_list(undefined->prev);
		continue;
	    }

	    /*
	     * If -twolevel_namespace is in effect then when each dynamic
	     * library is seen all of its sub-images are searched at that
	     * point.  So to avoid searching a dynamic library more than once
	     * per symbol the twolevel_searched field is set to TRUE when
	     * searched and the library is skipped if it is encountered again
	     * when looking for the same symbol.
	     */
	    if(twolevel_namespace == TRUE){
		for(p = dynamic_libs; p != NULL; p = p->next)
		    p->twolevel_searched = FALSE;
	    }
	    found = FALSE;
	    for(p = dynamic_libs; p != NULL && found == FALSE; p = p->next){
		switch(p->type){

		case DYLIB:
		    if(twolevel_namespace == TRUE &&
		       p->twolevel_searched == TRUE)
			break;
		    /*
		     * This could be a dylib that was missing so its
		     * dynamic_library struct will be just an LC_LOAD_DYLIB,
		     * LC_LOAD_WEAK_DYLIB or LC_REEXPORT_DYLIB command and a
		     * name with no strings, symbols, sub_images, etc.
		     */
		    if(p->dl->cmd == LC_LOAD_DYLIB ||
		       p->dl->cmd == LC_LOAD_WEAK_DYLIB ||
		       p->dl->cmd == LC_REEXPORT_DYLIB)
			break;
		    q = p;
		    bsearch_strings = q->strings;
		    bsearch_symbols = q->symbols;
		    toc = bsearch(undefined->merged_symbol->nlist.n_un.n_name,
				  q->tocs, q->definition_obj->dysymtab->ntoc,
				  sizeof(struct dylib_table_of_contents),
				  (int (*)(const void *, const void *))
				    dylib_bsearch);
		    if(toc == NULL && twolevel_namespace == TRUE){
			q->twolevel_searched = TRUE;
			for(i = 0; toc == NULL && i < p->nsub_images; i++){
			    q = p->sub_images[i];
			    q->twolevel_searched = TRUE;
			    if(q->dl->cmd == LC_LOAD_DYLIB ||
			       q->dl->cmd == LC_LOAD_WEAK_DYLIB ||
			       q->dl->cmd == LC_REEXPORT_DYLIB)
				break;
			    /*
			     * Don't search images that cannot be two level
			     * encoded.
			     *
			     * This logic seems questionable that this is not
			     * conditional on building a two-level namespace
			     * image.
			     */
			    if(q->definition_obj->library_ordinal == 0)
				continue;
			    bsearch_strings = q->strings;
			    bsearch_symbols = q->symbols;
			    toc = bsearch(undefined->merged_symbol->
				      nlist.n_un.n_name, q->tocs,
				      q->definition_obj->dysymtab->ntoc,
				      sizeof(struct dylib_table_of_contents),
				      (int (*)(const void *, const void *))
					dylib_bsearch);
			}
		    }
		    if(toc != NULL){
			/*
			 * There is a module that defineds this symbol so
			 * load it.
			 */
			cur_obj = new_object_file();
			*cur_obj = *(q->definition_obj);
			cur_obj->dylib_module = q->mods + toc->module_index;
			if(q->linked_modules != NULL)
			    q->linked_modules[toc->module_index / 8] |=
				1 << toc->module_index % 8;
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s\n",
			       undefined->merged_symbol->nlist.n_un.n_name);
			}
			merge_dylib_module_symbols(q);
			/* make sure this symbol got defined */
			if(errors == 0 &&
			   undefined->merged_symbol->nlist.n_type ==
			    (N_UNDF|N_EXT)
			   && undefined->merged_symbol->nlist.n_value == 0){
			    error("malformed table of contents in library: "
			       "%s (module %s did not define symbol %s)",
			       cur_obj->file_name, bsearch_strings +
			       cur_obj->dylib_module->module_name,
			       undefined->merged_symbol->nlist.n_un.n_name);
			}
			if(twolevel_namespace == TRUE)
			    undefined->merged_symbol->itoc = toc - q->tocs;
			undefined = undefined->next;
			delete_from_undefined_list(undefined->prev);
			found = TRUE;

			/*
			 * Since something from this dynamic library is being
			 * used, if there is a library initialization routine
			 * make sure that the module that defines it is loaded.
			 */
			load_init_dylib_module(q);
		    }
		    break;

		case SORTED_ARCHIVE:
		    bsearch_strings = p->ranlib_strings;
		    ranlib = bsearch(undefined->merged_symbol->
				     nlist.n_un.n_name, p->ranlibs, p->nranlibs,
				     sizeof(struct ranlib),
				     (int (*)(const void *, const void *))
					ranlib_bsearch);
		    if(ranlib != NULL){
			if(ld_trace_archives == TRUE &&
			   p->ld_trace_archive_printed == FALSE){
			    char resolvedname[MAXPATHLEN];
			    if(realpath(p->file_name, resolvedname) != NULL)
				ld_trace("[Logging for XBS] Used "
					 "static archive: %s\n", resolvedname);
			    else
				ld_trace("[Logging for XBS] Used "
					 "static archive: %s\n", p->file_name);
			    p->ld_trace_archive_printed = TRUE;
			}
			/*
			 * There is a member that defineds this symbol so
			 * load it.
			 */
			cur_obj = new_object_file();
			cur_obj->file_name = p->file_name;
			cur_obj->ar_hdr = (struct ar_hdr *)(p->file_addr +
							    ranlib->ran_off);
			if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				   sizeof(AR_EFMT1)-1) == 0){
			    ar_name = p->file_addr + ranlib->ran_off +
				      sizeof(struct ar_hdr);
			    ar_name_size =
				    strtoul(cur_obj->ar_hdr->ar_name +
					    sizeof(AR_EFMT1) - 1, NULL, 10);
			    j = ar_name_size;
			}
			else{
			    ar_name = cur_obj->ar_hdr->ar_name;
			    ar_name_size = 0;
			    j = size_ar_name(cur_obj->ar_hdr);
			}
			cur_obj->ar_name = ar_name;
			cur_obj->ar_name_size = j;
			cur_obj->obj_addr = p->file_addr +
					    ranlib->ran_off +
					    sizeof(struct ar_hdr) +
					    ar_name_size;
			cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						   NULL, 10) - ar_name_size;
			if(ranlib->ran_off + sizeof(struct ar_hdr) +
			   ar_name_size + cur_obj->obj_size > p->file_size){
			    error("malformed library: %s (member %.*s extends "
				  "past the end of the file, can't load from "
				  "it)",p->file_name, (int)j, ar_name);
			    return;
			}
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s\n",
				   undefined->merged_symbol->nlist.n_un.n_name);
			}

			merge(FALSE, FALSE, FALSE);

			/* make sure this symbol got defined */
			if(errors == 0 &&
			   undefined->merged_symbol->nlist.n_type ==
			   (N_UNDF|N_EXT)
			   && undefined->merged_symbol->nlist.n_value == 0){
			    error("malformed table of contents in library: %s "
				  "(member %.*s did not define symbol %s)",
				  p->file_name, (int)j, ar_name,
				  undefined->merged_symbol->nlist.n_un.n_name);
			}
			undefined = undefined->next;
			delete_from_undefined_list(undefined->prev);
			found = TRUE;
		    }
		    break;

		case UNSORTED_ARCHIVE:
		    for(i = 0; found == FALSE && i < p->nranlibs; i++){
			if(strcmp(undefined->merged_symbol->nlist.n_un.n_name,
				  p->ranlib_strings +
				  p->ranlibs[i].ran_un.ran_strx) == 0){
			    if(ld_trace_archives == TRUE &&
			       p->ld_trace_archive_printed == FALSE){
				char resolvedname[MAXPATHLEN];
				if(realpath(p->file_name, resolvedname) != NULL)
				    ld_trace("[Logging for XBS] "
					     "Used static archive: %s\n",
					     resolvedname);
				else
				    ld_trace("[Logging for XBS] "
					     "Used static archive: %s\n",
					     p->file_name);
				p->ld_trace_archive_printed = TRUE;
			    }
			    /*
			     * There is a member that defineds this symbol so
			     * load it.
			     */
			    cur_obj = new_object_file();
			    cur_obj->file_name = p->file_name;
			    cur_obj->ar_hdr = (struct ar_hdr *)(p->file_addr +
							p->ranlibs[i].ran_off);
			    if(strncmp(cur_obj->ar_hdr->ar_name, AR_EFMT1,
				       sizeof(AR_EFMT1)-1) == 0){
				ar_name = p->file_addr + p->ranlibs[i].ran_off +
					  sizeof(struct ar_hdr);
				ar_name_size =
					strtoul(cur_obj->ar_hdr->ar_name +
					        sizeof(AR_EFMT1) - 1, NULL, 10);
				j = ar_name_size;
			    }
			    else{
				ar_name = cur_obj->ar_hdr->ar_name;
				ar_name_size = 0;
				j = size_ar_name(cur_obj->ar_hdr);
			    }
			    cur_obj->ar_name = ar_name;
			    cur_obj->ar_name_size = j;
			    cur_obj->obj_addr = p->file_addr +
						p->ranlibs[i].ran_off +
						sizeof(struct ar_hdr) +
						ar_name_size;
			    cur_obj->obj_size = strtol(cur_obj->ar_hdr->ar_size,
						       NULL, 10) - ar_name_size;
			    if(p->ranlibs[i].ran_off + sizeof(struct ar_hdr) +
			       ar_name_size + cur_obj->obj_size > p->file_size){
				error("malformed library: %s (member %.*s "
				      "extends past the end of the file, can't "
				      "load from it)", p->file_name, (int)j,
				      ar_name);
				return;
			    }
			    if(whyload){
				print_obj_name(cur_obj);
				print("loaded to resolve symbol: %s\n",
				   undefined->merged_symbol->nlist.n_un.n_name);
			    }

			    merge(FALSE, FALSE, FALSE);

			    /* make sure this symbol got defined */
			    if(errors == 0 &&
			       undefined->merged_symbol->nlist.n_type ==
			       (N_UNDF|N_EXT)
			       && undefined->merged_symbol->nlist.n_value == 0){
				error("malformed table of contents in library: "
				   "%s (member %.*s did not define symbol %s)",
				   p->file_name, (int)j, ar_name,
				   undefined->merged_symbol->nlist.n_un.n_name);
			    }
			    undefined = undefined->next;
			    delete_from_undefined_list(undefined->prev);
			    found = TRUE;
			}
		    }
		    break;

		case BUNDLE_LOADER:
		    bsearch_strings = p->definition_obj->obj_addr +
			      p->definition_obj->symtab->stroff;
		    extdef_symbols = (struct nlist *)(
				  p->definition_obj->obj_addr +
				  p->definition_obj->symtab->symoff) +
				  p->definition_obj->dysymtab->iextdefsym;
		    extdef =bsearch(undefined->merged_symbol->nlist.n_un.n_name,
				  extdef_symbols,
				  p->definition_obj->dysymtab->nextdefsym,
				  sizeof(struct nlist),
				  (int (*)(const void *, const void *))
				    nlist_bsearch);
		    if(extdef != NULL){
			/*
			 * There is a external symbol in the bundle loader that
			 * defineds this symbol so load it.
			 */
			cur_obj = p->definition_obj;
			if(whyload){
			    print_obj_name(cur_obj);
			    print("loaded to resolve symbol: %s\n",
			       undefined->merged_symbol->nlist.n_un.n_name);
			}

			merge_bundle_loader_symbols(p);

			/* make sure this symbol got defined */
			if(errors == 0 &&
			   undefined->merged_symbol->nlist.n_type ==
			    (N_UNDF|N_EXT)
			   && undefined->merged_symbol->nlist.n_value == 0){
			    error("malformed external defined symbols of "
			       "-bundle_loader: %s (it did not define symbol "
			       "%s)", cur_obj->file_name,
			       undefined->merged_symbol->nlist.n_un.n_name);
			}
			undefined = undefined->next;
			delete_from_undefined_list(undefined->prev);
			found = TRUE;
		    }
		    break;
		}
	    }
	    if(found == FALSE)
		undefined = undefined->next;
	}

	/*
	 * Check to see all merged symbols coming from dynamic libraries
	 * came from the first one defining the symbol.  If not issue a warning
	 * suggesting -bind_at_launch be used.
	 */
	if(filetype == MH_EXECUTE && bind_at_load == FALSE){
	    bind_at_load_warning = FALSE;
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
		merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if(merged_symbol->defined_in_dylib != TRUE)
			continue;
		    if(merged_symbol->coalesced_defined_in_dylib == TRUE)
			continue;
		    if(twolevel_namespace == TRUE){
			for(p = dynamic_libs; p != NULL; p = p->next)
			    p->twolevel_searched = FALSE;
		    }
		    for(p = dynamic_libs; p != NULL; p = p->next){
			if(p->type == DYLIB){
			    if(twolevel_namespace == TRUE &&
			       p->twolevel_searched == TRUE)
				break;
			    q = p;
			    if(q->dl->cmd == LC_LOAD_DYLIB ||
			       q->dl->cmd == LC_LOAD_WEAK_DYLIB ||
			       q->dl->cmd == LC_REEXPORT_DYLIB)
				break;
			    bsearch_strings = q->strings;
			    bsearch_symbols = q->symbols;
			    toc = bsearch(merged_symbol->nlist.n_un.n_name,
				      q->tocs,q->definition_obj->dysymtab->ntoc,
				      sizeof(struct dylib_table_of_contents),
				      (int (*)(const void *, const void *))
					dylib_bsearch);
			    if(toc == NULL && twolevel_namespace == TRUE){
				q->twolevel_searched = TRUE;
				for(j = 0;
				    toc == NULL && j < p->nsub_images;
				    j++){
				    q = p->sub_images[j];
				    q->twolevel_searched = TRUE;
				    if(q->dl->cmd == LC_LOAD_DYLIB ||
				       q->dl->cmd == LC_LOAD_WEAK_DYLIB ||
				       q->dl->cmd == LC_REEXPORT_DYLIB)
					break;
				    bsearch_strings = q->strings;
				    bsearch_symbols = q->symbols;
				    toc = bsearch(merged_symbol->
				      nlist.n_un.n_name, q->tocs,
				      q->definition_obj->dysymtab->ntoc,
				      sizeof(struct dylib_table_of_contents),
				      (int (*)(const void *, const void *))
					dylib_bsearch);
				}
			    }
			    if(toc != NULL){
				if(merged_symbol->definition_object->obj_addr
				   == q->definition_obj->obj_addr)
				    break;
				if(merged_symbol->definition_object->obj_addr
				   != q->definition_obj->obj_addr){
				    if(bind_at_load_warning == FALSE){
					warning("suggest use of -bind_at_load, "
						"as lazy binding may result in "
						"errors or different symbols "
						"being used");
					bind_at_load_warning = TRUE;
				    }
				    printf("symbol %s used from dynamic "
					"library %s(%s) not from earlier "
					"dynamic library %s(%s)\n",
					merged_symbol->nlist.n_un.n_name,
					merged_symbol->definition_object->
					    file_name,
					(merged_symbol->definition_object->
					    obj_addr + merged_symbol->
					    definition_object->symtab->stroff) +
					merged_symbol->definition_object->
					    dylib_module->module_name,
					q->dylib_name,
					bsearch_strings + (q->mods +
					    toc->module_index)->module_name);
				}
			    }
			}
		    }
		}
	    }
	}

	/*
	 * If the -prebind_all_twolevel_modules is specified and prebinding is
	 * is still enabled and the output is an executable and we are not
	 * building with -force_flat_namespace then change the bit vectors in
	 * the two-level dynamic libraries to mark all modules as used.
	 */
	if(prebind_all_twolevel_modules == TRUE && prebinding == TRUE &&
	   filetype == MH_EXECUTE && force_flat_namespace == FALSE){
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB &&
		   p->dl->cmd == LC_ID_DYLIB &&
		   p->linked_modules != NULL){
		    mh = (struct mach_header *)(p->definition_obj->obj_addr);
		    if((mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
			nmodules = p->definition_obj->dysymtab->nmodtab;
			for(i = 0; i < nmodules; i++){
			    p->linked_modules[i / 8] |= 1 << i % 8;
			}
		    }
		}
	    }
	}
}

/*
 * load_init_dylib_module() is passed a pointer to a dynamic library that just
 * had a module loaded.  Since something from this dynamic library is being used
 * if there is a library initialization routine make sure that the module that
 * defines it is loaded.
 */
static
void
load_init_dylib_module(
struct dynamic_library *q)
{
	if(q->definition_obj->rc != NULL &&
	   q->definition_obj->init_module_loaded == FALSE){
	    if(is_dylib_module_loaded(q->mods +
		  q->definition_obj->rc->init_module) == FALSE){
		cur_obj = new_object_file();
		*cur_obj = *(q->definition_obj);
		cur_obj->dylib_module = q->mods +
		    q->definition_obj->rc->init_module;
		if(q->linked_modules != NULL)
		    q->linked_modules[q->definition_obj->rc->
				  init_module / 8] |= 1 <<
			q->definition_obj->rc->init_module % 8;
		if(whyload){
		    print_obj_name(cur_obj);
		    print("loaded for library initialization "
			  "routine\n");
		}
		merge_dylib_module_symbols(q);
	    }
	    q->definition_obj->init_module_loaded = TRUE;
	}
}

/*
 * setup_sub_images() is called to set up the sub images that make up the
 * specified "primary" dynamic library.  If not all of its sub_umbrella's and
 * sub_librarys are set up then it will return FALSE and not set up the sub
 * images.  The caller will loop through all the libraries until all libraries
 * are setup.  This routine will return TRUE when it sets up the sub_images and
 * will also set the sub_images_setup field to TRUE in the specified library.
 */
static
enum bool
setup_sub_images(
struct dynamic_library *p)
{
    unsigned long i, j, k, l, n, max_libraries;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;
    struct sub_framework_command *sub;
    struct dynamic_library **deps;
    char *sub_umbrella_name, *sub_library_name, *sub_framework_name;
    enum bool found;

	max_libraries = 0;
	deps = p->dependent_images;

	/*
	 * First see if this library has any sub-umbrellas or sub-librarys and
	 * that they have had their sub-images set up.  If not return FALSE and
	 * wait for this to be set up.  If so add the count of sub-images to
	 * max_libraries value which will be used for allocating the array for
	 * the sub-images of this library.
	 */
	mh = (struct mach_header *)(p->definition_obj->obj_addr);
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		sub_umbrella_name = (char *)usub + usub->sub_umbrella.offset;
		for(j = 0; j < p->ndependent_images; j++){
		    if(deps[j]->umbrella_name != NULL &&
		       strcmp(sub_umbrella_name, deps[j]->umbrella_name) == 0){
			/*
			 * TODO: can't this logic (here and in our caller) hang
		         * if there is a circular loop?  And is that even
			 * possible to create?  See comments in our caller.
			 */
			if(deps[j]->sub_images_setup == FALSE)
			    return(FALSE);
			max_libraries += 1 + deps[j]->nsub_images;
		    }
		}
		break;
	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		sub_library_name = (char *)lsub + lsub->sub_library.offset;
		for(j = 0; j < p->ndependent_images; j++){
		    if(deps[j]->library_name != NULL &&
		       strcmp(sub_library_name, deps[j]->library_name) == 0){
			/*
			 * TODO: can't this logic (here and in our caller) hang
		         * if there is a circular loop?  And is that even
			 * possible to create?  See comments in our caller.
			 */
			if(deps[j]->sub_images_setup == FALSE)
			    return(FALSE);
			max_libraries += 1 + deps[j]->nsub_images;
		    }
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Allocate the sub-images array of pointers to dynamic libraries that
	 * make up this "primary" library.  Allocate enough to handle the max.
	 */
	max_libraries += p->ndependent_images;
	p->sub_images = allocate(max_libraries *
				 sizeof(struct dynamic_library *));
	n = 0;

	/*
	 * First add the dependent images which are sub-frameworks of this
	 * image to the sub images list.
	 */
	if(p->umbrella_name != NULL){
	    for(i = 0; i < p->ndependent_images; i++){
		mh = (struct mach_header *)(deps[i]->definition_obj->obj_addr);
		load_commands = (struct load_command *)((char *)mh +
						    sizeof(struct mach_header));
		lc = load_commands;
		for(j = 0; j < mh->ncmds; j++){
		    if(lc->cmd == LC_SUB_FRAMEWORK){
			sub = (struct sub_framework_command *)lc;
			sub_framework_name = (char *)sub + sub->umbrella.offset;
			if(p->umbrella_name != NULL &&
			   strcmp(sub_framework_name, p->umbrella_name) == 0){
			    p->sub_images[n++] = deps[i];
			    if(p->force_weak_dylib == TRUE)
				deps[i]->force_weak_dylib = TRUE;
			    goto next_dep;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
next_dep:	;
	    }
	}

	/*
	 * Second add the sub-umbrella's and sub-library's sub-images to the
	 * sub images list.
	 */
	mh = (struct mach_header *)p->definition_obj->obj_addr;
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		sub_umbrella_name = (char *)usub + usub->sub_umbrella.offset;
		for(j = 0; j < p->ndependent_images; j++){
		    if(deps[j]->umbrella_name != NULL &&
		       strcmp(sub_umbrella_name, deps[j]->umbrella_name) == 0){

			/* make sure this image is not already on the list */
			found = FALSE;
			for(l = 0; l < n; l++){
			    if(p->sub_images[l] == deps[j]){
				found = TRUE;
				break;
			    }
			}
			if(found == FALSE){
			    p->sub_images[n++] = deps[j];
			    if(p->force_weak_dylib == TRUE)
				deps[j]->force_weak_dylib = TRUE;
			}

			for(k = 0; k < deps[j]->nsub_images; k++){
			    /* make sure this image is not already on the list*/
			    found = FALSE;
			    for(l = 0; l < n; l++){
				if(p->sub_images[l] == deps[j]->sub_images[k]){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE)
				p->sub_images[n++] = deps[j]->sub_images[k];
			}
		    }
		}
		break;
	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		sub_library_name = (char *)lsub + lsub->sub_library.offset;
		for(j = 0; j < p->ndependent_images; j++){
		    if(deps[j]->library_name != NULL &&
		       strcmp(sub_library_name, deps[j]->library_name) == 0){

			/* make sure this image is not already on the list */
			found = FALSE;
			for(l = 0; l < n; l++){
			    if(p->sub_images[l] == deps[j]){
				found = TRUE;
				break;
			    }
			}
			if(found == FALSE)
			    p->sub_images[n++] = deps[j];

			for(k = 0; k < deps[j]->nsub_images; k++){
			    /* make sure this image is not already on the list*/
			    found = FALSE;
			    for(l = 0; l < n; l++){
				if(p->sub_images[l] == deps[j]->sub_images[k]){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE)
				p->sub_images[n++] = deps[j]->sub_images[k];
			}
		    }
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	p->nsub_images = n;
	p->sub_images_setup = TRUE;
	return(TRUE);
}

/*
 * prebinding_check_for_dylib_override_symbols() checks to make sure that no
 * symbols are being overridden in a dependent library if prebinding is to
 * be done.  If a symbol is overridden prebinding is disabled and a warning
 * is printed.
 */
__private_extern__
void
prebinding_check_for_dylib_override_symbols(
void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	if(prebinding == TRUE){
	    for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				     merged_symbol_root->list;
		merged_symbol_list != NULL;
		merged_symbol_list = merged_symbol_list->next){
		for(i = 0; i < merged_symbol_list->used; i++){
		    merged_symbol = merged_symbol_list->symbols[i];
		    if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT)
			continue;
		    check_dylibs_for_definition(merged_symbol, TRUE, FALSE);
		}
	    }
	}
}

/*
 * twolevel_namespace_check_for_unused_dylib_symbols() checks dylibs to make
 * sure the user sees a warning about unused symbols defined in a dylib that
 * where another symbol of the same name is being used from some other object
 * or dynamic library.
 */
__private_extern__
void
twolevel_namespace_check_for_unused_dylib_symbols(
void)
{
    unsigned long i;
    struct merged_symbol_list *merged_symbol_list;
    struct merged_symbol *merged_symbol;

	for(merged_symbol_list = merged_symbol_root == NULL ? NULL :
				 merged_symbol_root->list;
	    merged_symbol_list != NULL;
	    merged_symbol_list = merged_symbol_list->next){
	    for(i = 0; i < merged_symbol_list->used; i++){
		merged_symbol = merged_symbol_list->symbols[i];
		if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT)
		    continue;
		check_dylibs_for_definition(merged_symbol, FALSE, TRUE);
	    }
	}
}

/*
 * check_dylibs_for_definition() checks to see if the merged symbol is defined
 * in any of the dependent dynamic shared libraries.
 *
 * If prebind_check is TRUE and the symbol is defined in a dylib and also
 * refernced a warning is printed, prebinding is disabled and the symbols are
 * traced.
 *
 * If twolevel_namespace_check is TRUE and the symbol is defined in a dylib the
 * a warning about an unused defintion is printed and the symbols are traced.
 */
static
void
check_dylibs_for_definition(
struct merged_symbol *merged_symbol,
enum bool prebind_check,
enum bool twolevel_namespace_check)
{
    struct dynamic_library *p;
    struct dylib_table_of_contents *toc;
    static enum bool printed_override, printed_unused, merged_symbol_printed;

    printed_override = FALSE;
    printed_unused = FALSE;
    merged_symbol_printed = FALSE;

	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		/*
		 * If this symbol is defined in the this dylib it is not an
		 * overridden symbol.
		 */
		if(merged_symbol->defined_in_dylib == TRUE &&
		   p->definition_obj->file_name ==
		   merged_symbol->definition_object->file_name)
		    continue;

		bsearch_strings = p->strings;
		bsearch_symbols = p->symbols;

		toc = bsearch(merged_symbol->nlist.n_un.n_name,
			      p->tocs, p->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		if(toc != NULL){
		    if(prebind_check == TRUE){
			/*
			 * There is a module that defineds this symbol.  If this
			 * symbol is also referenced by the libraries then we
			 * can't prebind.
			 */
			if(check_dylibs_for_reference(merged_symbol) == TRUE){
			    if(printed_override == FALSE){
				if(ld_trace_prebinding_disabled == TRUE)
				    ld_trace("[Logging for XBS] "
					     "prebinding disabled for %s because "
					     "of symbols overridden in dependent "
					     "dynamic shared libraries\n",
					     final_output != NULL ? final_output :
					     outputfile);
				warning("prebinding disabled because of symbols"
				   " overridden in dependent dynamic shared "
				   "libraries:");
				printed_override = TRUE;
			    }
			    trace_merged_symbol(merged_symbol);
			    printf("%s(%s) definition of %s\n",
				   p->definition_obj->file_name,
				   p->strings +
					p->mods[toc->module_index].module_name,
				   merged_symbol->nlist.n_un.n_name);
			    prebinding = FALSE;
			}
		    }
		    if(twolevel_namespace_check == TRUE){
			/*
			 * If this module was loaded then warnings about
			 * multiply defined symbols in it have previously been
			 * flagged.
			 */
			if(is_dylib_module_loaded(p->mods + toc->module_index)
			   == TRUE)
			    continue;
			if(printed_unused == FALSE){
			    if(multiply_defined_unused_flag ==
			       MULTIPLY_DEFINED_ERROR)
				error("unused multiple definitions of symbol "
				      "%s", merged_symbol->nlist.n_un.n_name);
			    else
				warning("unused multiple definitions of symbol "
					"%s", merged_symbol->nlist.n_un.n_name);
			    printed_unused = TRUE;
			}
			/*
			 * First print the symbol that is being used if not
			 * already printed.
			 */
			if(merged_symbol_printed == FALSE){
			    trace_merged_symbol(merged_symbol);
			    merged_symbol_printed = TRUE;
			}
			printf("%s(%s) unused definition of %s\n",
			       p->definition_obj->file_name,
			       p->strings +
				    p->mods[toc->module_index].module_name,
			       merged_symbol->nlist.n_un.n_name);
		    }
		}
	    }
	}
}

/*
 * check_dylibs_for_reference() checks the dependent dynamic shared libraries
 * to see if the specified merged symbol is referenced in a flat namespace
 * library.  If it is TRUE is returned else FALSE is returned.
 */
static
enum bool
check_dylibs_for_reference(
struct merged_symbol *merged_symbol)
{
    struct dynamic_library *p;
    struct dylib_table_of_contents *toc;
    struct nlist *symbol;
    struct dylib_reference *dylib_references;
    unsigned long i, symbol_index;
    struct mach_header *mh;

	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type == DYLIB){
		/*
		 * If we are not building an output file that forces flat name
		 * space and this library is a two-level namespace library then
		 * all references to undefined symbols are to specific
		 * libraries and can't be overridden.
		 */
		mh = (struct mach_header *)(p->definition_obj->obj_addr);
		if(force_flat_namespace == FALSE &&
		   (mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
		    continue;
		/*
		 * See if this symbol appears at all (defined or undefined)
		 * in this library.
		 */
		bsearch_strings = p->strings;
		bsearch_symbols = p->symbols;
		toc = bsearch(merged_symbol->nlist.n_un.n_name,
			      p->tocs, p->definition_obj->dysymtab->ntoc,
			      sizeof(struct dylib_table_of_contents),
			      (int (*)(const void *, const void *))
				dylib_bsearch);
		if(toc != NULL){
		    symbol_index = toc->symbol_index;
		}
		else{
		    symbol = bsearch(merged_symbol->nlist.n_un.n_name,
			     bsearch_symbols +
				p->definition_obj->dysymtab->iundefsym,
			     p->definition_obj->dysymtab->nundefsym,
			     sizeof(struct nlist),
			     (int (*)(const void *,const void *))nlist_bsearch);
		    if(symbol == NULL)
			continue;
		    symbol_index = symbol - bsearch_symbols;
		}
		/*
		 * The symbol appears in this library.  Now see if it is
		 * referenced by a module in the library.
		 */
		dylib_references = (struct dylib_reference *)
		    (p->definition_obj->obj_addr +
		     p->definition_obj->dysymtab->extrefsymoff);
		for(i = 0; i < p->definition_obj->dysymtab->nextrefsyms; i++){
		    if(dylib_references[i].isym == symbol_index &&
		       (dylib_references[i].flags ==
			    REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		        dylib_references[i].flags ==
			    REFERENCE_FLAG_UNDEFINED_LAZY))
		    return(TRUE);
		}
	    }
	}
	return(FALSE);
}

/*
 * open_dylib() attempts to open the dynamic library specified by the pointer
 * to the dynamic_library structure.  This is only called for dependent
 * libraries found in the object loaded or in other dynamic libraries.  Since
 * this is only used for undefined checking and prebinding it is not fatal if
 * the library can't be opened.  But if it can't be opened and undefined
 * checking or prebinding is to be done a warning is issued.
 */
static
enum bool
open_dylib(
struct dynamic_library *p)
{
    unsigned long i, file_size;
    char *colon, *file_name, *dylib_name, *file_addr;
    int fd;
    struct stat stat_buf;
    kern_return_t r;
    struct fat_header *fat_header;
    struct mach_header *mh;
    struct load_command *lc;
    struct dylib_command *dl;

	/*
	 * First see if there is a -dylib_file option for this dylib and if so
	 * use that as the file name to open for the dylib.
	 */
	for(i = 0; i < ndylib_files; i++){
	    colon = strchr(dylib_files[i], ':');
	    *colon = '\0';
	    if(strcmp(p->dylib_name, dylib_files[i]) == 0){
		p->dylib_file = dylib_files[i];
		p->file_name = colon + 1;
		*colon = ':';
		break;
	    }
	    *colon = ':';
	}
#if 0
	/*
	 * It has been determined that this warning is distracting.  Even though
	 * the user may want to be alerted that this library is being included
	 * indirectly.  It has been determined this rarely a problem.  So when
	 * it is a problem we will just hope the user is smart enought to figure
	 * it out without any clue.
	 */
	if(p->dylib_file == NULL &&
	   (undefined_flag != UNDEFINED_SUPPRESS ||
	    prebinding == TRUE)){
	    if(p->definition_obj->ar_hdr != NULL)
		warning("using file: %s for reference to dynamic shared library"
			" from: %s(%.*s) because no -dylib_file specified",
			p->dylib_name, p->definition_obj->file_name,
			(int)p->definition_obj->ar_name_size,
			p->definition_obj->ar_name);

	    else
		warning("using file: %s for reference to dynamic shared library"
			" from: %s because no -dylib_file specified",
			p->dylib_name, p->definition_obj->file_name);
	}
#endif

	/*
	 * Try to open the dynamic library.  If it can't be opened it is only
	 * a warning if undefined checking or prebinding is to be done.  Once
	 * the file is opened sucessfully then any futher problems are treated
	 * as errors.
	 */
	if(p->dylib_file != NULL)
	    file_name = p->file_name;
	else{
	    if(executable_path != NULL &&
	       strncmp(p->dylib_name, "@executable_path",
                       sizeof("@executable_path") - 1) == 0){
		file_name = mkstr(executable_path,
				  p->dylib_name + sizeof("@executable_path") -1,
				  NULL);
	    }
	    else{
		file_name = p->dylib_name;
	    }
	}
	if((fd = open(file_name, O_RDONLY, 0)) == -1){
	    if(undefined_flag != UNDEFINED_SUPPRESS){
		system_warning("can't open dynamic library: %s referenced "
		    "from: %s (checking for undefined symbols may be affected)",
		    file_name, p->definition_obj->file_name);
	    }
	    return(FALSE);
	}

	/*
	 * Now that the file_name has been determined and opened get it into
	 * memory by mapping it.
	 */
	if(fstat(fd, &stat_buf) == -1){
	    system_error("can't stat dynamic library file: %s", file_name);
	    close(fd);
	    return(FALSE);
	}
	file_size = stat_buf.st_size;
	/*
	 * For some reason mapping files with zero size fails so it has to
	 * be handled specially.
	 */
	if(file_size == 0){
	    error("file: %s is empty (not a dynamic library)", file_name);
	    close(fd);
	    return(FALSE);
	}
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&file_addr,
	    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
	    close(fd);
	    mach_fatal(r, "can't map dynamic library file: %s", file_name);
	}
	close(fd);

	/*
	 * This file must be a dynamic library (it can be fat too).
	 */
	cur_obj = NULL;
	if(sizeof(struct fat_header) > file_size){
	    error("truncated or malformed dynamic library file: %s (file size "
		  "too small to be a dynamic library)", file_name);
	    return(FALSE);
	}
	fat_header = (struct fat_header *)file_addr;
#ifdef __BIG_ENDIAN__
	if(fat_header->magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	{
	    pass1_fat(file_name, file_addr, file_size, FALSE, TRUE, FALSE,
		      FALSE);
	}
	else{
	    pass1_object(file_name, file_addr, file_size, FALSE, FALSE, TRUE,
			 FALSE, FALSE);
	}
	if(errors)
	    return(FALSE);
	if(cur_obj == NULL || cur_obj->dylib == FALSE)
	    return(FALSE);

	dylib_name = NULL;
	mh = (struct mach_header *)cur_obj->obj_addr;
	lc = (struct load_command *)((char *)cur_obj->obj_addr +
				     sizeof(struct mach_header));
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmd == LC_ID_DYLIB){
		dl = (struct dylib_command *)lc;
		dylib_name = (char *)dl + dl->dylib.name.offset;
#ifdef notdef
		if(strcmp(p->dylib_name, dylib_name) != 0){
		    error("wrong dynamic library: %s (the name in the "
			  "LC_ID_DYLIB command (%s) is not %s)", file_name,
			  dylib_name, p->dylib_name);
		}
#endif
		p->dl = dl;
		p->definition_obj = cur_obj;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(TRUE);
}

/*
 * set_sub_frameworks_ordinals() sets the library ordinal for other libraries
 * that are sub-frameworks of the specified dynamic library.  If any ordinals
 * are set then TRUE is returned else FALSE is returned.
 */
static
enum bool
set_sub_frameworks_ordinals(
struct dynamic_library *umbrella)
{
    enum bool set_some_ordinals;
    struct dynamic_library *p;
    unsigned long i;
    struct mach_header *mh;
    struct load_command *lc;
    struct sub_framework_command *sub;

	set_some_ordinals = FALSE;
	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type != DYLIB)
		continue;
	    /*
	     * If this library's ordinal is not set the see if it has an
	     * LC_SUB_FRAMEWORK command with the same name as the umbrella
	     * library.
	     */
	    if(p->definition_obj->library_ordinal == 0){
		mh = (struct mach_header *)p->definition_obj->obj_addr;
		lc = (struct load_command *)
			((char *)p->definition_obj->obj_addr +
				sizeof(struct mach_header));
		for(i = 0; i < mh->ncmds; i++){
		    if(lc->cmd == LC_SUB_FRAMEWORK){
			sub = (struct sub_framework_command *)lc;
			if(strcmp((char *)sub + sub->umbrella.offset,
				  umbrella->umbrella_name) == 0){
			    p->definition_obj->library_ordinal =
				umbrella->definition_obj->library_ordinal;
			    set_isub_image(p, p);
			    set_some_ordinals = TRUE;
			    break;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
	return(set_some_ordinals);
}

/*
 * set_sub_umbrella_sub_library_ordinal() sets the library ordinal for the
 * specified dynamic library if it is a sub-umbrella or a sub-library of another
 * dynamic library who's library ordinal is set.  If the ordinal is set then
 * TRUE is returned else FALSE is returned.
 */
static
enum bool
set_sub_umbrella_sub_library_ordinal(
struct dynamic_library *sub)
{
    struct dynamic_library *p;
    unsigned long i;
    struct mach_header *mh;
    struct load_command *lc;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;

	for(p = dynamic_libs; p != NULL; p = p->next){
	    if(p->type != DYLIB)
		continue;
	    /*
	     * If this library's ordinal is set the see if it has an
	     * LC_SUB_UMBRELLA or LC_SUB_LIBRARY command with the same name as
	     * the sub library.
	     */
	    if(p->definition_obj->library_ordinal != 0){
		mh = (struct mach_header *)p->definition_obj->obj_addr;
		lc = (struct load_command *)
			((char *)p->definition_obj->obj_addr +
				sizeof(struct mach_header));
		for(i = 0; i < mh->ncmds; i++){
		    if(lc->cmd == LC_SUB_UMBRELLA){
			usub = (struct sub_umbrella_command *)lc;
			if(sub->umbrella_name != NULL &&
			   strcmp((char *)usub + usub->sub_umbrella.offset,
				  sub->umbrella_name) == 0){
			    sub->definition_obj->library_ordinal =
				p->definition_obj->library_ordinal;
			    set_isub_image(p, sub);
			    return(TRUE);
			}
		    }
		    else if(lc->cmd == LC_SUB_LIBRARY){
			lsub = (struct sub_library_command *)lc;
			if(sub->library_name != NULL &&
			   strcmp((char *)lsub + lsub->sub_library.offset,
				  sub->library_name) == 0){
			    sub->definition_obj->library_ordinal =
				p->definition_obj->library_ordinal;
			    set_isub_image(p, sub);
			    return(TRUE);
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
	return(FALSE);
}

/*
 * set_isub_image() sets the isub_image of the specified sub dynamic library to
 * the index into the sub_images of the specified dynamic library p.
 */
static
void
set_isub_image(
struct dynamic_library *p,
struct dynamic_library *sub)
{
    struct dynamic_library *q;
    unsigned long j;

	/*
	 * Find the first library in the list with this
	 * ordinal which is the primary library.  Then walk
	 * the primary library's sub_images to figure out
	 * what the sub_image index is for this library.
	 */
	for(q = dynamic_libs; q != NULL; q = q->next){
	    if(q->type != DYLIB)
		continue;
	    if(q->definition_obj->library_ordinal ==
	       p->definition_obj->library_ordinal){
		for(j = 0; j < q->nsub_images; j++){
		    if(q->sub_images[j] == sub){
			sub->definition_obj->isub_image = j + 1;
			return;
		    }
		}
	    }
	}
}

/*
 * add_dynamic_lib() adds a library to the list of specified
 * libraries.  A specified library is a library that is referenced from the
 * object files loaded.  It does not include libraries referenced from dynamic
 * libraries.  This returns a pointer to the dynamic_library struct for the
 * dylib_name specified in the dylib_command (or a new dynamic_library struct
 * for archive types).
 */
__private_extern__
struct dynamic_library *
add_dynamic_lib(
enum library_type type,
struct dylib_command *dl,
struct object_file *definition_obj)
{
    struct dynamic_library *p, *q;
    char *dylib_name;

	dylib_name = NULL;
	/*
	 * If this is a dynamic shared library check to see if it is all ready
	 * on the list.
	 */
	if(type == DYLIB){
	    dylib_name = (char *)dl + dl->dylib.name.offset;
	    /*
	     * See if this library is already on the list of specified libraries
	     * and if so merge the two.  If only one is an LC_ID_DYLIB then use
	     * that one.
	     */
	    for(p = dynamic_libs; p != NULL; p = p->next){
		if(p->type == DYLIB &&
		   strcmp(p->dylib_name, dylib_name) == 0){
		    if(p->dl->cmd == LC_ID_DYLIB){
			/*
			 * If the new one is also a LC_ID_DYLIB use the one
			 * with the highest compatiblity number.  Else if the
			 * new one is just an LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB
			 * or LC_REEXPORT_DYLIB ignore it and use the one that
			 * is on the list which is a LC_ID_DYLIB.
			 */
			if(dl->cmd == LC_ID_DYLIB){
			   if(dl->dylib.compatibility_version >
			      p->dl->dylib.compatibility_version){
				p->dylib_name = dylib_name;
				p->dl = dl;
				p->definition_obj = definition_obj;
			    }
			}
		    }
		    else{
			if(dl->cmd == LC_ID_DYLIB){
			    p->dylib_name = dylib_name;
			    p->dl = dl;
			    p->definition_obj = definition_obj;
			}
		    }
		    return(p);
		}
	    }
	}
	/*
	 * If this library is not the lists of libraries or is an archive
	 * library.  Create a new dynamic_library struct for it.  Add it to the
	 * end of the list of specified libraries.  Then return a pointer new
	 * dynamic_library struct.
	 */
	p = allocate(sizeof(struct dynamic_library));
	memset(p, '\0', sizeof(struct dynamic_library));
	if(dynamic_libs == NULL)
	    dynamic_libs = p;
	else{
	    for(q = dynamic_libs; q->next != NULL; q = q->next)
		;
	    q->next = p;
	}

	if(type == DYLIB){
	    p->type = DYLIB;
	    p->dylib_name = dylib_name;
	    p->dl = dl;
	    p->definition_obj = definition_obj;
	    /*
	     * If the environment variable NEXT_ROOT is set then the file_name
	     * for this library prepended with NEXT_ROOT.  Basicly faking out
	     * as if a -dylib_file argument was seen.
	     */
	    if(next_root != NULL && *dylib_name == '/'){
		p->file_name = allocate(strlen(next_root) +
				      strlen(dylib_name) + 1);
		strcpy(p->file_name, next_root);
		strcat(p->file_name, dylib_name);
		p->dylib_file = p->dylib_name;
	    }
	}

	if(type == BUNDLE_LOADER){
	    p->type = BUNDLE_LOADER;
	    p->dylib_name = NULL;
	    p->dl = NULL;
	    p->definition_obj = definition_obj;
	}
	return(p);
}

/*
 * Function for bsearch() for finding a symbol name in a dylib table of
 * contents.
 */
__private_extern__
int
dylib_bsearch(
const char *symbol_name,
const struct dylib_table_of_contents *toc)
{
	return(strcmp(symbol_name,
		      bsearch_strings +
		      bsearch_symbols[toc->symbol_index].n_un.n_strx));
}

/*
 * Function for bsearch() for finding a symbol name in the sorted list of
 * undefined symbols.
 */
static
int
nlist_bsearch(
const char *symbol_name,
const struct nlist *symbol)
{
	return(strcmp(symbol_name, bsearch_strings + symbol->n_un.n_strx));
}
#endif /* !defined(RLD) */

/*
 * Function for bsearch() for finding a symbol name in a ranlib table of
 * contents.
 */
static
int
ranlib_bsearch(
const char *symbol_name,
const struct ranlib *ran)
{
	return(strcmp(symbol_name, bsearch_strings + ran->ran_un.ran_strx));
}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

/*
 * merge() merges all the global information from the cur_obj into the merged
 * data structures for the output object file to be built from.
 */
__private_extern__
void
merge(
enum bool dylib_only,
enum bool bundle_loader,
enum bool force_weak)
{
    unsigned long previous_errors;

	/*
	 * save the previous errors and only return out of here if something
	 * we do in here gets an error.
	 */
	previous_errors = errors;
	errors = 0;

	/* print the object file name if tracing */
	if(trace){
	    print_obj_name(cur_obj);
	    print("\n");
	}

	/* check the header and load commands of the object file */
	check_cur_obj(dylib_only, bundle_loader);
	if(errors)
	    goto merge_return;

	/* if this was called to via an open_dylib() we are done */
	if(dylib_only == TRUE)
	    goto merge_return;

#ifndef RLD
	/*
	 * if this is the -bundle_loader argument then put it on the list
	 * of dynamic libraries where it will be searched.
	 */
	if(bundle_loader){
	    /* If this object file has no symbols then don't add it */
	    if(cur_obj->symtab != NULL)
		(void)add_dynamic_lib(BUNDLE_LOADER, NULL, cur_obj);
	    goto merge_return;
	}
#endif /* !defined(RLD) */

	/* if this object has any fixed VM shared library stuff merge it */
	if(cur_obj->fvmlib_stuff){
#ifndef RLD
	    merge_fvmlibs();
	    if(errors)
		goto merge_return;
#else /* defined(RLD) */
	    if(cur_obj != base_obj){
		error_with_cur_obj("can't dynamicly load fixed VM shared "
				   "library");
		goto merge_return;
	    }
#endif /* defined(RLD) */
	}

	/* if this object has any dynamic shared library stuff merge it */
	if(cur_obj->dylib_stuff){
#ifndef RLD
	    merge_dylibs(force_weak);
	    if(errors)
		goto merge_return;
	    if(cur_obj->dylib)
		goto merge_return;
	    if(cur_obj->dylinker)
		goto merge_return;
#else /* defined(RLD) */
	    if(cur_obj != base_obj){
		error_with_cur_obj("can't used dynamic libraries or dynamic "
		    "linker with rld based interfaces");
		goto merge_return;
	    }
#endif /* defined(RLD) */
	}

#ifndef KLD
	/* read the DWARF information if any */
	if(strip_level < STRIP_DEBUG)
	  read_dwarf_info();
#endif

	/* merged it's sections */
	merge_sections();
	if(errors)
	    goto merge_return;

	/* merged it's symbols */
	merge_symbols();
	if(errors)
	    goto merge_return;

merge_return:
	errors += previous_errors;
}

/*
 * check_cur_obj() checks to see if the cur_obj object file is really an object
 * file and that all the offset and sizes in the headers are within the memory
 * the object file is mapped in.  This allows the rest of the code in the link
 * editor to use the offsets and sizes in the headers without bounds checking.
 *
 * Since this is making a pass through the headers a number of things are filled
 * in in the object structrure for this object file including: the symtab field,
 * the dysymtab field, the section_maps and nsection_maps fields (this routine
 * allocates the section_map structures and fills them in too), the fvmlib_
 * stuff field is set if any SG_FVMLIB segments or LC_LOADFVMLIB commands are
 * seen and the dylib_stuff field is set if the file is a MH_DYLIB or
 * MH_DYLIB_STUB type and has a LC_ID_DYLIB command or a LC_LOAD_DYLIB,
 * LC_LOAD_WEAK_DLIB or LC_REEXPORT_DYLIB command is seen.
 */
static
void
check_cur_obj(
enum bool dylib_only,
enum bool bundle_loader)
{
    unsigned long i, j, section_type;
    uint32_t magic;
    struct mach_header *mh;
    struct mach_header_64 *mh64;
    struct load_command l, *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct routines_command *rc;
    struct symseg_command *ss;
    struct fvmlib_command *fl;
    struct dylib_command *dl, *dlid;
    struct dylinker_command *dyld, *dyldid;
    struct sub_framework_command *sub;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;
    struct sub_client_command *csub;
    struct twolevel_hints_command *hints;
    struct prebind_cksum_command *cs;
    struct uuid_command *uuid;
    char *fvmlib_name, *dylib_name, *dylib_id_name, *dylinker_name,
	 *umbrella_name, *sub_umbrella_name, *sub_library_name,*sub_client_name;
    cpu_subtype_t new_cpusubtype;
    const char *new_arch, *prev_arch;
    const struct arch_flag *family_arch_flag;
    uint32_t *indirect_symtab;
    struct dylib_table_of_contents *tocs;
    struct dylib_module *mods;
    struct dylib_reference *refs;
#ifndef RLD
    enum bool is_framework, allowable_client;
    char *short_name, *has_suffix, *this_client_name;
#endif

    static const struct symtab_command empty_symtab = { 0 };
    static const struct dysymtab_command empty_dysymtab = { 0 };

#ifdef KLD
	memset(&output_uuid_info, '\0', sizeof(struct uuid_info));
#endif
	/* check to see the mach_header is valid */
	if(sizeof(struct mach_header) > cur_obj->obj_size){
	    error_with_cur_obj("truncated or malformed object (mach header "
			       "extends past the end of the file)");
	    return;
	}

	magic = *((uint32_t *)cur_obj->obj_addr);

	if(magic ==  MH_MAGIC ||
	   magic == SWAP_LONG(MH_MAGIC)){
	  mh = (struct mach_header *)cur_obj->obj_addr;
	  if(magic == MH_MAGIC){
	    cur_obj->swapped = FALSE;
	  }
	  else{
	    cur_obj->swapped = TRUE;
	    swap_mach_header(mh, host_byte_sex);
	  }
	}
	else if(cur_obj->obj_size >= sizeof(struct mach_header_64) &&
		(magic == MH_MAGIC_64 ||
		 magic == SWAP_LONG(MH_MAGIC_64))){

	  mh64 = (struct mach_header_64 *)cur_obj->obj_addr;
	  if(magic == MH_MAGIC_64){
	    cur_obj->swapped = FALSE;
	  }
	  else{
	    cur_obj->swapped = TRUE;
        swap_mach_header_64(mh64, host_byte_sex);
	  }

	  /* If no architecture has been explicitly given, and
	   *  this is the first object seen, set up arch_flag
	   *  without interrogating the object file further
	   */
	  if(!arch_flag.cputype) {
		family_arch_flag = get_arch_family_from_cputype(mh64->cputype);
		if(family_arch_flag == NULL){
		    error_with_cur_obj("cputype (%d) unknown (file not loaded)",
			 mh64->cputype);
		    return;
		}
		arch_flag.cputype = mh64->cputype;
		if(force_cpusubtype_ALL == TRUE)
		    arch_flag.cpusubtype = family_arch_flag->cpusubtype;
		else
		    arch_flag.cpusubtype = mh64->cpusubtype;
	  }

	  return;
	}
	else{
	  if(no_arch_warnings != TRUE)
	    error_with_cur_obj("bad magic number (not a Mach-O file)");
	  return;
	}
	if(mh->cputype != 0){
	    if(target_byte_sex == UNKNOWN_BYTE_SEX){
		if(cur_obj->swapped == TRUE)
		    target_byte_sex = host_byte_sex == BIG_ENDIAN_BYTE_SEX ?
    			LITTLE_ENDIAN_BYTE_SEX : BIG_ENDIAN_BYTE_SEX;
		else
		    target_byte_sex = host_byte_sex;
	    }
	    /*
	     * If for this cputype we are to always output the ALL cpusubtype
	     * then set force_cpusubtype_ALL.
	     */
	    if(force_cpusubtype_ALL_for_cputype(mh->cputype) == TRUE)
		force_cpusubtype_ALL = TRUE;
	    /*
	     * If we have previous loaded something or an -arch flag was
	     * specified something so make sure the cputype of this object
	     * matches (the case the architecture has been previous selected).
	     */
	    if(arch_flag.cputype){
		if(arch_flag.cputype != mh->cputype){
		    new_arch = get_arch_name_from_types(mh->cputype,
						        mh->cpusubtype);
		    prev_arch = get_arch_name_from_types(arch_flag.cputype,
						         arch_flag.cpusubtype);
		    if(no_arch_warnings == TRUE)
			return;
		    if(arch_flag.name != NULL){
			if(arch_errors_fatal == TRUE){
			    error_with_cur_obj("cputype (%d, architecture %s) "
				"does not match cputype (%d) for specified "
				"-arch flag: %s", mh->cputype, new_arch,
				arch_flag.cputype, arch_flag.name);
			}
			else
			    warning_with_cur_obj("cputype (%d, architecture %s)"
				" does not match cputype (%d) for specified "
				"-arch flag: %s (file not loaded)", mh->cputype,
				 new_arch, arch_flag.cputype, arch_flag.name);
		    }
		    else{
			if(arch_errors_fatal == TRUE){
			    error_with_cur_obj("cputype (%d, architecture %s) "
				"does not match cputype (%d architecture %s) "
				"of objects files previously loaded",
				mh->cputype, new_arch, arch_flag.cputype,
				prev_arch);
			}
			else
			    warning_with_cur_obj("cputype (%d, architecture %s)"
				" does not match cputype (%d architecture %s) "
				"of objects files previously loaded (file not "
				"loaded)", mh->cputype, new_arch,
				arch_flag.cputype,prev_arch);
		    }
		    return;
		}
		/* deal with combining this cpusubtype and what is current */
		if(force_cpusubtype_ALL == FALSE){
		    new_cpusubtype = cpusubtype_combine(arch_flag.cputype,
					  arch_flag.cpusubtype, mh->cpusubtype);
		    if(new_cpusubtype == -1){
			new_arch = get_arch_name_from_types(mh->cputype,
							    mh->cpusubtype);
			prev_arch = get_arch_name_from_types(arch_flag.cputype,
							 arch_flag.cpusubtype);
			if(no_arch_warnings == TRUE)
			    return;
			if(arch_flag.name != NULL){
			    if(arch_errors_fatal == TRUE){
				error_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d) for specified -arch flag: "
				    "%s and -force_cpusubtype_ALL not "
				    "specified", mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, arch_flag.name);
			    }
			    else
				warning_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d) for specified -arch flag: "
				    "%s and -force_cpusubtype_ALL not specified"
				    " (file not loaded)", mh->cpusubtype,
				    new_arch, arch_flag.cpusubtype,
				    arch_flag.name);
			}
			else{
			    if(arch_errors_fatal == TRUE){
				error_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d, architecture %s) of "
				    "objects files previously loaded and "
				    "-force_cpusubtype_ALL not specified",
				    mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, prev_arch);
			    }
			    else
				warning_with_cur_obj("cpusubtype (%d, "
				    "architecture %s) does not combine with "
				    "cpusubtype (%d, architecture %s) of "
				    "objects files previously loaded and "
				    "-force_cpusubtype_ALL not specified (file "
				    "not loaded)", mh->cpusubtype, new_arch,
				    arch_flag.cpusubtype, prev_arch);
			}
			return;
		    }
		    else{
			/*
			 * No -force_cpusubtype_ALL is specified if an -arch
			 * flag for a specific implementation of an architecture
			 * was specified then the resulting cpusubtype will be
			 * for that specific implementation of that architecture
			 * and all cpusubtypes must combine with the cpusubtype
			 * for the -arch flag to the cpusubtype for the -arch
			 * flag else an error must be flaged.
			 */
			if(specific_arch_flag == TRUE){
			    if(arch_flag.cpusubtype != new_cpusubtype){
			      new_arch = get_arch_name_from_types(mh->cputype,
								mh->cpusubtype);
			      warning_with_cur_obj("cpusubtype (%d, "
				"architecture %s) does not combine with "
				"cpusubtype (%d) for specified -arch flag: %s "
				"and -force_cpusubtype_ALL not specified (file "
				"not loaded)", mh->cpusubtype, new_arch,
				arch_flag.cpusubtype, arch_flag.name);
			    }
			}
			else if(mh->filetype != MH_DYLIB &&
				bundle_loader == FALSE)
			    arch_flag.cpusubtype = new_cpusubtype;
		    }
		}
		else{ /* force_cpusubtype_ALL == TRUE */
		    family_arch_flag =get_arch_family_from_cputype(mh->cputype);
		    if(family_arch_flag != NULL)
			arch_flag.cpusubtype = family_arch_flag->cpusubtype;
		    else{
			warning_with_cur_obj("cputype (%d) unknown (file not "
			    "loaded)", mh->cputype);
			return;
		    }
		}
	    }
	    /*
	     * Nothing has been loaded yet and no -arch flag has been specified
	     * so use this object to set what is to be loaded (the case the
	     * architecture has not been selected).
	     */
	    else{
		family_arch_flag = get_arch_family_from_cputype(mh->cputype);
		if(family_arch_flag == NULL){
		    error_with_cur_obj("cputype (%d) unknown (file not loaded)",
			 mh->cputype);
		    return;
		}
		arch_flag.cputype = mh->cputype;
		if(force_cpusubtype_ALL == TRUE)
		    arch_flag.cpusubtype = family_arch_flag->cpusubtype;
		else
		    arch_flag.cpusubtype = mh->cpusubtype;
		if(target_byte_sex != get_byte_sex_from_flag(family_arch_flag))
		    error_with_cur_obj("wrong bytesex for cputype (%d) for "
			"-arch %s (bad object or this program out of sync with "
			"get_arch_family_from_cputype() and get_byte_sex_"
			"from_flag())", arch_flag.cputype,
			family_arch_flag->name);
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
		/*
		 * Pick up the Mac OS X deployment target if not done already.
		 */
		if(macosx_deployment_target.major == 0)
		    get_macosx_deployment_target(&macosx_deployment_target);
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */
	    }
	}
	if(mh->sizeofcmds + sizeof(struct mach_header) > cur_obj->obj_size){
	    error_with_cur_obj("truncated or malformed object (load commands "
			       "extend past the end of the file)");
	    return;
	}
	if((mh->flags & MH_INCRLINK) != 0){
	    error_with_cur_obj("was the output of an incremental link, can't "
			       "be link edited again");
	    return;
	}
	if((mh->flags & MH_DYLDLINK) != 0 &&
	   (mh->filetype != MH_DYLIB &&
	    mh->filetype != MH_DYLIB_STUB &&
	    mh->filetype != MH_DYLINKER) &&
	   bundle_loader == FALSE){
	    error_with_cur_obj("is input for the dynamic link editor, is not "
			       "relocatable by the static link editor again");
	    return;
	}
	/*
	 * If this is a MH_DYLIB or MH_DYLIB_STUB file then a single LC_ID_DYLIB
	 * command must be seen to identify the library.
	 */
	cur_obj->dylib = (enum bool)(mh->filetype == MH_DYLIB ||
				     mh->filetype == MH_DYLIB_STUB);
	dlid = NULL;
	dylib_id_name = NULL;
	if(cur_obj->dylib == TRUE && dynamic == FALSE){
	    error_with_cur_obj("incompatible, file is a dynamic shared library "
		  "(must specify \"-dynamic\" to be used)");
	    return;
	}
	if(dylib_only == TRUE && cur_obj->dylib == FALSE){
	    error_with_cur_obj("file is not a dynamic shared library");
	    return;
	}
	/*
	 * If this is a MH_DYLINKER file then a single LC_ID_DYLINKER command
	 * must be seen to identify the dynamic linker.
	 */
	cur_obj->dylinker = (enum bool)(mh->filetype == MH_DYLINKER);
	dyldid = NULL;
	if(cur_obj->dylinker == TRUE && dynamic == FALSE){
	    error_with_cur_obj("incompatible, file is a dynamic link editor "
		  "(must specify \"-dynamic\" to be used)");
	    return;
	}

	/* check to see that the load commands are valid */
	load_commands = (struct load_command *)((char *)cur_obj->obj_addr +
			    sizeof(struct mach_header));
	st = NULL;
	dyst = NULL;
	rc = NULL;
	sub = NULL;
	umbrella_name = NULL;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    l = *lc;
	    if(cur_obj->swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0){
		error_with_cur_obj("load command %lu size not a multiple of "
				   "sizeof(long)", i);
		return;
	    }
	    if(l.cmdsize <= 0){
		error_with_cur_obj("load command %lu size is less than or equal"
				   " to zero", i);
		return;
	    }
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error_with_cur_obj("load command %lu extends past end of all "
				   "load commands", i);
		return;
	    }
	    switch(l.cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(cur_obj->swapped)
		    swap_segment_command(sg, host_byte_sex);
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error_with_cur_obj("cmdsize field of load command %lu is "
				       "inconsistant for a segment command "
				       "with the number of sections it has", i);
		    return;
		}
		if(sg->flags == SG_FVMLIB){
		    if(sg->nsects != 0){
			error_with_cur_obj("SG_FVMLIB segment %.16s contains "
					   "sections and shouldn't",
					   sg->segname);
			return;
		    }
		    cur_obj->fvmlib_stuff = TRUE;
		    break;
		}
		check_size_offset(sg->filesize, sg->fileoff, sizeof(long),
				  "filesize", "fileoff", i);
		if(errors)
		    return;
		/*
		 * Segments without sections are an error to see on input except
		 * for the segments created by the link-editor (which are
		 * recreated).
		 */
		if(sg->nsects == 0){
		    if(strcmp(sg->segname, SEG_PAGEZERO) != 0 &&
		       strcmp(sg->segname, SEG_LINKEDIT) != 0 &&
		       strcmp(sg->segname, SEG_UNIXSTACK) != 0){
			error_with_cur_obj("segment %.16s contains no "
					   "sections and can't be link-edited",
					   sg->segname);
			return;
		    }
		}
		else{
		    /*
		     * Doing a reallocate here is not bad beacuse in the
		     * normal case this is an MH_OBJECT file type and has only
		     * one segment.  So this only gets done once per object.
		     */
		    cur_obj->section_maps = reallocate(cur_obj->section_maps,
					(cur_obj->nsection_maps + sg->nsects) *
					sizeof(struct section_map));
		    memset(cur_obj->section_maps + cur_obj->nsection_maps, '\0',
			   sg->nsects * sizeof(struct section_map));
		}
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		if(cur_obj->swapped)
		    swap_section(s, sg->nsects, host_byte_sex);
		for(j = 0 ; j < sg->nsects ; j++){
		    cur_obj->section_maps[cur_obj->nsection_maps++].s = s;
		    /* check to see that segment name in the section structure
		       matches the one in the segment command if this is not in
		       an MH_OBJECT filetype */
		    if(mh->filetype != MH_OBJECT &&
		       strcmp(sg->segname, s->segname) != 0){
			error_with_cur_obj("segment name %.16s of section %lu "
				"(%.16s,%.16s) in load command %lu does not "
				"match segment name %.16s", s->segname, j,
				s->segname, s->sectname, i, sg->segname);
			return;
		    }
		    /* check to see that flags (type) of this section is some
		       thing the link-editor understands */
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type != S_REGULAR &&
		       section_type != S_ZEROFILL &&
		       section_type != S_CSTRING_LITERALS &&
		       section_type != S_4BYTE_LITERALS &&
		       section_type != S_8BYTE_LITERALS &&
		       section_type != S_LITERAL_POINTERS &&
		       section_type != S_NON_LAZY_SYMBOL_POINTERS &&
		       section_type != S_LAZY_SYMBOL_POINTERS &&
		       section_type != S_SYMBOL_STUBS &&
		       section_type != S_COALESCED &&
		       section_type != S_MOD_INIT_FUNC_POINTERS &&
		       section_type != S_MOD_TERM_FUNC_POINTERS &&
		       section_type != S_DTRACE_DOF){
			error_with_cur_obj("unknown flags (type) of section %lu"
					   " (%.16s,%.16s) in load command %lu",
					   j, s->segname, s->sectname, i);
			return;
		    }
		    if((s->flags & S_ATTR_DEBUG) == S_ATTR_DEBUG &&
		       section_type != S_REGULAR){
			error_with_cur_obj("malformed object, debug section %lu"
			    " (%.16s,%.16s) in load command %lu is not of type "
			    "S_REGULAR\n", j, s->segname, s->sectname, i);
			return;
		    }
		    if(dynamic == FALSE){
			if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
			   section_type == S_LAZY_SYMBOL_POINTERS ||
			   section_type == S_SYMBOL_STUBS ||
			   section_type == S_MOD_INIT_FUNC_POINTERS ||
			   section_type == S_MOD_TERM_FUNC_POINTERS){
			    error_with_cur_obj("incompatible, file contains "
				"unsupported type of section %lu (%.16s,%.16s) "
				"in load command %lu (must specify "
				"\"-dynamic\" to be used)", j, s->segname,
				s->sectname, i);
			    return;
			}
		    }
#if defined(SA_RLD) || (defined(KLD) && defined(__STATIC__))
		    if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		       section_type == S_LAZY_SYMBOL_POINTERS ||
		       section_type == S_SYMBOL_STUBS ||
		       section_type == S_MOD_INIT_FUNC_POINTERS ||
		       section_type == S_MOD_TERM_FUNC_POINTERS){
			error_with_cur_obj("unsupported type of section %lu "
			   "(%.16s,%.16s) for %s in load command %lu", j,
			   s->segname, s->sectname, progname, i);
			return;
		    }
#endif /* defined(SA_RLD) || (defined(KLD) && defined(__STATIC__)) */
		    /* check to make sure the alignment is reasonable */
		    if(s->align > MAXSECTALIGN){
			error_with_cur_obj("align (%u) of section %lu "
			    "(%.16s,%.16s) in load command %lu greater "
			    "than maximum section alignment (%d)", s->align,
			     j, s->segname, s->sectname, i, MAXSECTALIGN);
			return;
		    }
		    /* check the size and offset of the contents if it has any*/
		    if(mh->filetype != MH_DYLIB_STUB &&
		       section_type != S_ZEROFILL){
			check_size_offset_sect(s->size, s->offset, sizeof(char),
			    "size", "offset", i, j, s->segname, s->sectname);
			if(errors)
			    return;
		    }
		    /* check the relocation entries if it can have them */
		    if(section_type == S_ZEROFILL ||
		       section_type == S_CSTRING_LITERALS ||
		       section_type == S_4BYTE_LITERALS ||
		       section_type == S_8BYTE_LITERALS ||
		       section_type == S_NON_LAZY_SYMBOL_POINTERS){
			if(s->nreloc != 0){
			    error_with_cur_obj("section %lu (%.16s,%.16s) in "
				"load command %lu has relocation entries which "
				"it shouldn't for its type (flags)", j,
				 s->segname, s->sectname, i);
			    return;
			}
		    }
		    else{
			if(s->nreloc != 0){
			    if(mh->cputype == 0 && mh->cpusubtype == 0){
				error_with_cur_obj("section %lu (%.16s,%.16s)"
				    "in load command %lu has relocation entries"
				    " but the cputype and cpusubtype for the "
				    "object are not set", j, s->segname,
				    s->sectname, i);
				return;
			    }
			}
			else{
			    check_size_offset_sect(s->nreloc * sizeof(struct
				 relocation_info), s->reloff, sizeof(long),
				 "nreloc * sizeof(struct relocation_info)",
				 "reloff", i, j, s->segname, s->sectname);
			    if(errors)
				return;
			}
		    }
		    if(section_type == S_SYMBOL_STUBS && s->reserved2 == 0){
			error_with_cur_obj("symbol stub section %lu "
			    "(%.16s,%.16s) in load command %lu, sizeof stub in "
			    "reserved2 field is zero", j, s->segname,
			    s->sectname, i);
			return;
		    }
		    s++;
		}
		break;

	    case LC_SYMTAB:
		if(st != NULL){
		    error_with_cur_obj("contains more than one LC_SYMTAB load "
				       "command");
		    return;
		}
		st = (struct symtab_command *)lc;
		if(cur_obj->swapped)
		    swap_symtab_command(st, host_byte_sex);
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SYMTAB", i);
		    return;
		}
		check_size_offset(st->nsyms * sizeof(struct nlist), st->symoff,
				  sizeof(long), "nsyms * sizeof(struct nlist)",
				  "symoff", i);
		if(errors)
		    return;
		check_size_offset(st->strsize, st->stroff, sizeof(long),
				  "strsize", "stroff", i);
		if(errors)
		    return;
		cur_obj->symtab = st;
		break;

	    case LC_DYSYMTAB:
		if(dyst != NULL){
		    error_with_cur_obj("contains more than one LC_DYSYMTAB "
				       "load command");
		    return;
		}
		dyst = (struct dysymtab_command *)lc;
		if(cur_obj->swapped)
		    swap_dysymtab_command(dyst, host_byte_sex);
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_DYSYMTAB", i);
		    return;
		}
		check_size_offset(dyst->ntoc *
				  sizeof(struct dylib_table_of_contents),
				  dyst->tocoff, sizeof(long),
				"ntoc * sizeof(struct dylib_table_of_contents)",
				"tocoff", i);
		if(dyst->ntoc == 0 && cur_obj->dylib == TRUE &&
		   dyst->nextdefsym != 0)
		    warning_with_cur_obj("shared library has no table of "
					 "contents entries (can't resolve "
				         "symbols from it)");
		if(errors)
		    return;
		check_size_offset(dyst->nmodtab * sizeof(struct dylib_module),
				  dyst->modtaboff, sizeof(long),
				  "nmodtab * sizeof(struct dylib_module)",
				  "modtaboff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nextrefsyms *
				  sizeof(struct dylib_reference),
				  dyst->extrefsymoff, sizeof(long),
				 "nextrefsyms * sizeof(struct dylib_reference)",
				 "extrefsymoff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nindirectsyms * sizeof(long),
				  dyst->indirectsymoff, sizeof(long),
				  "nindirectsyms * sizeof(long)",
				  "indirectsymoff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nextrel *sizeof(struct relocation_info),
				  dyst->extreloff, sizeof(long),
				  "nextrel * sizeof(struct relocation_info)",
				  "extreloff", i);
		if(errors)
		    return;
		check_size_offset(dyst->nlocrel *sizeof(struct relocation_info),
				  dyst->locreloff, sizeof(long),
				  "nlocrel * sizeof(struct relocation_info)",
				  "locreloff", i);
		if(errors)
		    return;
		cur_obj->dysymtab = dyst;
		break;

	    case LC_ROUTINES:
		if(rc != NULL){
		    error_with_cur_obj("contains more than one LC_ROUTINES "
				       "load command");
		    return;
		}
		rc = (struct routines_command *)lc;
		if(cur_obj->swapped)
		    swap_routines_command(rc, host_byte_sex);
		if(rc->cmdsize != sizeof(struct routines_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ROUTINES", i);
		    return;
		}
		if(errors)
		    return;
		cur_obj->rc = rc;
		break;

	    case LC_SYMSEG:
		ss = (struct symseg_command *)lc;
		if(cur_obj->swapped)
		    swap_symseg_command(ss, host_byte_sex);
		if(ss->size != 0){
		    warning_with_cur_obj("contains obsolete LC_SYMSEG load "
			"command with non-zero size (produced with a pre-1.0 "
			"version of the compiler, please recompile)");
		}
		break;

	    case LC_IDFVMLIB:
		if(filetype != MH_FVMLIB){
		    error_with_cur_obj("LC_IDFVMLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for output filetypes other than MH_FVMLIB)");
		    return;
		}
		cur_obj->fvmlib_stuff = TRUE;
		fl = (struct fvmlib_command *)lc;
		if(cur_obj->swapped)
		    swap_fvmlib_command(fl, host_byte_sex);
		break;

	    case LC_LOADFVMLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_LOADFVMLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		fl = (struct fvmlib_command *)lc;
		if(cur_obj->swapped)
		    swap_fvmlib_command(fl, host_byte_sex);
		if(fl->cmdsize < sizeof(struct fvmlib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_LOADFVMLIB", i);
		    return;
		}
		if(fl->fvmlib.name.offset >= fl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i);
		    return;
		}
		fvmlib_name = (char *)fl + fl->fvmlib.name.offset;
		for(j = 0 ; j < fl->cmdsize - fl->fvmlib.name.offset; j++){
		    if(fvmlib_name[j] == '\0')
			break;
		}
		if(j >= fl->cmdsize - fl->fvmlib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->fvmlib_stuff = TRUE;
		break;

	    case LC_ID_DYLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLIB load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB && mh->filetype != MH_DYLIB_STUB){
		    error_with_cur_obj("LC_ID_DYLIB load command in non-"
			"%s filetype", mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		if(dlid != NULL){
		    error_with_cur_obj("malformed object (more than one "
			"LC_ID_DYLIB load command in %s file)", mh->filetype ==
			MH_DYLIB ? "MH_DYLIB" : "MH_DYLIB_STUB");
		    return;
		}
		dl = (struct dylib_command *)lc;
		dlid = dl;
		if(cur_obj->swapped)
		    swap_dylib_command(dl, host_byte_sex);
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ID_DYLIB", i);
		    return;
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i);
		    return;
		}
		dylib_id_name = (char *)dl + dl->dylib.name.offset;
		for(j = 0 ; j < dl->cmdsize - dl->dylib.name.offset; j++){
		    if(dylib_id_name[j] == '\0')
			break;
		}
		if(j >= dl->cmdsize - dl->dylib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
	    case LC_REEXPORT_DYLIB:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("%s load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			l.cmd == LC_LOAD_DYLIB ? "LC_LOAD_DYLIB" :
			"LC_LOAD_WEAK_DYLIB",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		dl = (struct dylib_command *)lc;
		if(cur_obj->swapped)
		    swap_dylib_command(dl, host_byte_sex);
		if(dl->cmdsize < sizeof(struct dylib_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
			"for %s", i, l.cmd == LC_LOAD_DYLIB ?  "LC_LOAD_DYLIB" :
			"LC_LOAD_WEAK_DYLIB");
		    return;
		}
		if(dl->dylib.name.offset >= dl->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i);
		    return;
		}
		dylib_name = (char *)dl + dl->dylib.name.offset;
		for(j = 0 ; j < dl->cmdsize - dl->dylib.name.offset; j++){
		    if(dylib_name[j] == '\0')
			break;
		}
		if(j >= dl->cmdsize - dl->dylib.name.offset){
		    error_with_cur_obj("library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		cur_obj->nload_dylibs++;
		break;

	    case LC_SUB_FRAMEWORK:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_SUB_FRAMEWORK load command in "
			"object file (should not be in an input file to the "
			"link editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB && mh->filetype != MH_DYLIB_STUB){
		    error_with_cur_obj("LC_SUB_FRAMEWORK load command in non-"
			"%s filetype", mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		if(sub != NULL){
		    error_with_cur_obj("malformed object (more than one "
			"LC_SUB_FRAMEWORK load command in %s file)",
			mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		sub = (struct sub_framework_command *)lc;
		if(cur_obj->swapped)
		    swap_sub_framework_command(sub, host_byte_sex);
		if(sub->cmdsize < sizeof(struct sub_framework_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SUB_FRAMEWORK", i);
		    return;
		}
		if(sub->umbrella.offset >= sub->cmdsize){
		    error_with_cur_obj("umbrella.offset of load command %lu "
				"extends past the end of the load command", i);
		    return;
		}
		umbrella_name = (char *)sub + sub->umbrella.offset;
		for(j = 0 ; j < sub->cmdsize - sub->umbrella.offset; j++){
		    if(umbrella_name[j] == '\0')
			break;
		}
		if(j >= sub->cmdsize - sub->umbrella.offset){
		    error_with_cur_obj("umbrella name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		break;

	    case LC_SUB_UMBRELLA:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_SUB_UMBRELLA load command in "
			"object file (should not be in an input file to the "
			"link editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB && mh->filetype != MH_DYLIB_STUB){
		    error_with_cur_obj("LC_SUB_UMBRELLA load command in non-"
			"%s filetype", mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		usub = (struct sub_umbrella_command *)lc;
		if(cur_obj->swapped)
		    swap_sub_umbrella_command(usub, host_byte_sex);
		if(usub->cmdsize < sizeof(struct sub_umbrella_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SUB_UMBRELLA", i);
		    return;
		}
		if(usub->sub_umbrella.offset >= usub->cmdsize){
		    error_with_cur_obj("sub_umbrella.offset of load command "
			"%lu extends past the end of the load command", i);
		    return;
		}
		sub_umbrella_name = (char *)usub + usub->sub_umbrella.offset;
		for(j = 0 ; j < usub->cmdsize - usub->sub_umbrella.offset; j++){
		    if(sub_umbrella_name[j] == '\0')
			break;
		}
		if(j >= usub->cmdsize - usub->sub_umbrella.offset){
		    error_with_cur_obj("sub_umbrella name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		break;

	    case LC_SUB_LIBRARY:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_SUB_LIBRARY load command in "
			"object file (should not be in an input file to the "
			"link editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB && mh->filetype != MH_DYLIB_STUB){
		    error_with_cur_obj("LC_SUB_LIBRARY load command in non-"
			"%s filetype", mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		lsub = (struct sub_library_command *)lc;
		if(cur_obj->swapped)
		    swap_sub_library_command(lsub, host_byte_sex);
		if(lsub->cmdsize < sizeof(struct sub_library_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SUB_LIBRARY", i);
		    return;
		}
		if(lsub->sub_library.offset >= lsub->cmdsize){
		    error_with_cur_obj("sub_library.offset of load command "
			"%lu extends past the end of the load command", i);
		    return;
		}
		sub_library_name = (char *)lsub + lsub->sub_library.offset;
		for(j = 0 ; j < lsub->cmdsize - lsub->sub_library.offset; j++){
		    if(sub_library_name[j] == '\0')
			break;
		}
		if(j >= lsub->cmdsize - lsub->sub_library.offset){
		    error_with_cur_obj("sub_library name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		break;

	    case LC_SUB_CLIENT:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_SUB_CLIENT load command in "
			"object file (should not be in an input file to the "
			"link editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" : "MH_DYLINKER");
		    return;
		}
		if(mh->filetype != MH_DYLIB && mh->filetype != MH_DYLIB_STUB){
		    error_with_cur_obj("LC_SUB_CLIENT load command in non-"
			"%s filetype", mh->filetype == MH_DYLIB ? "MH_DYLIB" :
			"MH_DYLIB_STUB");
		    return;
		}
		csub = (struct sub_client_command *)lc;
		if(cur_obj->swapped)
		    swap_sub_client_command(csub, host_byte_sex);
		if(csub->cmdsize < sizeof(struct sub_client_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SUB_CLIENT", i);
		    return;
		}
		if(csub->client.offset >= csub->cmdsize){
		    error_with_cur_obj("client.offset of load command %lu "
				"extends past the end of the load command", i);
		    return;
		}
		sub_client_name = (char *)csub + csub->client.offset;
		for(j = 0 ; j < csub->cmdsize - csub->client.offset; j++){
		    if(sub_client_name[j] == '\0')
			break;
		}
		if(j >= csub->cmdsize - csub->client.offset){
		    error_with_cur_obj("sub_client name of load command %lu "
				       "not null terminated", i);
		    return;
		}
		break;

	    case LC_ID_DYLINKER:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLINKER load command in object "
			"file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		if(mh->filetype != MH_DYLINKER){
		    error_with_cur_obj("LC_ID_DYLINKER load command in non-"
			"MH_DYLINKER filetype");
		    return;
		}
		if(dyldid != NULL){
		    error_with_cur_obj("malformed object (more than one "
			"LC_ID_DYLINKER load command in MH_DYLINKER file)");
		    return;
		}
		dyld = (struct dylinker_command *)lc;
		dyldid = dyld;
		if(cur_obj->swapped)
		    swap_dylinker_command(dyld, host_byte_sex);
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_ID_DYLINKER", i);
		    return;
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i);
		    return;
		}
		dylinker_name = (char *)dyld + dyld->name.offset;
		for(j = 0 ; j < dyld->cmdsize - dyld->name.offset; j++){
		    if(dylinker_name[j] == '\0')
			break;
		}
		if(j >= dyld->cmdsize - dyld->name.offset){
		    error_with_cur_obj("dynamic linker name of load command "
			"%lu not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_LOAD_DYLINKER:
		if(filetype == MH_FVMLIB ||
		   filetype == MH_DYLIB ||
		   filetype == MH_DYLINKER){
		    error_with_cur_obj("LC_LOAD_DYLINKER load command in object"
			" file (should not be in an input file to the link "
			"editor for the output file type %s)",
			filetype == MH_FVMLIB ? "MH_FVMLIB" :
			(filetype == MH_DYLIB ? "MH_DYLIB" : "MH_DYLINKER"));
		    return;
		}
		dyld = (struct dylinker_command *)lc;
		if(cur_obj->swapped)
		    swap_dylinker_command(dyld, host_byte_sex);
		if(dyld->cmdsize < sizeof(struct dylinker_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_LOAD_DYLINKER", i);
		    return;
		}
		if(dyld->name.offset >= dyld->cmdsize){
		    error_with_cur_obj("name.offset of load command %lu extends"
				       " past the end of the load command", i);
		    return;
		}
		dylinker_name = (char *)dyld + dyld->name.offset;
		for(j = 0 ; j < dyld->cmdsize - dyld->name.offset; j++){
		    if(dylinker_name[j] == '\0')
			break;
		}
		if(j >= dyld->cmdsize - dyld->name.offset){
		    error_with_cur_obj("dynamic linker name of load command "
			"%lu not null terminated", i);
		    return;
		}
		cur_obj->dylib_stuff = TRUE;
		break;

	    case LC_TWOLEVEL_HINTS:
		hints = (struct twolevel_hints_command *)lc;
		if(cur_obj->swapped)
		    swap_twolevel_hints_command(hints, host_byte_sex);
		if(hints->cmdsize != sizeof(struct twolevel_hints_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_TWOLEVEL_HINTS", i);
		    return;
		}
		check_size_offset(hints->nhints * sizeof(struct twolevel_hint),
				  hints->offset, sizeof(long),
				  "nhints * sizeof(struct twolevel_hint)",
				  "offset", i);
		if(errors)
		    return;
		break;

	    case LC_PREBIND_CKSUM:
		cs = (struct prebind_cksum_command *)lc;
		if(cur_obj->swapped)
		    swap_prebind_cksum_command(cs, host_byte_sex);
		if(cs->cmdsize != sizeof(struct prebind_cksum_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_PREBIND_CKSUM", i);
		    return;
		}
		if(errors)
		    return;
		break;

	    case LC_UUID:
		uuid = (struct uuid_command *)lc;
		if(cur_obj->swapped)
		    swap_uuid_command(uuid, host_byte_sex);
		if(uuid->cmdsize != sizeof(struct uuid_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_UUID", i);
		    return;
		}
		if(errors)
		    return;
#ifndef KLD
		/*
		 * If we see an input file with an LC_UUID load command then
		 * set up to emit one in the output.
		 */
		output_uuid_info.emit = TRUE;
#else
		/*
		 * For kernel extensions preserve the existing UUID in the
		 * output.
		 */
		output_uuid_info.uuid_command.cmd = LC_UUID;
		output_uuid_info.uuid_command.cmdsize =
						sizeof(struct uuid_command);
		memcpy(&(output_uuid_info.uuid_command.uuid[0]), uuid->uuid,
		       sizeof(uuid->uuid));
#endif

		break;

	    /*
	     * All of these are not looked at so the whole command is not
	     * swapped.  But we need to swap just the first part in memory in
	     * case they are in a dylib so other code can step over them.
	     */
	    case LC_UNIXTHREAD:
	    case LC_THREAD:
	    case LC_IDENT:
	    case LC_FVMFILE:
	    case LC_PREPAGE:
	    case LC_PREBOUND_DYLIB:
	    case LC_CODE_SIGNATURE:
	    case LC_SEGMENT_SPLIT_INFO:
		if(cur_obj->swapped)
		    swap_load_command(lc, host_byte_sex);
		break;

	    default:
		error_with_cur_obj("load command %lu unknown cmd field", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	}
	/*
	 * The -bundle_loader argument must be an object file with a dynamic
	 * symbol table command.
	 */
	if(bundle_loader){
	    if(cur_obj->dysymtab == NULL){
		error_with_cur_obj("bundle loader does not have an LC_DYSYMTAB "
				   "command");
		return;
	    }
	    cur_obj->bundle_loader = TRUE;
	}
	/*
	 * If this object does not have a symbol table command then set it's
	 * symtab pointer to the empty symtab.  This makes symbol number range
	 * checks in relocation cleaner.
	 */
	if(cur_obj->symtab == NULL){
	    if(cur_obj->dysymtab != NULL)
		error_with_cur_obj("contains LC_DYSYMTAB load command without "
				   "a LC_SYMTAB load command");
	    cur_obj->symtab = (struct symtab_command *)&empty_symtab;
	    cur_obj->dysymtab = (struct dysymtab_command *)&empty_dysymtab;
	}
	else{
	    if(cur_obj->dysymtab == NULL){
		cur_obj->dysymtab = (struct dysymtab_command *)&empty_dysymtab;
		if(cur_obj->rc != NULL)
		    error_with_cur_obj("contains LC_ROUTINES load command "
				       "without a LC_DYSYMTAB load command");
	    }
	    else{
		if(dyst->nlocalsym != 0 &&
		   dyst->ilocalsym > st->nsyms)
		    error_with_cur_obj("ilocalsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nlocalsym != 0 &&
		   dyst->ilocalsym + dyst->nlocalsym > st->nsyms)
		    error_with_cur_obj("ilocalsym plus nlocalsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->nextdefsym != 0 &&
		   dyst->iextdefsym > st->nsyms)
		    error_with_cur_obj("iextdefsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nextdefsym != 0 &&
		   dyst->iextdefsym + dyst->nextdefsym > st->nsyms)
		    error_with_cur_obj("iextdefsym plus nextdefsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->nundefsym != 0 &&
		   dyst->iundefsym > st->nsyms)
		    error_with_cur_obj("iundefsym in LC_DYSYMTAB load "
			"command extends past the end of the symbol table");
		if(dyst->nundefsym != 0 &&
		   dyst->iundefsym + dyst->nundefsym > st->nsyms)
		    error_with_cur_obj("iundefsym plus nundefsym in "
			"LC_DYSYMTAB load command extends past the "
			"end of the symbol table");

		if(dyst->ntoc != 0){
		    tocs =(struct dylib_table_of_contents *)(cur_obj->obj_addr +
							     dyst->tocoff);
		    if(cur_obj->swapped)
			swap_dylib_table_of_contents(tocs, dyst->ntoc,
						     host_byte_sex);
		    for(i = 0; i < dyst->ntoc; i++){
			if(tocs[i].symbol_index > st->nsyms)
			    error_with_cur_obj("symbol_index field of table of "
				"contents entry %lu past the end of the symbol "
				"table", i);
			if(tocs[i].module_index > dyst->nmodtab)
			    error_with_cur_obj("module_index field of table of "
				"contents entry %lu past the end of the module "
				"table", i);
		    }
		}

		if(dyst->nmodtab != 0){
		    mods = (struct dylib_module *)(cur_obj->obj_addr +
						   dyst->modtaboff);
		    if(cur_obj->swapped)
			swap_dylib_module(mods, dyst->nmodtab, host_byte_sex);
		    for(i = 0; i < dyst->nmodtab; i++){
			if(mods[i].module_name > st->strsize)
			    error_with_cur_obj("module_name field of module "
				"table entry %lu past the end of the string "
				"table", i);
			if(mods[i].iextdefsym > st->nsyms)
			    error_with_cur_obj("iextdefsym field of module "
				"table entry %lu past the end of the symbol "
				"table", i);
			if(mods[i].iextdefsym +
			   mods[i].nextdefsym > st->nsyms)
			    error_with_cur_obj("iextdefsym field plus "
				"nextdefsym field of module table entry %lu "
				"past the end of the symbol table", i);
			if(mods[i].irefsym > dyst->nextrefsyms)
			    error_with_cur_obj("irefsym field of module table "
				"entry %lu past the end of the reference table",
				i);
			if(mods[i].irefsym +
			   mods[i].nrefsym > dyst->nextrefsyms)
			    error_with_cur_obj("irefsym field plus "
				"nrefsym field of module table entry %lu past "
				"the end of the reference table", i);
			if(mods[i].ilocalsym > st->nsyms)
			    error_with_cur_obj("ilocalsym field of module "
				"table entry %lu past the end of the symbol "
				"table", i);
			if(mods[i].ilocalsym +
			   mods[i].nlocalsym > st->nsyms)
			    error_with_cur_obj("ilocalsym field plus "
				"nlocalsym field of module table entry %lu "
				"past the end of the symbol table", i);
		    }
		}

		if(dyst->nextrefsyms != 0){
		    refs = (struct dylib_reference *)(cur_obj->obj_addr +
						      dyst->extrefsymoff);
		    if(cur_obj->swapped)
			swap_dylib_reference(refs, dyst->nextrefsyms,
					     host_byte_sex);
		    for(i = 0; i < dyst->nextrefsyms; i++){
			if(refs[i].isym > st->nsyms)
			    error_with_cur_obj("isym field of reference table "
				"entry %lu past the end of the symbol table",i);
		    }
		}

		if(dyst->nindirectsyms != 0){
		    indirect_symtab = (uint32_t *)(cur_obj->obj_addr +
					    dyst->indirectsymoff);
		    if(cur_obj->swapped)
			swap_indirect_symbols(indirect_symtab,
			    dyst->nindirectsyms, host_byte_sex);
		    for(i = 0; i < dyst->nindirectsyms; i++){
			if(indirect_symtab[i] != INDIRECT_SYMBOL_LOCAL &&
			   indirect_symtab[i] != INDIRECT_SYMBOL_ABS){
			    if(indirect_symtab[i] > st->nsyms)
				error_with_cur_obj("indirect symbol table entry"
				    " %lu past the end of the symbol table", i);
			}
		    }
		}

		if(rc != NULL && rc->init_module > dyst->nmodtab)
		    error_with_cur_obj("init_module field of LC_ROUTINES load "
			"command past the end of the module table");
	    }
	}
	/*
	 * If this is a MH_DYLIB or MH_DYLIB_STUB file then a single
	 * LC_ID_DYLIB command must be seen to identify the library.
	 */
	if((mh->filetype == MH_DYLIB || mh->filetype == MH_DYLIB_STUB) &&
	   dlid == NULL){
	    error_with_cur_obj("malformed object (no LC_ID_DYLIB load command "
			       "in %s file)", mh->filetype == MH_DYLIB ?
			       "MH_DYLIB" : "MH_DYLIB_STUB");
	    return;
	}
	/*
	 * If this is a MH_DYLINKER file then a single LC_ID_DYLINKER command
	 * must be seen to identify the library.
	 */
	if(mh->filetype == MH_DYLINKER && dyldid == NULL){
	    error_with_cur_obj("malformed object (no LC_ID_DYLINKER load "
		"command in MH_DYLINKER file)");
	    return;
	}
#ifndef RLD
	/*
	 * If we find a sub_framework_command for a dynamic library that was
	 * directly linked against then we must generate a link error
	 * if what we are building is not the umbrella framework for this
	 * subframework or it is not a another subframework of the same
	 * umbrella framework.
	 *
	 * The following error is a non-standard form but it is exactly what is
	 * specified when this feature was added.
	 */
	if(indirect_dylib == FALSE && sub != NULL &&
	   (umbrella_framework_name == NULL ||
	    strcmp(umbrella_name, umbrella_framework_name) != 0)){

	    short_name = guess_short_name(dylib_id_name, &is_framework,
					  &has_suffix);
	    /*
	     * This may still be an allowable of this sub framework.  Check the
	     * LC_SUB_CLIENT commands to see if this client name is listed.
	     */
	    if(umbrella_framework_name != NULL)
		this_client_name = umbrella_framework_name;
	    else
		this_client_name = client_name;
	    allowable_client = FALSE;
	    if(this_client_name != NULL){
		lc = load_commands;
		for(i = 0; i < mh->ncmds; i++){
		    if(lc->cmd == LC_SUB_CLIENT){
			csub = (struct sub_client_command *)lc;
			sub_client_name = (char *)csub + csub->client.offset;
			if(strcmp(sub_client_name, this_client_name) == 0){
			    allowable_client = TRUE;
			    break;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	    if(allowable_client == FALSE){
		if(short_name != NULL && is_framework == TRUE)
		    error_with_cur_obj("'%s.framework' is a subframework.  "
			"Link against the umbrella framework '%s.framework' "
			"instead.", short_name, umbrella_name);
		else
		    error_with_cur_obj("is a subframework.  Link against the "
			"umbrella framework '%s.framework' instead.",
			umbrella_name);
	    }
	}
#endif /* RLD */
}

#ifdef RLD
/*
 * merge_base_program() does the pass1 functions for the base program that
 * called rld_load() using it's SEG_LINKEDIT.  It does the same things as the
 * routines pass1(),check_obj() and merge() except that the offset are assumed
 * to be correct in most cases (if they weren't the program would not be
 * executing).  If seg_linkedit is NULL it then the symbol and string table
 * passed in is used instead of seg_linkedit.
 *
 * A hand crafted object structure is created so to work with the rest of the
 * code.  Like check_obj() a number of things are filled in in the object
 * structrure including: the symtab field, the section_maps and nsection_maps
 * fields (this routine allocates the section_map structures and fills them in
 * too), all fvmlib stuff is ignored since the output is loaded as well as
 * linked.  The file offsets in symtab are faked since this is not a file mapped
 * into memory but rather a running process.  This involves setting where the
 * object starts to the address of the _mh_execute_header and calcating the
 * file offset of the symbol and string table as the differences of the
 * addresses from the _mh_execute_header.  This makes using the rest of the
 * code easy.
 */
__private_extern__
void
merge_base_program(
char *basefile_name,
struct mach_header *basefile_addr,
struct segment_command *seg_linkedit,
struct nlist *symtab,
unsigned long nsyms,
char *strtab,
unsigned long strsize)
{
    unsigned long i, j, section_type;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct dysymtab_command *dyst;

    static const struct symtab_command empty_symtab = { 0 };
    static struct symtab_command base_program_symtab = { 0 };

	/*
	 * Hand craft the object structure as in pass1().  Note the obj_size
	 * feild should never be tested against since this is not a file mapped
	 * into memory but rather a running program.
	 */
	cur_obj = new_object_file();
	cur_obj->file_name = basefile_name;
	cur_obj->obj_addr = (char *)basefile_addr;
	cur_obj->obj_size = 0;
	cur_obj->from_fat_file = FALSE;
	base_obj = cur_obj;
	/* Set the output cpu types from the base program's header */
	mh = basefile_addr;
	arch_flag.cputype = mh->cputype;
	arch_flag.cpusubtype = mh->cpusubtype;

	/*
	 * Go through the load commands and do what would be done in check_obj()
	 * but not checking for offsets.
	 */
	load_commands = (struct load_command *)((char *)cur_obj->obj_addr +
			    sizeof(struct mach_header));
	st = NULL;
	dyst = NULL;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmdsize % sizeof(long) != 0){
		error_with_cur_obj("load command %lu size not a multiple of "
				   "sizeof(long)", i);
		return;
	    }
	    if(lc->cmdsize <= 0){
		error_with_cur_obj("load command %lu size is less than or equal"
				   " to zero", i);
		return;
	    }
	    if((char *)lc + lc->cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error_with_cur_obj("load command %lu extends past end of all "
				   "load commands", i);
		return;
	    }
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error_with_cur_obj("cmdsize field of load command %lu is "
				       "inconsistant for a segment command "
				       "with the number of sections it has", i);
		    return;
		}
		if(sg->flags == SG_FVMLIB){
		    if(sg->nsects != 0){
			error_with_cur_obj("SG_FVMLIB segment %.16s contains "
					   "sections and shouldn't",
					   sg->segname);
			return;
		    }
		    break;
		}
		/*
		 * Segments without sections are an error to see on input except
		 * for the segments created by the link-editor (which are
		 * recreated).
		 */
		if(sg->nsects == 0){
		    if(strcmp(sg->segname, SEG_PAGEZERO) != 0 &&
		       strcmp(sg->segname, SEG_LINKEDIT) != 0){
			error_with_cur_obj("segment %.16s contains no "
					   "sections and can't be link-edited",
					   sg->segname);
			return;
		    }
		}
		else{
		    /*
		     * Doing a reallocate here is not bad beacuse in the
		     * normal case this is an MH_OBJECT file type and has only
		     * one section.  So this only gets done once per object.
		     */
		    cur_obj->section_maps = reallocate(cur_obj->section_maps,
					(cur_obj->nsection_maps + sg->nsects) *
					sizeof(struct section_map));
		    memset(cur_obj->section_maps + cur_obj->nsection_maps, '\0',
			   sg->nsects * sizeof(struct section_map));
		}
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    cur_obj->section_maps[cur_obj->nsection_maps++].s = s;
		    /* check to see that segment name in the section structure
		       matches the one in the segment command if this is not in
		       an MH_OBJECT filetype */
		    if(mh->filetype != MH_OBJECT &&
		       strcmp(sg->segname, s->segname) != 0){
			error_with_cur_obj("segment name %.16s of section %lu "
				"(%.16s,%.16s) in load command %lu does not "
				"match segment name %.16s", s->segname, j,
				s->segname, s->sectname, i, sg->segname);
			return;
		    }
		    /* check to see that flags (type) of this section is some
		       thing the link-editor understands */
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type != S_REGULAR &&
		       section_type != S_ZEROFILL &&
		       section_type != S_CSTRING_LITERALS &&
		       section_type != S_4BYTE_LITERALS &&
		       section_type != S_8BYTE_LITERALS &&
		       section_type != S_LITERAL_POINTERS &&
		       section_type != S_NON_LAZY_SYMBOL_POINTERS &&
		       section_type != S_LAZY_SYMBOL_POINTERS &&
		       section_type != S_SYMBOL_STUBS &&
		       section_type != S_COALESCED &&
		       section_type != S_MOD_INIT_FUNC_POINTERS &&
		       section_type != S_MOD_TERM_FUNC_POINTERS &&
		       section_type != S_DTRACE_DOF){
			error_with_cur_obj("unknown flags (type) of section %lu"
					   " (%.16s,%.16s) in load command %lu",
					   j, s->segname, s->sectname, i);
			return;
		    }
		    /* check to make sure the alignment is reasonable */
		    if(s->align > MAXSECTALIGN){
			error_with_cur_obj("align (%u) of section %lu "
			    "(%.16s,%.16s) in load command %lu greater "
			    "than maximum section alignment (%d)", s->align,
			     j, s->segname, s->sectname, i, MAXSECTALIGN);
			return;
		    }
		    s++;
		}
		break;

	    case LC_SYMTAB:
		if(st != NULL){
		    error_with_cur_obj("contains more than one LC_SYMTAB load "
				       "command");
		    return;
		}
		st = (struct symtab_command *)lc;
		if(st->cmdsize != sizeof(struct symtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_SYMTAB", i);
		    return;
		}
		break;

	    case LC_DYSYMTAB:
		if(dyst != NULL){
		    error_with_cur_obj("contains more than one LC_DYSYMTAB "
				       "load command");
		    return;
		}
		dyst = (struct dysymtab_command *)lc;
		if(dyst->cmdsize != sizeof(struct dysymtab_command)){
		    error_with_cur_obj("cmdsize of load command %lu incorrect "
				       "for LC_DYSYMTAB", i);
		    return;
		}
		break;

	    case LC_SYMSEG:
	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
	    case LC_REEXPORT_DYLIB:
	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
	    case LC_UNIXTHREAD:
	    case LC_THREAD:
	    case LC_IDENT:
	    case LC_FVMFILE:
	    case LC_PREPAGE:
		break;

	    default:
		error_with_cur_obj("load command %lu unknown cmd", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * Now the slightly tricky part of faking up a symtab command that
	 * appears to have offsets to the symbol and string table when added
	 * to the cur_obj->obj_addr get the correct addresses.
	 */
	if(seg_linkedit == NULL){
	    base_program_symtab.cmd = LC_SYMTAB;
	    base_program_symtab.cmdsize = sizeof(struct symtab_command);
	    base_program_symtab.symoff = (long)symtab - (long)cur_obj->obj_addr;
	    base_program_symtab.nsyms = nsyms;
	    base_program_symtab.stroff = (long)strtab - (long)cur_obj->obj_addr;
	    base_program_symtab.strsize = strsize;
	    cur_obj->symtab = &base_program_symtab;
	}
	else if(st != NULL && st->nsyms != 0){
	    if(st->symoff < seg_linkedit->fileoff ||
	       st->symoff + st->nsyms * sizeof(struct nlist) >
				seg_linkedit->fileoff + seg_linkedit->filesize){
		error_with_cur_obj("symbol table is not contained in "
				   SEG_LINKEDIT " segment");
		return;
	    }
	    if(st->stroff < seg_linkedit->fileoff ||
	       st->stroff + st->strsize >
				seg_linkedit->fileoff + seg_linkedit->filesize){
		error_with_cur_obj("string table is not contained in "
				   SEG_LINKEDIT " segment");
		return;
	    }
	    base_program_symtab = *st;
	    base_program_symtab.symoff = (seg_linkedit->vmaddr + (st->symoff -
					  seg_linkedit->fileoff)) -
					  (long)cur_obj->obj_addr;
	    base_program_symtab.stroff = (seg_linkedit->vmaddr + (st->stroff -
					  seg_linkedit->fileoff)) -
					  (long)cur_obj->obj_addr;
	    cur_obj->symtab = &base_program_symtab;
	}
	/*
	 * If this object does not have a symbol table command then set it's
	 * symtab pointer to the empty symtab.  This makes symbol number range
	 * checks in relocation cleaner.
	 */
	else{
	    cur_obj->symtab = (struct symtab_command *)&empty_symtab;
	}

	/*
	 * Now finish with the base program by doing what would be done in
	 * merge() by merging the base program's sections and symbols.
	 */
	/* merged the base program's sections */
	merge_sections();

	/* merged the base program's symbols */
	merge_symbols();
}
#endif /* RLD */

/*
 * check_size_offset() is used by check_cur_obj() to check a pair of sizes,
 * and offsets from the object file to see it they are aligned correctly and
 * containded with in the file.
 */
static
void
check_size_offset(
unsigned long size,
unsigned long offset,
unsigned long align,
char *size_str,
char *offset_str,
unsigned long cmd)
{
	if(size != 0){
	    if(offset % align != 0){
#ifdef mc68000
		/*
		 * For the mc68000 the alignment is only a warning because it
		 * can deal with all accesses on bad alignment.
		 */
		warning_with_cur_obj("%s in load command %lu not aligned on %lu"
				     " byte boundary", offset_str, cmd, align);
#else /* !defined(mc68000) */
		error_with_cur_obj("%s in load command %lu not aligned on %lu "
				   "byte boundary", offset_str, cmd, align);
#endif /* mc68000 */
		return;
	    }
	    if(offset > cur_obj->obj_size){
		error_with_cur_obj("%s in load command %lu extends past the "
				   "end of the file", offset_str, cmd);
		return;
	    }
	    if(offset + size > cur_obj->obj_size){
		error_with_cur_obj("%s plus %s in load command %lu extends past"
				   " the end of the file", offset_str, size_str,
				   cmd);
		return;
	    }
	}
}

/*
 * check_size_offset_sect() is used by check_cur_obj() to check a pair of sizes,
 * and offsets from a section in the object file to see it they are aligned
 * correctly and containded with in the file.
 */
static
void
check_size_offset_sect(
unsigned long size,
unsigned long offset,
unsigned long align,
char *size_str,
char *offset_str,
unsigned long cmd,
unsigned long sect,
char *segname,
char *sectname)
{
	if(size != 0){
	    if(offset % align != 0){
#ifdef mc68000
		/*
		 * For the mc68000 the alignment is only a warning because it
		 * can deal with all accesses on bad alignment.
		 */
		warning_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu not aligned on %lu byte boundary", offset_str,
		    sect, segname, sectname, cmd, align);
#else /* !defined(mc68000) */
		error_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu not aligned on %lu byte boundary", offset_str,
		    sect, segname, sectname, cmd, align);
#endif /* mc68000 */
		return;
	    }
	    if(offset > cur_obj->obj_size){
		error_with_cur_obj("%s of section %lu (%.16s,%.16s) in load "
		    "command %lu extends past the end of the file", offset_str,
		    sect, segname, sectname, cmd);
		return;
	    }
	    if(offset + size > cur_obj->obj_size){
		error_with_cur_obj("%s plus %s of section %lu (%.16s,%.16s) "
		    "in load command %lu extends past the end of the file",
		    offset_str, size_str, sect, segname, sectname, cmd);
		return;
	    }
	}
}

#ifndef RLD
/*
 * collect_base_obj_segments() collects the segments from the base file on a
 * merged segment list used for overlap checking in
 * check_for_overlapping_segments().
 */
static
void
collect_base_obj_segments(void)
{
    unsigned long i;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;

	mh = (struct mach_header *)base_obj->obj_addr;
	load_commands = (struct load_command *)((char *)base_obj->obj_addr +
			    sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		add_base_obj_segment(sg, base_obj->file_name);
		break;

	    default:
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * add_base_obj_segment() adds the specified segment to the list of
 * base_obj_segments as comming from the specified base filename.
 */
static
void
add_base_obj_segment(
struct segment_command *sg,
char *filename)
{
    struct merged_segment **p, *msg;

	p = &base_obj_segments;
	while(*p){
	    msg = *p;
	    p = &(msg->next);
	}
	*p = allocate(sizeof(struct merged_segment));
	msg = *p;
	memset(msg, '\0', sizeof(struct merged_segment));
	msg->sg = *sg;
	msg->filename = filename;
}

#ifndef KLD
/*
 * symbol_address_compare takes two pointers to pointers to symbol entries,
 * and returns an ordering on them by address.  It also looks for STABS
 * symbols and if found sets *(int *)fail_p.
 */
static int
symbol_address_compare (void *fail_p, const void *a_p, const void *b_p)
{
  const struct nlist * const * aa = a_p;
  const struct nlist * a = *aa;
  const struct nlist * const * bb = b_p;
  const struct nlist * b = *bb;

  if (a->n_type & N_STAB)
    *(int *)fail_p = 1;
  if ((a->n_type & N_TYPE) != (b->n_type & N_TYPE))
    return (a->n_type & N_TYPE) < (b->n_type & N_TYPE) ? -1 : 1;
  if (a->n_value != b->n_value)
    {
      /* This is before the symbols are swapped, so this routine must
	 swap what it needs.  */
      if (cur_obj->swapped)
	return SWAP_LONG (a->n_value) < SWAP_LONG (b->n_value) ? -1 : 1;
      else
	return a->n_value < b->n_value ? -1 : 1;
    }

  else
    return 0;
}

/*
 * read_dwarf_info looks for DWARF sections in cur_obj and if found,
 * fills in the dwarf_name and dwarf_comp_dir fields in cur_obj.
 *
 * Once this routine has completed, no section marked with
 * S_ATTR_DEBUG will be needed in the link, and so if the object file
 * layout is appropriate those sections can be unmapped.
 */
static void
read_dwarf_info(void)
{
  enum { chunksize = 256 };

  struct ld_chunk {
    struct ld_chunk * next;
    size_t filedata[chunksize];
  };

  int little_endian;
  struct section * debug_info = NULL;
  struct section * debug_line = NULL;
  struct section * debug_abbrev = NULL;

  const char * name;
  const char * comp_dir;
  uint64_t stmt_list;
  int has_stabs = FALSE;

  struct line_reader_data * lrd;
  /* 'st' is the symbol table, 'sst' is pointers into that table
     sorted by the symbol's address.  */
  struct nlist *st;
  struct nlist **sst;

  struct ld_chunk * chunks;
  struct ld_chunk * lastchunk;
  size_t * symdata;
  size_t lastused;
  size_t num_line_syms = 0;
  size_t dwarf_source_i;
  size_t max_files = 0;

  struct line_info li_start, li_end;

  size_t i;

#if __LITTLE_ENDIAN__
  little_endian = !cur_obj->swapped;
#else
  little_endian = cur_obj->swapped;
#endif

  /* Find the sections containing the DWARF information we need.  */
  for (i = 0; i < cur_obj->nsection_maps; i++)
    {
      struct section * s = cur_obj->section_maps[i].s;

      if (strncmp (s->segname, "__DWARF", 16) != 0)
	continue;
      if (strncmp (s->sectname, "__debug_info", 16) == 0)
	debug_info = s;
      else if (strncmp (s->sectname, "__debug_line", 16) == 0)
	debug_line = s;
      else if (strncmp (s->sectname, "__debug_abbrev", 16) == 0)
	debug_abbrev = s;
    }

  /* No DWARF means nothing to do.
     However, no line table may just mean that there's no code in this
     file, in which case processing continues.  */
  if (! debug_info || ! debug_abbrev || ! cur_obj->symtab
      || debug_info->size == 0)
    return;

  /* Read the debug_info (and debug_abbrev) sections, and determine
     the name and working directory to put in the SO stabs, and also
     the offset into the line number section.  */
  if (read_comp_unit ((const uint8_t *) cur_obj->obj_addr + debug_info->offset,
		      debug_info->size,
		      ((const uint8_t *)cur_obj->obj_addr
		       + debug_abbrev->offset),
		      debug_abbrev->size, little_endian,
		      &name, &comp_dir, &stmt_list)
      && name) {
    cur_obj->dwarf_name = strdup (name);
    if (comp_dir)
      cur_obj->dwarf_comp_dir = strdup (comp_dir);
    else
      cur_obj->dwarf_comp_dir = NULL;
  } else {
    warning_with_cur_obj("could not understand DWARF debug information");
    return;
  }

  /* If there is no line table, don't do any more processing.  No N_FUN
     or N_SOL stabs will be output.  */
  if (! debug_line || stmt_list == (uint64_t) -1)
    return;

  /* At this point check_symbol has not been called, so all the code below
     that processes symbols must not assume that the symbol's contents
     make any sense.  */

  /* Generate the line number information into
     cur_obj->dwarf_source_data.  The format of dwarf_source_data is a
     sequence of size_t-sized words made up of subsequences.  Each
     subsequence describes the source files which the debug_line information
     says contributed to the code of the entity starting at a particular
     symbol.

     The subsequences are sorted by the index of the symbol to which they
     refer.  The format of a subsequence is a word containing the index
     of the symbol, a word giving the end of the entity starting with
     that symbol, and then one or more words which are file numbers
     with their high bit set.

     A file number is simply an index into cur_obj->dwarf_paths; each
     dwarf_paths entry is either NULL (if not used) or the path of the
     source file.  */

  st = (struct nlist *)(cur_obj->obj_addr + cur_obj->symtab->symoff);
  /* The processing is easier if we have a list of symbols sorted by
     address.  */
  sst = allocate (sizeof (struct nlist *) * cur_obj->symtab->nsyms);
  for (i = 0; i < cur_obj->symtab->nsyms; i++)
    sst[i] = st + i;
  qsort_r (sst, cur_obj->symtab->nsyms, sizeof (struct nlist *), &has_stabs,
	   symbol_address_compare);
  if (has_stabs) {
    error_with_cur_obj("has both STABS and DWARF debugging info");
    free (sst);
    return;
  }

  if (stmt_list >= debug_line->size){
    warning_with_cur_obj("offset in DWARF debug_info for line number data is too large");
    free (sst);
    return;
  }

  lrd = line_open ((const uint8_t *) cur_obj->obj_addr + debug_line->offset
		   + stmt_list,
		   debug_line->size - stmt_list, little_endian);
  if (! lrd) {
    warning_with_cur_obj("could not understand DWARF line number information");
    free (sst);
    return;
  }

  /* In this first pass, we process the symbols in address order and
     put them into a linked list of chunks to avoid having to call
     reallocate().  */
  chunks = allocate (sizeof (*chunks));
  chunks->next = NULL;
  lastchunk = chunks;
  lastused = 0;
  /* There's also an index by symbol number so we can easily sort them
     later.  */
  symdata = allocate (sizeof (size_t) * cur_obj->symtab->nsyms);
  memset (symdata, 0, sizeof (size_t) * cur_obj->symtab->nsyms);

  li_start.end_of_sequence = TRUE;
  for (i = 0; i < cur_obj->symtab->nsyms; i++){
    struct nlist * s = sst[i];
    size_t idx = s - st;
    struct ld_chunk * symchunk;
    size_t n_value = s->n_value;

    size_t limit, max_limit;

    if ((s->n_type & N_TYPE) != N_SECT
	|| s->n_sect == NO_SECT)
      continue;
    /* Looking for line number information that isn't there is
       expensive, so we only look for line numbers for symbols in
       sections that might contain instructions.  */
    if (! (cur_obj->section_maps[s->n_sect - 1].s->flags
	   & (S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS)))
      continue;

    if (i + 1 < cur_obj->symtab->nsyms
	&& (sst[i + 1]->n_type & N_TYPE) == N_SECT)
      limit = sst[i + 1]->n_value;
    else
      limit = (uint32_t) -1;

    if (cur_obj->swapped){
      n_value = SWAP_LONG (n_value);
      limit = SWAP_LONG (limit);
    }

    if (li_start.pc > n_value || li_end.pc <= n_value
	|| li_start.end_of_sequence)
      {
	if (! line_find_addr (lrd, &li_start, &li_end, n_value)) {
	  if (li_start.end_of_sequence)
	    continue;
	  else
	    goto line_err;
	}

	/* Make the start...end range as large as possible.  */
	if (li_start.file == li_end.file && ! li_end.end_of_sequence)
	  if (! line_next (lrd, &li_end, line_stop_file))
	    goto line_err;
      }

    symdata[idx] = lastused + 1;

    symchunk = lastchunk;
    num_line_syms++;

    max_limit = 0;
    for (;;)
      {
	size_t j;
	struct ld_chunk * curchunk = symchunk;

	/* File numbers this large are probably an error, and if not
	   they're certainly too large for this linker to handle.  */
	if (li_start.file >= 0x10000000)
	  goto line_err;

	if (li_end.pc > max_limit)
	  max_limit = li_end.pc;

	for (j = symdata[idx]; j < lastused; j++)
	  {
	    if (j % chunksize == 0)
	      curchunk = curchunk->next;
	    if (curchunk->filedata[j % chunksize] == li_start.file)
	      goto skipfile;
	  }
	lastchunk->filedata[lastused % chunksize] = li_start.file;
	lastused++;
	if (li_start.file >= max_files)
	  max_files = li_start.file + 1;
	if (lastused % chunksize == 0)
	  {
	    lastchunk->next = allocate (sizeof (*chunks));
	    lastchunk = lastchunk->next;
	    lastchunk->next = NULL;
	  }

      skipfile:
	if (li_end.pc >= limit || li_end.end_of_sequence)
	  break;
	li_start = li_end;
	if (! line_next (lrd, &li_end, line_stop_file))
	  goto line_err;
      }

    /* The function ends at either the next symbol, or after the last
       byte which has a line number.  */
    if (limit > max_limit)
      limit = max_limit;

    lastchunk->filedata[lastused % chunksize] = (size_t) -1;
    lastused++;
    if (lastused % chunksize == 0)
      {
	lastchunk->next = allocate (sizeof (*chunks));
	lastchunk = lastchunk->next;
	lastchunk->next = NULL;
      }
    lastchunk->filedata[lastused % chunksize] = limit - n_value;
    lastused++;
    if (lastused % chunksize == 0)
      {
	lastchunk->next = allocate (sizeof (*chunks));
	lastchunk = lastchunk->next;
	lastchunk->next = NULL;
      }
  }

  free (sst);

  /* Now take the data in the chunks out ordered by symbol index, so
     the final result can be iterated through easily.  */

  cur_obj->dwarf_paths = allocate (max_files * sizeof (const char *));
  memset (cur_obj->dwarf_paths, 0, max_files * sizeof (const char *));
  cur_obj->dwarf_num_paths = max_files;

  cur_obj->dwarf_source_data = allocate ((lastused + num_line_syms*2 + 1)
					 * sizeof (size_t));
  dwarf_source_i = 0;
  for (i = 0; i < cur_obj->symtab->nsyms; i++)
    if (symdata[i]) {
      struct ld_chunk * symchunk = chunks;
      size_t j;
      size_t * limit_space;

      cur_obj->dwarf_source_data[dwarf_source_i++] = i;
      limit_space = cur_obj->dwarf_source_data + dwarf_source_i++;
      for (j = 0; j < (symdata[i] - 1) / chunksize; j++)
	symchunk = symchunk->next;
      for (j = symdata[i] - 1;
	   symchunk->filedata[j % chunksize] != (size_t) -1;
	   j++) {
	size_t filenum = symchunk->filedata[j % chunksize];
	cur_obj->dwarf_source_data[dwarf_source_i++] = filenum | 0x80000000;
	if (! cur_obj->dwarf_paths[filenum])
	  cur_obj->dwarf_paths[filenum] = line_file (lrd, filenum);
	if (j % chunksize == chunksize - 1)
	  symchunk = symchunk->next;
      }
      j++;
      if (j % chunksize == 0)
	symchunk = symchunk->next;
      *limit_space = symchunk->filedata[j % chunksize];
    }
  /* Terminate with 0x7fffffff, which is larger than any valid symbol
     index.  */
  cur_obj->dwarf_source_data[dwarf_source_i++] = 0x7fffffff;

  /* Finish up by freeing everything.  */
  line_free (lrd);
  free (symdata);
  lastchunk = chunks;
  while (lastchunk) {
    struct ld_chunk * tmp = lastchunk->next;
    free (lastchunk);
    lastchunk = tmp;
  };

  return;

 line_err:
  line_free (lrd);
  free (sst);
  free (symdata);
  lastchunk = chunks;
  while (lastchunk) {
    struct ld_chunk * tmp = lastchunk->next;
    free (lastchunk);
    lastchunk = tmp;
  };

  warning_with_cur_obj("invalid DWARF line number information");
  return;
}
#endif

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
