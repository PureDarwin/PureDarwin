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
 * Global types, variables and routines declared in the file layout.c.
 * (and the global varaiable declared in rld.c).
 *
 * The following include file need to be included before this file:
 * #include <sys/loader.h>
 */

/* The output file's mach header */
__private_extern__ struct mach_header output_mach_header;

/*
 * The output file's symbol table load command and the offsets used in the
 * second pass to output the symbol table and string table.
 */
struct symtab_info {
    struct symtab_command symtab_command;
    unsigned long
      output_strpad,	     /* the number of padded bytes at the end */
      output_merged_strsize, /* the current merged string table size in pass2 */
      output_local_strsize;  /* the current local string table size in pass2 */
};
__private_extern__ struct symtab_info output_symtab_info;

/*
 * The output file's dynamic symbol table load command.
 */
struct dysymtab_info {
    struct dysymtab_command dysymtab_command;
};
__private_extern__ struct dysymtab_info output_dysymtab_info;

/*
 * The output file's two level hints load command.
 */
struct hints_info {
    struct twolevel_hints_command twolevel_hints_command;
};
__private_extern__ struct hints_info output_hints_info;

/*
 * The output file's prebind_cksum load command.
 */
struct cksum_info {
    struct prebind_cksum_command prebind_cksum_command;
};
__private_extern__ struct cksum_info output_cksum_info;

/*
 * The output file's uuid load command.
 */
struct uuid_info {
    enum bool suppress;	/* suppress when -no_uuid is specified */
    enum bool emit;	/* TRUE if any input file has a debug section or
			   an LC_UUID load command */
    struct uuid_command uuid_command;
};
__private_extern__ struct uuid_info output_uuid_info;

/*
 * The output file's thread load command and the machine specific information
 * for it.
 */
struct thread_info {
    struct thread_command thread_command;
    enum bool thread_in_output;	/* TRUE if the output file has a thread cmd */
    unsigned long flavor;	/* the flavor for the registers with the pc */
    unsigned long count;	/* the count (sizeof(long)) of the state */
    int *entry_point;		/* pointer to the entry point in the state */
    int *stack_pointer;		/* pointer to the stack pointer in the state */
    void *state;		/* a pointer to a thread state */
    unsigned long second_flavor;/* the flavor for the registers with the sp */
    unsigned long second_count;	/* the count (sizeof(long)) of second_state */
    void *second_state;		/* a pointer to the second thread state */
};
__private_extern__ struct thread_info output_thread_info;

/*
 * The output file's routines load command and the information for it.
 */
struct routines_info {
    struct routines_command routines_command;
    enum bool routines_in_output;/* TRUE if the output file has a routines cmd*/
};
__private_extern__ struct routines_info output_routines_info;

__private_extern__ void layout(
    void);
#if defined(RLD) && !defined(SA_RLD)
__private_extern__ void layout_rld_symfile(
    void);
#endif /* defined(RLD) && !defined(SA_RLD) */

#ifdef RLD
/*
 * The user's address function to be called in layout to get the address of
 * where to link edit the result.
 */
__private_extern__ unsigned long (
    *address_func)(
	unsigned long size,
	unsigned long header_address);
#endif /* RLD */

#ifdef DEBUG
__private_extern__ void print_mach_header(
    void);
__private_extern__ void print_symtab_info(
    void);
__private_extern__ void print_thread_info(
    void);
#endif /* DEBUG */
