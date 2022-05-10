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
#undef moninitrld
#endif /* SHLIB */
#ifdef RLD
/*
 * This file contains the functions of the RLD package.
 */
#include <mach-o/loader.h>
#ifndef __OPENSTEP__
extern const struct segment_command *getsegbyname(const char *segname);
extern const struct section *getsectbynamefromheader(
	const struct mach_header *mhp,
	const char *segname,
	const char *sectname);
#endif
#if !(defined(KLD) && defined(__STATIC__))
#include <libc.h>
#include <stdio.h>
#include <mach/mach.h>
#include "stuff/vm_flush_cache.h"
#else /* defined(KLD) && defined(__STATIC__) */
#include <stdlib.h>
#include <unistd.h>
#include <mach/kern_return.h>
#include <mach/vm_map.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/openstep_mach.h"
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#ifdef KLD
#include <mach-o/kld.h>
#else /* !defined(KLD) */
#include <mach-o/rld.h>
#include <streams/streams.h>
#include <objc/zone.h>
#endif /* KLD */
#include <mach-o/rld_state.h>
#include <mach-o/ldsyms.h>
#define __darwin_i386_float_state i386_float_state
#include "stuff/arch.h"
#include "stuff/best_arch.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sections.h"
#include "symbols.h"
#include "pass1.h"
#include "layout.h"
#include "pass2.h"
#include "sets.h"
#ifdef SA_RLD
#include "mach-o/sarld.h"
#endif /* SA_RLD */
#ifdef KLD
#include "mach-o/kld.h"
#endif /* KLD */
#if defined(SA_RLD)
#include "standalone/libsa.h"
#endif

#ifndef __OPENSTEP__
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
#include <crt_externs.h>
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */
#else /* defined(__OPENSTEP__) */
#ifdef __DYNAMIC__
#include "mach-o/dyld.h"
#endif /* __DYNAMIC__ */
#endif /* !defined(__OPENSTEP__) */

/*
 * The user's address function to be called in layout to get the address of
 * where to link edit the result.
 */
__private_extern__
unsigned long (*address_func)(unsigned long size, unsigned long headers_size) =
									   NULL;

static
enum strip_levels kld_requested_strip_level = STRIP_ALL;

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/*
 * The function pointer passed to moninitrld() to do profiling of rld loaded
 * code.  If this function pointer is not NULL at the time of an rld_load()
 * called it is called indirectly to set up the profiling buffers.
 */
static void (*rld_monaddition)(char *lowpc, char *highpc) = NULL;
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

/*
 * This is a hack to let the code in layout_segments() in layout.c to know if
 * the special output_file flag RLD_DEBUG_OUTPUT_FILENAME is being used so it
 * can call the address_func (above) with the allocated_size of memory
 * (including the symbol table) so that it can deallocate it correctly.
 */
__private_extern__ long RLD_DEBUG_OUTPUT_FILENAME_flag = 0;

#ifndef KLD
/*
 * The stream passed in to the rld routines to print errors on.
 */
static NXStream *error_stream = NULL;
#endif /* !defined(KLD) */

#if !defined(SA_RLD) && !defined(KLD)
/*
 * The zone allocation is done from.
 */
static NXZone *zonep = NULL;
#endif /* !defined(SA_RLD) && !defined(KLD) */

/*
 * The jump buffer to get back to rld_load() or rld_unload() used by the error
 * routines.
 */
static jmp_buf rld_env;

/*
 * Indicator that a fatal error occured and that no more processing will be
 * done on all future calls to protect calls from causing a core dump.
 */
static volatile int fatals = 0;

/*
 * The base file name passed to rld_load_basefile() if it has been called.
 * This points at an allocated copy of the name.
 */
__private_extern__ char *base_name = NULL;

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
/* 
 * These are maintained for the debugger's use.  See <rld_state.h> for details.
 */
static enum bool rld_maintain_states = FALSE;
static unsigned long rld_nallocated_states = 0;

static unsigned long rld_nloaded_states = 0;
static struct rld_loaded_state *rld_loaded_state = NULL;
static void rld_loaded_state_changed(void);
#define NSTATES_INCREMENT 10
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

#ifdef KLD
/* hook for kext tools to set target byte order */
void
kld_set_byteorder(enum NXByteOrder order)
{
    switch (order) {
	case NX_BigEndian:
	target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	break;
    case NX_LittleEndian:
	target_byte_sex = LITTLE_ENDIAN_BYTE_SEX;
	break;
    default:
	target_byte_sex = UNKNOWN_BYTE_SEX;
    }
}
#endif

