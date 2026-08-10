/*
 * Copyright (c) 2003-2006,2008,2010-2012 Apple Inc. All Rights Reserved.
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
 *
 * nssUtils.cpp
 */

#include "nssUtils.h"
#include <string.h>

/*
 * Compare two SecAsn1Items (or two SecAsn1Oids), return true if identical.
 */
int nssCompareSecAsn1Items(
	const SecAsn1Item *data1,
	const SecAsn1Item *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return 0;
	}
	if(data1->Length != data2->Length) {
		return 0;
	}
	return memcmp(data1->Data, data2->Data, data1->Length) == 0;
}

int nssCompareCssmData(
    const SecAsn1Item *data1,
    const SecAsn1Item *data2)
{
    return nssCompareSecAsn1Items(data1,data2);
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
unsigned nssArraySize(
	const void **array)
{
    unsigned count = 0;
    if (array) {
		while (*array++) {
			count++;
		}
    }
    return count;
}

