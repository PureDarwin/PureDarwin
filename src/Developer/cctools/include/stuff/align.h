/*
 * Copyright (c) 2019 Apple Computer, Inc. All rights reserved.
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

#ifndef align_h
#define align_h

#include <mach-o/loader.h>
#include "stuff/bool.h"

/* The maximum segment alignment allowed to be specified, as a power of two */
#define MAXSEGALIGN         15 /* 2**15 or 0x8000 */

/* The minimum segment alignment for Mach-O files, as a power of two */
#define MINSEGALIGN32        2 /* 2**2 or sizeof(uint32_t) */
#define MINSEGALIGN64        3 /* 2**3 or sizeof(uint64_t) */

/*
 * guess_align is passed a vmaddr of a segment and guesses what the segment
 * alignment was.  It uses the most conservative guess within the minimum and
 * maximum values supplied.
 */
__private_extern__
uint32_t
guess_align(uint64_t vmaddr, uint32_t min, uint32_t max);

/*
 * get_seg_align() returns the segment alignment for a Mach-O, as an exponent of
 * a power of 2; e.g., a segment alignment of 0x4000 will be described as 14.
 * You can convert the segment alignment into a page size by computing like so:
 *
 *     pagesize = 1 << segalign;
 *
 * (Note that sometimes cctools uses "segalign" as a synonym for "pagesize".
 * It may not be obvious if the segment alignment is in exponent form or in
 * power-of-two form.)
 *
 * Since the actual segment alignment used by the linker is not recorded in the
 * Mach-O file, get_seg_align() will choose an alignment based on the file
 * contents. The Mach-O is described by a mach header (either 32-bit or 64-bit),
 * its load commands, and the final size of the file. get_seg_align() uses this
 * data to guess the alignment passed to ld(1) in the following ways:
 *
 *   If the Mach-O is an MH_OBJECT (.o) file, get_seg_align() will return the
 *   largest section alignment within the first-and-only segment.
 *
 *   If the Mach-O is any other file type, get_seg_align() will guess the
 *   alignment for each segment from vmaddr, and then return the smallest such
 *   value. Since all well-formed segments are required to be page aligned, the
 *   resulting alignment will be legal, but there is a risk that unlucky
 *   binaries will choose an alignment value that is larger than necessary.
 *
 * In either method, the result of get_seg_align() on a 32-bit file will be
 * bounded by 2 (which is log2(sizeof(uint32_t))) and MAXSECTALIGN, and on a
 * 64-bit file will be bounded by 3 (log2(sizeof(uint64_t))) and MAXSECTALIGN.
 *
 * The swap flag should be set to TRUE if the calling program has not yet
 * swapped the load commands. This flag really is unnecessary.)
 */
__private_extern__
uint32_t
get_seg_align(struct mach_header *mhp,
              struct mach_header_64 *mhp64,
              struct load_command *load_commands,
              enum bool swap_load_commands,
              uint64_t size,
              char *name);

#endif /* align_h */