/* The internal routine that implements rld_load_basefiles()'s */
#ifdef KLD
static long internal_kld_load(
#else /* !defined(KLD) */
static long internal_rld_load(
    NXStream *stream,
#endif /* KLD */
    struct mach_header **header_addr,
    const char * const *object_filenames,
    const char *output_filename,
    const char *file_name,
    const char *obj_addr,
    long obj_size);


/* The internal routine that implements rld_unload()'s */
#ifdef KLD
static long internal_kld_unload(
#else /* !defined(KLD) */
static long internal_rld_unload(
    NXStream *stream,
#endif /* KLD */
    enum bool internal_cleanup);

#if !defined(SA_RLD) && !defined(KLD)
static long internal_rld_load_basefile(
    NXStream *stream,
    const char *base_filename,
    char *base_addr,
    long base_size);
#endif /* !defined(SA_RLD) && !defined(KLD) */

#if defined(KLD)
static long internal_kld_load_basefile(
    const char *base_filename,
    char *base_addr,
    long base_size);
#endif /* defined(KLD) */

#if !defined(SA_RLD) && !defined(KLD)
/*
 * rld_load() link edits and loads the specified object filenames in the NULL
 * terminated array of object file names, object_files, into the program that
 * called it.  If the program wishes to allow the loaded object files to use
 * symbols from itself it must be built with the -seglinkedit link editor
 * option to have its symbol table mapped into memory.  The symbol table may
 * be trimed to exactly which symbol are allowed to be referenced by use of the
 * '-s list_filenam' option to strip(1).  For this routine only global symbols
 * are used so the -x option to the link editor or strip(1) can be used to save
 * space in the final program.  The set of object files being loaded will only
 * be successfull if there are no link edit errors (undefined symbols, etc.).
 * If an error ocurrs the set of object files is unloaded automaticly.  If
 * errors occur and the value specified for stream is not NULL error messages
 * are printed in that stream.  If the link editing and loading is successfull
 * the address of the header of what was loaded is returned through the pointer
 * header_addr it if is not NULL.  rld_load() returns 1 for success and 0 for
 * failure.  If a fatal system error (out of memory, etc.) occurs then all
 * future calls will fail.
 */
long
rld_load(
NXStream *stream,
struct mach_header **header_addr,
const char * const *object_filenames,
const char *output_filename)
{
	return(internal_rld_load(stream, header_addr, object_filenames,
				 output_filename, NULL, NULL, 0));
}
#endif /* !defined(SA_RLD) && !defined(KLD) */

#if defined(KLD) && defined(__DYNAMIC__)
/*
 * kld_load() is used by kextload(8) for loading kernel drivers running in
 * user space.  It is like rld_load() above but only takes one object_filename
 * argument.  Errors for the kld api's are done through kld_error_vprintf()
 * which kextload(8) provides.
 * 
 * Note thes symbols are really __private_extern__ and done by the "nmedit -p"
 * command in the Makefile so that the other __private_extern__ symbols can be
 * hidden by the "ld -r" first.
 */
long
kld_load(
struct mach_header **header_addr,
const char *object_filename,
const char *output_filename)
{
    const char *object_filenames[2];

	object_filenames[0] = object_filename;
	object_filenames[1] = NULL;

	return(internal_kld_load(header_addr, object_filenames,
				 output_filename, NULL, NULL, 0));
}

/*
 * kld_load_from_memory() is the same as kld_load() but loads one object file
 * that has been mapped into memory.  The object is described by its name,
 * object_name, at address object_addr and is of size object_size.
 */
long
kld_load_from_memory(
struct mach_header **header_addr,
const char *object_name,
char *object_addr,
long object_size,
const char *output_filename)
{
	return(internal_kld_load(header_addr, NULL, output_filename,
				 object_name, object_addr, object_size));
}
#endif /* defined(KLD) && defined(__DYNAMIC__) */

#if !defined(SA_RLD) && !defined(KLD)
/*
 * rld_load_from_memory() is the same as rld_load() but loads one object file
 * that has been mapped into memory.  The object is described by its name,
 * object_name, at address object_addr and is of size object_size.
 */
long
rld_load_from_memory(
NXStream *stream,
struct mach_header **header_addr,
const char *object_name,
char *object_addr,
long object_size,
const char *output_filename)
{
	return(internal_rld_load(stream, header_addr, NULL, output_filename,
				 object_name, object_addr, object_size));
}
#endif /* !defined(SA_RLD) && !defined(KLD) */

#if defined(KLD) && defined(__STATIC__)
/*
 * rld_load_from_memory() is used by /mach_kernel for loading boot drivers
 * running in the kernel.  It is like rld_load_from_memory() above but 
 * does not produce an output file. Errors for the kld api's are done through
 * kld_error_vprintf() which /mach_kernel provides.
 * 
 * Note this symbol is really __private_extern__ and done by the "nmedit -p"
 * command in the Makefile so that the other __private_extern__ symbols can be
 * hidden by the "ld -r" first.
 */
long
kld_load_from_memory(
struct mach_header **header_addr,
const char *object_name,
char *object_addr,
long object_size)
{
	return(internal_kld_load(header_addr, NULL, NULL,
				 object_name, object_addr, object_size));
}
#endif /* defined(KLD) && defined(__DYNAMIC__) */

/*
 * internal_rld_load() is the internal routine that implements rld_load()'s.
 */
static
long
#ifdef KLD
internal_kld_load(
#else /* !defined(KLD) */
internal_rld_load(
NXStream *stream,
#endif /* KLD */
struct mach_header **header_addr,
const char * const *object_filenames,
const char *output_filename,
const char *file_name,
const char *obj_addr,
long obj_size)
{
#if !(defined(KLD) && defined(__STATIC__))
    kern_return_t r;
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
    int i, n;
    int fd;
    long symbol_size, deallocate_size;
    char dir[MAXPATHLEN];
    long dir_len;
    const struct section *s;
    void (**routines)(void);

    	dir[0] = '\0';
	dir_len = 0;
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	if(header_addr != NULL)
	    *header_addr = NULL;

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was not a fatal
	     * error unload the set of object files being loaded.  Otherwise
	     * just return failure.
	     */
	    if(fatals == 0)
#ifdef KLD
		internal_kld_unload(TRUE);
#else /* !defined(KLD) */
		internal_rld_unload(stream, TRUE);
#endif /* KLD */
	    return(0);
	}

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_pagesize = getpagesize();
	host_byte_sex = get_host_byte_sex();
	force_cpusubtype_ALL = TRUE;
	filetype = MH_OBJECT;
	flush = FALSE;
	nmerged_symbols = 0;
	merged_string_size = 0;
	nlocal_symbols = 0;
	local_string_size = 0;
	/*
	 * If there is to be an output file then save the symbols.  Only the
	 * symbols from the current set will be placed in the output file.  The
	 * symbols from the base file are never placed in any output file.
	 */
	strip_base_symbols = TRUE;
	if(output_filename != NULL)
	    strip_level = STRIP_NONE;
	else
	    strip_level = kld_requested_strip_level;

	/* This must be cleared for each call to rld() */
	errors = 0;

#if !defined(SA_RLD) && !(defined(KLD) && defined(__DYNAMIC__))
	/*
	 * If the symbols from base program has not been loaded load them.
	 * This will happen the first time rld() is called or will not happen.
	 */
	if(base_obj == NULL){
	    /*
	     * The NeXT global variable that gets set to argv in crt0.o.  Used
	     * here to set the name of the base program's object file
	     * (NXArgv[0]).
	     */
#if !defined(__OPENSTEP__) && !defined(KLD)
	    static char ***NXArgv_pointer = NULL;
	    static struct mach_header *_mh_execute_header_pointer = NULL;
	    struct segment_command *sg;

	    if(NXArgv_pointer == NULL)
		NXArgv_pointer = _NSGetArgv();
	    if(_mh_execute_header_pointer == NULL)
		_mh_execute_header_pointer = _NSGetMachExecuteHeader();

	    sg = (struct segment_command *)getsegbyname(SEG_LINKEDIT);
	    if(sg != NULL)
		merge_base_program((*NXArgv_pointer)[0],
				   _mh_execute_header_pointer, sg,
				   NULL, 0, NULL, 0);
#else /* !defined(__OPENSTEP__) && !defined(KLD) */
	    struct segment_command *sg;
#ifndef __DYNAMIC__
#ifdef KLD
#ifndef _LIBSA_STDLIB_H_
	    /*
	     * This needs to match what is in
	     * /System/Library/Frameworks/Kernel.framework/PrivateHeaders/
	     *     libsa/stdlib.h
	     * if it exists on the system.
	     */
	    __private_extern__ const char *kld_basefile_name;
#endif /* !defined(_LIBSA_STDLIB_H_) */
#else /* !defined(KLD) */
	    extern char **NXArgv;
#endif /* KLD */
#else /* defined(__DYNAMIC__) */
	    static char ***NXArgv_pointer = NULL;
	    static struct mach_header *_mh_execute_header_pointer = NULL;

	    if(NXArgv_pointer == NULL)
		_dyld_lookup_and_bind("_NXArgv",
		    (unsigned long *)&NXArgv_pointer, NULL);
	    if(_mh_execute_header_pointer == NULL)
		_dyld_lookup_and_bind("__mh_execute_header",
		    (unsigned long *)&_mh_execute_header_pointer, NULL);
#endif /* !defined(__DYNAMIC__) */

	    sg = (struct segment_command *)getsegbyname(SEG_LINKEDIT);
	    if(sg != NULL)
#ifndef __DYNAMIC__
#ifdef KLD
		merge_base_program(kld_basefile_name,
		    (struct mach_header *)&_mh_execute_header, sg,
		    NULL, 0, NULL, 0);
#else
		merge_base_program(
		    NXArgv[0], (struct mach_header *)&_mh_execute_header, sg,
		    NULL, 0, NULL, 0);
#endif /* KLD */
#else /* defined(__DYNAMIC__) */
		merge_base_program((*NXArgv_pointer)[0],
				   _mh_execute_header_pointer, sg,
				   NULL, 0, NULL, 0);
#endif /* !defined(__DYNAMIC__) */
#endif /* !defined(__OPENSTEP__) && !defined(KLD) */
	    if (target_byte_sex == UNKNOWN_BYTE_SEX)
		target_byte_sex = host_byte_sex;
	    /*
	     * If there were any errors in processing the base program it is
	     * treated as a fatal error and no futher processing is done.
	     */
	    if(errors){
		fatals = 1;
		return(0);
	    }
#ifndef KLD
	    /*
	     * Since we are loading into this program maintain state for the
	     * debugger.
	     */
	    rld_maintain_states = TRUE;
#endif /* KLD */
	}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__DYNAMIC__)) */

	/*
	 * Create an entry in the sets array for this new set.  This has to be
	 * done after the above base program has been merged so it does not
	 * appear apart of any set.
	 */
	new_set();

	/*
	 * The merged section sizes need to be zeroed before we start loading.
	 * The only case they would not be zero would be if a previous rld_load
	 * failed with a pass1 error they would not get reset.
	 */
	zero_merged_sections_sizes();

	/*
	 * Do pass1() for each object file or merge() for the one object in
	 * memory.
	 */
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	if(file_name == NULL){
	    for(i = 0; object_filenames[i] != NULL; i++)
		pass1((char *)object_filenames[i], FALSE, FALSE, FALSE, FALSE,
		      FALSE);
	}
	else
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */
	{
	    cur_obj = new_object_file();
	    cur_obj->file_name = allocate(strlen(file_name) + 1);
	    strcpy(cur_obj->file_name, file_name);
	    cur_obj->user_obj_addr = TRUE;
	    cur_obj->obj_addr = (char *)obj_addr;
	    cur_obj->obj_size = obj_size;
	    merge(FALSE, FALSE, FALSE);
	}

	if(errors){
#ifdef KLD
	    internal_kld_unload(TRUE);
#else /* !defined(KLD) */
	    internal_rld_unload(stream, TRUE);
#endif /* KLD */
	    return(0);
	}

	if(output_filename == RLD_DEBUG_OUTPUT_FILENAME)
	    RLD_DEBUG_OUTPUT_FILENAME_flag = 1;
	else
	    RLD_DEBUG_OUTPUT_FILENAME_flag = 0;
	layout();
	if(errors){
#ifdef KLD
	    internal_kld_unload(TRUE);
#else /* !defined(KLD) */
	    internal_rld_unload(stream, TRUE);
#endif /* KLD */
	    return(0);
	}

	pass2();
	if(errors){
#ifdef KLD
	    internal_kld_unload(TRUE);
#else /* !defined(KLD) */
	    internal_rld_unload(stream, TRUE);
#endif /* KLD */
	    return(0);
	}

	/*
	 * Place the merged sections back on their list of their merged segment
	 * (since now the are all in one segment after layout() placed them
	 * there for the MH_OBJECT format) and also reset the sizes of the 
	 * sections to zero for any future loads.
	 */
	reset_merged_sections();

	/*
	 * Clean the object structures of things from this set that are not
	 * needed once the object has been successfully loaded.
	 */
	clean_objects();
	clean_archives_and_fats();

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	if(output_filename != NULL &&
	   output_filename != RLD_DEBUG_OUTPUT_FILENAME){
	    /*
	     * Create the output file.  The unlink() is done to handle the
	     * problem when the outputfile is not writable but the directory
	     * allows the file to be removed (since the file may not be there
	     * the return code of the unlink() is ignored).
	     */
	    symbol_size = output_symtab_info.symtab_command.nsyms *
			  sizeof(struct nlist) +
			  output_symtab_info.symtab_command.strsize;
	    (void)unlink(output_filename);
	    if((fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC,
			  0666)) == -1){
		system_error("can't create output file: %s", output_filename);
#ifdef KLD
		internal_kld_unload(TRUE);
#else /* !defined(KLD) */
		internal_rld_unload(stream, TRUE);
#endif /* KLD */
		return(0);
	    }
	    else {
		/*
		 * Write the entire output file.
		 */
		if(write(fd, output_addr, output_size + symbol_size) !=
		   (int)(output_size + symbol_size)){
		    system_error("can't write output file: %s",output_filename);
#ifdef KLD
		    internal_kld_unload(TRUE);
#else /* !defined(KLD) */
		    internal_rld_unload(stream, TRUE);
#endif /* KLD */
		    return(0);
		}
		if(close(fd) == -1){
		    system_error("can't close output file: %s",output_filename);
#ifdef KLD
		    internal_kld_unload(TRUE);
#else /* !defined(KLD) */
		    internal_rld_unload(stream, TRUE);
#endif /* KLD */
		    return(0);
		}
	    }
	    /*
	     * Deallocate the pages of memory for the symbol table if there are
	     * any whole pages.
	     */
	    if (strip_level == STRIP_ALL)
		deallocate_size = rnd(output_size + symbol_size, host_pagesize) -
				rnd(output_size, host_pagesize);
	    else {
		deallocate_size = 0;
		sets[cur_set].output_size += symbol_size;
	    }

	    if(deallocate_size > 0){
		if((r = vm_deallocate(mach_task_self(),
				      (vm_address_t)(output_addr +
				      rnd(output_size, host_pagesize)),
				      deallocate_size)) != KERN_SUCCESS)
		    mach_fatal(r, "can't vm_deallocate() buffer for output "
			       "file's symbol table");
#ifdef RLD_VM_ALLOC_DEBUG
		print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
		      (unsigned int)(output_addr +
				     rnd(output_size, host_pagesize)),
		      (unsigned int)deallocate_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	    }
	}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

	/*
	 * Now that this was successfull all that is left to do is return the
	 * address of the header if requested.
	 */
	if(header_addr != NULL)
	    *header_addr = (struct mach_header *)output_addr;

