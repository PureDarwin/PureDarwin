
#include <mach-o/reloc.h>
#include <mach-o/loader.h>

typedef int (*FixupsLogFunc)(const char*, ...);
static const int LogFixupsClassic = 0;

// We may not have strcmp, so make our own
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int areEqualClassic(const char* a, const char* b) {
	while (*a && *b) {
		if (*a != *b)
			return 0;
        ++a;
        ++b;
	}
	return *a == *b;
}

// Temporary until we have <rdar://problem/57025372>
#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
int slideClassic(const struct mach_header* mh, FixupsLogFunc logFunc) {

#if !__x86_64__
    return 0;
#endif

	// First find the slide and dysymtab fixups load command
    uint64_t textVMAddr                             = 0;
    uint64_t firstWritableVMAddr                    = ~0ULL;
	const struct dysymtab_command* dynSymbolTable   = 0;
	uint64_t linkeditVMAddr 	                    = 0;
	uint64_t linkeditFileOffset                     = 0;

    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: mh %p\n", mh);
    }

    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: parsing load commands\n");
    }

	const struct load_command* startCmds = 0;
    if ( mh->magic == MH_MAGIC_64 )
        startCmds = (struct load_command*)((char *)mh + sizeof(struct mach_header_64));
    else if ( mh->magic == MH_MAGIC )
        startCmds = (struct load_command*)((char *)mh + sizeof(struct mach_header));
    else {
        const uint32_t* h = (uint32_t*)mh;
        //diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return 1;  // not a mach-o file
    }
    const struct load_command* const cmdsEnd = (struct load_command*)((char*)startCmds + mh->sizeofcmds);
    const struct load_command* cmd = startCmds;
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        if (LogFixupsClassic) {
            logFunc("[LOG] kernel-classic-relocs: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
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
        if ( cmd->cmd == LC_DYSYMTAB ) {
        	dynSymbolTable = (const struct dysymtab_command*)cmd;
        } else if ( cmd->cmd == LC_SEGMENT_64 ) {
        	const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            if ( areEqualClassic(seg->segname, "__TEXT") ) {
                textVMAddr = seg->vmaddr;
            } else if ( areEqualClassic(seg->segname, "__LINKEDIT") ) {
        		linkeditVMAddr = seg->vmaddr;
        		linkeditFileOffset = seg->fileoff;
        	}
            if ( (seg->initprot & VM_PROT_WRITE) && (firstWritableVMAddr == ~0ULL) ) {
                firstWritableVMAddr = seg->vmaddr;
                if (LogFixupsClassic) {
                    logFunc("[LOG] kernel-classic-relocs: first writable segment %s = 0x%llx\n", seg->segname, seg->vmaddr);
                }
            }
        }
        cmd = nextCmd;
    }

    uintptr_t slide = (uintptr_t)mh - textVMAddr;

    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: slide 0x%llx\n", slide);
    }

    if ( dynSymbolTable == 0 )
    	return 0;

    if ( dynSymbolTable->nlocrel == 0 )
        return 0;

    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: found dynamic symbol table %p\n", dynSymbolTable);
        logFunc("[LOG] kernel-classic-relocs: found linkeditVMAddr %p\n", (void*)linkeditVMAddr);
        logFunc("[LOG] kernel-classic-relocs: found linkeditFileOffset %p\n", (void*)linkeditFileOffset);
    }

    // Now we have the dynamic symbol table, walk it to apply all the rebases
    uint32_t offsetInLinkedit   = dynSymbolTable->locreloff - linkeditFileOffset;
    uintptr_t linkeditStartAddr = linkeditVMAddr + slide;
    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: offsetInLinkedit 0x%x\n", offsetInLinkedit);
        logFunc("[LOG] kernel-classic-relocs: linkeditStartAddr %p\n", (void*)linkeditStartAddr);
    }

    const uint64_t                  relocsStartAddress = firstWritableVMAddr;
    const struct relocation_info* const    relocsStart = (const struct relocation_info*)(linkeditStartAddr + offsetInLinkedit);
    const struct relocation_info* const    relocsEnd   = &relocsStart[dynSymbolTable->nlocrel];
    for (const struct relocation_info* reloc = relocsStart; reloc < relocsEnd; ++reloc) {
        if ( reloc->r_length == 2 ) {
            uint32_t* fixupLoc = (uint32_t*)(relocsStartAddress + reloc->r_address + slide);
            uint32_t slidValue = *fixupLoc + slide;
            if (LogFixupsClassic) {
                logFunc("[LOG] kernel-classic-relocs: fixupLoc %p = 0x%x + 0x%x + 0x%x\n", fixupLoc, relocsStartAddress, reloc->r_address, slide);
                logFunc("[LOG] kernel-classic-relocs: slidValue *%p = 0x%x\n", fixupLoc, slidValue);
            }
            *fixupLoc = slidValue;
            continue;
        }
        if ( reloc->r_length == 3 ) {
            uint64_t* fixupLoc = (uint64_t*)(relocsStartAddress + reloc->r_address + slide);
            uint64_t slidValue = *fixupLoc + slide;
            if (LogFixupsClassic) {
                logFunc("[LOG] kernel-classic-relocs: fixupLoc %p = 0x%x + 0x%x + 0x%x\n", fixupLoc, relocsStartAddress, reloc->r_address, slide);
                logFunc("[LOG] kernel-classic-relocs: slidValue *%p = 0x%llx\n", fixupLoc, slidValue);
            }
            *fixupLoc = slidValue;
            continue;
        }
        logFunc("[LOG] kernel-fixups: unknown reloc size\n", reloc->r_length);
        return 1;
    }

    if (LogFixupsClassic) {
        logFunc("[LOG] kernel-classic-relocs: Done\n");
    }

    return 0;
}


