
#ifndef KERNEL_FIXUPS_H
#define KERNEL_FIXUPS_H

#include <mach-o/fixup-chains.h>
#include <mach-o/loader.h>

typedef int (*FixupsLogFunc)(const char*, ...);
static const int LogFixups = 0;

// We may not have strcmp, so make our own
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int areEqual(const char* a, const char* b) {
	while (*a && *b) {
		if (*a != *b)
			return 0;
        ++a;
        ++b;
	}
	return *a == *b;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-declarations"
union ChainedFixupPointerOnDisk
{
    uint64_t											raw64;
    struct dyld_chained_ptr_64_kernel_cache_rebase		fixup64;
};
#pragma clang diagnostic pop

// Temporary until we have <rdar://problem/57025372>
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static uint64_t signPointer(struct dyld_chained_ptr_64_kernel_cache_rebase pointer,
							void* loc,
							uint64_t target)
{
#if __has_feature(ptrauth_calls)
    uint64_t discriminator = pointer.diversity;
    if ( pointer.addrDiv ) {
        if ( discriminator != 0 ) {
            discriminator = __builtin_ptrauth_blend_discriminator(loc, discriminator);
        } else {
            discriminator = (uint64_t)(uintptr_t)loc;
        }
    }
    switch ( pointer.key ) {
        case 0: // IA
            return (uint64_t)__builtin_ptrauth_sign_unauthenticated((void*)target, 0, discriminator);
        case 1: // IB
            return (uint64_t)__builtin_ptrauth_sign_unauthenticated((void*)target, 1, discriminator);
        case 2: // DA
            return (uint64_t)__builtin_ptrauth_sign_unauthenticated((void*)target, 2, discriminator);
        case 3: // DB
            return (uint64_t)__builtin_ptrauth_sign_unauthenticated((void*)target, 3, discriminator);
    }
#endif
    return target;
}

// Temporary until we have <rdar://problem/57025372>
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
void fixupValue(union ChainedFixupPointerOnDisk* fixupLoc,
				const struct dyld_chained_starts_in_segment* segInfo,
				uintptr_t slide,
				const void* basePointers[4],
				int* stop,
				FixupsLogFunc logFunc) {
	if (LogFixups) {
        logFunc("[LOG] kernel-fixups: fixupValue %p\n", fixupLoc);
    }
    switch (segInfo->pointer_format) {
#if __LP64__
        case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
        case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE: {
        	const void* baseAddress = basePointers[fixupLoc->fixup64.cacheLevel];
        	if ( baseAddress == 0 ) {
        		logFunc("Invalid cache level: %d\n", fixupLoc->fixup64.cacheLevel);
        		*stop = 1;
        		return;
        	}
        	uintptr_t slidValue = (uintptr_t)baseAddress + fixupLoc->fixup64.target;
        	if (LogFixups) {
		        logFunc("[LOG] kernel-fixups: slidValue %p = %p + %p\n", (void*)slidValue, (void*)baseAddress, (void*)fixupLoc->fixup64.target);
		    }
#if __has_feature(ptrauth_calls)
        	if ( fixupLoc->fixup64.isAuth ) {
        		slidValue = signPointer(fixupLoc->fixup64, fixupLoc, slidValue);
        	}
#else
        	if ( fixupLoc->fixup64.isAuth ) {
        		logFunc("Unexpected authenticated fixup\n");
        		*stop = 1;
        		return;
        	}
#endif // __has_feature(ptrauth_calls)
            fixupLoc->raw64 = slidValue;
            break;
        }
#endif // __LP64__
        default:
            logFunc("unsupported pointer chain format: 0x%04X", segInfo->pointer_format);
            *stop = 1;
            break;
    }
}

// Temporary until we have <rdar://problem/57025372>
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
int walkChain(const struct mach_header* mh,
			  const struct dyld_chained_starts_in_segment* segInfo,
			  uint32_t pageIndex,
			  uint16_t offsetInPage,
			  uintptr_t slide,
			  const void* basePointers[4],
			  FixupsLogFunc logFunc)
{
	if (LogFixups) {
        logFunc("[LOG] kernel-fixups: walkChain page[%d]\n", pageIndex);
    }
    int                        stop = 0;
    uint8_t*                   pageContentStart = (uint8_t*)mh + segInfo->segment_offset + (pageIndex * segInfo->page_size);
    union ChainedFixupPointerOnDisk* chain = (union ChainedFixupPointerOnDisk*)(pageContentStart+offsetInPage);
    int                       chainEnd = 0;
    while (!stop && !chainEnd) {
        // copy chain content, in case handler modifies location to final value
        union ChainedFixupPointerOnDisk chainContent = *chain;
        fixupValue(chain, segInfo, slide, basePointers, &stop, logFunc);
        if ( !stop ) {
            switch (segInfo->pointer_format) {
#if __LP64__
                case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
                    if ( chainContent.fixup64.next == 0 )
                        chainEnd = 1;
                    else
                        chain = (union ChainedFixupPointerOnDisk*)((uint8_t*)chain + chainContent.fixup64.next*4);
                    break;
                case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE:
                    if ( chainContent.fixup64.next == 0 )
                        chainEnd = 1;
                    else
                        chain = (union ChainedFixupPointerOnDisk*)((uint8_t*)chain + chainContent.fixup64.next);
                    break;
#endif // __LP64__
                default:
                    logFunc("unknown pointer format 0x%04X", segInfo->pointer_format);
                    stop = 1;
            }
        }
    }
    return stop;
}

// Temporary until we have <rdar://problem/57025372>
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
int slide(const struct mach_header* mh, const void* basePointers[4], FixupsLogFunc logFunc) {
	// First find the slide and chained fixups load command
    uint64_t textVMAddr     = 0;
	const struct linkedit_data_command* chainedFixups = 0;
	uint64_t linkeditVMAddr 	= 0;
	uint64_t linkeditFileOffset = 0;

    if (LogFixups) {
        logFunc("[LOG] kernel-fixups: mh %p\n", mh);
    }

    if (LogFixups) {
        logFunc("[LOG] kernel-fixups: parsing load commands\n");
    }

	const struct load_command* startCmds = 0;
    if ( mh->magic == MH_MAGIC_64 )
        startCmds = (struct load_command*)((char *)mh + sizeof(struct mach_header_64));
    else if ( mh->magic == MH_MAGIC )
        startCmds = (struct load_command*)((char *)mh + sizeof(struct mach_header));
    else {
        const uint32_t* h = (uint32_t*)mh;
        logFunc("[LOG] kernel-fixups: file does not start with MH_MAGIC[_64] 0x%08X 0x%08X\n", h[0], h [1]);
        //diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return 1;  // not a mach-o file
    }
    const struct load_command* const cmdsEnd = (struct load_command*)((char*)startCmds + mh->sizeofcmds);
    const struct load_command* cmd = startCmds;
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        if (LogFixups) {
            logFunc("[LOG] kernel-fixups: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
        }
        const struct load_command* nextCmd = (struct load_command*)((char *)cmd + cmd->cmdsize);
        if ( cmd->cmdsize < 8 ) {
            //diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) too small", i, this->ncmds, cmd, this, cmd->cmdsize);
            return 1;
        }
        if ( (nextCmd > cmdsEnd) || (nextCmd < startCmds) ) {
            //diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) is too large, load commands end at %p", i, this->ncmds, cmd, this, cmd->cmdsize, cmdsEnd);
            return 1;
        }
        if ( cmd->cmd == LC_DYLD_CHAINED_FIXUPS ) {
        	chainedFixups = (const struct linkedit_data_command*)cmd;
        } else if ( cmd->cmd == LC_SEGMENT_64 ) {
        	const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            if ( areEqual(seg->segname, "__TEXT") ) {
                textVMAddr = seg->vmaddr;
            } else if ( areEqual(seg->segname, "__LINKEDIT") ) {
        		linkeditVMAddr = seg->vmaddr;
        		linkeditFileOffset = seg->fileoff;
        	}
        }
        cmd = nextCmd;
    }

    uintptr_t slide = (uintptr_t)mh - textVMAddr;

    if (LogFixups) {
        logFunc("[LOG] kernel-fixups: slide 0x%llx\n", slide);
    }

    if ( chainedFixups == 0 )
    	return 0;

    if (LogFixups) {
        logFunc("[LOG] kernel-fixups: found chained fixups %p\n", chainedFixups);
        logFunc("[LOG] kernel-fixups: found linkeditVMAddr %p\n", (void*)linkeditVMAddr);
        logFunc("[LOG] kernel-fixups: found linkeditFileOffset %p\n", (void*)linkeditFileOffset);
    }

    // Now we have the chained fixups, walk it to apply all the rebases
    uint32_t offsetInLinkedit   = chainedFixups->dataoff - linkeditFileOffset;
    uintptr_t linkeditStartAddr = linkeditVMAddr + slide;
    if (LogFixups) {
        logFunc("[LOG] kernel-fixups: offsetInLinkedit 0x%x\n", offsetInLinkedit);
        logFunc("[LOG] kernel-fixups: linkeditStartAddr %p\n", (void*)linkeditStartAddr);
    }

    const struct dyld_chained_fixups_header* fixupsHeader = (const struct dyld_chained_fixups_header*)(linkeditStartAddr + offsetInLinkedit);
    const struct dyld_chained_starts_in_image* fixupStarts = (const struct dyld_chained_starts_in_image*)((uint8_t*)fixupsHeader + fixupsHeader->starts_offset);
	if (LogFixups) {
        logFunc("[LOG] kernel-fixups: fixupsHeader %p\n", fixupsHeader);
        logFunc("[LOG] kernel-fixups: fixupStarts %p\n", fixupStarts);
    }

    int stopped = 0;
    for (uint32_t segIndex=0; segIndex < fixupStarts->seg_count && !stopped; ++segIndex) {
    	if (LogFixups) {
	        logFunc("[LOG] kernel-fixups: segment %d\n", segIndex);
	    }
        if ( fixupStarts->seg_info_offset[segIndex] == 0 )
            continue;
        const struct dyld_chained_starts_in_segment* segInfo = (const struct dyld_chained_starts_in_segment*)((uint8_t*)fixupStarts + fixupStarts->seg_info_offset[segIndex]);
        for (uint32_t pageIndex=0; pageIndex < segInfo->page_count && !stopped; ++pageIndex) {
            uint16_t offsetInPage = segInfo->page_start[pageIndex];
            if ( offsetInPage == DYLD_CHAINED_PTR_START_NONE )
                continue;
            if ( offsetInPage & DYLD_CHAINED_PTR_START_MULTI ) {
            	// FIXME: Implement this
                return 1;
            }
            else {
                // one chain per page
                if ( walkChain(mh, segInfo, pageIndex, offsetInPage, slide, basePointers, logFunc) )
                    stopped = 1;
            }
        }
    }

    return stopped;
}

#endif /* KERNEL_FIXUPS_H */