#if !(defined(KLD) && defined(__STATIC__))
	/*
	 * Flush the cache of the output buffer so the the caller can execute
	 * the instructions written on by the relocation.
	 */
	if((r = vm_flush_cache(mach_task_self(), (vm_address_t)output_addr,
			       output_size)) != KERN_SUCCESS)
#ifndef SA_RLD
	    mach_fatal(r, "can't vm_flush_cache() output buffer");
#else
	    fatal("can't vm_flush_cache() output buffer");
#endif
#endif /* !(defined(KLD) && defined(__STATIC__)) */


#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	/*
	 * Now that this was successfull if the state for the debugger is to
	 * be maintained fill it in.
	 */
	if(rld_maintain_states == TRUE){
	    if(rld_nloaded_states + 1 >= rld_nallocated_states){
		rld_loaded_state = reallocate(rld_loaded_state,
					sizeof(struct rld_loaded_state) *
				(rld_nallocated_states + NSTATES_INCREMENT) );
		rld_nallocated_states += NSTATES_INCREMENT;
	    }

	    if(file_name == NULL){
		for(i = 0; object_filenames[i] != NULL; )
		    i++;
		rld_loaded_state[rld_nloaded_states].object_filenames =
						allocate(i * sizeof(char *));
		rld_loaded_state[rld_nloaded_states].nobject_filenames = i;

		for(i = 0; object_filenames[i] != NULL; i++){
		    if(object_filenames[i][0] != '/'){
			if(dir[0] == '\0'){
			    getwd(dir);
			    dir_len = strlen(dir);
			}
			rld_loaded_state[rld_nloaded_states].
			    object_filenames[i] =
				allocate(dir_len + 1 +
					 strlen(object_filenames[i]) + 1);
			strcpy(rld_loaded_state[rld_nloaded_states].
				object_filenames[i], dir);
			strcat(rld_loaded_state[rld_nloaded_states].
				object_filenames[i], "/");
			strcat(rld_loaded_state[rld_nloaded_states].
				object_filenames[i], object_filenames[i]);
		    }
		    else{
			rld_loaded_state[rld_nloaded_states].
			    object_filenames[i] =
				allocate(strlen(object_filenames[i]) + 1);
			strcpy(rld_loaded_state[rld_nloaded_states].
				object_filenames[i], object_filenames[i]);
		    }
		}
	    }
	    else{
		rld_loaded_state[rld_nloaded_states].object_filenames =
						allocate(sizeof(char *));
		rld_loaded_state[rld_nloaded_states].nobject_filenames = 1;
		rld_loaded_state[rld_nloaded_states].object_filenames[0] =
			allocate(strlen(file_name) + 1);
		strcpy(rld_loaded_state[rld_nloaded_states].
			object_filenames[0], file_name);
	    }
	    rld_loaded_state[rld_nloaded_states].header_addr =
					(struct mach_header *)output_addr;

	    rld_nloaded_states += 1;
	    rld_loaded_state_changed();
	}

	/*
	 * If the base file comes from the executable then the profiling and
	 * constructor calls should be made.  Else an rld_load_basefile() was
	 * done and these calls should not be made.
	 */
	if(base_name == NULL){
	    /*
	     * If moninitrld() was called and it save away a pointer to
	     * monaddition() then profiling for the rld_load'ed code is wanted
	     * and make the indirect call to monaddition().
	     */
	    if(rld_monaddition != NULL && rld_maintain_states == TRUE){
		(*rld_monaddition)(output_addr, output_addr + output_size);
	    }
	    /*
	     * Call the C++ constructors and register the C++ destructors.
	     */
	    s = getsectbynamefromheader((struct mach_header *)output_addr,
					"__TEXT", "__constructor");
	    if(s != NULL){
		routines = (void(**)(void))s->addr;
		n = s->size / sizeof(routines[0]);
		for(i = 0; i < n; i++)
		    (*routines[i])();
	    }
	    s = getsectbynamefromheader((struct mach_header *)output_addr,
					"__TEXT", "__destructor");
	    if(s != NULL){
		routines = (void(**)(void))s->addr;
		n = s->size / sizeof(routines[0]);
		for(i = 0; i < n; i++)
		    atexit(routines[i]);
	    }
	}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

	return(1);
}

