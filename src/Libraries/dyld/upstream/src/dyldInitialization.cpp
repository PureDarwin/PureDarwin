/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2004-2008 Apple Inc. All rights reserved.
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

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <mach/mach.h>

#include "dyld2.h"
#include "dyldSyscallInterface.h"
#include "MachOAnalyzer.h"
#include "Tracing.h"

// from libc.a
extern "C" void mach_init();
extern "C" void __guard_setup(const char* apple[]);
extern "C" void _subsystem_init(const char* apple[]);

// from dyld_debugger.cpp
extern void syncProcessInfo();

const dyld::SyscallHelpers* gSyscallHelpers = NULL;


//
//  Code to bootstrap dyld into a runnable state
//
//

namespace dyldbootstrap {


// currently dyld has no initializers, but if some come back, set this to non-zero
#define DYLD_INITIALIZER_SUPPORT  0


#if DYLD_INITIALIZER_SUPPORT

typedef void (*Initializer)(int argc, const char* argv[], const char* envp[], const char* apple[]);

extern const Initializer  inits_start  __asm("section$start$__DATA$__mod_init_func");
extern const Initializer  inits_end    __asm("section$end$__DATA$__mod_init_func");

//
// For a regular executable, the crt code calls dyld to run the executables initializers.
// For a static executable, crt directly runs the initializers.
// dyld (should be static) but is a dynamic executable and needs this hack to run its own initializers.
// We pass argc, argv, etc in case libc.a uses those arguments
//
static void runDyldInitializers(int argc, const char* argv[], const char* envp[], const char* apple[])
{
	for (const Initializer* p = &inits_start; p < &inits_end; ++p) {
		(*p)(argc, argv, envp, apple);
	}
}
#endif // DYLD_INITIALIZER_SUPPORT


//
// On disk, all pointers in dyld's DATA segment are chained together.
// They need to be fixed up to be real pointers to run.
//
#if __arm__
// Both the magic and the filetype, so a stray 0xfeedface inside __TEXT that
// happens to sit on a page boundary cannot be mistaken for the header.
static bool isDyldMachHeader(const void* p)
{
    const uint32_t* w = (const uint32_t*)p;
    return (w[0] == MH_MAGIC) && (w[3] == MH_DYLINKER);
}
#endif

#if !__LP64__
__attribute__((noinline))
static void rebaseDyldChained32(const uint8_t* mh, uintptr_t slide)
{
    struct mh32     { uint32_t magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags; };
    struct lc       { uint32_t cmd, cmdsize; };
    struct seg32    { uint32_t cmd, cmdsize; char segname[16]; uint32_t vmaddr, vmsize, fileoff, filesize; };
    struct ledata   { uint32_t cmd, cmdsize, dataoff, datasize; };
    struct chdr     { uint32_t fixups_version, starts_offset, imports_offset, symbols_offset,
                               imports_count, imports_format, symbols_format; };
    struct startsIn { uint32_t seg_count; uint32_t seg_info_offset[1]; };
    struct startsSeg { uint32_t size; uint16_t page_size, pointer_format; uint64_t segment_offset;
                       uint32_t max_valid_pointer; uint16_t page_count, page_start[1]; };

    const struct mh32* h        = (const struct mh32*)mh;
    uint32_t           fixupOff = 0;
    uint32_t           leVmaddr = 0, leFileoff = 0;
    bool               haveLE   = false;

    const uint8_t* p = mh + sizeof(struct mh32);
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        const struct lc* c = (const struct lc*)p;
        if ( c->cmd == LC_SEGMENT ) {
            const struct seg32* s = (const struct seg32*)p;
            if ( s->segname[2] == 'L' && s->segname[3] == 'I' && s->segname[4] == 'N' ) {
                leVmaddr = s->vmaddr;
                leFileoff = s->fileoff;
                haveLE = true;
            }
        }
        else if ( c->cmd == LC_DYLD_CHAINED_FIXUPS ) {
            fixupOff = ((const struct ledata*)p)->dataoff;
        }
        p += c->cmdsize;
    }
    if ( !haveLE || (fixupOff == 0) )
        return;

