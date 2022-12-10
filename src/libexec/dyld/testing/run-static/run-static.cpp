
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <mach-o/loader.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include <TargetConditionals.h>

#include "ClosureFileSystemPhysical.h"
#include "MachOAnalyzer.h"
#include "MachOFile.h"

#include "../testing/test-cases/kernel-test-runner.h"

const bool isLoggingEnabled = false;

int entryFunc(const TestRunnerFunctions* funcs);
typedef __typeof(&entryFunc) EntryFuncTy;

TestRunnerFunctions testFuncs = {
    .version        = 1,
    .mhs            = { nullptr, nullptr, nullptr, nullptr },
    .basePointers   = { nullptr, nullptr, nullptr, nullptr },
    .printf         = &::printf,
    .exit           = &::exit,
    .testPass       = &_PASS,
    .testFail       = &_FAIL,
    .testLog        = &_LOG,
    .testTimeout    = &_TIMEOUT,
};

struct LoadedMachO {
    const dyld3::MachOAnalyzer* ma              = nullptr;
    // base pointer is the same as 'ma' when the binary has __TEXT first,
    // but will point at where we mapped __DATA if building a reverse auxKC.
    const void*                 basePointer     = nullptr;
};

LoadedMachO loadPath(const char* binaryPath) {
    __block Diagnostics diag;
    dyld3::closure::FileSystemPhysical fileSystem;
    dyld3::closure::LoadedFileInfo info;
    char realerPath[MAXPATHLEN];
    __block bool printedError = false;
    if (!fileSystem.loadFile(binaryPath, info, realerPath, ^(const char* format, ...) {
        fprintf(stderr, "run-static: ");
        va_list list;
        va_start(list, format);
        vfprintf(stderr, format, list);
        va_end(list);
        printedError = true;
    })) {
        if (!printedError )
            fprintf(stderr, "run-static: %s: file not found\n", binaryPath);
        exit(1);
    }

    const char* currentArchName = dyld3::MachOFile::currentArchName();
    const dyld3::GradedArchs& currentArchs = dyld3::GradedArchs::forName(currentArchName);
    __block const dyld3::MachOFile* mf = nullptr;
    __block uint64_t sliceOffset = 0;
    if ( dyld3::FatFile::isFatFile(info.fileContent) ) {
        const dyld3::FatFile* ff = (dyld3::FatFile*)info.fileContent;
        ff->forEachSlice(diag, info.fileContentLen, ^(uint32_t sliceCpuType, uint32_t sliceCpuSubType,
                                                      const void* sliceStart, uint64_t sliceSize, bool& stop) {
            const dyld3::MachOFile* sliceMF = (dyld3::MachOFile*)sliceStart;
            if ( currentArchs.grade(sliceMF->cputype, sliceMF->cpusubtype, false) != 0 ) {
                mf = sliceMF;
                sliceOffset = (uint64_t)mf - (uint64_t)ff;
                stop = true;
                return;
            }
        });

        if ( diag.hasError() ) {
            fprintf(stderr, "Error: %s\n", diag.errorMessage());
            return { nullptr, nullptr };
        }

        if ( mf == nullptr ) {
            fprintf(stderr, "Could not use binary '%s' because it does not contain a slice compatible with host '%s'\n",
                    binaryPath, currentArchName);
            return { nullptr, nullptr };
        }
    } else {
        mf = (dyld3::MachOFile*)info.fileContent;
        if ( !mf->isMachO(diag, info.sliceLen) ) {
            fprintf(stderr, "Could not use binary '%s' because '%s'\n", binaryPath, diag.errorMessage());
            return { nullptr, nullptr };
        }

        if ( currentArchs.grade(mf->cputype, mf->cpusubtype, false) == 0 ) {
            fprintf(stderr, "Could not use binary '%s' because 'incompatible arch'\n", binaryPath);
            return { nullptr, nullptr };
        }
    }

    if ( !mf->isFileSet() ) {
        fprintf(stderr, "Could not use binary '%s' because 'it is not a static executable'\n", binaryPath);
        return { nullptr, nullptr };
    }

    uint64_t mappedSize = ((dyld3::MachOAnalyzer*)mf)->mappedSize();
    vm_address_t mappedAddr;
    if ( ::vm_allocate(mach_task_self(), &mappedAddr, (size_t)mappedSize, VM_FLAGS_ANYWHERE) != 0 ) {
        fprintf(stderr, "Could not use binary '%s' because 'vm allocation failure'\n", binaryPath);
        return { nullptr, nullptr };
    }

    int fd = open(binaryPath, O_RDONLY);
    if ( fd == 0 ) {
        fprintf(stderr, "Could not open binary '%s' because '%s'\n", binaryPath, strerror(errno));
        return { nullptr, nullptr };
    }

    __block uint64_t baseAddress = ~0ULL;
    __block uint64_t textSegVMAddr = ~0ULL;
    mf->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
        baseAddress = std::min(baseAddress, info.vmAddr);
        if ( strcmp(info.segName, "__TEXT") == 0 ) {
            textSegVMAddr = info.vmAddr;
        }
    });

    uint64_t loadAddress = (uint64_t)mappedAddr;
    if ( isLoggingEnabled ) {
        fprintf(stderr, "Mapping binary built at 0x%llx to 0x%llx\n", baseAddress, loadAddress);
    }
    mf->forEachSegment(^(const dyld3::MachOFile::SegmentInfo &info, bool &stop) {
        uint64_t requestedLoadAddress = info.vmAddr - baseAddress + loadAddress;
        if ( isLoggingEnabled )
            fprintf(stderr, "Mapping %p: %s with perms %d\n", (void*)requestedLoadAddress, info.segName, info.protections);
        if ( info.vmSize == 0 )
            return;
        size_t readBytes = pread(fd, (void*)requestedLoadAddress, (uintptr_t)info.fileSize, sliceOffset + info.fileOffset);
        if ( readBytes != info.fileSize ) {
            fprintf(stderr, "Didn't read enough bytes\n");
            exit(1);
        }
        // __DATA_CONST is read-only when we actually run live, but this test runner fixes up __DATA_CONST after this vm_protect
        // For now just don't make __DATA_CONST read only
        uint32_t protections = info.protections;
        if ( !strcmp(info.segName, "__DATA_CONST") )
            protections = VM_PROT_READ | VM_PROT_WRITE;
        const bool setCurrentPermissions = false;
        kern_return_t r = vm_protect(mach_task_self(), (vm_address_t)requestedLoadAddress, (uintptr_t)info.vmSize, setCurrentPermissions, protections);
        if ( r != KERN_SUCCESS ) {
            diag.error("vm_protect didn't work because %d", r);
            stop = true;
            return;
        }
    });

    if ( diag.hasError() ) {
        fprintf(stderr, "Error: %s\n", diag.errorMessage());
        return { nullptr, nullptr };
    }

    if ( textSegVMAddr != baseAddress ) {
        // __DATA is first.  ma should still point to __TEXT
        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)(mappedAddr + textSegVMAddr - baseAddress);
        if ( !ma->validMachOForArchAndPlatform(diag, (size_t)mappedSize, binaryPath, currentArchs, dyld3::Platform::unknown, false) ) {
            fprintf(stderr, "Error: %s\n", diag.errorMessage());
            exit(1);
        }
        return { ma, (const void*)mappedAddr };
    }

    // __TEXT is first, so ma and base address are the same
    const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mappedAddr;
    if ( !ma->validMachOForArchAndPlatform(diag, (size_t)mappedSize, binaryPath, currentArchs, dyld3::Platform::unknown, false) ) {
        fprintf(stderr, "Error: %s\n", diag.errorMessage());
        exit(1);
    }
    return { ma, (const void*)mappedAddr };
}