#if !defined(SA_RLD)
/*
 * rld_load_basefile() loads a base file from an object file rather than just
 * picking up the link edit segment from this program.
 */
#if defined(KLD)
#if !defined(__STATIC__)
long
kld_load_basefile(
const char *base_filename)
{
	return(internal_kld_load_basefile(base_filename, NULL, 0));
}
#endif /* !defined(__STATIC__) */

long
kld_load_basefile_from_memory(
const char *base_filename,
char *base_addr,
long base_size)
{
	return(internal_kld_load_basefile(base_filename, base_addr, base_size));
}

#else /* !defined(KLD) */
long
rld_load_basefile(
NXStream *stream,
const char *base_filename)
{
	return(internal_rld_load_basefile(stream, base_filename, NULL, 0));
}
#endif /* defined(KLD) */

/*
 * rld_load_basefile() loads a base file from an object file rather than just
 * picking up the link edit segment from this program.
 */
static long
#ifdef KLD
internal_kld_load_basefile(
#else /* !defined(KLD) */
internal_rld_load_basefile(
NXStream *stream,
#endif
const char *base_filename,
char *base_addr,
long base_size)
{
#if !(defined(KLD) && defined(__STATIC__))
    unsigned long size;
    char *addr;
    int fd;
    struct stat stat_buf;
    kern_return_t r;
    struct fat_header *fat_header;
#ifdef __LITTLE_ENDIAN__
    struct fat_header struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    struct fat_arch *fat_archs, *best_fat_arch;
    struct arch_flag host_arch_flag;
    enum bool from_fat_file;

	size = 0;
	from_fat_file = FALSE;

#endif /* !(defined(KLD) && defined(__STATIC__)) */

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was not a fatal
	     * error unload the base file being loaded.  Otherwise just return
	     * failure.
	     */
	    if(fatals == 0)
#ifdef KLD
		kld_unload_all(1);
#else /* !defined(KLD) */
		rld_unload_all(stream, 1);
#endif /* KLD */
	    return(0);
	}

	/* This must be cleared for each call to rld() */
	errors = 0;

	/*
	 * If a base file has been loaded at this point return failure.
	 */
	if(base_obj != NULL){
	    error("a base program is currently loaded");
	    return(0);
	}
	if(cur_set != -1){
	    error("object sets are currently loaded (base file must be loaded"
		  "before object sets)");
	    return(0);
	}

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_pagesize = getpagesize();
	host_byte_sex = get_host_byte_sex();
	strip_base_symbols = TRUE;
	force_cpusubtype_ALL = TRUE;
	base_name = allocate(strlen(base_filename) + 1);
	strcpy(base_name, base_filename);

#if !(defined(KLD) && defined(__STATIC__))

	/*
	 * If there is to be an output file then save the symbols.  Only the
	 * symbols from the current set will be placed in the output file.  The
	 * symbols from the base file are never placed in any output file.
	 */

	if (base_addr == NULL) {
	    /*
	    * Open this file and map it in.
	    */
	    if((fd = open(base_name, O_RDONLY, 0)) == -1){
		system_error("Can't open: %s", base_name);
		free(base_name);
		base_name = NULL;
		return(0);
	    }
	    if(fstat(fd, &stat_buf) == -1)
		system_fatal("Can't stat file: %s", base_name);
	    /*
	    * For some reason mapping files with zero size fails so it has to
	    * be handled specially.
	    */
	    if(stat_buf.st_size == 0){
		error("file: %s is empty (not an object)", base_name);
		close(fd);
		free(base_name);
		base_name = NULL;
		return(0);
	    }
	    size = stat_buf.st_size;
	    if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&addr,
		(boolean_t)TRUE, (vm_size_t)size)) != KERN_SUCCESS)
		mach_fatal(r, "can't map file: %s", base_name);
#ifdef RLD_VM_ALLOC_DEBUG
	    print("rld() map_fd: addr = 0x%0x size = 0x%x\n",
		(unsigned int)addr, (unsigned int)size);
#endif /* RLD_VM_ALLOC_DEBUG */
	    /*
	     * The mapped file can't be made read-only because even in the case
	     * of errors where a wrong bytesex file is attempted to be loaded
	     * it must be writeable to detect the error.
	     *  if((r = vm_protect(mach_task_self(), (vm_address_t)addr, size,
	     * 		       FALSE, VM_PROT_READ)) != KERN_SUCCESS)
	     *      mach_fatal(r, "can't make memory for mapped file: %s "
	     *      	   "read-only", base_name);
	     */
	    close(fd);
    
	    /*
	    * Determine what type of file it is (fat or thin object file).
	    */
	    if(sizeof(struct fat_header) > size){
		error("truncated or malformed file: %s (file size too small "
		    "to be any kind of object)", base_name);
		free(base_name);
		base_name = NULL;
		return(0);
	    }
	    from_fat_file = FALSE;
	    fat_header = (struct fat_header *)addr;
#ifdef __LITTLE_ENDIAN__
	    fat_archs = NULL;
