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
#undef CATS_BUG_FIX1
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines to manage the structures that hold the
 * information for the rld package of the object file sets.
 */
#ifdef RLD
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#include <mach/vm_map.h>
#include <mach/mach.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "sets.h"
/*
 * The ammount to increase the sets array when needed.
 */
#define NSETS_INCREMENT 10

/*
 * A pointer to the array of sets
 */
__private_extern__ struct set *sets = NULL;
/*
 * The number of set structures allocated in the above array.
 */
static long nsets = 0;
/*
 * The index into the sets array for the current set.
 */
__private_extern__ long cur_set = -1;

/*
 * new_set() allocates another structure for a new set in the sets array and
 * increments the cur_set to index into the sets array for the new set.
 */
__private_extern__
void
new_set(void)
{
    long i;

	if(cur_set + 2 > nsets){
	    sets = reallocate(sets,
			      (nsets + NSETS_INCREMENT) * sizeof(struct set));
	    for(i = 0; i < NSETS_INCREMENT; i++){
		memset(sets + nsets + i, '\0', sizeof(struct set));
		sets[nsets + i].link_edit_common_object =
			allocate(sizeof(struct object_file));
		memset(sets[nsets + i].link_edit_common_object, '\0',
			sizeof(struct object_file));
		sets[nsets + i].link_edit_section_maps =
			allocate(sizeof(struct section_map));
		memset(sets[nsets + i].link_edit_section_maps, '\0',
			sizeof(struct section_map));
		sets[nsets + i].link_edit_common_section =
			allocate(sizeof(struct section));
		memset(sets[nsets + i].link_edit_common_section, '\0',
		       sizeof(struct section));
	    }
	    nsets += NSETS_INCREMENT;
	}
	cur_set++;
}

/*
 * new_archive_or_fat() is called from pass1() for rld_load and keeps track of
 * the archives and fat files that are mapped so clean_archives_and_fats() can
 * deallocate their memory.
 */
__private_extern__
void
new_archive_or_fat(
char *file_name,
char *file_addr,
unsigned long file_size)
{
	sets[cur_set].archives = reallocate(sets[cur_set].archives,
					    (sets[cur_set].narchives + 1) *
					    sizeof(struct archive));

	sets[cur_set].archives[sets[cur_set].narchives].file_name =
						allocate(strlen(file_name) + 1);
	strcpy(sets[cur_set].archives[sets[cur_set].narchives].file_name,
	       file_name);
	sets[cur_set].archives[sets[cur_set].narchives].file_addr = file_addr;
	sets[cur_set].archives[sets[cur_set].narchives].file_size = file_size;
	sets[cur_set].narchives++;
}

/*
 * clean_archives_and_fats() deallocates any archives and fat files that were
 * loaded in the current set.
 */
__private_extern__
void
clean_archives_and_fats(void)
{
#ifndef SA_RLD
    unsigned long i;
    kern_return_t r;
    char *file_addr, *file_name;
    long file_size;

	if(sets != NULL && cur_set != -1){
	    for(i = 0; i < sets[cur_set].narchives; i++){
		file_addr = sets[cur_set].archives[i].file_addr;
		file_size = sets[cur_set].archives[i].file_size;
		file_name = sets[cur_set].archives[i].file_name;
		if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
				      file_size)) != KERN_SUCCESS)
		    mach_fatal(r, "can't vm_deallocate() memory for "
			       "mapped file %s", file_name);
#ifdef RLD_VM_ALLOC_DEBUG
		    print("rld() vm_deallocate: addr = 0x%0x size = 0x%x\n",
			  (unsigned int)file_addr,
			  (unsigned int)file_size);
#endif /* RLD_VM_ALLOC_DEBUG */
		free(file_name);
	    }
	    if(sets[cur_set].archives != NULL)
		free(sets[cur_set].archives);
	    sets[cur_set].archives = NULL;
	    sets[cur_set].narchives = 0;
	}
#endif /* !defined(SA_RLD) */
}

/*
 * remove_set deallocates the current set structure from the sets array.
 */
__private_extern__
void
remove_set(void)
{
	if(cur_set >= 0){
	    sets[cur_set].output_addr = NULL;
	    sets[cur_set].output_size = 0;
	    memset(sets[cur_set].link_edit_common_object, '\0',
		    sizeof(struct object_file));
	    memset(sets[cur_set].link_edit_section_maps, '\0',
		    sizeof(struct section_map));
	    memset(sets[cur_set].link_edit_common_section, '\0',
		   sizeof(struct section));
	    cur_set--;
	}
}

/*
 * free_sets frees all storage for the sets and resets everything back to the
 * initial state.
 */
__private_extern__
void
free_sets(void)
{
    long i;

	if(sets != NULL){
	    for(i = 0; i < nsets; i++){
		if(sets[i].link_edit_common_object != NULL){
		    free(sets[i].link_edit_common_object);
		}
		if(sets[i].link_edit_section_maps != NULL){
		    free(sets[i].link_edit_section_maps);
		}
		if(sets[i].link_edit_common_section != NULL){
		    free(sets[i].link_edit_common_section);
		}
	    }
	    free(sets);
	}

	sets = NULL;
	nsets = 0;
	cur_set = -1;
}
#endif /* RLD */
