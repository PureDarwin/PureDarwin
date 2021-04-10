/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#ifndef _MACH_O_DYLD_PRIV_H_
#define _MACH_O_DYLD_PRIV_H_


#include <mach-o/dyld.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Given an imageOffset into an ObjectFileImage, returns 
 * the segment/section name and offset into that section of
 * that imageOffset.  Returns FALSE if the imageOffset is not 
 * in any section.  You can used the resulting sectionOffset to
 * index into the data returned by NSGetSectionDataInObjectFileImage.
 * 
 * First appeared in Mac OS X 10.3 
 *
 * SPI: currently only used by ZeroLink to detect +load methods
 */
enum DYLD_BOOL 
NSFindSectionAndOffsetInObjectFileImage(
    NSObjectFileImage objectFileImage, 
    unsigned long imageOffset,
    const char** segmentName, 	/* can be NULL */
    const char** sectionName, 	/* can be NULL */
    unsigned long* sectionOffset);	/* can be NULL */




#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MACH_O_DYLD_PRIV_H_ */