#endif /* __LITTLE_ENDIAN__ */
#ifdef __BIG_ENDIAN__
	    if(fat_header->magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	    if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	    {
		from_fat_file = TRUE;
#ifdef __LITTLE_ENDIAN__
		struct_fat_header = *fat_header;
		swap_fat_header(&struct_fat_header, host_byte_sex);
		fat_header = &struct_fat_header;
#endif /* __LITTLE_ENDIAN__ */
    
		if(sizeof(struct fat_header) + fat_header->nfat_arch *
		   sizeof(struct fat_arch) > (unsigned long)size){
		    error("fat file: %s truncated or malformed (fat_arch "
			"structs would extend past the end of the file)",
			base_name);
		    goto rld_load_basefile_error_return;
		}
    
#ifdef __BIG_ENDIAN__
		fat_archs = (struct fat_arch *)
				    (addr + sizeof(struct fat_header));
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
		fat_archs = allocate(fat_header->nfat_arch *
				    sizeof(struct fat_arch));
		memcpy(fat_archs, addr + sizeof(struct fat_header),
		    fat_header->nfat_arch * sizeof(struct fat_arch));
		swap_fat_arch(fat_archs, fat_header->nfat_arch, host_byte_sex);
#endif /* __LITTLE_ENDIAN__ */
    
		/* check the fat file */
		check_fat(base_name, size, fat_header, fat_archs, NULL, 0);
		if(errors){
		    goto rld_load_basefile_error_return;
		    return(0);
		}
    
#if defined(KLD) && defined(__STATIC__)
		best_fat_arch = cpusubtype_findbestarch(
				arch_flag.cputype, arch_flag.cpusubtype,
				fat_archs, fat_header->nfat_arch);
#else /* !(defined(KLD) && defined(__STATIC__)) */
		if(get_arch_from_host(&host_arch_flag, NULL) == 0){
		    error("can't determine the host architecture (fix "
			"get_arch_from_host() )");
		    goto rld_load_basefile_error_return;
		}
		best_fat_arch = cpusubtype_findbestarch(
				host_arch_flag.cputype,
				host_arch_flag.cpusubtype,
				fat_archs, fat_header->nfat_arch);
#endif /* defined(KLD) && defined(__STATIC__) */
		if(best_fat_arch != NULL){
		    cur_obj = new_object_file();
		    cur_obj->file_name = base_name;
		    cur_obj->obj_addr = addr + best_fat_arch->offset;
		    cur_obj->obj_size = best_fat_arch->size;
		    cur_obj->from_fat_file = TRUE;
		    base_obj = cur_obj;
		}
		if(base_obj == NULL){
		    error("fat file: %s does not contain the host architecture "
			"(can't be used as a base file)", base_name);
		    goto rld_load_basefile_error_return;
		}
#ifdef __LITTLE_ENDIAN__
		free(fat_archs);
#endif /* __LITTLE_ENDIAN__ */
	    }
	    else{
		cur_obj = new_object_file();
		cur_obj->file_name = base_name;
		cur_obj->obj_addr = addr;
		cur_obj->obj_size = size;
		base_obj = cur_obj;
	    }
	}
	else
#endif /* !(defined(KLD) && defined(__STATIC__)) */
	{
	    cur_obj = new_object_file();
	    cur_obj->file_name = base_name;
	    cur_obj->obj_addr = base_addr;
	    cur_obj->obj_size = base_size;
	    cur_obj->user_obj_addr = TRUE;
	    base_obj = cur_obj;
	}

	/*
	 * Now that the file is mapped in merge it as the base file.
	 */
	merge(FALSE, FALSE, FALSE);

	if(errors){
#ifdef KLD
	    kld_unload_all(1);
#else /* !defined(KLD) */
	    rld_unload_all(stream, 1);
#endif /* KLD */
	    return(0);
	}

	/*
	 * This is called to deallocate the memory for the base file and to
	 * clean up it's section map.
	 */
	clean_objects();
	clean_archives_and_fats();
#if !(defined(KLD) && defined(__STATIC__))
	if(from_fat_file == TRUE){
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)addr,
				  (vm_size_t)size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() memory for mapped file %s",
			   base_name);
#ifdef RLD_VM_ALLOC_DEBUG
	    print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
		  (unsigned int)addr, (unsigned int)size);
#endif /* RLD_VM_ALLOC_DEBUG */
	}

	/*
	 * Since we are NOT loading into this program don't maintain state for
	 * the debugger.
	 */
	rld_maintain_states = FALSE;

#endif /* !(defined(KLD) && defined(__STATIC__)) */

	return(1);

#if !(defined(KLD) && defined(__STATIC__))
rld_load_basefile_error_return:
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)addr,
			      (vm_size_t)size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() memory for mapped file %s",
		       base_name);
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
	      (unsigned int)addr, (unsigned int)size);
#endif /* RLD_VM_ALLOC_DEBUG */
	free(base_name);
	base_name = NULL;
#ifdef __LITTLE_ENDIAN__
	if(fat_archs != NULL)
	    free(fat_archs);
#endif /* __LITTLE_ENDIAN__ */
	return(0);
#endif /* !(defined(KLD) && defined(__STATIC__)) */
}

#ifndef KLD
/*
 * rld_unload() unlinks and unloads that last object set that was loaded.
 * It returns 1 if it is successfull and 0 otherwize.  If any errors ocurr
 * and the specified stream, stream, is not zero the error messages are printed
 * on that stream.
 */
long
rld_unload(
NXStream *stream)
{
    return(internal_rld_unload(stream, FALSE));
}
#endif /* KLD */
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

/*
 * internal_rld_unload() does the work for rld_unload() and takes one extra
 * parameter which is used to know if to remove the state maintained by the
 * debugger.
 */
static
long
#ifdef KLD
internal_kld_unload(
#else /* !defined(KLD) */
internal_rld_unload(
NXStream *stream,
#endif /* KLD */
enum bool internal_cleanup)
{
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
    kern_return_t r;
    unsigned long i;
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was a fatal
	     * error or not just return failure.
	     */
	    return(0);
	}

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_byte_sex = get_host_byte_sex();
	force_cpusubtype_ALL = TRUE;

	/* This must be cleared for each call to rld() */
	errors = 0;

	free_multiple_defs();
	free_undefined_list();

	/*
	 * If no set has been loaded at this point return failure.
	 */
	if(cur_set == -1){
	    error("no object sets currently loaded");
	    return(0);
	}

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	/*
	 * First adjust the state maintained for the debugger.  This is done
	 * first before the unload happens so if the debugger attaches while
	 * in this unload it does not use the state that is being unloaded.
	 */
	if(internal_cleanup == FALSE && rld_maintain_states == TRUE){
	    rld_nloaded_states -= 1;
	    for(i = 0;
		i < rld_loaded_state[rld_nloaded_states].nobject_filenames;
		i++){
		    free(rld_loaded_state[rld_nloaded_states].
			 object_filenames[i]);
	    }
	    free(rld_loaded_state[rld_nloaded_states].object_filenames);

	    rld_loaded_state[rld_nloaded_states].object_filenames = NULL;
	    rld_loaded_state[rld_nloaded_states].nobject_filenames = 0;
	    rld_loaded_state[rld_nloaded_states].header_addr = NULL;
	}
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

	/*
	 * Remove the merged symbols for the current set of objects.
	 */
	remove_merged_symbols();

	/*
	 * Remove the merged sections for the current set of objects.
	 */
	remove_merged_sections();

	/*
	 * Clean and remove the object strcutures for the current set of
	 * objects.
	 */
	clean_objects();
	clean_archives_and_fats();
	remove_objects();

	/*
	 * deallocate the output memory for the current set if it had been
	 * allocated.
	 */
	if(sets[cur_set].output_addr != NULL){
#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	    if((r = vm_deallocate(mach_task_self(),
				  (vm_address_t)sets[cur_set].output_addr,
				  sets[cur_set].output_size)) != KERN_SUCCESS)
		mach_fatal(r, "can't vm_deallocate() memory for output");
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */
#ifdef RLD_VM_ALLOC_DEBUG
	    print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
		  (unsigned int)sets[cur_set].output_addr,
		  (unsigned int)sets[cur_set].output_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	    sets[cur_set].output_addr = NULL;
	}

	/*
	 * The very last thing to do to unload a set is to remove the set
	 * allocated in the sets array and reduce the cur_set.
	 */
	remove_set();

#if !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__))
	/*
	 * If we were maintaining state for the debugger let it know the state
	 * has changed.
	 */
	if(rld_maintain_states == TRUE)
	    rld_loaded_state_changed();
#endif /* !defined(SA_RLD) && !(defined(KLD) && defined(__STATIC__)) */

	return(1);
}

