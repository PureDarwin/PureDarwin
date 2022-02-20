

#include <mach-o/fixup-chains.h>
#include <mach-o/loader.h>

#include "kernel-fixups.h"

#ifndef LC_FILESET_ENTRY
#define LC_FILESET_ENTRY      (0x35 | LC_REQ_DYLD) /* used with fileset_entry_command */
struct fileset_entry_command {
    uint32_t        cmd;        /* LC_FILESET_ENTRY */
    uint32_t        cmdsize;    /* includes id string */
    uint64_t        vmaddr;     /* memory address of the dylib */
    uint64_t        fileoff;    /* file offset of the dylib */
    union lc_str    entry_id;   /* contained entry id */
    uint32_t        reserved;   /* entry_id is 32-bits long, so this is the reserved padding */
};
#endif

typedef int (*ModInitLogFunc)(const char*, ...);
static const int LogModInits = 0;

struct TestRunnerFunctions;
typedef int (*InitializerFunc)(const TestRunnerFunctions*);

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int getSlide(const struct mach_header* mh, ModInitLogFunc logFunc,
					uintptr_t* slide) {
    uint64_t textVMAddr     = 0;

    if (LogFixups) {
        logFunc("[LOG] kernel-slide: mh %p\n", mh);
    }

    if (LogFixups) {
        logFunc("[LOG] kernel-slide: parsing load commands\n");
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
        if (LogFixups) {
            logFunc("[LOG] kernel-slide: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
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
        if ( cmd->cmd == LC_SEGMENT_64 ) {
        	const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            if ( areEqual(seg->segname, "__TEXT") ) {
                textVMAddr = seg->vmaddr;
            }
        }
        cmd = nextCmd;
    }

    *slide = (uintptr_t)mh - textVMAddr;
    return 0;
}

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int runAllModInitFunctions(const struct mach_header* mh, ModInitLogFunc logFunc,
                                  const TestRunnerFunctions* funcs)  {
	uintptr_t slide = 0;
	if ( getSlide(mh, logFunc, &slide) != 0 ) {
		return 1;
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
        if (LogModInits) {
            logFunc("[LOG] kernel-mod-inits: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
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
        if ( cmd->cmd == LC_SEGMENT_64 ) {
        	const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            const struct section_64* const sectionsStart = (struct section_64*)((char*)seg + sizeof(struct segment_command_64));
			const struct section_64* const sectionsEnd = &sectionsStart[seg->nsects];
			for (const struct section_64* sect = sectionsStart; sect < sectionsEnd; ++sect) {
				const uint8_t type = sect->flags & SECTION_TYPE;
				if ( LogModInits ) {
					logFunc("[LOG] kernel-mod-inits: section: %s %s\n", sect->segname, sect->sectname);
				}
				if ( type == S_MOD_INIT_FUNC_POINTERS ) {
					InitializerFunc* inits = (InitializerFunc*)(sect->addr + slide);
					const uintptr_t count = sect->size / sizeof(uintptr_t);
					// Ensure __mod_init_func section is within segment
					if ( (sect->addr < seg->vmaddr) || (sect->addr+sect->size > seg->vmaddr+seg->vmsize) || (sect->addr+sect->size < sect->addr) ) {
						logFunc("[LOG] kernel-mod-inits: __mod_init_funcs section has malformed address range\n");
						return 1;
					}
					for (uintptr_t j = 0; j < count; ++j) {
						InitializerFunc func = inits[j];
#if __has_feature(ptrauth_calls)
						func = (InitializerFunc)__builtin_ptrauth_sign_unauthenticated((void*)func, ptrauth_key_asia, 0);
#endif
						if ( LogModInits ) {
							logFunc("[LOG] kernel-mod-inits: running mod init %p\n", (const void*)func);
						}
						int initResult = func(funcs);
						if ( initResult != 0 ) {
							logFunc("[LOG] kernel-mod-inits: mod init %p, result = %d\n", (const void*)func, initResult);
							return 1;
						}
					}
				}
			}
        }
        cmd = nextCmd;
    }

    return 0;
}

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int runAllModInitFunctionsForAppCache(const struct mach_header* appCacheMH, ModInitLogFunc logFunc,
                                             const TestRunnerFunctions* funcs) {
	uintptr_t slide = 0;
	if ( getSlide(appCacheMH, logFunc, &slide) != 0 ) {
		return 1;
	}

    if (LogFixups) {
        logFunc("[LOG] mod-init: appCacheMH %p\n", appCacheMH);
    }

    if (LogFixups) {
        logFunc("[LOG] mod-init: parsing load commands\n");
    }

	const struct load_command* startCmds = 0;
    if ( appCacheMH->magic == MH_MAGIC_64 )
        startCmds = (struct load_command*)((char *)appCacheMH + sizeof(struct mach_header_64));
    else if ( appCacheMH->magic == MH_MAGIC )
        startCmds = (struct load_command*)((char *)appCacheMH + sizeof(struct mach_header));
    else {
        const uint32_t* h = (uint32_t*)appCacheMH;
        //diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return 1;  // not a mach-o file
    }
    const struct load_command* const cmdsEnd = (struct load_command*)((char*)startCmds + appCacheMH->sizeofcmds);
    const struct load_command* cmd = startCmds;
    for (uint32_t i = 0; i < appCacheMH->ncmds; ++i) {
        if (LogFixups) {
            logFunc("[LOG] mod-init: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
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
        if ( cmd->cmd == LC_FILESET_ENTRY ) {
            const struct fileset_entry_command* app_cache_cmd = (const struct fileset_entry_command*)cmd;
            const char* name = (char*)app_cache_cmd + app_cache_cmd->entry_id.offset;
            const struct mach_header* mh = (const struct mach_header*)(app_cache_cmd->vmaddr + slide);
            if ( LogModInits ) {
                logFunc("[LOG] mod-init: Running mod inits for %p: %s\n", mh, name);
            }
            int result = runAllModInitFunctions(mh, logFunc, funcs);
        	if (result != 0) {
        		return 1;
        	}
        }
        cmd = nextCmd;
    }

    return 0;
}

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
static int slideKextsInsideKernelCollection(const struct mach_header* appCacheMH, const void* basePointers[4],
                                            FixupsLogFunc logFunc, const TestRunnerFunctions* funcs) {
    uintptr_t slideAmount = 0;
    if ( getSlide(appCacheMH, logFunc, &slideAmount) != 0 ) {
        return 1;
    }

    if (LogFixups) {
        logFunc("[LOG] slide-pageable: appCacheMH %p\n", appCacheMH);
    }

    if (LogFixups) {
        logFunc("[LOG] slide-pageable: parsing load commands\n");
    }

	const struct load_command* startCmds = 0;
    if ( appCacheMH->magic == MH_MAGIC_64 )
        startCmds = (struct load_command*)((char *)appCacheMH + sizeof(struct mach_header_64));
    else if ( appCacheMH->magic == MH_MAGIC )
        startCmds = (struct load_command*)((char *)appCacheMH + sizeof(struct mach_header));
    else {
        const uint32_t* h = (uint32_t*)appCacheMH;
        //diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return 1;  // not a mach-o file
    }
    const struct load_command* const cmdsEnd = (struct load_command*)((char*)startCmds + appCacheMH->sizeofcmds);
    const struct load_command* cmd = startCmds;
    for (uint32_t i = 0; i < appCacheMH->ncmds; ++i) {
        if (LogFixups) {
            logFunc("[LOG] slide-pageable: parsing load command %d with cmd=0x%x\n", i, cmd->cmd);
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
        if ( cmd->cmd == LC_FILESET_ENTRY ) {
            const struct fileset_entry_command* app_cache_cmd = (const struct fileset_entry_command*)cmd;
            const char* name = (char*)app_cache_cmd + app_cache_cmd->entry_id.offset;
            const struct mach_header* mh = (const struct mach_header*)(app_cache_cmd->vmaddr + slideAmount);
            if ( LogModInits ) {
                logFunc("[LOG] slide-pageable: Sliding %p: %s\n", mh, name);
            }
            int slideReturnCode = slide(mh, basePointers, logFunc);
            if ( slideReturnCode != 0 ) {
                FAIL("mh slide = %d\n", slideReturnCode);
                return 1;
            }
        }
        cmd = nextCmd;
    }

    return 0;
}
