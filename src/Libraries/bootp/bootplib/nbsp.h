/*
 * Copyright (c) 2001-2002 Apple Inc. All rights reserved.
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

/*
 * nbsp.h
 * - NetBoot SharePoint routines
 */

#ifndef _S_NBSP_H
#define _S_NBSP_H

#include <stdbool.h>

#define NBSP_NO_READONLY	FALSE
#define NBSP_READONLY_OK	TRUE

typedef struct {
    char *	name;
    char *	path;
    bool	is_hfs;
    bool	is_readonly;
} NBSPEntry, * NBSPEntryRef;

struct NBSPList_s;

typedef struct NBSPList_s * NBSPListRef;

int		NBSPList_count(NBSPListRef list);
NBSPEntryRef	NBSPList_element(NBSPListRef list, int i);
void		NBSPList_print(NBSPListRef list);
void		NBSPList_free(NBSPListRef * list);
NBSPListRef	NBSPList_init(const char * symlink_name, bool readonly_ok);

#endif /* _S_NBSP_H */