#ifndef SA_RLD
/*
 * rld_unload_all() frees up all dynamic memory for the rld package that store
 * the information about all object sets and the base program.  Also if the
 * parameter deallocate_sets is non-zero it deallocates the object sets
 * otherwise it leaves them around and can be still be used by the program.
 * It returns 1 if it is successfull and 0 otherwize.  If any errors ocurr
 * and the specified stream, stream, is not zero the error messages are printed
 * on that stream.
 */
long
#ifdef KLD
kld_unload_all(
#else /* !defined(KLD) */
rld_unload_all(
NXStream *stream,
#endif /* KLD */
long deallocate_sets)
{
#if !(defined(KLD) && defined(__STATIC__))
    kern_return_t r;
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#ifndef KLD
    unsigned long i, j, n;
#endif /* !defined(KLD) */

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was a fatal
	     * error or not just return failure.
	     */
	    return(0);
	}

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_byte_sex = get_host_byte_sex();
	force_cpusubtype_ALL = TRUE;

	/* This must be cleared for each call to rld() */
	errors = 0;

	free_multiple_defs();
	free_undefined_list();

	/*
	 * If nothing has been loaded at this point return failure.
	 */
	if(cur_set == -1 && base_obj == NULL){
	    error("no object sets or base program currently loaded");
	    return(0);
	}

#ifndef KLD
	/*
	 * First adjust the state maintained for the debugger.  This is done
	 * first before the unload happens so if the debugger attaches while
	 * in this unload it does not use the state that is being unloaded.
	 */
	if(rld_maintain_states == TRUE){
	    n = rld_nloaded_states;
	    rld_nloaded_states = 0;
	    for(i = 0; i < n; i++){
		for(j = 0; j < rld_loaded_state[i].nobject_filenames; j++)
		    free(rld_loaded_state[i].object_filenames[j]);
		free(rld_loaded_state[i].object_filenames);

		rld_loaded_state[i].object_filenames = NULL;
		rld_loaded_state[i].nobject_filenames = 0;
		rld_loaded_state[i].header_addr = NULL;
	    }
	    free(rld_loaded_state);
	    rld_loaded_state = NULL;
	    rld_nallocated_states = 0;
	}
#endif /* !defined(KLD) */

	/*
	 * Remove all sets currently loaded.
	 */
	while(cur_set != -1){
	    /*
	     * Remove the merged symbols for the current set of objects.
	     */
	    remove_merged_symbols();

	    /*
	     * Remove the merged sections for the current set of objects.
	     */
	    remove_merged_sections();

	    /*
	     * Clean and remove the object structures for the current set of
	     * objects.
	     */
	    clean_objects();
	    clean_archives_and_fats();
	    remove_objects();

#if !(defined(KLD) && defined(__STATIC__))
	    /*
	     * deallocate the output memory for the current set if specified and
	     * it had been allocated.
	     */
	    if(deallocate_sets && sets[cur_set].output_addr != NULL){
		if((r = vm_deallocate(mach_task_self(),
				  (vm_address_t)sets[cur_set].output_addr,
				  sets[cur_set].output_size)) != KERN_SUCCESS)
		    mach_fatal(r, "can't vm_deallocate() memory for output");
#ifdef RLD_VM_ALLOC_DEBUG
		print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
		      (unsigned int)sets[cur_set].output_addr,
		      (unsigned int)sets[cur_set].output_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	    }
#endif /* !(defined(KLD) && defined(__STATIC__)) */
	    sets[cur_set].output_addr = NULL;

	    /*
	     * The very last thing to do to unload a set is to remove the set
	     * allocated in the sets array and reduce the cur_set.
	     */
	    remove_set();
	}
	/*
	 * Remove the merged symbols for the base program.
	 */
	remove_merged_symbols();

	/*
	 * Remove the merged sections for the base program.
	 */
	remove_merged_sections();

	/*
	 * Remove the object structure for the base program.
	 */
	if(base_name != NULL){
	    clean_objects();
	    clean_archives_and_fats();
	    free(base_name);
	    base_name = NULL;
	}
	remove_objects();

	/*
	 * Now free the memory for the sets.
	 */
	free_sets();

	/*
	 * Set the pointer to the base object to NULL so that if another load
	 * is done it will get reloaded.
	 */
	base_obj = NULL;

#ifndef KLD
	/*
	 * If we were maintaining state for the debugger let it know the state
	 * has changed.  Then clear the flag for maintaining state for the
	 * debugger.
	 */
	if(rld_maintain_states == TRUE)
	    rld_loaded_state_changed();
	rld_maintain_states = FALSE;

	if(zonep != NULL)
	    NXDestroyZone(zonep);
	zonep = NULL;
#endif /* !defined(KLD) */

	target_byte_sex = UNKNOWN_BYTE_SEX;
	return(1);
}
#endif /* !defined(SA_RLD) */

#ifndef SA_RLD

/*
 * rld_lookup() looks up the specified symbol name, symbol_name, and returns
 * its value indirectly through the pointer specified, value.  It returns
 * 1 if it finds the symbol and 0 otherwise.  If any errors ocurr and the
 * specified stream, stream, is not zero the error messages are printed on
 * that stream (for this routine only internal errors could result).
 */
long
#ifdef KLD
kld_lookup(
#else /* !defined(KLD) */
rld_lookup(
NXStream *stream,
#endif /* KLD */
const char *symbol_name,
unsigned long *value)
{
    struct merged_symbol *merged_symbol;

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/* This must be cleared for each call to rld() */
	errors = 0;

	merged_symbol = lookup_symbol((char *)symbol_name);
	if(merged_symbol->name_len != 0){
	    if(value != NULL)
		*value = merged_symbol->nlist.n_value;
	    return(1);
	}
	else{
	    if(value != NULL)
		*value = 0;
	    return(0);
	}
}

/*
 * rld_forget_symbol() looks up the specified symbol name, symbol_name, and
 * stomps on the name so rld effectively forgets the symbol exists. It returns
 * 1 if it finds the symbol and 0 otherwise.  If any errors ocurr and the
 * specified stream, stream, is not zero the error messages are printed on
 * that stream (for this routine only internal errors could result).
 */
long
#ifdef KLD
kld_forget_symbol(
#else /* !defined(KLD) */
rld_forget_symbol(
NXStream *stream,
#endif /* KLD */
const char *symbol_name)
{
    struct merged_symbol *merged_symbol;

#ifndef KLD
	error_stream = stream;
#endif /* !defined(KLD) */

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/* This must be cleared for each call to rld() */
	errors = 0;

	merged_symbol = lookup_symbol((char *)symbol_name);
	if(merged_symbol->name_len != 0){
	    merged_symbol->nlist.n_un.n_name[0] = '\0';
	    return(1);
	}
	else{
	    return(0);
	}
}

#ifndef KLD
/*
 * rld_write_symfile() writes a object file containing just absolute symbols
 * mirroring the last set loaded.  This can be used to recreate the stack of
 * the loaded state to come back after things are unloaded and load more stuff.
 */
long
rld_write_symfile(
NXStream *stream,
const char *output_filename)
{
    int fd;
    long symbol_size, return_value;
    kern_return_t r;

	return_value = 1;
	error_stream = stream;

	/* If a fatal error has ever occured no other calls will be processed */
	if(fatals == 1){
	    print("previous fatal errors occured, can no longer succeed");
	    return(0);
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was a fatal
	     * error or not just return failure.
	     */
	    return(0);
	}

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_byte_sex = get_host_byte_sex();
	force_cpusubtype_ALL = TRUE;

	/* This must be cleared for each call to rld() */
	errors = 0;

	/*
	 * If no set has been loaded at this point return failure.  That is a
	 * basefile and at least one object set must be loaded to create a
	 * symfile.
	 */
	if(cur_set < 0){
	    error("no object sets currently loaded");
	    return(0);
	}

	layout_rld_symfile();
	if(errors){
	    return_value = 0;
	    goto deallocate_and_return;
	}

	pass2_rld_symfile();
	if(errors){
	    return_value = 0;
	    goto deallocate_and_return;
	}

	/*
	 * Create the output file.  The unlink() is done to handle the
	 * problem when the outputfile is not writable but the directory
	 * allows the file to be removed (since the file may not be there
	 * the return code of the unlink() is ignored).
	 */
	symbol_size = output_symtab_info.symtab_command.nsyms *
		      sizeof(struct nlist) +
		      output_symtab_info.symtab_command.strsize;
	(void)unlink(output_filename);
	if((fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC,
		      0666)) == -1){
	    system_error("can't create output file: %s", output_filename);
	    return_value = 0;
	    goto deallocate_and_return;
	}
	else {
	    /*
	     * Write the entire output file.
	     */
	    if(write(fd, output_addr, output_size + symbol_size) !=
	       (int)(output_size + symbol_size)){
		system_error("can't write output file: %s",output_filename);
		(void)unlink(output_filename);
		return_value = 0;
		goto deallocate_and_return;
	    }
	    if(close(fd) == -1){
		system_error("can't close output file: %s",output_filename);
		(void)unlink(output_filename);
		return_value = 0;
		goto deallocate_and_return;
	    }
	}

deallocate_and_return:

	if((r = vm_deallocate(mach_task_self(), (vm_address_t)(output_addr),
			      output_size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() buffer for output "
		       "file's symbol table");
#ifdef RLD_VM_ALLOC_DEBUG
	print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
	      (unsigned int)(output_addr), output_size);
			     rnd(output_size, host_pagesize)),
	      (unsigned int)deallocate_size);
#endif /* RLD_VM_ALLOC_DEBUG */
	return(return_value);
}
#endif /* !defined(KLD) */

