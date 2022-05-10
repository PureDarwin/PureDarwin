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

#include "stuff/align.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stuff/errors.h"
#include "stuff/bytesex.h"

/*
 * guess_align is passed a vmaddr of a segment and guesses what the segment
 * alignment was.  It uses the most conservative guess up to the maximum
 * alignment that the link editor uses.
 */
__private_extern__
uint32_t
guess_align(uint64_t vmaddr, uint32_t min, uint32_t max)
{
    uint32_t align = max;

    if (vmaddr)
    {
        /* find the smallest legal alignment for this address */
        uint64_t segalign = 1;
        align = 0;
        while((segalign & vmaddr) == 0)
        {
            segalign = segalign << 1;
            align++;
        }

        /* clamp to minimum and maximum values */
        align = align < min ? min : align;
        align = align > max ? max : align;
    }

    return align;
}

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
 */
__private_extern__
uint32_t
get_seg_align(struct mach_header *mhp,
              struct mach_header_64 *mhp64,
              struct load_command *load_commands,
              enum bool swap_load_commands,
              uint64_t size,
              char *name)
{
    uint32_t cur_align = MAXSEGALIGN;
    uint32_t ncmds = mhp ? mhp->ncmds : mhp64->ncmds;
    uint32_t sizeofcmds = mhp ? mhp->sizeofcmds : mhp64->sizeofcmds;
    size_t sizeofhdr = mhp ? sizeof(*mhp) : sizeof(*mhp64);
    uint32_t filetype = mhp ? mhp->filetype : mhp64->filetype;

    if (sizeofcmds + sizeofhdr > size)
        fatal("truncated or malformed object (load commands would "
              "extend past the end of the file) in: %s", name);
    if (mhp && mhp64)
        fatal("internal error: file has both a 32-bit and 64-bit mach header:"
              "%s", name);

    struct load_command *lcp = load_commands;
    enum byte_sex host_byte_sex = get_host_byte_sex();
    for(uint32_t i = 0; i < ncmds; ++i)
    {
        uint32_t align = MAXSEGALIGN;
        struct load_command l = *lcp;
        if(swap_load_commands)
            swap_load_command(&l, host_byte_sex);
        if(l.cmdsize % sizeof(uint32_t) != 0)
            error("load command %u size not a multiple of "
                  "sizeof(uint32_t) in: %s", i, name);
        if(l.cmdsize <= 0)
            fatal("load command %u size is less than or equal to zero "
                  "in: %s", i, name);
        if((char *)lcp + l.cmdsize >
           (char *)load_commands + sizeofcmds)
            fatal("load command %u extends past end of all load "
                  "commands in: %s", i, name);

        if(l.cmd == LC_SEGMENT)
        {
            struct segment_command* sgp = (struct segment_command *)lcp;
            struct segment_command sg = *sgp;
            if(swap_load_commands)
                swap_segment_command(&sg, host_byte_sex);
            if(filetype == MH_OBJECT)
            {
                /* this is the minimum alignment, then take largest */
                align = MINSEGALIGN32;
                struct section *sp = (struct section *)((char *)sgp +
                                      sizeof(struct segment_command));
                for(uint32_t j = 0; j < sg.nsects; ++j)
                {
                    struct section s = *sp;
                    if(swap_load_commands)
                        swap_section(&s, 1, host_byte_sex);
                    if(s.align > align)
                        align = s.align;
                    sp++;
                }
            }
            else
            {
                /* guess the smallest alignment and use that */
                align = guess_align(sg.vmaddr, MINSEGALIGN32, MAXSEGALIGN);
            }
        }
        else if(l.cmd == LC_SEGMENT_64)
        {
            struct segment_command_64* sgp = (struct segment_command_64 *)lcp;
            struct segment_command_64 sg = *sgp;
            if(swap_load_commands)
                swap_segment_command_64(&sg, host_byte_sex);
            if(mhp64->filetype == MH_OBJECT)
            {
                /* this is the minimum alignment, then take largest */
                align = MINSEGALIGN64;
                struct section_64 *sp = (struct section_64 *)((char *)sgp +
                                         sizeof(struct segment_command_64));
                for(uint32_t j = 0; j < sg.nsects; ++j)
                {
                    struct section_64 s = *sp;
                    if(swap_load_commands)
                        swap_section_64(&s, 1, host_byte_sex);
                    if(s.align > align)
                        align = s.align;
                    sp++;
                }
            }
            else
            {
                /* guess the smallest alignment and use that */
                align = guess_align(sg.vmaddr, MINSEGALIGN64, MAXSEGALIGN);
            }
        }

        if(align < cur_align)
            cur_align = align;

        lcp = (struct load_command *)((char *)lcp + l.cmdsize);
    }

    return cur_align;
}
