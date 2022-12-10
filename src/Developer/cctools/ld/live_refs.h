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
 * For dead stipping the routine mark_fine_relocs_references_live() calls a
 * machine dependent *_get_reloc_refs() routine that fills in the live_refs
 * structure for a relocation entry in a section.
 */
enum live_ref_type {
    LIVE_REF_NONE,	/* there is no reference */
    LIVE_REF_VALUE,	/* the reference is in value, an address in on one
			   of the object's sections */
    LIVE_REF_SYMBOL	/* the reference is in the merged_symbol */
};
    
struct live_ref {
    enum live_ref_type ref_type;
    unsigned long value;
    struct merged_symbol *merged_symbol;
};

struct live_refs {
   struct live_ref ref1;
   struct live_ref ref2;
};