#if !(defined(KLD) && defined(__STATIC__))
/*
 * The debugger places a break point at this routine and it is called when the
 * the loaded state into the program has changed.
 */
static
void
rld_loaded_state_changed(
void)
{
#ifdef RLD_TEST

    unsigned long i, j;

	if(rld_maintain_states == TRUE)
	    print("rld_maintain_states = TRUE\n");
	else
	    print("rld_maintain_states = FALSE\n");
	print("rld_nloaded_states = %lu\n", rld_nloaded_states);
	print("rld_loaded_state 0x%x\n", (unsigned int)rld_loaded_state);
	for(i = 0; i < rld_nloaded_states; i++){
	    print("state %lu\n\tnobject_filenames %lu\n\tobject_filenames 0x%x"
		 "\n\theader_addr 0x%x\n", i,
		 rld_loaded_state[i].nobject_filenames,
		 (unsigned int)(rld_loaded_state[i].object_filenames),
		 (unsigned int)(rld_loaded_state[i].header_addr));
	    for(j = 0; j < rld_loaded_state[i].nobject_filenames; j++)
		print("\t\t%s\n", rld_loaded_state[i].object_filenames[j]);
	}
#endif /* RLD_TEST */
}
#endif /* !(defined(KLD) && defined(__STATIC__)) */

#ifndef KLD
/*
 * rld_get_loaded_state() is returned by moninitrld() to allow the profiling
 * runtime routine monoutput() to get the rld_loaded_state and write it into
 * the gmon.out file for later processing by gprof(1).
 */
static
void
rld_get_loaded_state(
struct rld_loaded_state **s,
unsigned long *n)
{
	*s = rld_loaded_state;
	*n = rld_nloaded_states;
}

/*
 * moninitrld() is called from the profiling runtime routine moninit() to cause 
 * the rld loaded code to be profiled.  It is passed a pointer to the the
 * profiling runtime routine monaddtion() to be called after a sucessfull
 * rld_load.  It returns a pointer to rld_get_loaded_state() and is used as
 * described above.
 */
void (*
moninitrld(
void (*m)(char *lowpc, char *highpc))
)(struct rld_loaded_state **s, unsigned long *n)
{
	rld_monaddition = m;
	return(rld_get_loaded_state);
}

/*
 * rld_get_current_header() is only used by the objective-C runtime to do
 * unloading to get the current header so it does not have to save this
 * information.  It returns NULL if there is nothing is loaded currently.
 */
char *
rld_get_current_header(
void)
{
	/*
	 * If no set has been loaded at this point return NULL.
	 */
	if(cur_set == -1)
	    return(NULL);
	else
	    return(sets[cur_set].output_addr);
}
#endif /* !defined(KLD) */

/*
 * rld_address_func() is passed a pointer to a function that is then called on
 * subsequent rld_load() calls to get the address that the user wants the object
 * set loaded at.  That function is passed the memory size of the resulting
 * object set.
 */
