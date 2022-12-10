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
 * The mod_term_data is only used when -dead_strip is specified to break up
 * the section so each pointer has a fine relocation entry.
 */
struct mod_term_data {
    unsigned long output_offset;
};

/*
 * Global types, variables and routines declared in the file modinit_sections.c.
 */
__private_extern__ unsigned long ninit;
__private_extern__ unsigned long nterm;

__private_extern__ void mod_section_merge(
    struct mod_term_data *data, 
    struct merged_section *ms,
    struct section *s, 
    struct section_map *section_map,
    enum bool redo_live);
__private_extern__ void mod_section_order(
    struct mod_term_data *data, 
    struct merged_section *ms);
__private_extern__ void mod_section_reset_live(
    struct mod_term_data *data, 
    struct merged_section *ms);
__private_extern__ void mod_section_free(
    struct mod_term_data *data);
