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
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#include <time.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"
#include "stuff/guess_short_name.h"

#include "dylibs.h"
#include "ld.h"
#include "live_refs.h"
#include "objects.h"
#include "pass1.h"
#include "sections.h"

__private_extern__ enum bool has_dynamic_linker_command = FALSE;

#ifndef RLD

/* the pointer to the head of the dynamicly linked shared library commands */
__private_extern__ struct merged_dylib *merged_dylibs = NULL;

/* the pointer to the merged the dynamic linker command if any */
__private_extern__ struct merged_dylinker *merged_dylinker = NULL;

/* the pointer to the merged sub_framework command if any */
__private_extern__ struct merged_sub_frameworks *merged_sub_framework = NULL;

/* the pointer to the head of the dynamicly linked shared library segments */
__private_extern__ struct merged_segment *dylib_segments = NULL;

/* the pointer to the merged sub_umbrella commands if any */
__private_extern__ struct merged_sub_umbrella *merged_sub_umbrellas = NULL;

/* the pointer to the merged sub_library commands if any */
__private_extern__ struct merged_sub_library *merged_sub_librarys = NULL;

/* the pointer to the merged sub_client commands if any */
__private_extern__ struct merged_sub_client *merged_sub_clients = NULL;

static struct merged_dylib *lookup_merged_dylib(
    struct dylib_command *dl);

/*
 * create_dylib_id_command() creates the LC_ID_DYLIB load command from the
 * command line argument values.  It is called from layout() when the output
 * filetype is MH_DYLIB.
 */
__private_extern__
void
create_dylib_id_command(
void)
{
    char *name;
    unsigned long cmdsize;
    struct dylib_command *dl;
    struct merged_dylib *mdl;

	if(dylib_install_name != NULL)
	    name = dylib_install_name;
	else
	    name = outputfile;

	cmdsize = sizeof(struct dylib_command) +
		  rnd(strlen(name) + 1, sizeof(long));
	dl = allocate(cmdsize);
	memset(dl, '\0', cmdsize);
	dl->cmd = LC_ID_DYLIB;
	dl->cmdsize = cmdsize;
	dl->dylib.name.offset = sizeof(struct dylib_command);
	dl->dylib.timestamp = time(0);
	dl->dylib.current_version = dylib_current_version;
	dl->dylib.compatibility_version = dylib_compatibility_version;
	strcpy((char *)dl + sizeof(struct dylib_command), name);

	mdl = allocate(sizeof(struct merged_dylib));
	memset(mdl, '\0', sizeof(struct merged_dylib));
	mdl->dylib_name = name;
	mdl->dl = dl;
	mdl->output_id = TRUE;
	mdl->next = merged_dylibs;
	merged_dylibs = mdl;
}

/*
 * merge_dylibs() merges in the dylib commands from the current object.
 */
__private_extern__
void
merge_dylibs(
enum bool force_weak)
{
    unsigned long i;
    struct mach_header *mh;
    struct load_command *lc;
    struct dylib_command *dl;
    struct dylinker_command *dyld;
    char *dyld_name;
    struct merged_dylib *mdl;
    struct dynamic_library *p;