void
#ifdef KLD
kld_address_func(
#else /* !defined(KLD) */
rld_address_func(
#endif /* KLD */
unsigned long (*func)(unsigned long size, unsigned long headers_size))
{
	address_func = func;
}

/*
 * kld_set_link_options() .
 */
void
#ifdef KLD
kld_set_link_options(
#else /* !defined(KLD) */
rld_set_link_options(
#endif /* KLD */
unsigned long link_options)
{
#ifdef KLD
	if(KLD_STRIP_NONE & link_options)
	    kld_requested_strip_level = STRIP_NONE;
	else
#endif /* KLD */
	    kld_requested_strip_level = STRIP_ALL;
}
#endif /* !defined(SA_RLD) */

/*
 * cleanup() is called by all routines handling fatal errors.
 */
__private_extern__
void
cleanup(void)
{
	fatals = 1;
	longjmp(rld_env, 1);
}

#if !defined(SA_RLD) && !defined(KLD)
/*
 * All printing of all messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
	if(error_stream != NULL)
	    NXVPrintf(error_stream, format, ap);
NXVPrintf(error_stream, format, ap);
}
#endif /* !defined(SA_RLD) && !defined(KLD) */

#ifdef KLD
/*
 * All printing of all messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
	kld_error_vprintf(format, ap);
}
#endif /* KLD */

#if !defined(SA_RLD) && !defined(KLD)
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

	if(zonep == NULL){
	    zonep = NXCreateZone(vm_page_size, vm_page_size, 1);
	    if(zonep == NULL)
		fatal("can't create NXZone");
	    NXNameZone(zonep, "rld");
	}
	if(size == 0)
	    return(NULL);
	if((p = NXZoneMalloc(zonep, size)) == NULL)
	    system_fatal("virtual memory exhausted (NXZoneMalloc failed)");
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
	if(zonep == NULL){
	    zonep = NXCreateZone(vm_page_size, vm_page_size, 1);
	    if(zonep == NULL)
		fatal("can't create NXZone");
	    NXNameZone(zonep, "rld");
	}
	if(p == NULL)
	    return(allocate(size));
	if((p = NXZoneRealloc(zonep, p, size)) == NULL)
	    system_fatal("virtual memory exhausted (NXZoneRealloc failed)");
	return(p);
}
#endif /* !defined(SA_RLD) && !defined(KLD) */

#ifdef SA_RLD
/*
 * These two variables are set in sa_rld() and used in layout_segments()
 * as the place to put the output in memory.
 */
__private_extern__ char         *sa_rld_output_addr = NULL;
__private_extern__ unsigned long sa_rld_output_size = 0;

/*
 * These two variables are set in sa_rld() and used in vprint() (defined in this
 * file) as the buffer to put error messages and the size of the buffer.
 */
static char         *sa_rld_error_buf_addr = NULL;
static unsigned long sa_rld_error_buf_size = 0;

/*
 * If this is FALSE the SA_RLD malloc package has not been initialized
 * and needs to be.
 */
static enum bool sa_rld_malloc_initialized = FALSE;

/*
 * sa_rld_internal() is the function that implements sa_rld() and
 * sa_rld_with_symtab().  If symtab is NULL then the symbol table is found via
 * the mach header.
 */
static
int
sa_rld_internal(
char		   *basefile_name,  /* base file name */
struct mach_header *basefile_addr,  /* mach header of the base file */

char               *object_name,    /* name of the object to load */
char               *object_addr,    /* addr of the object in memory to load */
unsigned long       object_size,    /* size of the object in memory to load */

char               *workmem_addr,   /* address of working memory */
unsigned long      *workmem_size,   /* size of working memory (in/out) */

char               *error_buf_addr, /* address of error message buffer */
unsigned long       error_buf_size, /* size of error message buffer */

char               *malloc_addr,    /* address to use for initializing malloc */
unsigned long       malloc_len,     /* length to use for same */

struct nlist       *symtab,         /* pointer to the symbol table */
unsigned long      nsyms,           /* number of symbols */

char               *strtab,         /* pointer to the string table */
unsigned long      strsize)         /* sizeof the string table */
{
    int status;
    struct segment_command *linkedit;

	/*
	 * Initialized the stand alone malloc package if needed.
	 */
	if(sa_rld_malloc_initialized == FALSE){
	    malloc_init(malloc_addr, malloc_len, 1000);
	    sa_rld_malloc_initialized = TRUE;
	}

	/*
	 * Set up and handle link edit errors and fatal errors
	 */
	if(setjmp(rld_env) != 0){
	    /*
	     * It takes a longjmp() to get to this point.  If it was not a fatal
	     * error unload the base file being loaded.  Otherwise just return
	     * failure.
	     */
	    if(fatals == 0)
		rld_unload_all(NULL, 1);
	    return(0);
	}

	/* This must be cleared for each call to rld() */
	errors = 0;

	/* Set up the globals for rld */
#ifdef KLD
	progname = "kld()";
#else /* !defined(KLD) */
	progname = "rld()";
#endif /* KLD */
	host_pagesize = getpagesize();
	host_byte_sex = get_host_byte_sex();
	strip_base_symbols = TRUE;
	force_cpusubtype_ALL = TRUE;

	/* Set up the globals for sa_rld */
	sa_rld_output_size = *workmem_size;
	sa_rld_output_addr = workmem_addr;
	sa_rld_error_buf_addr = error_buf_addr;
	sa_rld_error_buf_size = error_buf_size;

	/*
	 * If the symbols from base program has not been loaded load them.
	 * This will happen the first time rld() is called or will not happen.
	 */
	if(base_obj == NULL){
 	    if(symtab == NULL){
		linkedit = getsegbynamefromheader(basefile_addr, SEG_LINKEDIT);
		if(linkedit != NULL)
		    merge_base_program(basefile_name, basefile_addr, linkedit,
					NULL, 0, NULL, 0);
	    }
	    else{
		merge_base_program(basefile_name, basefile_addr, NULL,
				   symtab, nsyms, strtab, strsize);
	    }
	    if (target_byte_sex == UNKNOWN_BYTE_SEX)
		target_byte_sex = host_byte_sex;
	    /*
	     * If there were any errors in processing the base program it is
	     * treated as a fatal error and no futher processing is done.
	     */
	    if(errors){
		fatals = 1;
		return(0);
	    }
	}

	/*
	 * The work of loading the mapped file is done like very much like
	 * a call to rld_load_from_memory().
	 */
	status = internal_rld_load(NULL, /* NXStream *stream */
				   NULL, /* struct mach_header **header_addr */
				   NULL, /* char * const *object_filenames, */
				   NULL, /* char *output_filename */
				   object_name, object_addr, object_size);
	if(status == 0)
	    return(0);

	/*
	 * Now that the mapped file has been loaded unload it but leave the
	 * linked output in memory.  This is done with a normal call to
	 * internal_rld_unload() which has in it an ifdef SA_RLD to not
	 * deallocate the output memory.
	 */ 
	status = internal_rld_unload(NULL, FALSE);

	/*
	 * Return the size of the working memory used for the output.
	 */
	*workmem_size = output_size;

	return(status);
}

/*
 * sa_rld() loads the specified object in memory against the specified base file
 * in memory.  The output is placed in memory starting at the value of the
 * parameter workmem_addr and the size of the memory used for the output
 * returned indirectly through workmem_size.  Initially *workmem_size is the
 * size of the working memory.
 */
int
sa_rld(
char		   *basefile_name,  /* base file name */
struct mach_header *basefile_addr,  /* mach header of the base file */

char               *object_name,    /* name of the object to load */
char               *object_addr,    /* addr of the object in memory to load */
unsigned long       object_size,    /* size of the object in memory to load */

char               *workmem_addr,   /* address of working memory */
unsigned long      *workmem_size,   /* size of working memory (in/out) */

char               *error_buf_addr, /* address of error message buffer */
unsigned long       error_buf_size, /* size of error message buffer */

char               *malloc_addr,    /* address to use for initializing malloc */
unsigned long       malloc_len)     /* length to use for same */
{
	return(sa_rld_internal(basefile_name, basefile_addr, object_name,
			       object_addr, object_size, workmem_addr,
			       workmem_size, error_buf_addr, error_buf_size,
			       malloc_addr, malloc_len, NULL, 0, NULL, 0));
}

/*
 * sa_rld_with_symtab() is the same as sa_rld() except it passed in a pointer
 * to the symbol table, its size and a pointer to the string table and its
 * size.  Rather getting the the symbol table off of the mach header and the
 * link edit segment.
 */
int
sa_rld_with_symtab(
char		   *basefile_name,  /* base file name */
struct mach_header *basefile_addr,  /* mach header of the base file */

char               *object_name,    /* name of the object to load */
char               *object_addr,    /* addr of the object in memory to load */
unsigned long       object_size,    /* size of the object in memory to load */

char               *workmem_addr,   /* address of working memory */
unsigned long      *workmem_size,   /* size of working memory (in/out) */

char               *error_buf_addr, /* address of error message buffer */
unsigned long       error_buf_size, /* size of error message buffer */

char               *malloc_addr,    /* address to use for initializing malloc */
unsigned long       malloc_len,     /* length to use for same */

struct nlist       *symtab,         /* pointer to the symbol table */
unsigned long      nsyms,           /* number of symbols */

char               *strtab,         /* pointer to the string table */
unsigned long      strsize)         /* sizeof the string table */
{
	return(sa_rld_internal(basefile_name, basefile_addr, object_name,
			       object_addr, object_size, workmem_addr,
			       workmem_size, error_buf_addr, error_buf_size,
			       malloc_addr, malloc_len, symtab, nsyms, strtab,
			       strsize));
}

/*
 * All printing of all SA_RLD messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
    unsigned long new;

	new = slvprintf(sa_rld_error_buf_addr,
			sa_rld_error_buf_size, format, ap);
	sa_rld_error_buf_addr += new;
	sa_rld_error_buf_size -= new;
}
#endif /* SA_RLD */

#if defined(SA_RLD) || defined(KLD)

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
	    fatal("virtual memory exhausted (malloc failed)");
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
	    fatal("virtual memory exhausted (realloc failed)");
	return(p);
}
#endif /* defined(SA_RLD) || defined(KLD) */

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

#if defined(KLD) && defined(__STATIC__)
/*
 * The Kernel framework does not provide this API so we have a copy here.
 */
__private_extern__
struct mach_header *
_NSGetMachExecuteHeader(void)
{
    return((struct mach_header *)&_mh_execute_header);
}
#endif /* defined(KLD) && defined(__STATIC__) */

#endif /* RLD */
