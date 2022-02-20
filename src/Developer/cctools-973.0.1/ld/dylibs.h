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
 * Global types, variables and routines declared in the file dylibs.c.
 */

__private_extern__ enum bool has_dynamic_linker_command;

#ifndef RLD

struct merged_dylib {
    char *dylib_name;		/* The name of this dynamic shared library. */
    struct dylib_command *dl;	/* The load command for this dynamicly linked */
				/*  shared library. */
    struct object_file		/* Pointer to the object file the load */
	*definition_object;	/*  command was found in */
    enum bool output_id;	/* This is the output file's LC_ID_DYLIB */
				/*  command others get turned into */
				/*  LD_LOAD_DYLIB commands */
    struct dynamic_library	/* The dynamic_library struct for this */
	*dynamic_library;	/*  dynamic library shared library */
    struct merged_dylib *next;	/* The next in the list, NULL otherwise */
};
/* the pointer to the head of the dynamicly linked shared library commands */
__private_extern__ struct merged_dylib *merged_dylibs;

/* the pointer to the head of the dynamicly linked shared library segments */
__private_extern__ struct merged_segment *dylib_segments;

__private_extern__ void create_dylib_id_command(
    void);
__private_extern__ void merge_dylibs(
    enum bool force_weak);
__private_extern__ void add_dylib_segment(
    struct segment_command *sg,
    char *dylib_name,
    enum bool split_dylib);

struct merged_dylinker {
    char *dylinker_name;	/* The name of dynamic linker */
    struct dylinker_command
	*dyld;			/* The load command for the dynamicly linker */
    struct object_file		/* Pointer to the object file the load */
	*definition_object;	/*  command was found in */
};
/* the pointer to the merged the dynamic linker command if any */
__private_extern__ struct merged_dylinker *merged_dylinker;

__private_extern__ void create_dylinker_id_command(
    void);

struct merged_sub_frameworks {
    char *unbrell_name;		/* The name of the unbrella framework */
    struct sub_framework_command
	*sub;			/* The load command for the output file */
};
/* the pointer to the merged sub_framework command if any */
__private_extern__ struct merged_sub_frameworks *merged_sub_framework;

__private_extern__ void create_sub_framework_command(
    void);

struct merged_sub_umbrella {
    struct sub_umbrella_command
	*sub;			/* The load command for the output file */
};
/* the pointer to the merged sub_umbrella commands if any */
__private_extern__ struct merged_sub_umbrella *merged_sub_umbrellas;

__private_extern__ unsigned long create_sub_umbrella_commands(
    void);

struct merged_sub_library {
    struct sub_library_command
	*sub;			/* The load command for the output file */
};
/* the pointer to the merged sub_library commands if any */
__private_extern__ struct merged_sub_library *merged_sub_librarys;

__private_extern__ unsigned long create_sub_library_commands(
    void);

struct merged_sub_client {
    struct sub_client_command
	*sub;			/* The load command for the output file */
};
/* the pointer to the merged sub_client commands if any */
__private_extern__ struct merged_sub_client *merged_sub_clients;

__private_extern__ unsigned long create_sub_client_commands(
    void);

#endif /* !defined(RLD) */