	/*
	 * Process all the load commands for the dynamic shared libraries.
	 */
	mh = (struct mach_header *)cur_obj->obj_addr;
	lc = (struct load_command *)((char *)cur_obj->obj_addr +
				     sizeof(struct mach_header));
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmd == LC_ID_DYLIB ||
	       lc->cmd == LC_LOAD_DYLIB ||
	       lc->cmd == LC_LOAD_WEAK_DYLIB ||
	       lc->cmd == LC_REEXPORT_DYLIB){
		/*
		 * Do not record dynamic libraries dependencies in the output
		 * file.  Only record the library itself.
		 */
		if((lc->cmd != LC_LOAD_DYLIB &&
		    lc->cmd != LC_LOAD_WEAK_DYLIB &&
		    lc->cmd != LC_REEXPORT_DYLIB) ||
		   (mh->filetype != MH_DYLIB &&
		    mh->filetype != MH_DYLIB_STUB) ){
		    dl = (struct dylib_command *)lc;
		    mdl = lookup_merged_dylib(dl);
		    if(filetype == MH_DYLIB && dylib_install_name != NULL &&
		       strcmp(mdl->dylib_name, dylib_install_name) == 0)
			error_with_cur_obj("can't be linked because it has the "
			   "same install_name (%s) as the output", 
			   dylib_install_name);
		    p = add_dynamic_lib(DYLIB, dl, cur_obj);
		    p->force_weak_dylib = force_weak;
		    mdl->dynamic_library = p;
		}
	    }
	    else if(lc->cmd == LC_LOAD_DYLINKER || lc->cmd == LC_ID_DYLINKER){
		dyld = (struct dylinker_command *)lc;
		dyld_name = (char *)dyld + dyld->name.offset;
		if(merged_dylinker == NULL){
		    merged_dylinker = allocate(sizeof(struct merged_dylinker));
		    memset(merged_dylinker, '\0',
			   sizeof(struct merged_dylinker));
		    merged_dylinker->dylinker_name = dyld_name;
		    merged_dylinker->definition_object = cur_obj;
		    merged_dylinker->dyld = dyld;
		    has_dynamic_linker_command = TRUE;
		    if(save_reloc == FALSE)
			output_for_dyld = TRUE;
		}
		else if(strcmp(dyld_name, merged_dylinker->dylinker_name) != 0){
		    error("multiple dynamic linkers loaded (only one allowed)");
		    print_obj_name(merged_dylinker->definition_object);
		    print("loads dynamic linker %s\n",
			  merged_dylinker->dylinker_name);
		    print_obj_name(cur_obj);
		    print("loads dynamic linker %s\n", dyld_name);
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * lookup_merged_dylib() adds a LC_LOAD_DYLIB command to it to the merged list
 * of dynamic shared libraries for the load command passed to it.  It ignores
 * the command if it see the same library twice.
 */
static
struct merged_dylib *
lookup_merged_dylib(
struct dylib_command *dl)
{
    char *dylib_name;
    struct merged_dylib **p, *mdl;

	dylib_name = (char *)dl + dl->dylib.name.offset;
	p = &merged_dylibs;
	while(*p){
	    mdl = *p;
	    if(strcmp(mdl->dylib_name, dylib_name) == 0){
		if(mdl->dl->cmd == LC_ID_DYLIB){
		    /*
		     * If the new one is also a LC_ID_DYLIB use the one with the
		     * highest compatiblity number.  Else if the new one is just
		     * an LC_LOAD_DYLIB ignore it and use the merged one that is
		     * a LC_ID_DYLIB.
		     */
		    if(dl->cmd == LC_ID_DYLIB){
		       if(dl->dylib.compatibility_version >
			  mdl->dl->dylib.compatibility_version){
			    if(strcmp(mdl->definition_object->file_name,
				      cur_obj->file_name) != 0)
				warning("multiple references to dynamic shared "
				    "library: %s (from %s and %s, using %s "
				    "which has higher compatibility_version)",
				    dylib_name,
				    mdl->definition_object->file_name,
				    cur_obj->file_name, cur_obj->file_name);
			    mdl->dylib_name = dylib_name;
			    mdl->dl = dl;
			    mdl->definition_object = cur_obj;
			}
		    }
		}
		else{
		    if(dl->cmd == LC_ID_DYLIB){
			mdl->dylib_name = dylib_name;
			mdl->dl = dl;
			mdl->definition_object = cur_obj;
		    }
		}
		return(mdl);
	    }
	    p = &(mdl->next);
	}
	*p = allocate(sizeof(struct merged_dylib));
	memset(*p, '\0', sizeof(struct merged_dylib));
	mdl = *p;
	mdl->dylib_name = dylib_name;
	mdl->dl = dl;
	mdl->definition_object = cur_obj;
	mdl->output_id = FALSE;
	return(mdl);
}

/*
 * create_dylinker_id_command() creates the LC_ID_DYLINKER load command from the
 * command line argument values.  It is called from layout() when the output
 * filetype is MH_DYLINKER.
 */
__private_extern__
void
create_dylinker_id_command(
void)
{
    char *name;
    unsigned long cmdsize;
    struct dylinker_command *dyld;
    struct merged_dylinker *mdyld;

	if(dylinker_install_name != NULL)
	    name = dylinker_install_name;
	else
	    name = outputfile;

	cmdsize = sizeof(struct dylinker_command) +
		  rnd(strlen(name) + 1, sizeof(long));
	dyld = allocate(cmdsize);
	memset(dyld, '\0', cmdsize);
	dyld->cmd = LC_ID_DYLINKER;
	dyld->cmdsize = cmdsize;
	dyld->name.offset = sizeof(struct dylinker_command);
	strcpy((char *)dyld + sizeof(struct dylinker_command), name);

	mdyld = allocate(sizeof(struct merged_dylinker));
	memset(mdyld, '\0', sizeof(struct merged_dylinker));
	mdyld->dylinker_name = name;
	mdyld->dyld = dyld;
	merged_dylinker = mdyld;
}

/*
 * create_sub_framework_command() creates a LC_SUB_FRAMEWORK load command from
 * the command line argument values.  It is called from layout() when the output
 * filetype is MH_DYLIB and -sub_framework was specified.
 */
__private_extern__
void
create_sub_framework_command(
void)
{
    char *name;
    unsigned long cmdsize;
    struct sub_framework_command *sub;
    struct merged_sub_frameworks *msub;

	name = umbrella_framework_name;

	cmdsize = sizeof(struct sub_framework_command) +
		  rnd(strlen(name) + 1, sizeof(long));
	sub = allocate(cmdsize);
	memset(sub, '\0', cmdsize);
	sub->cmd = LC_SUB_FRAMEWORK;
	sub->cmdsize = cmdsize;
	sub->umbrella.offset = sizeof(struct sub_framework_command);
	strcpy((char *)sub + sizeof(struct sub_framework_command), name);

	msub = allocate(sizeof(struct merged_sub_frameworks));
	memset(msub, '\0', sizeof(struct merged_sub_frameworks));
	msub->unbrell_name = name;
	msub->sub = sub;
	merged_sub_framework = msub;
}

/*
 * create_sub_umbrella_commands() creates the LC_SUB_UMBRELLA load commands from
 * the command line options.  It is called from layout() when the output 
 * filetype is MH_DYLIB and one or more -sub_umbrella flags were specified.
 * It returns the total size of the load commands it creates.
 */
__private_extern__
unsigned long
create_sub_umbrella_commands(
void)
{
    unsigned long i;
    char *name, *umbrella_framework_name, *has_suffix;
    unsigned long cmdsize, sizeofcmds;
    struct sub_umbrella_command *sub;
    enum bool found, is_framework;
    struct merged_dylib **p, *mdl;

	sizeofcmds = 0;
	merged_sub_umbrellas = allocate(sizeof(struct merged_sub_umbrella) *
				        nsub_umbrellas);
	for(i = 0; i < nsub_umbrellas ; i++){
	    name = sub_umbrellas[i];

	    found = FALSE;
	    p = &merged_dylibs;
	    while(*p){
		mdl = *p;
		umbrella_framework_name = guess_short_name(mdl->dylib_name,
			&is_framework, &has_suffix);
		if(umbrella_framework_name != NULL &&
		   is_framework == TRUE &&
		   strcmp(umbrella_framework_name, name) == 0){
		    found = TRUE;
		    break;
		}
		p = &(mdl->next);
	    }
	    if(found == FALSE)
		error("-sub_umbrella %s specified but no framework by that "
		      "name is linked in", name);

	    cmdsize = sizeof(struct sub_umbrella_command) +
		      rnd(strlen(name) + 1, sizeof(long));
	    sub = allocate(cmdsize);
	    memset(sub, '\0', cmdsize);
	    sub->cmd = LC_SUB_UMBRELLA;
	    sub->cmdsize = cmdsize;
	    sub->sub_umbrella.offset = sizeof(struct sub_umbrella_command);
	    strcpy((char *)sub + sizeof(struct sub_umbrella_command), name);

	    sizeofcmds += cmdsize;
	    merged_sub_umbrellas[i].sub = sub;
	}
	return(sizeofcmds);
}

/*
 * create_sub_library_commands() creates the LC_SUB_LIBRARY load commands from
 * the command line options.  It is called from layout() when the output 
 * filetype is MH_DYLIB and one or more -sub_library flags were specified.
 * It returns the total size of the load commands it creates.
 */
__private_extern__
unsigned long
create_sub_library_commands(
void)
{
    unsigned long i;
    char *name, *library_name, *has_suffix;
    unsigned long cmdsize, sizeofcmds;
    struct sub_library_command *sub;
    enum bool found, is_framework;
    struct merged_dylib **p, *mdl;

	sizeofcmds = 0;
	merged_sub_librarys = allocate(sizeof(struct merged_sub_library) *
				        nsub_librarys);
	for(i = 0; i < nsub_librarys ; i++){
	    name = sub_librarys[i];

	    found = FALSE;
	    p = &merged_dylibs;
	    while(*p){
		mdl = *p;
		library_name = guess_short_name(mdl->dylib_name,
			&is_framework, &has_suffix);
		if(library_name != NULL &&
		   is_framework == FALSE &&
		   strcmp(library_name, name) == 0){
		    found = TRUE;
		    break;
		}
		p = &(mdl->next);
	    }
	    if(found == FALSE)
		error("-sub_library %s specified but no library by that "
		      "name is linked in", name);

	    cmdsize = sizeof(struct sub_library_command) +
		      rnd(strlen(name) + 1, sizeof(long));
	    sub = allocate(cmdsize);
	    memset(sub, '\0', cmdsize);
	    sub->cmd = LC_SUB_LIBRARY;
	    sub->cmdsize = cmdsize;
	    sub->sub_library.offset = sizeof(struct sub_library_command);
	    strcpy((char *)sub + sizeof(struct sub_library_command), name);

	    sizeofcmds += cmdsize;
	    merged_sub_librarys[i].sub = sub;
	}
	return(sizeofcmds);
}

/*
 * create_sub_client_commands() creates the LC_SUB_CLIENT load commands from
 * the command line options.  It is called from layout() when the output 
 * filetype is MH_DYLIB and one or more -allowable_client flags were specified.
 * It returns the total size of the load commands it creates.
 */
__private_extern__
unsigned long
create_sub_client_commands(
void)
{
    unsigned long i;
    char *name;
    unsigned long cmdsize, sizeofcmds;
    struct sub_client_command *sub;

	sizeofcmds = 0;
	merged_sub_clients = allocate(sizeof(struct merged_sub_client) *
				      nallowable_clients);
	for(i = 0; i < nallowable_clients ; i++){
	    name = allowable_clients[i];
	    cmdsize = sizeof(struct sub_client_command) +
		      rnd(strlen(name) + 1, sizeof(long));
	    sub = allocate(cmdsize);
	    memset(sub, '\0', cmdsize);
	    sub->cmd = LC_SUB_CLIENT;
	    sub->cmdsize = cmdsize;
	    sub->client.offset = sizeof(struct sub_client_command);
	    strcpy((char *)sub + sizeof(struct sub_client_command), name);

	    sizeofcmds += cmdsize;
	    merged_sub_clients[i].sub = sub;
	}
	return(sizeofcmds);
}

/*
 * add_dylib_segment() adds the specified segment to the list of
 * dylib_segments as comming from the specified dylib_name.
 */
__private_extern__
void
add_dylib_segment(
struct segment_command *sg,
char *dylib_name,
enum bool split_dylib)
{
    struct merged_segment **p, *msg;

	p = &dylib_segments;
	while(*p){
	    msg = *p;
	    p = &(msg->next);
	}
	*p = allocate(sizeof(struct merged_segment));
	msg = *p;
	memset(msg, '\0', sizeof(struct merged_segment));
	msg->sg = *sg;
	msg->filename = dylib_name;
	msg->split_dylib = split_dylib;
}
#endif /* !defined(RLD) */