int main(int argc, const char * argv[]) {
    bool unsupported = false;
#if TARGET_OS_WATCH
    // HACK: Watch archs are not supported right now, so just return
    unsupported = true;
#endif
    if ( unsupported ) {
        funcs = &testFuncs;
        PASS("Success");
    }

    if ( (argc < 2) || (argc > 5) ) {
        fprintf(stderr, "Usage: run-static *path to static binary* [- - *path to auc kc*]\n");
        return 1;
    }

    for (unsigned i = 1; i != argc; ++i) {
        if ( !strcmp(argv[i], "-") )
            continue;
        LoadedMachO macho = loadPath(argv[i]);
        if ( macho.ma == nullptr )
            return 1;
        testFuncs.mhs[i - 1] = macho.ma;
        testFuncs.basePointers[i - 1] = macho.basePointer;
    }

    uint64_t entryOffset    = 0;
    bool usesCRT            = false;
    const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)testFuncs.mhs[0];
    if ( !ma->getEntry(entryOffset, usesCRT) ) {
        fprintf(stderr, "Could not use binary '%s' because 'no entry defined'\n", argv[1]);
        return 1;
    }

    EntryFuncTy entryFunc = (EntryFuncTy)((uint8_t*)testFuncs.mhs[0] + entryOffset);
#if __has_feature(ptrauth_calls)
    entryFunc = (EntryFuncTy)__builtin_ptrauth_sign_unauthenticated((void*)entryFunc, 0, 0);
#endif
    fprintf(stderr, "Entering static binary at %p\n", entryFunc);
    //kill(getpid(), SIGSTOP);
    int returnCode = entryFunc(&testFuncs);
    if ( returnCode != 0 ) {
        fprintf(stderr, "Binary '%s' returned non-zero value %d\n", argv[1], returnCode);
        return returnCode;
    }
    return 0;
}