    // dyld links at base zero, so an unslid vmaddr is just an offset from mh.
    const uint8_t*        fixups = mh + leVmaddr + (fixupOff - leFileoff);
    const struct chdr*    fh     = (const struct chdr*)fixups;
    if ( fh->fixups_version != 0 )
        return;
    const struct startsIn* si = (const struct startsIn*)(fixups + fh->starts_offset);

    for (uint32_t s = 0; s < si->seg_count; ++s) {
        if ( si->seg_info_offset[s] == 0 )
            continue;
        const struct startsSeg* ss = (const struct startsSeg*)((const uint8_t*)si + si->seg_info_offset[s]);
        if ( ss->pointer_format != DYLD_CHAINED_PTR_32 )
            continue;
        const uint8_t* segBase = mh + (uintptr_t)ss->segment_offset;

        for (uint32_t pg = 0; pg < ss->page_count; ++pg) {
            uint16_t start = ss->page_start[pg];
            if ( start == DYLD_CHAINED_PTR_START_NONE )
                continue;

            // A page whose chains do not all descend from one head lists its
            // extra heads in an overflow array, terminated by START_LAST.
            const uint16_t* multi = NULL;
            if ( start & DYLD_CHAINED_PTR_START_MULTI ) {
                multi = &ss->page_start[start & ~DYLD_CHAINED_PTR_START_MULTI];
                start = *multi & ~DYLD_CHAINED_PTR_START_LAST;
            }

            for (;;) {
                uint32_t* loc = (uint32_t*)(segBase + (pg * ss->page_size) + start);
                for (;;) {
                    uint32_t v    = *loc;
                    uint32_t next = (v >> 26) & 0x1F;
                    if ( (v & 0x80000000) == 0 ) {
                        uint32_t target = v & 0x03FFFFFF;
                        if ( (ss->max_valid_pointer != 0) && (target > ss->max_valid_pointer) ) {
                            // Not a pointer: a plain value large enough that the
                            // linker had to bias it to fit the target field.
                            *loc = target - ((0x04000000 + ss->max_valid_pointer) / 2);
                        }
                        else {
                            *loc = target + (uint32_t)slide;
                        }
                    }
                    // A bind needs an import table dyld has none of; skip it but
                    // keep following the chain it sits in.
                    if ( next == 0 )
                        break;
                    loc += next;
                }
                if ( (multi == NULL) || (*multi & DYLD_CHAINED_PTR_START_LAST) )
                    break;
                ++multi;
                start = *multi & ~DYLD_CHAINED_PTR_START_LAST;
            }
        }
    }
}
#endif // !__LP64__


static void rebaseDyld(const dyld3::MachOLoaded* dyldMH)
{
    // walk all fixups chains and rebase dyld
    const dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)dyldMH;
    uintptr_t slide = (long)ma; // all fixup chain based images have a base address of zero, so slide == load address
#if __LP64__
    assert(ma->hasChainedFixups());
    __block Diagnostics diag;
    ma->withChainStarts(diag, 0, ^(const dyld_chained_starts_in_image* starts) {
        ma->fixupAllChainedFixups(diag, starts, slide, dyld3::Array<const void*>(), nullptr);
    });
    diag.assertNoError();
#else
    rebaseDyldChained32((const uint8_t*)ma, slide);
#endif

    // now that rebasing done, initialize mach/syscall layer
    mach_init();

    // <rdar://47805386> mark __DATA_CONST segment in dyld as read-only (once fixups are done)
    ma->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& info, bool& stop) {
        if ( info.readOnlyData ) {
            ::mprotect(((uint8_t*)(dyldMH))+info.vmAddr, (size_t)info.vmSize, VM_PROT_READ);
        }
    });
}



