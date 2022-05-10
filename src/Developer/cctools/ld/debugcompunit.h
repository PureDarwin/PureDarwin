/*
 * Copyright (c) 2006 Apple Computer, Inc.  All Rights Reserved.
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
#include <stdint.h>
#include <stddef.h>

/* Given pointers to the DEBUG_INFO and DEBUG_ABBREV sections, and
   their corresponding sizes, and whether the object file is
   LITTLE_ENDIAN or not, look at the compilation unit DIE and
   determine its NAME, compilation directory (in COMP_DIR) and its
   line number information offset (in STMT_LIST).  NAME and COMP_DIR
   may be NULL (especially COMP_DIR) if they are not in the .o file;
   STMT_LIST will be (uint64_t) -1.

   At present this assumes that there's only one compilation unit DIE.  */

int read_comp_unit (const uint8_t * debug_info,
		    size_t debug_info_size,
		    const uint8_t * debug_abbrev,
		    size_t debug_abbrev_size,
		    int little_endian,
		    const char ** name,
		    const char ** comp_dir,
		    uint64_t *stmt_list);