//
//  This is code to bootstrap dyld.  This work in normally done for a program by dyld and crt.
//  In dyld we have to do this manually.
//
uintptr_t start(const dyld3::MachOLoaded* appsMachHeader, int argc, const char* argv[],
				const dyld3::MachOLoaded* dyldsMachHeader, uintptr_t* startGlue)
{
    // Emit kdebug tracepoint to indicate dyld bootstrap has started <rdar://46878536>
    dyld3::kdebug_trace_dyld_marker(DBG_DYLD_TIMING_BOOTSTRAP_START, 0, 0, 0, 0);

#if __arm__
    // dyldStartup.s computes dyld's mach header as "__dyld_start - 0x1000",
    // which only holds if __dyld_start is the first thing in __TEXT. It is not
    // here (it links at +0xdf39c), so the pointer lands somewhere in the middle
    // of __TEXT - readable, but not a header, which left the fixup walk below
    // reading garbage load commands. Recover the real header by scanning down
    // page by page; it is at __TEXT's start and so is page aligned. No global
    // is touched, because this runs before the rebase.
    if ( !isDyldMachHeader(dyldsMachHeader) ) {
        uintptr_t p = ((uintptr_t)dyldsMachHeader) & ~(uintptr_t)0xFFF;
        for (unsigned i = 0; i < 0x4000; ++i, p -= 0x1000) {
            if ( isDyldMachHeader((const void*)p) ) {
                dyldsMachHeader = (const dyld3::MachOLoaded*)p;
                break;
            }
        }
    }
#endif

	// if kernel had to slide dyld, we need to fix up load sensitive locations
	// we have to do this before using any global variables
    rebaseDyld(dyldsMachHeader);

	// kernel sets up env pointer to be just past end of agv array
	const char** envp = &argv[argc+1];
	
	// kernel sets up apple pointer to be just past end of envp array
	const char** apple = envp;
	while(*apple != NULL) { ++apple; }
	++apple;

	// set up random value for stack canary
	__guard_setup(apple);

#if DYLD_INITIALIZER_SUPPORT
	// run all C++ initializers inside dyld
	runDyldInitializers(argc, argv, envp, apple);
#endif

	_subsystem_init(apple);

	// now that we are done bootstrapping dyld, call dyld's main
	uintptr_t appsSlide = appsMachHeader->getSlide();
	return dyld::_main((macho_header*)appsMachHeader, appsSlide, argc, argv, envp, apple, startGlue);
}


#if TARGET_OS_SIMULATOR

extern "C" uintptr_t start_sim(int argc, const char* argv[], const char* envp[], const char* apple[],
							const dyld3::MachOLoaded* mainExecutableMH, const dyld3::MachOLoaded* dyldMH, uintptr_t dyldSlide,
							const dyld::SyscallHelpers*, uintptr_t* startGlue);
					

uintptr_t start_sim(int argc, const char* argv[], const char* envp[], const char* apple[],
					const dyld3::MachOLoaded* mainExecutableMH, const dyld3::MachOLoaded* dyldSimMH, uintptr_t dyldSlide,
					const dyld::SyscallHelpers* sc, uintptr_t* startGlue)
{
    // save table of syscall pointers
    gSyscallHelpers = sc;

	// dyld_sim uses chained rebases, so it always need to be fixed up
    rebaseDyld(dyldSimMH);

	// set up random value for stack canary
	__guard_setup(apple);

	// setup gProcessInfo to point to host dyld's struct
	dyld::gProcessInfo = (struct dyld_all_image_infos*)(sc->getProcessInfo());
	syncProcessInfo();

	// now that we are done bootstrapping dyld, call dyld's main
    uintptr_t appsSlide = mainExecutableMH->getSlide();
	return dyld::_main((macho_header*)mainExecutableMH, appsSlide, argc, argv, envp, apple, startGlue);
}
#endif


} // end of namespace




