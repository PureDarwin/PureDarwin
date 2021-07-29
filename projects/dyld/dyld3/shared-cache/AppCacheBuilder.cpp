/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
*
* Copyright (c) 2014 Apple Inc. All rights reserved.
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

#include "AppCacheBuilder.h"

#include <mach/mach_time.h>
#include <sys/stat.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

AppCacheBuilder::AppCacheBuilder(const DyldSharedCache::CreateOptions& options,
                                 const Options& appCacheOptions,
                                 const dyld3::closure::FileSystem& fileSystem)
    : CacheBuilder(options, fileSystem), appCacheOptions(appCacheOptions)
{
    // FIXME: 32-bit support
    _is64 = true;
}

AppCacheBuilder::~AppCacheBuilder() {
    if (prelinkInfoDict) {
        CFRelease(prelinkInfoDict);
    }
    if (_fullAllocatedBuffer) {
        vm_deallocate(mach_task_self(), _fullAllocatedBuffer, _allocatedBufferSize);
    }
}


void AppCacheBuilder::makeSortedDylibs(const std::vector<InputDylib>& dylibs)
{
    for (const InputDylib& file : dylibs) {
        if ( file.dylib.loadedFileInfo.fileContent == nullptr ) {
            codelessKexts.push_back(file);
        } else {
            AppCacheDylibInfo& dylibInfo = sortedDylibs.emplace_back();
            dylibInfo.input         = &file.dylib;
            dylibInfo.dylibID       = file.dylibID;
            dylibInfo.dependencies  = file.dylibDeps;
            dylibInfo.infoPlist     = file.infoPlist;
            dylibInfo.errors        = file.errors;
            dylibInfo.bundlePath    = file.bundlePath;
            dylibInfo.stripMode     = file.stripMode;
        }
    }

    std::sort(sortedDylibs.begin(), sortedDylibs.end(), [&](const DylibInfo& a, const DylibInfo& b) {
        // Sort the kernel first, then kext's
        bool isStaticExecutableA = a.input->mappedFile.mh->isStaticExecutable();
        bool isStaticExecutableB = b.input->mappedFile.mh->isStaticExecutable();
        if (isStaticExecutableA != isStaticExecutableB)
            return isStaticExecutableA;

        // Sort split seg next
        bool splitSegA = a.input->mappedFile.mh->hasSplitSeg();
        bool splitSegB = b.input->mappedFile.mh->hasSplitSeg();
        if (splitSegA != splitSegB)
            return splitSegA;

        // Finally sort by path
        return a.input->mappedFile.runtimePath < b.input->mappedFile.runtimePath;
    });

    // Sort codeless kext's by ID
    std::sort(codelessKexts.begin(), codelessKexts.end(), [&](const InputDylib& a, const InputDylib& b) {
        return a.dylibID < b.dylibID;
    });
}


void AppCacheBuilder::forEachCacheDylib(void (^callback)(const dyld3::MachOAnalyzer* ma,
                                                         const std::string& dylibID,
                                                         DylibStripMode stripMode,
                                                         const std::vector<std::string>& dependencies,
                                                         Diagnostics& dylibDiag,
                                                         bool& stop)) const {
    bool stop = false;
    for (const AppCacheDylibInfo& dylib : sortedDylibs) {
        for (const SegmentMappingInfo& loc : dylib.cacheLocation) {
            if (!strcmp(loc.segName, "__TEXT")) {
                // Assume __TEXT contains the mach header
                callback((const dyld3::MachOAnalyzer*)loc.dstSegment, dylib.dylibID, dylib.stripMode,
                         dylib.dependencies, *dylib.errors, stop);
                break;
            }
        }
        if (stop)
            break;
    }
}

void AppCacheBuilder::forEachDylibInfo(void (^callback)(const DylibInfo& dylib, Diagnostics& dylibDiag)) {
    for (const AppCacheDylibInfo& dylibInfo : sortedDylibs)
        callback(dylibInfo, *dylibInfo.errors);
}

const CacheBuilder::DylibInfo* AppCacheBuilder::getKernelStaticExecutableInputFile() const {
    for (const auto& dylib : sortedDylibs) {
        const dyld3::MachOAnalyzer* ma = dylib.input->mappedFile.mh;
        if ( ma->isStaticExecutable() )
            return &dylib;
    }
    return nullptr;
}

const dyld3::MachOAnalyzer* AppCacheBuilder::getKernelStaticExecutableFromCache() const {
    // FIXME: Support reading this from a prebuilt KC
    assert(appCacheOptions.cacheKind == Options::AppCacheKind::kernel);

    __block const dyld3::MachOAnalyzer* kernelMA = nullptr;
    forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                        DylibStripMode stripMode, const std::vector<std::string>& dependencies,
                        Diagnostics& dylibDiag,
                        bool& stop) {
        if ( ma->isStaticExecutable() ) {
            kernelMA = ma;
            stop = true;
        }
    });

    assert(kernelMA != nullptr);
    return kernelMA;
}

void AppCacheBuilder::forEachRegion(void (^callback)(const Region& region)) const {
    // cacheHeaderRegion
    callback(cacheHeaderRegion);

    // readOnlyTextRegion
    callback(readOnlyTextRegion);

    // readExecuteRegion
    if ( readExecuteRegion.sizeInUse != 0 )
        callback(readExecuteRegion);

    // branchStubsRegion
    if ( branchStubsRegion.bufferSize != 0 )
        callback(branchStubsRegion);

    // dataConstRegion
    if ( dataConstRegion.sizeInUse != 0 )
        callback(dataConstRegion);

    // branchGOTsRegion
    if ( branchGOTsRegion.bufferSize != 0 )
        callback(branchGOTsRegion);

    // readWriteRegion
    if ( readWriteRegion.sizeInUse != 0 )
        callback(readWriteRegion);

    // hibernateRegion
    if ( hibernateRegion.sizeInUse != 0 )
        callback(hibernateRegion);

    // -sectcreate
    for (const Region& region : customDataRegions)
        callback(region);

    // prelinkInfoRegion
    if ( prelinkInfoDict != nullptr )
        callback(prelinkInfoRegion);

    // nonSplitSegRegions
    for (const Region& region : nonSplitSegRegions)
        callback(region);

    // _readOnlyRegion
    callback(_readOnlyRegion);

    // fixupsRegion
    // We don't count this as its not a real region
}

uint64_t AppCacheBuilder::numRegions() const {
    __block uint64_t count = 0;

    forEachRegion(^(const Region &region) {
        ++count;
    });

    return count;
}

uint64_t AppCacheBuilder::fixupsPageSize() const {
    bool use4K = false;
    use4K |= (_options.archs == &dyld3::GradedArchs::x86_64);
    use4K |= (_options.archs == &dyld3::GradedArchs::x86_64h);
    return use4K ? 4096 : 16384;
}

uint64_t AppCacheBuilder::numWritablePagesToFixup(uint64_t numBytesToFixup) const {
    uint64_t pageSize = fixupsPageSize();
    assert((numBytesToFixup % pageSize) == 0);
    uint64_t numPagesToFixup = numBytesToFixup / pageSize;
    return numPagesToFixup;
}

// Returns true if each kext inside the KC needs to be reloadable, ie, have its
// pages reset and its start method rerun.  This means we can't pack pages and need
// fixups on each kext individually
bool AppCacheBuilder::fixupsArePerKext() const {
    if ( appCacheOptions.cacheKind == Options::AppCacheKind::pageableKC )
        return true;
    bool isX86 = (_options.archs == &dyld3::GradedArchs::x86_64) || (_options.archs == &dyld3::GradedArchs::x86_64h);
    return isX86 && (appCacheOptions.cacheKind == Options::AppCacheKind::auxKC);
}

// x86_64 kext's don't contain stubs for branches so we need to generate some
// if branches cross from one KC to another, eg, from the auxKC to the base KC
uint64_t AppCacheBuilder::numBranchRelocationTargets() {
    bool mayHaveBranchRelocations = false;
    mayHaveBranchRelocations |= (_options.archs == &dyld3::GradedArchs::x86_64);
    mayHaveBranchRelocations |= (_options.archs == &dyld3::GradedArchs::x86_64h);
    if ( !mayHaveBranchRelocations )
        return 0;

    switch (appCacheOptions.cacheKind) {
        case Options::AppCacheKind::none:
        case Options::AppCacheKind::kernel:
            // Nothing to do here as we can't bind from a lower level up to a higher one
            return 0;
        case Options::AppCacheKind::pageableKC:
        case Options::AppCacheKind::kernelCollectionLevel2:
        case Options::AppCacheKind::auxKC:
            // Any calls in these might be to a lower level so add space for each call target
            break;
    }

    uint64_t totalTargets = 0;
    for (const DylibInfo& dylib : sortedDylibs) {
        // We need the symbol name and libOrdinal just in case we bind to the same symbol name in 2 different KCs
        typedef std::pair<std::string_view, int> Symbol;
        struct SymbolHash
        {
            size_t operator() (const Symbol& symbol) const
            {
                return std::hash<std::string_view>{}(symbol.first) ^ std::hash<int>{}(symbol.second);
            }
        };
        __block std::unordered_set<Symbol, SymbolHash> seenSymbols;
        dylib.input->mappedFile.mh->forEachBind(_diagnostics,
                                                ^(uint64_t runtimeOffset, int libOrdinal, uint8_t type,
                                                  const char *symbolName, bool weakImport,
                                                  bool lazyBind, uint64_t addend, bool &stop) {
            if ( type != BIND_TYPE_TEXT_PCREL32 )
                return;
            seenSymbols.insert({ symbolName, libOrdinal });
        }, ^(const char *symbolName) {
        });
        totalTargets += seenSymbols.size();
    }
    return totalTargets;
}

void AppCacheBuilder::assignSegmentRegionsAndOffsets()
{
    // Segments can be re-ordered in memory relative to the order of the LC_SEGMENT load comamnds
    // so first make space for all the cache location objects so that we get the order the same
    // as the LC_SEGMENTs
    for (DylibInfo& dylib : sortedDylibs) {
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            dylib.cacheLocation.push_back({});
        });
    }

    // If we are building the kernel collection, then inherit the base address of the statically linked kernel
    const dyld3::MachOAnalyzer* kernelMA = nullptr;
    if ( appCacheOptions.cacheKind == Options::AppCacheKind::kernel ) {
        for (DylibInfo& dylib : sortedDylibs) {
            if ( dylib.input->mappedFile.mh->isStaticExecutable() ) {
                kernelMA = dylib.input->mappedFile.mh;
                break;
            }
        }
        if ( kernelMA == nullptr ) {
            _diagnostics.error("Could not find kernel image");
            return;
        }
        cacheBaseAddress = kernelMA->preferredLoadAddress();
    }

    // x86_64 doesn't have stubs for kext branches.  So work out how many potential targets
    // we need to emit stubs for.
    uint64_t branchTargetsFromKexts = numBranchRelocationTargets();

    uint32_t minimumSegmentAlignmentP2 = 14;
    if ( (_options.archs == &dyld3::GradedArchs::x86_64) || (_options.archs == &dyld3::GradedArchs::x86_64h) ) {
        minimumSegmentAlignmentP2 = 12;
    }

    auto getMinAlignment = ^(const dyld3::MachOAnalyzer* ma) {
        // The kernel wants to be able to unmap its own segments so always align it.
        // And align the pageable KC as each kext can be mapped individually
        if ( ma == kernelMA )
            return minimumSegmentAlignmentP2;
        if ( fixupsArePerKext() )
            return minimumSegmentAlignmentP2;
        if ( _options.archs == &dyld3::GradedArchs::arm64e )
            return minimumSegmentAlignmentP2;
        return 0U;
    };

    {
        // __TEXT segments with r/o permissions
        __block uint64_t offsetInRegion = 0;
        for (DylibInfo& dylib : sortedDylibs) {
            bool canBePacked = dylib.input->mappedFile.mh->hasSplitSeg();
            if (!canBePacked)
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( segInfo.protections != (VM_PROT_READ) )
                    return;
                if ( (strcmp(segInfo.segName, "__DATA_CONST") == 0)
                    || (strcmp(segInfo.segName, "__PPLDATA_CONST") == 0)
                    || (strcmp(segInfo.segName, "__LASTDATA_CONST") == 0) )
                    return;
                if ( strcmp(segInfo.segName, "__LINKEDIT") == 0 )
                    return;
                if ( strcmp(segInfo.segName, "__LINKINFO") == 0 )
                    return;

                uint32_t minAlignmentP2 = getMinAlignment(dylib.input->mappedFile.mh);
                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
                uint64_t dstCacheSegmentSize = align(segInfo.sizeOfSections, minAlignmentP2);

                // __CTF is not mapped in to the kernel, so remove it from the final binary.
                if ( strcmp(segInfo.segName, "__CTF") == 0 ) {
                    copySize = 0;
                    dstCacheSegmentSize = 0;
                }

                // kxld packs __TEXT so we will do
                // Note we align to at least 16-bytes as LDR's can scale up to 16 from their address
                // and aligning them less than 16 would break that
                offsetInRegion = align(offsetInRegion, std::max(segInfo.p2align, 4U));
                offsetInRegion = align(offsetInRegion, minAlignmentP2);
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)dstCacheSegmentSize;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &readOnlyTextRegion;
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += dstCacheSegmentSize;
            });
        }

        // kclist needs this segment, even if its empty, so leave it in there
        readOnlyTextRegion.bufferSize   = align(offsetInRegion, 14);
        readOnlyTextRegion.sizeInUse    = readOnlyTextRegion.bufferSize;
        readOnlyTextRegion.initProt     = VM_PROT_READ;
        readOnlyTextRegion.maxProt      = VM_PROT_READ;
        readOnlyTextRegion.name         = "__PRELINK_TEXT";
    }

    // __TEXT segments with r/x permissions
    {
        // __TEXT segments with r/x permissions
        __block uint64_t offsetInRegion = 0;
        for (DylibInfo& dylib : sortedDylibs) {
            bool canBePacked = dylib.input->mappedFile.mh->hasSplitSeg();
            if (!canBePacked)
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( strcmp(segInfo.segName, "__HIB") == 0 )
                    return;
                if ( segInfo.protections != (VM_PROT_READ | VM_PROT_EXECUTE) )
                   return;
                // kxld packs __TEXT_EXEC so we will do
                // Note we align to at least 16-bytes as LDR's can scale up to 16 from their address
                // and aligning them less than 16 would break that
                uint32_t minAlignmentP2 = getMinAlignment(dylib.input->mappedFile.mh);
                offsetInRegion = align(offsetInRegion, std::max(segInfo.p2align, 4U));
                offsetInRegion = align(offsetInRegion, minAlignmentP2);
                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
                uint64_t dstCacheSegmentSize = align(segInfo.sizeOfSections, minAlignmentP2);
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)dstCacheSegmentSize;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &readExecuteRegion;
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += loc.dstCacheSegmentSize;
            });
        }

        // align r/x region end
        readExecuteRegion.bufferSize  = align(offsetInRegion, 14);
        readExecuteRegion.sizeInUse   = readExecuteRegion.bufferSize;
        readExecuteRegion.initProt    = VM_PROT_READ | VM_PROT_EXECUTE;
        readExecuteRegion.maxProt     = VM_PROT_READ | VM_PROT_EXECUTE;
        readExecuteRegion.name        = "__TEXT_EXEC";
    }

    if ( branchTargetsFromKexts != 0 ) {
        // 6-bytes per jmpq
        branchStubsRegion.bufferSize    = align(branchTargetsFromKexts * 6, 14);
        branchStubsRegion.sizeInUse     = branchStubsRegion.bufferSize;
        branchStubsRegion.initProt      = VM_PROT_READ | VM_PROT_EXECUTE;
        branchStubsRegion.maxProt       = VM_PROT_READ | VM_PROT_EXECUTE;
        branchStubsRegion.name          = "__BRANCH_STUBS";
    }

    // __DATA_CONST segments
    {
        __block uint64_t offsetInRegion = 0;
        for (DylibInfo& dylib : sortedDylibs) {
            if (!dylib.input->mappedFile.mh->hasSplitSeg())
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( (segInfo.protections & VM_PROT_EXECUTE) != 0 )
                    return;
                if ( (strcmp(segInfo.segName, "__DATA_CONST") != 0)
                    && (strcmp(segInfo.segName, "__PPLDATA_CONST") != 0)
                    && (strcmp(segInfo.segName, "__LASTDATA_CONST") != 0)  )
                    return;
                // kxld packs __DATA_CONST so we will do
                uint32_t minAlignmentP2 = getMinAlignment(dylib.input->mappedFile.mh);
                offsetInRegion = align(offsetInRegion, segInfo.p2align);
                offsetInRegion = align(offsetInRegion, minAlignmentP2);
                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
                uint64_t dstCacheSegmentSize = align(segInfo.sizeOfSections, minAlignmentP2);
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)dstCacheSegmentSize;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &dataConstRegion;
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += loc.dstCacheSegmentSize;
            });
        }

        // align r/o region end
        dataConstRegion.bufferSize  = align(offsetInRegion, 14);
        dataConstRegion.sizeInUse   = dataConstRegion.bufferSize;
        dataConstRegion.initProt    = VM_PROT_READ;
        dataConstRegion.maxProt     = VM_PROT_READ;
        dataConstRegion.name        = "__DATA_CONST";
    }

    // Branch GOTs
    if ( branchTargetsFromKexts != 0 ) {
        // 8-bytes per GOT
        branchGOTsRegion.bufferSize     = align(branchTargetsFromKexts * 8, 14);
        branchGOTsRegion.sizeInUse      = branchGOTsRegion.bufferSize;
        branchGOTsRegion.initProt       = VM_PROT_READ | VM_PROT_WRITE;
        branchGOTsRegion.maxProt        = VM_PROT_READ | VM_PROT_WRITE;
        branchGOTsRegion.name           = "__BRANCH_GOTS";
    }

    // __DATA* segments
    {
        __block uint64_t offsetInRegion = 0;
        for (DylibInfo& dylib : sortedDylibs) {
            if (!dylib.input->mappedFile.mh->hasSplitSeg())
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( strcmp(segInfo.segName, "__HIB") == 0 )
                    return;
                if ( (strcmp(segInfo.segName, "__DATA_CONST") == 0)
                    || (strcmp(segInfo.segName, "__PPLDATA_CONST") == 0)
                    || (strcmp(segInfo.segName, "__LASTDATA_CONST") == 0) )
                    return;
                if ( segInfo.protections != (VM_PROT_READ | VM_PROT_WRITE) )
                    return;
                // kxld packs __DATA so we will do
                uint32_t minAlignmentP2 = getMinAlignment(dylib.input->mappedFile.mh);
                offsetInRegion = align(offsetInRegion, segInfo.p2align);
                offsetInRegion = align(offsetInRegion, minAlignmentP2);
                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
                uint64_t dstCacheSegmentSize = align(segInfo.sizeOfSections, minAlignmentP2);
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)dstCacheSegmentSize;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &readWriteRegion;
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += loc.dstCacheSegmentSize;
            });
        }

        // align r/w region end
        readWriteRegion.bufferSize  = align(offsetInRegion, 14);
        readWriteRegion.sizeInUse   = readWriteRegion.bufferSize;
        readWriteRegion.initProt    = VM_PROT_READ | VM_PROT_WRITE;
        readWriteRegion.maxProt     = VM_PROT_READ | VM_PROT_WRITE;
        readWriteRegion.name        = "__DATA";
    }

    {
        // Hibernate region
        __block uint64_t offsetInRegion = 0;
        for (DylibInfo& dylib : sortedDylibs) {
            if ( !dylib.input->mappedFile.mh->isStaticExecutable() )
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( strcmp(segInfo.segName, "__HIB") != 0 )
                    return;
                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)segInfo.vmSize;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &hibernateRegion;
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += loc.dstCacheSegmentSize;

                hibernateAddress = segInfo.vmAddr;
            });

            // Only xnu has __HIB, so no need to continue once we've found it.
            break;
        }

        hibernateRegion.bufferSize   = align(offsetInRegion, 14);
        hibernateRegion.sizeInUse    = hibernateRegion.bufferSize;
        hibernateRegion.initProt     = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
        hibernateRegion.maxProt      = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
        hibernateRegion.name         = "__HIB";
    }

    // __TEXT and __DATA from non-split seg dylibs, if we have any
    {
        for (DylibInfo& dylib : sortedDylibs) {
            bool canBePacked = dylib.input->mappedFile.mh->hasSplitSeg();
            if (canBePacked)
                continue;

            __block uint64_t textSegVmAddr = 0;
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                    textSegVmAddr = segInfo.vmAddr;
                if ( strcmp(segInfo.segName, "__LINKEDIT") == 0 )
                    return;

                nonSplitSegRegions.emplace_back();
                nonSplitSegRegions.back().initProt    = segInfo.protections;
                nonSplitSegRegions.back().maxProt     = segInfo.protections;
                nonSplitSegRegions.back().name        = "__REGION" + std::to_string(nonSplitSegRegions.size() - 1);

                // Note we don't align the region offset as we have no split seg
                uint64_t offsetInRegion = 0;
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = nullptr;
                loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
                loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
                loc.dstCacheSegmentSize    = (uint32_t)segInfo.vmSize;
                loc.dstCacheFileSize       = (uint32_t)segInfo.fileSize;
                loc.copySegmentSize        = (uint32_t)segInfo.fileSize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                loc.parentRegion           = &nonSplitSegRegions.back();
                dylib.cacheLocation[segInfo.segIndex] = loc;
                offsetInRegion += loc.dstCacheSegmentSize;

                // record non-split seg region end
                nonSplitSegRegions.back().bufferSize  = offsetInRegion;
                nonSplitSegRegions.back().sizeInUse   = nonSplitSegRegions.back().bufferSize;
            });
        }
    }

    // -sectcreate
    if ( !customSegments.empty() ) {
        for (CustomSegment& segment: customSegments) {
            uint64_t offsetInRegion = 0;
            for (CustomSegment::CustomSection& section : segment.sections) {
                section.offsetInRegion = offsetInRegion;
                offsetInRegion += section.data.size();
            }

            Region& customRegion = customDataRegions.emplace_back();
            segment.parentRegion = &customRegion;

            // align region end
            customRegion.bufferSize  = align(offsetInRegion, 14);
            customRegion.sizeInUse   = customRegion.bufferSize;
            customRegion.initProt    = VM_PROT_READ;
            customRegion.maxProt     = VM_PROT_READ;
            customRegion.name        = segment.segmentName;
        }
    }

    // __PRELINK_INFO
    {
        // This is populated with regular kexts and codeless kexts
        struct PrelinkInfo {
            CFDictionaryRef             infoPlist       = nullptr;
            const dyld3::MachOAnalyzer* ma              = nullptr;
            std::string_view            bundlePath;
            std::string_view            executablePath;
        };
        std::vector<PrelinkInfo> infos;
        for (AppCacheDylibInfo& dylib : sortedDylibs) {
            if (dylib.infoPlist == nullptr)
                continue;
            infos.push_back({ dylib.infoPlist, dylib.input->mappedFile.mh, dylib.bundlePath, dylib.input->loadedFileInfo.path });
        }
        for (InputDylib& dylib : codelessKexts) {
            infos.push_back({ dylib.infoPlist, nullptr, dylib.bundlePath, "" });
        }

        CFMutableArrayRef bundlesArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                                 &kCFTypeArrayCallBacks);
        for (PrelinkInfo& info : infos) {
            CFDictionaryRef infoPlist = info.infoPlist;
            // Create a copy of the dictionary so that we can add more fields
            CFMutableDictionaryRef dictCopyRef = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, infoPlist);

            // _PrelinkBundlePath
            CFStringRef bundlePath = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, info.bundlePath.data(),
                                                                     kCFStringEncodingASCII, kCFAllocatorNull);
            CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkBundlePath"), bundlePath);
            CFRelease(bundlePath);

            // Note we want this address to be a large enough integer in the xml format that we have enough space
            // to replace it with its real address later
            const uint64_t largeAddress = 0x7FFFFFFFFFFFFFFF;

            // _PrelinkExecutableLoadAddr
            // Leave a placeholder for this for now just so that we have enough space for it later
            CFNumberRef loadAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &largeAddress);
            CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkExecutableLoadAddr"), loadAddrRef);
            CFRelease(loadAddrRef);

            // _PrelinkExecutableRelativePath
            if ( info.executablePath != "" ) {
                const char* relativePath = info.executablePath.data();
                if ( strncmp(relativePath, info.bundlePath.data(), info.bundlePath.size()) == 0 ) {
                    relativePath = relativePath + info.bundlePath.size();
                    if ( relativePath[0] == '/' )
                        ++relativePath;
                } else if ( const char* lastSlash = strrchr(relativePath, '/') )
                    relativePath = lastSlash+1;
                CFStringRef executablePath = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, relativePath,
                                                                             kCFStringEncodingASCII, kCFAllocatorNull);
                CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkExecutableRelativePath"), executablePath);
                CFRelease(executablePath);
            }

            // _PrelinkExecutableSize
            // This seems to be the file size of __TEXT
            __block uint64_t textSegFileSize = 0;
            if ( info.ma != nullptr ) {
                info.ma->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                    if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                        textSegFileSize = segInfo.fileSize;
                });
            }
            if (textSegFileSize != 0) {
                CFNumberRef fileSizeRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &textSegFileSize);
                CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkExecutableSize"), fileSizeRef);
                CFRelease(fileSizeRef);
            }

            // _PrelinkExecutableSourceAddr
            // Leave a placeholder for this for now just so that we have enough space for it later
            CFNumberRef sourceAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &largeAddress);
            CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkExecutableSourceAddr"), sourceAddrRef);
            CFRelease(sourceAddrRef);

            // _PrelinkKmodInfo
            // Leave a placeholder for this for now just so that we have enough space for it later
            dyld3::MachOAnalyzer::FoundSymbol foundInfo;
            if ( (info.ma != nullptr) ) {
                // Check for a global first
                __block bool found = false;
                found = info.ma->findExportedSymbol(_diagnostics, "_kmod_info", true, foundInfo, nullptr);
                if ( !found ) {
                    // And fall back to a local if we need to
                    info.ma->forEachLocalSymbol(_diagnostics, ^(const char* aSymbolName, uint64_t n_value, uint8_t n_type,
                                                                uint8_t n_sect, uint16_t n_desc, bool& stop) {
                        if ( strcmp(aSymbolName, "_kmod_info") == 0 ) {
                            found = true;
                            stop = true;
                        }
                    });
                }

                if ( found ) {
                    CFNumberRef kmodInfoAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &largeAddress);
                    CFDictionarySetValue(dictCopyRef, CFSTR("_PrelinkKmodInfo"), kmodInfoAddrRef);
                    CFRelease(kmodInfoAddrRef);
                }
            }

            CFArrayAppendValue(bundlesArrayRef, dictCopyRef);
            // Release the temporary dictionary now that its in the array
            CFRelease(dictCopyRef);
        }

        prelinkInfoDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks);

        // First add any data from addPrelinkInfo()
        if ( extraPrelinkInfo != nullptr ) {
            CFDictionaryApplierFunction applier = [](const void *key, const void *value, void *context) {
                CFMutableDictionaryRef parentDict = (CFMutableDictionaryRef)context;
                CFDictionaryAddValue(parentDict, key, value);
            };
            CFDictionaryApplyFunction(extraPrelinkInfo, applier, (void*)prelinkInfoDict);
        }

        if ( bundlesArrayRef != nullptr ) {
            CFDictionaryAddValue(prelinkInfoDict, CFSTR("_PrelinkInfoDictionary"), bundlesArrayRef);
            CFRelease(bundlesArrayRef);
        }

        // Add a placeholder for the collection UUID
        {
            uuid_t uuid = {};
            CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)&uuid, sizeof(uuid));
            CFDictionaryAddValue(prelinkInfoDict, CFSTR("_PrelinkKCID"), dataRef);
            CFRelease(dataRef);
        }

        // The pageable/aux KCs should embed the UUID of the base kernel collection
        if ( existingKernelCollection != nullptr ) {
            uuid_t uuid = {};
            bool foundUUID = existingKernelCollection->getUuid(uuid);
            if ( !foundUUID ) {
                _diagnostics.error("Could not find UUID in base kernel collection");
                return;
            }
            CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)&uuid, sizeof(uuid));
            CFDictionaryAddValue(prelinkInfoDict, CFSTR("_BootKCID"), dataRef);
            CFRelease(dataRef);
        }

        // The aux KC should embed the UUID of the pageable kernel collection if we have one
        if ( pageableKernelCollection != nullptr ) {
            uuid_t uuid = {};
            bool foundUUID = pageableKernelCollection->getUuid(uuid);
            if ( !foundUUID ) {
                _diagnostics.error("Could not find UUID in pageable kernel collection");
                return;
            }
            CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)&uuid, sizeof(uuid));
            CFDictionaryAddValue(prelinkInfoDict, CFSTR("_PageableKCID"), dataRef);
            CFRelease(dataRef);
        }

        CFErrorRef errorRef = nullptr;
        CFDataRef xmlData = CFPropertyListCreateData(kCFAllocatorDefault, prelinkInfoDict,
                                                     kCFPropertyListXMLFormat_v1_0, 0, &errorRef);
        if (errorRef != nullptr) {
            CFStringRef errorString = CFErrorCopyDescription(errorRef);
            _diagnostics.error("Could not serialise plist because :%s",
                               CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
            CFRelease(xmlData);
            CFRelease(errorRef);
            return;
        } else {
            CFIndex xmlDataLength = CFDataGetLength(xmlData);
            CFRelease(xmlData);

            // align region end
            prelinkInfoRegion.bufferSize  = align(xmlDataLength, 14);
            prelinkInfoRegion.sizeInUse   = prelinkInfoRegion.bufferSize;
            prelinkInfoRegion.initProt    = VM_PROT_READ | VM_PROT_WRITE;
            prelinkInfoRegion.maxProt     = VM_PROT_READ | VM_PROT_WRITE;
            prelinkInfoRegion.name        = "__PRELINK_INFO";
        }
    }

    // Do all __LINKINFO regardless of split seg
    _nonLinkEditReadOnlySize = 0;
    __block uint64_t offsetInRegion = 0;
    for (DylibInfo& dylib : sortedDylibs) {
        __block uint64_t textSegVmAddr = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                textSegVmAddr = segInfo.vmAddr;
            if ( segInfo.protections != VM_PROT_READ )
                return;
            if ( strcmp(segInfo.segName, "__LINKINFO") != 0 )
                return;
            // Keep segments 4K or more aligned
            offsetInRegion = align(offsetInRegion, std::max((int)segInfo.p2align, (int)12));
            size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
            SegmentMappingInfo loc;
            loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
            loc.segName                = segInfo.segName;
            loc.dstSegment             = nullptr;
            loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
            loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
            loc.dstCacheSegmentSize    = (uint32_t)align(segInfo.sizeOfSections, 12);
            loc.dstCacheFileSize       = (uint32_t)copySize;
            loc.copySegmentSize        = (uint32_t)copySize;
            loc.srcSegmentIndex        = segInfo.segIndex;
            loc.parentRegion           = &_readOnlyRegion;
            dylib.cacheLocation[segInfo.segIndex] = loc;
            offsetInRegion += loc.dstCacheSegmentSize;
        });
    }

    // Align the end of the __LINKINFO
    offsetInRegion = align(offsetInRegion, 14);
    _nonLinkEditReadOnlySize = offsetInRegion;

    // Do all __LINKEDIT, regardless of split seg
    for (DylibInfo& dylib : sortedDylibs) {
        __block uint64_t textSegVmAddr = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                textSegVmAddr = segInfo.vmAddr;
            if ( segInfo.protections != VM_PROT_READ )
                return;
            if ( strcmp(segInfo.segName, "__LINKEDIT") != 0 )
                return;
            // Keep segments 4K or more aligned
            offsetInRegion = align(offsetInRegion, std::max((int)segInfo.p2align, (int)12));
            size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
            SegmentMappingInfo loc;
            loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
            loc.segName                = segInfo.segName;
            loc.dstSegment             = nullptr;
            loc.dstCacheUnslidAddress  = offsetInRegion; // This will be updated later once we've assigned addresses
            loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
            loc.dstCacheSegmentSize    = (uint32_t)align(segInfo.sizeOfSections, 12);
            loc.dstCacheFileSize       = (uint32_t)copySize;
            loc.copySegmentSize        = (uint32_t)copySize;
            loc.srcSegmentIndex        = segInfo.segIndex;
            loc.parentRegion           = &_readOnlyRegion;
            dylib.cacheLocation[segInfo.segIndex] = loc;
            offsetInRegion += loc.dstCacheSegmentSize;
        });
    }

    // align r/o region end
    _readOnlyRegion.bufferSize  = align(offsetInRegion, 14);
    _readOnlyRegion.sizeInUse   = _readOnlyRegion.bufferSize;
    _readOnlyRegion.initProt    = VM_PROT_READ;
    _readOnlyRegion.maxProt     = VM_PROT_READ;
    _readOnlyRegion.name        = "__LINKEDIT";

    // Add space in __LINKEDIT for chained fixups and classic relocs
    {

        // The pageableKC (and sometimes auxKC) has 1 LC_DYLD_CHAINED_FIXUPS per kext
        // while other KCs have 1 for the whole KC.
        // It also tracks each segment in each kext for chained fixups, not the segments on the KC itself
        __block uint64_t numSegmentsForChainedFixups = 0;
        uint64_t numChainedFixupHeaders = 0;
        if ( fixupsArePerKext() ) {
            for (DylibInfo& dylib : sortedDylibs) {
                dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                    ++numSegmentsForChainedFixups;
                });
            }
            numChainedFixupHeaders = sortedDylibs.size();

            // Branch stubs need fixups on the GOTs region.  So add in a top-level chained fixup entry
            // and for now all the regions as we don't know what segment index the branch GOTs will be
            numSegmentsForChainedFixups += numRegions();
            numChainedFixupHeaders++;
        } else {
            numSegmentsForChainedFixups = numRegions();
            numChainedFixupHeaders = 1;
        }

        uint64_t numBytesForPageStarts = 0;
        if ( dataConstRegion.sizeInUse != 0 )
            numBytesForPageStarts += sizeof(dyld_chained_starts_in_segment) + (sizeof(uint16_t) * numWritablePagesToFixup(dataConstRegion.bufferSize));
        if ( branchGOTsRegion.bufferSize != 0 )
            numBytesForPageStarts += sizeof(dyld_chained_starts_in_segment) + (sizeof(uint16_t) * numWritablePagesToFixup(branchGOTsRegion.bufferSize));
        if ( readWriteRegion.sizeInUse != 0 )
            numBytesForPageStarts += sizeof(dyld_chained_starts_in_segment) + (sizeof(uint16_t) * numWritablePagesToFixup(readWriteRegion.bufferSize));
        if ( hibernateRegion.sizeInUse != 0 )
            numBytesForPageStarts += sizeof(dyld_chained_starts_in_segment) + (sizeof(uint16_t) * numWritablePagesToFixup(hibernateRegion.bufferSize));
        for (const Region& region : nonSplitSegRegions) {
            // Assume writable regions have fixups to emit
            // Note, third party kext's have __TEXT fixups, so assume all of these have fixups
            // LINKEDIT is already elsewhere
            numBytesForPageStarts += sizeof(dyld_chained_starts_in_segment) + (sizeof(uint16_t) * numWritablePagesToFixup(region.bufferSize));
        }

        uint64_t numBytesForChainedFixups = 0;
        if ( numBytesForPageStarts != 0 ) {
            numBytesForChainedFixups = numBytesForPageStarts;
            numBytesForChainedFixups += sizeof(dyld_chained_fixups_header) * numChainedFixupHeaders;
            numBytesForChainedFixups += sizeof(dyld_chained_starts_in_image) * numChainedFixupHeaders;
            numBytesForChainedFixups += sizeof(uint32_t) * numSegmentsForChainedFixups;
        }

        __block uint64_t numBytesForClassicRelocs = 0;
        if ( appCacheOptions.cacheKind == Options::AppCacheKind::kernel ) {
            if ( const DylibInfo* dylib = getKernelStaticExecutableInputFile() ) {
                if ( dylib->input->mappedFile.mh->usesClassicRelocationsInKernelCollection() ) {
                    dylib->input->mappedFile.mh->forEachRebase(_diagnostics, false, ^(uint64_t runtimeOffset, bool &stop) {
                        numBytesForClassicRelocs += sizeof(relocation_info);
                    });
                }
            }
        }

        // align fixups region end
        if ( (numBytesForChainedFixups != 0) || (numBytesForClassicRelocs != 0) ) {
            uint64_t numBytes = align(numBytesForChainedFixups, 3) + align(numBytesForClassicRelocs, 3);
            fixupsSubRegion.bufferSize  = align(numBytes, 14);
            fixupsSubRegion.sizeInUse   = fixupsSubRegion.bufferSize;
            fixupsSubRegion.initProt    = VM_PROT_READ;
            fixupsSubRegion.maxProt     = VM_PROT_READ;
            fixupsSubRegion.name        = "__FIXUPS";
        }
    }
}

void AppCacheBuilder::assignSegmentAddresses() {
    // Segments already have offsets in to their regions.  Now assign the regions their addresses
    // in the full allocated buffer, and then assign all segments in those regions
    for (DylibInfo& dylib : sortedDylibs) {
        for (SegmentMappingInfo& loc : dylib.cacheLocation) {
            loc.dstSegment = loc.parentRegion->buffer + loc.dstCacheFileOffset;
            loc.dstCacheUnslidAddress   = loc.parentRegion->unslidLoadAddress + loc.dstCacheFileOffset;
            loc.dstCacheFileOffset      = (uint32_t)loc.parentRegion->cacheFileOffset + loc.dstCacheFileOffset;
        }
    }
}

void AppCacheBuilder::copyRawSegments() {
    const bool log = false;

    // Call the base class to copy segment data
    CacheBuilder::copyRawSegments();

    // The copy any custom sections
    for (const CustomSegment& segment : customSegments) {
        for (const CustomSegment::CustomSection& section : segment.sections) {
            uint8_t* dstBuffer = segment.parentRegion->buffer + section.offsetInRegion;
            uint64_t dstVMAddr = segment.parentRegion->unslidLoadAddress + section.offsetInRegion;
            if (log) fprintf(stderr, "copy %s segment %s %s (0x%08lX bytes) from %p to %p (logical addr 0x%llX)\n",
                             _options.archs->name(), segment.segmentName.c_str(), section.sectionName.c_str(),
                             section.data.size(), section.data.data(), dstBuffer, dstVMAddr);
            ::memcpy(dstBuffer, section.data.data(), section.data.size());
        }
    }
}

static uint8_t getFixupLevel(AppCacheBuilder::Options::AppCacheKind kind) {
    uint8_t currentLevel = (uint8_t)~0U;
    switch (kind) {
        case AppCacheBuilder::Options::AppCacheKind::none:
            assert(0 && "Cache kind should have been set");
            break;
        case AppCacheBuilder::Options::AppCacheKind::kernel:
            currentLevel = 0;
            break;
        case AppCacheBuilder::Options::AppCacheKind::pageableKC:
            // The pageableKC sits right above the baseKC which is level 0
            currentLevel = 1;
            break;
        case AppCacheBuilder::Options::AppCacheKind::kernelCollectionLevel2:
            assert(0 && "Unimplemented");
            break;
        case AppCacheBuilder::Options::AppCacheKind::auxKC:
            currentLevel = 3;
            break;
    }
    return currentLevel;
}

uint32_t AppCacheBuilder::getCurrentFixupLevel() const {
    return getFixupLevel(appCacheOptions.cacheKind);
}

struct VTableBindSymbol {
    std::string_view binaryID;
    std::string symbolName;
};

// For every dylib, lets make a map from its exports to its defs
struct DylibSymbols {
    // Define a bunch of constructors so that we know we are getting move constructors not copies
    DylibSymbols() = default;
    DylibSymbols(const DylibSymbols&) = delete;
    DylibSymbols(DylibSymbols&&) = default;
    DylibSymbols(std::map<std::string_view, uint64_t>&& globals,
                 std::map<std::string_view, uint64_t>&& locals,
                 std::unique_ptr<std::unordered_set<std::string>> kpiSymbols,
                 uint32_t dylibLevel, const std::string& dylibName)
        : globals(std::move(globals)), locals(std::move(locals)), kpiSymbols(std::move(kpiSymbols)),
          dylibLevel(dylibLevel), dylibName(dylibName) { }

    DylibSymbols& operator=(const DylibSymbols& other) = delete;
    DylibSymbols& operator=(DylibSymbols&& other) = default;

    std::map<std::string_view, uint64_t> globals;

    // We also need to track locals as vtable patching supports patching with these too
    std::map<std::string_view, uint64_t> locals;

    // KPI (ie, a symbol set embedded in this binary)
    std::unique_ptr<std::unordered_set<std::string>> kpiSymbols;

    // Kernel collections can reference each other in levels.  This is the level
    // of the exported dylib.  Eg, the base KC is 0, and the aux KC is 3
    uint32_t dylibLevel = 0;

    // Store the name of the dylib for fast lookups
    std::string dylibName;

    // Keep track of the binds in this dylib as these tell us if a vtable slot is to a local
    // or external definition of a function
    std::unordered_map<const uint8_t*, VTableBindSymbol> resolvedBindLocations;
};

class VTablePatcher {
public:

    VTablePatcher(uint32_t numFixupLevels);

    bool hasError() const;

    void addKernelCollection(const dyld3::MachOAppCache* cacheMA, AppCacheBuilder::Options::AppCacheKind kind,
                             const uint8_t* basePointer, uint64_t baseAddress);
    void addDylib(Diagnostics& diags, const dyld3::MachOAnalyzer* ma, const std::string& dylibID,
                  const std::vector<std::string>& dependencies, uint8_t cacheLevel);

    void findMetaclassDefinitions(std::map<std::string, DylibSymbols>& dylibsToSymbols,
                                  const std::string& kernelID, const dyld3::MachOAnalyzer* kernelMA,
                                  AppCacheBuilder::Options::AppCacheKind cacheKind);
    void findExistingFixups(Diagnostics& diags,
                            const dyld3::MachOAppCache* existingKernelCollection,
                            const dyld3::MachOAppCache* pageableKernelCollection);
    void findBaseKernelVTables(Diagnostics& diags, const dyld3::MachOAppCache* existingKernelCollection,
                               std::map<std::string, DylibSymbols>& dylibsToSymbols);
    void findPageableKernelVTables(Diagnostics& diags, const dyld3::MachOAppCache* existingKernelCollection,
                                   std::map<std::string, DylibSymbols>& dylibsToSymbols);
    void findVTables(uint8_t currentLevel, const dyld3::MachOAnalyzer* kernelMA,
                     std::map<std::string, DylibSymbols>& dylibsToSymbols,
                     const AppCacheBuilder::ASLR_Tracker& aslrTracker,
                     const std::map<const uint8_t*, const VTableBindSymbol>& missingBindLocations);
    void calculateSymbols();
    void patchVTables(Diagnostics& diags,
                      std::map<const uint8_t*, const VTableBindSymbol>& missingBindLocations,
                      AppCacheBuilder::ASLR_Tracker& aslrTracker,
                      uint8_t currentLevel);

private:

    void logFunc(const char* format, ...) {
        if ( logPatching ) {
            va_list list;
            va_start(list, format);
            vfprintf(stderr, format, list);
            va_end(list);
        }
    };

    void logFuncVerbose(const char* format, ...) {
        if ( logPatchingVerbose ) {
            va_list list;
            va_start(list, format);
            vfprintf(stderr, format, list);
            va_end(list);
        }
    };

    // Extract a substring by dropping optional prefix/suffix
    std::string_view extractString(std::string_view str, std::string_view prefix, std::string_view suffix) {
        if ( !prefix.empty() ) {
            // Make sure we have the prefix we are looking for
            if ( str.find(prefix) != 0 ) {
                return std::string_view();
            }
            str.remove_prefix(prefix.size());
        }
        if ( !suffix.empty() ) {
            // Make sure we have the prefix we are looking for
            size_t pos = str.rfind(suffix);
            if ( pos != (str.size() - suffix.size()) ) {
                return std::string_view();
            }
            str.remove_suffix(suffix.size());
        }
        return str;
    };

    struct VTable {
        struct Entry {
            const uint8_t*  location            = nullptr;
            uint64_t        targetVMAddr        = ~0ULL;
            uint32_t        targetCacheLevel    = ~0;
            // Pointer auth
            uint16_t        diversity           = 0;
            bool            hasAddrDiv          = false;
            uint8_t         key                 = 0;
            bool            hasPointerAuth      = false;
        };

        const dyld3::MachOAnalyzer* ma                      = nullptr;
        const uint8_t*              superVTable             = nullptr;
        const DylibSymbols*         dylib                   = nullptr;
        bool                        fromParentCollection    = false;
        bool                        patched                 = false;
        std::string                 name                    = "";
        std::vector<Entry>          entries;
    };

    struct SymbolLocation {
        uint64_t    vmAddr          = 0;
        bool        foundSymbol     = 0;

        bool found() const {
            return foundSymbol;
        }
    };

    struct Fixup {
        uint64_t targetVMAddr = 0;
        uint8_t  cacheLevel   = 0;
        // Pointer auth
        uint16_t        diversity       = 0;
        bool            hasAddrDiv      = false;
        uint8_t         key             = 0;
        bool            hasPointerAuth  = false;
    };

    struct VTableDylib {
        Diagnostics*                diags           = nullptr;
        const dyld3::MachOAnalyzer* ma              = nullptr;
        std::string                 dylibID         = "";
        std::vector<std::string>    dependencies;
        uint32_t                    cacheLevel      = ~0U;
    };

    struct KernelCollection {
        const dyld3::MachOAppCache*                 ma                      = nullptr;

        // We need the base pointers to the buffers for every level
        // These are the base of the allocated memory, which corresponds to pointing to the lowest
        // vmAddr for the buffer.  These do *not* necessarily point to a mach_header
        const uint8_t*                              basePointer             = nullptr;

        // We also need the base vm addresses to the buffers for every level
        uint64_t                                    baseAddress             = ~0ULL;

        std::unordered_map<uint64_t, const char*>   symbolNames;
        std::map<uint64_t, std::string_view>        metaclassDefinitions;
    };

    SymbolLocation findVTablePatchingSymbol(std::string_view symbolName, const DylibSymbols& dylibSymbols);

    std::vector<VTableDylib>                                dylibs;
    std::map<const uint8_t*, VTable>                        vtables;
    std::vector<KernelCollection>                           collections;
    const uint8_t*                                          baseMetaClassVTableLoc  = nullptr;

    // Record all the fixup locations in the base/pageable KCs as we need to use them instead of the ASLR tracker
    std::map<const uint8_t*, Fixup>                         existingCollectionFixupLocations;

    const uint32_t                                          pointerSize             = 8;
    const bool                                              logPatching             = false;
    const bool                                              logPatchingVerbose      = false;

    // Magic constants for vtable patching
    //const char*                                           cxxPrefix                   = "__Z";
    const char*                                             vtablePrefix                = "__ZTV";
    const char*                                             osObjPrefix                 = "__ZN";
    // const char*                                          vtableReservedToken         = "_RESERVED";
    const char*                                             metaclassToken              = "10gMetaClassE";
    const char*                                             superMetaclassPointerToken  = "10superClassE";
    const char*                                             metaclassVTablePrefix       = "__ZTVN";
    const char*                                             metaclassVTableSuffix       = "9MetaClassE";
};

VTablePatcher::VTablePatcher(uint32_t numFixupLevels) {
    collections.resize(numFixupLevels);
}

bool VTablePatcher::hasError() const {
    for (const VTableDylib& dylib : dylibs) {
        if ( dylib.diags->hasError() )
            return true;
    }
    return false;
}

void VTablePatcher::addKernelCollection(const dyld3::MachOAppCache* cacheMA, AppCacheBuilder::Options::AppCacheKind kind,
                                        const uint8_t* basePointer, uint64_t baseAddress) {
    uint8_t cacheLevel = getFixupLevel(kind);

    assert(cacheLevel < collections.size());
    assert(collections[cacheLevel].ma == nullptr);

    collections[cacheLevel].ma          = cacheMA;
    collections[cacheLevel].basePointer = basePointer;
    collections[cacheLevel].baseAddress = baseAddress;
}

void VTablePatcher::addDylib(Diagnostics &diags, const dyld3::MachOAnalyzer *ma,
                             const std::string& dylibID, const std::vector<std::string>& dependencies,
                             uint8_t cacheLevel) {
    dylibs.push_back((VTableDylib){ &diags, ma, dylibID, dependencies, cacheLevel });
}

VTablePatcher::SymbolLocation VTablePatcher::findVTablePatchingSymbol(std::string_view symbolName,
                                                                      const DylibSymbols& dylibSymbols) {
    // First look in the globals
    auto globalsIt = dylibSymbols.globals.find(symbolName);
    if ( globalsIt != dylibSymbols.globals.end() ) {
        return { globalsIt->second, true };
    }

    // Then again in the locals
    auto localsIt = dylibSymbols.locals.find(symbolName);
    if ( localsIt != dylibSymbols.locals.end() ) {
        return { localsIt->second, true };
    }

    return { ~0ULL, false };
};

void VTablePatcher::findMetaclassDefinitions(std::map<std::string, DylibSymbols>& dylibsToSymbols,
                                             const std::string& kernelID, const dyld3::MachOAnalyzer* kernelMA,
                                             AppCacheBuilder::Options::AppCacheKind cacheKind) {
    for (VTableDylib& dylib : dylibs) {
        auto& metaclassDefinitions = collections[dylib.cacheLevel].metaclassDefinitions;
        dylib.ma->forEachGlobalSymbol(*dylib.diags, ^(const char *symbolName, uint64_t n_value,
                                                      uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
            if ( strstr(symbolName, metaclassToken) != nullptr )
                metaclassDefinitions[n_value] = symbolName;
        });
        dylib.ma->forEachLocalSymbol(*dylib.diags, ^(const char *symbolName, uint64_t n_value,
                                                     uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
            if ( strstr(symbolName, metaclassToken) != nullptr )
                metaclassDefinitions[n_value] = symbolName;
        });
    }

    // Keep track of the root OSMetaClass from which all other metaclasses inherit
    DylibSymbols& kernelDylibSymbols = dylibsToSymbols[kernelID];
    SymbolLocation symbolLocation = findVTablePatchingSymbol("__ZTV11OSMetaClass", kernelDylibSymbols);
    if ( symbolLocation.found() ) {
        baseMetaClassVTableLoc = (uint8_t*)kernelMA + (symbolLocation.vmAddr - kernelMA->preferredLoadAddress());

        VTable& vtable = vtables[baseMetaClassVTableLoc];
        vtable.ma                   = kernelMA;
        vtable.dylib                = &kernelDylibSymbols;
        vtable.fromParentCollection = (cacheKind != AppCacheBuilder::Options::AppCacheKind::kernel);
        vtable.patched              = true;
        vtable.name                 = "__ZTV11OSMetaClass";
    }
}

void VTablePatcher::findExistingFixups(Diagnostics& diags,
                                       const dyld3::MachOAppCache* existingKernelCollection,
                                       const dyld3::MachOAppCache* pageableKernelCollection) {

    const bool is64 = pointerSize == 8;

    if ( existingKernelCollection != nullptr ) {
        uint8_t kernelLevel = getFixupLevel(AppCacheBuilder::Options::AppCacheKind::kernel);
        uint64_t kernelBaseAddress = collections[kernelLevel].baseAddress;
        const uint8_t* kernelBasePointer = collections[kernelLevel].basePointer;

        // We may have both chained and classic fixups.  First add chained
        if ( existingKernelCollection->hasChainedFixupsLoadCommand() ) {
            existingKernelCollection->withChainStarts(diags, 0, ^(const dyld_chained_starts_in_image* starts) {
                existingKernelCollection->forEachFixupInAllChains(diags, starts, false,
                                                                  ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
                    uint64_t vmOffset = 0;
                    bool isRebase = fixupLoc->isRebase(segInfo->pointer_format, kernelBaseAddress, vmOffset);
                    assert(isRebase);
                    uint64_t targetVMAddr   = kernelBaseAddress + vmOffset;
                    uint16_t diversity      = fixupLoc->kernel64.diversity;
                    bool     hasAddrDiv     = fixupLoc->kernel64.addrDiv;
                    uint8_t  key            = fixupLoc->kernel64.key;
                    bool     hasPointerAuth = fixupLoc->kernel64.isAuth;
                    existingCollectionFixupLocations[(const uint8_t*)fixupLoc] = { targetVMAddr, kernelLevel, diversity, hasAddrDiv, key, hasPointerAuth };
                });
            });
        }

        // And add classic if we have them
        existingKernelCollection->forEachRebase(diags, ^(const char *opcodeName, const dyld3::MachOAnalyzer::LinkEditInfo &leInfo,
                                                                const dyld3::MachOAnalyzer::SegmentInfo *segments,
                                                                bool segIndexSet, uint32_t pointerSize, uint8_t segmentIndex,
                                                                uint64_t segmentOffset, dyld3::MachOAnalyzer::Rebase kind, bool &stop) {
            uint64_t rebaseVmAddr  = segments[segmentIndex].vmAddr + segmentOffset;
            uint64_t runtimeOffset = rebaseVmAddr - kernelBaseAddress;
            const uint8_t* fixupLoc = kernelBasePointer + runtimeOffset;
            uint64_t targetVMAddr = 0;
            if ( is64 ) {
                targetVMAddr = *(uint64_t*)fixupLoc;
            } else {
                targetVMAddr = *(uint32_t*)fixupLoc;
            }
            // Classic relocs have no pointer auth
            uint16_t diversity      = 0;
            bool     hasAddrDiv     = false;
            uint8_t  key            = 0;
            bool     hasPointerAuth = false;
            existingCollectionFixupLocations[(const uint8_t*)fixupLoc] = { targetVMAddr, kernelLevel, diversity, hasAddrDiv, key, hasPointerAuth };
        });
    }

    // Add pageable fixup locations if we have it
    if ( pageableKernelCollection != nullptr ) {
        // We only have chained fixups here to add, but they are on each kext, not on the KC itself
        pageableKernelCollection->forEachDylib(diags, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
            // Skip kexts without fixups
            if ( !ma->hasChainedFixupsLoadCommand() )
                return;
            ma->withChainStarts(diags, 0, ^(const dyld_chained_starts_in_image* starts) {
                ma->forEachFixupInAllChains(diags, starts, false, ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
                    uint64_t vmOffset = 0;
                    bool isRebase = fixupLoc->isRebase(DYLD_CHAINED_PTR_64_KERNEL_CACHE, 0, vmOffset);
                    assert(isRebase);
                    uint8_t targetFixupLevel    = fixupLoc->kernel64.cacheLevel;
                    uint64_t targetVMAddr       = collections[targetFixupLevel].baseAddress + vmOffset;
                    uint16_t diversity          = fixupLoc->kernel64.diversity;
                    bool     hasAddrDiv         = fixupLoc->kernel64.addrDiv;
                    uint8_t  key                = fixupLoc->kernel64.key;
                    bool     hasPointerAuth     = fixupLoc->kernel64.isAuth;
                    existingCollectionFixupLocations[(const uint8_t*)fixupLoc] = { targetVMAddr, targetFixupLevel, diversity, hasAddrDiv, key, hasPointerAuth };
                });
            });
        });
    }
}

void VTablePatcher::findBaseKernelVTables(Diagnostics& diags, const dyld3::MachOAppCache* existingKernelCollection,
                                          std::map<std::string, DylibSymbols>& dylibsToSymbols)
{
    const bool is64 = pointerSize == 8;

    uint8_t kernelLevel = getFixupLevel(AppCacheBuilder::Options::AppCacheKind::kernel);
    uint64_t kernelBaseAddress = collections[kernelLevel].baseAddress;
    const uint8_t* kernelBasePointer = collections[kernelLevel].basePointer;
    uint16_t chainedPointerFormat = 0;

    if ( existingKernelCollection->hasChainedFixupsLoadCommand() )
        chainedPointerFormat = existingKernelCollection->chainedPointerFormat();

    // Map from dylibID to list of dependencies
    std::map<std::string, const std::vector<std::string>*> kextDependencies;
    for (VTableDylib& dylib : dylibs) {
        if ( dylib.cacheLevel != kernelLevel )
            continue;
        kextDependencies[dylib.dylibID] = &dylib.dependencies;
    }

    bool kernelUsesClassicRelocs = existingKernelCollection->usesClassicRelocationsInKernelCollection();
    existingKernelCollection->forEachDylib(diags, ^(const dyld3::MachOAnalyzer *ma, const char *dylibID, bool &stop) {
        uint64_t loadAddress = ma->preferredLoadAddress();

        auto visitBaseKernelCollectionSymbols = ^(const char *symbolName, uint64_t n_value) {
            if ( strstr(symbolName, superMetaclassPointerToken) == nullptr )
                return;
            uint8_t* fixupLoc = (uint8_t*)ma + (n_value - loadAddress);
            logFunc("Found superclass pointer with name '%s' in '%s' at %p\n", symbolName, dylibID, fixupLoc);

            // 2 - Derive the name of the class from the super MetaClass pointer.
            std::string_view className = extractString(symbolName, osObjPrefix, superMetaclassPointerToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( className.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Class name: '%s'\n", std::string(className).c_str());

            // 3 - Derive the name of the class's vtable from the name of the class
            // We support namespaces too which means adding an N before the class name and E after
            std::string classVTableName = std::string(vtablePrefix) + std::string(className);
            logFunc("Class vtable name: '%s'\n", classVTableName.c_str());

            uint64_t classVTableVMAddr = 0;
            const DylibSymbols& dylibSymbols = dylibsToSymbols[dylibID];
            {
                std::string namespacedVTableName;
                SymbolLocation symbolLocation = findVTablePatchingSymbol(classVTableName, dylibSymbols);
                if ( !symbolLocation.found() ) {
                    // If we didn't find a name then try again with namespaces
                    namespacedVTableName = std::string(vtablePrefix) + "N" + std::string(className) + "E";
                    logFunc("Class namespaced vtable name: '%s'\n", namespacedVTableName.c_str());
                    symbolLocation = findVTablePatchingSymbol(namespacedVTableName, dylibSymbols);
                }
                if ( symbolLocation.found() ) {
                    classVTableVMAddr = symbolLocation.vmAddr;
                } else {
                    diags.error("Class vtables '%s' or '%s' is not exported from '%s'",
                                       classVTableName.c_str(), namespacedVTableName.c_str(), dylibID);
                    stop = true;
                    return;
                }
            }

            logFunc("Class vtable vmAddr: '0x%llx'\n", classVTableVMAddr);
            const uint8_t* classVTableLoc = kernelBasePointer + (classVTableVMAddr - kernelBaseAddress);

            // 4 - Follow the super MetaClass pointer to get the address of the super MetaClass's symbol
            uint64_t superMetaclassSymbolAddress = 0;
            auto existingKernelCollectionFixupLocIt = existingCollectionFixupLocations.find(fixupLoc);
            if ( existingKernelCollectionFixupLocIt != existingCollectionFixupLocations.end() ) {
                if ( ma->isKextBundle() || !kernelUsesClassicRelocs ) {
                    auto* chainedFixupLoc = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)fixupLoc;
                    uint64_t vmOffset = 0;
                    bool isRebase = chainedFixupLoc->isRebase(chainedPointerFormat, kernelBaseAddress, vmOffset);
                    assert(isRebase);
                    superMetaclassSymbolAddress = kernelBaseAddress + vmOffset;
                } else {
                    // The classic reloc is already the vmAddr so nothing special to do here.
                    assert(is64);
                    superMetaclassSymbolAddress = *(uint64_t*)fixupLoc;
                }
            }

            logFunc("Super MetaClass's symbol address: '0x%llx'\n", superMetaclassSymbolAddress);

            if ( superMetaclassSymbolAddress == 0 ) {
                if ( classVTableName == "__ZTV8OSObject" ) {
                    // This is the base class of all objects, so it doesn't have a super class
                    // We add it as a placeholder and set it to 'true' to show its already been processed
                    VTable& vtable = vtables[classVTableLoc];
                    vtable.ma                   = ma;
                    vtable.dylib                = &dylibSymbols;
                    vtable.fromParentCollection = true;
                    vtable.patched              = true;
                    vtable.name                 = classVTableName;
                    return;
                }
            }

            // 5 - Look up the super MetaClass symbol by address
            // FIXME: VTable patching the auxKC with the superclass in the baseKC
            uint8_t superclassFixupLevel = kernelLevel;

            auto& metaclassDefinitions = collections[superclassFixupLevel].metaclassDefinitions;
            auto metaclassIt = metaclassDefinitions.find(superMetaclassSymbolAddress);
            if ( metaclassIt == metaclassDefinitions.end() ) {
                diags.error("Cannot find symbol for metaclass pointed to by '%s' in '%s'",
                            symbolName, dylibID);
                stop = true;
                return;
            }

            // 6 - Derive the super class's name from the super MetaClass name
            std::string_view superClassName = extractString(metaclassIt->second, osObjPrefix, metaclassToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( superClassName.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Superclass name: '%s'\n", std::string(superClassName).c_str());

            // 7 - Derive the super class's vtable from the super class's name
            std::string superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);

            // We support namespaces, so first try the superclass without the namespace, then again with it
            const uint8_t* superclassVTableLoc = nullptr;
            for (unsigned i = 0; i != 2; ++i) {
                if ( i == 1 ) {
                    superclassVTableName = std::string(vtablePrefix) + + "N" + std::string(superClassName) + "E";
                }
                logFunc("Superclass vtable name: '%s'\n", superclassVTableName.c_str());

                if ( ma->isKextBundle() ) {
                    // First check if the superclass vtable comes from a dependent kext
                    auto it = kextDependencies.find(dylibID);
                    assert(it != kextDependencies.end());
                    const std::vector<std::string>& dependencies = *it->second;
                    for (const std::string& dependencyID : dependencies) {
                        auto depIt = dylibsToSymbols.find(dependencyID);
                        if (depIt == dylibsToSymbols.end()) {
                            diags.error("Failed to bind '%s' in '%s' as could not find a kext with '%s' bundle-id",
                                        symbolName, dylibID, dependencyID.c_str());
                            stop = true;
                            return;
                        }

                        const DylibSymbols& dylibSymbols = depIt->second;
                        SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                        if ( !symbolLocation.found() )
                            continue;

                        uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                        logFunc("Superclass vtable vmAddr: '0x%llx'\n", superclassVTableVMAddr);
                        superclassVTableLoc = collections[dylibSymbols.dylibLevel].basePointer + (superclassVTableVMAddr - collections[dylibSymbols.dylibLevel].baseAddress);
                        break;
                    }
                }
                if ( superclassVTableLoc == nullptr ) {
                    auto depIt = dylibsToSymbols.find(dylibID);
                    if (depIt == dylibsToSymbols.end()) {
                        diags.error("Failed to bind '%s' in '%s' as could not find a binary with '%s' bundle-id",
                                    symbolName, dylibID, dylibID);
                        stop = true;
                        return;
                    }

                    const DylibSymbols& dylibSymbols = depIt->second;
                    SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                    if ( symbolLocation.found() ) {
                        uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                        logFunc("Superclass vtable vmAddr: '0x%llx'\n", superclassVTableVMAddr);
                        superclassVTableLoc = collections[dylibSymbols.dylibLevel].basePointer + (superclassVTableVMAddr - collections[dylibSymbols.dylibLevel].baseAddress);
                    }
                }

                if ( superclassVTableLoc != nullptr )
                    break;
            }

            if ( superclassVTableLoc == nullptr ) {
                superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);
                diags.error("Superclass vtable '%s' is not exported from '%s' or its dependencies",
                            superclassVTableName.c_str(), dylibID);
                stop = true;
                return;
            }

            // Add an entry for this vtable
            VTable& vtable = vtables[classVTableLoc];
            vtable.superVTable = superclassVTableLoc;
            vtable.ma                   = ma;
            vtable.dylib                = &dylibSymbols;
            vtable.fromParentCollection = true;
            vtable.patched              = true;
            vtable.name                 = classVTableName;

            // And an entry for the superclass vtable
            VTable& supervtable = vtables[superclassVTableLoc];
            supervtable.fromParentCollection    = true;
            supervtable.patched                 = true;
            supervtable.name                    = superclassVTableName;
        };

        ma->forEachGlobalSymbol(diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                         uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitBaseKernelCollectionSymbols(symbolName, n_value);
        });

        if ( diags.hasError() ) {
            stop = true;
            return;
        }

        ma->forEachLocalSymbol(diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                        uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitBaseKernelCollectionSymbols(symbolName, n_value);
        });

        if ( diags.hasError() ) {
            stop = true;
            return;
        }
    });
}

void VTablePatcher::findPageableKernelVTables(Diagnostics& diags, const dyld3::MachOAppCache* pageableKernelCollection,
                                              std::map<std::string, DylibSymbols>& dylibsToSymbols)
{
    uint8_t collectionLevel = getFixupLevel(AppCacheBuilder::Options::AppCacheKind::pageableKC);
    uint64_t collectionBaseAddress = collections[collectionLevel].baseAddress;
    const uint8_t* collectionBasePointer = collections[collectionLevel].basePointer;

    // Map from dylibID to list of dependencies
    std::map<std::string, const std::vector<std::string>*> kextDependencies;
    for (VTableDylib& dylib : dylibs) {
        if ( dylib.cacheLevel != collectionLevel )
            continue;
        kextDependencies[dylib.dylibID] = &dylib.dependencies;
    }

    pageableKernelCollection->forEachDylib(diags, ^(const dyld3::MachOAnalyzer *ma, const char *dylibID, bool &stop) {
        uint64_t loadAddress = ma->preferredLoadAddress();
        auto visitPageableKernelCollectionSymbols = ^(const char *symbolName, uint64_t n_value) {
            if ( strstr(symbolName, superMetaclassPointerToken) == nullptr )
                return;
            uint8_t* fixupLoc = (uint8_t*)ma + (n_value - loadAddress);
            logFunc("Found superclass pointer with name '%s' in '%s' at %p\n", symbolName, dylibID, fixupLoc);

            // 2 - Derive the name of the class from the super MetaClass pointer.
            std::string_view className = extractString(symbolName, osObjPrefix, superMetaclassPointerToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( className.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Class name: '%s'\n", std::string(className).c_str());

            // 3 - Derive the name of the class's vtable from the name of the class
            // We support namespaces too which means adding an N before the class name and E after
            std::string classVTableName = std::string(vtablePrefix) + std::string(className);
            logFunc("Class vtable name: '%s'\n", classVTableName.c_str());

            uint64_t classVTableVMAddr = 0;
            const DylibSymbols& dylibSymbols = dylibsToSymbols[dylibID];
            {
                std::string namespacedVTableName;
                SymbolLocation symbolLocation = findVTablePatchingSymbol(classVTableName, dylibSymbols);
                if ( !symbolLocation.found() ) {
                    // If we didn't find a name then try again with namespaces
                    namespacedVTableName = std::string(vtablePrefix) + "N" + std::string(className) + "E";
                    logFunc("Class namespaced vtable name: '%s'\n", namespacedVTableName.c_str());
                    symbolLocation = findVTablePatchingSymbol(namespacedVTableName, dylibSymbols);
                }
                if ( symbolLocation.found() ) {
                    classVTableVMAddr = symbolLocation.vmAddr;
                } else {
                    diags.error("Class vtables '%s' or '%s' is not exported from '%s'",
                                classVTableName.c_str(), namespacedVTableName.c_str(), dylibID);
                    stop = true;
                    return;
                }
            }

            logFunc("Class vtable vmAddr: '0x%llx'\n", classVTableVMAddr);
            const uint8_t* classVTableLoc = collectionBasePointer + (classVTableVMAddr - collectionBaseAddress);

            // 4 - Follow the super MetaClass pointer to get the address of the super MetaClass's symbol
            uint8_t superclassFixupLevel = (uint8_t)~0U;
            uint64_t superMetaclassSymbolAddress = 0;
            auto existingKernelCollectionFixupLocIt = existingCollectionFixupLocations.find(fixupLoc);
            if ( existingKernelCollectionFixupLocIt != existingCollectionFixupLocations.end() ) {
                auto* chainedFixupLoc = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)fixupLoc;
                uint64_t vmOffset = 0;
                bool isRebase = chainedFixupLoc->isRebase(DYLD_CHAINED_PTR_64_KERNEL_CACHE, 0, vmOffset);
                assert(isRebase);
                // The superclass could be in the baseKC, while we are analysing the pageableKC, so we need to get the correct level
                // from the fixup
                superclassFixupLevel = chainedFixupLoc->kernel64.cacheLevel;
                superMetaclassSymbolAddress = collections[superclassFixupLevel].baseAddress + vmOffset;
            }

            logFunc("Super MetaClass's symbol address: '0x%llx'\n", superMetaclassSymbolAddress);

            if ( superMetaclassSymbolAddress == 0 ) {
                if ( classVTableName == "__ZTV8OSObject" ) {
                    // This is the base class of all objects, so it doesn't have a super class
                    // We add it as a placeholder and set it to 'true' to show its already been processed
                    VTable& vtable = vtables[classVTableLoc];
                    vtable.ma                   = ma;
                    vtable.dylib                = &dylibSymbols;
                    vtable.fromParentCollection = true;
                    vtable.patched              = true;
                    vtable.name                 = classVTableName;
                    return;
                }
            }

            // 5 - Look up the super MetaClass symbol by address
            auto& metaclassDefinitions = collections[superclassFixupLevel].metaclassDefinitions;
            auto metaclassIt = metaclassDefinitions.find(superMetaclassSymbolAddress);
            if ( metaclassIt == metaclassDefinitions.end() ) {
                diags.error("Cannot find symbol for metaclass pointed to by '%s' in '%s'",
                            symbolName, dylibID);
                stop = true;
                return;
            }

            // 6 - Derive the super class's name from the super MetaClass name
            std::string_view superClassName = extractString(metaclassIt->second, osObjPrefix, metaclassToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( superClassName.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Superclass name: '%s'\n", std::string(superClassName).c_str());

            // 7 - Derive the super class's vtable from the super class's name
            std::string superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);

            // We support namespaces, so first try the superclass without the namespace, then again with it
            const uint8_t* superclassVTableLoc = nullptr;
            for (unsigned i = 0; i != 2; ++i) {
                if ( i == 1 ) {
                    superclassVTableName = std::string(vtablePrefix) + + "N" + std::string(superClassName) + "E";
                }
                logFunc("Superclass vtable name: '%s'\n", superclassVTableName.c_str());

                if ( ma->isKextBundle() ) {
                    // First check if the superclass vtable comes from a dependent kext
                    auto it = kextDependencies.find(dylibID);
                    assert(it != kextDependencies.end());
                    const std::vector<std::string>& dependencies = *it->second;
                    for (const std::string& dependencyID : dependencies) {
                        auto depIt = dylibsToSymbols.find(dependencyID);
                        if (depIt == dylibsToSymbols.end()) {
                            diags.error("Failed to bind '%s' in '%s' as could not find a kext with '%s' bundle-id",
                                        symbolName, dylibID, dependencyID.c_str());
                            stop = true;
                            return;
                        }

                        const DylibSymbols& dylibSymbols = depIt->second;
                        SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                        if ( !symbolLocation.found() )
                            continue;

                        uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                        logFunc("Superclass vtable vmAddr: '0x%llx'\n", superclassVTableVMAddr);
                        superclassVTableLoc = collections[dylibSymbols.dylibLevel].basePointer + (superclassVTableVMAddr - collections[dylibSymbols.dylibLevel].baseAddress);
                        break;
                    }
                }
                if ( superclassVTableLoc == nullptr ) {
                    auto depIt = dylibsToSymbols.find(dylibID);
                    if (depIt == dylibsToSymbols.end()) {
                        diags.error("Failed to bind '%s' in '%s' as could not find a binary with '%s' bundle-id",
                                    symbolName, dylibID, dylibID);
                        stop = true;
                        return;
                    }

                    const DylibSymbols& dylibSymbols = depIt->second;
                    SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                    if ( symbolLocation.found() ) {
                        uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                        logFunc("Superclass vtable vmAddr: '0x%llx'\n", superclassVTableVMAddr);
                        superclassVTableLoc = collections[dylibSymbols.dylibLevel].basePointer + (superclassVTableVMAddr - collections[dylibSymbols.dylibLevel].baseAddress);
                    }
                }

                if ( superclassVTableLoc != nullptr )
                    break;
            }

            if ( superclassVTableLoc == nullptr ) {
                superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);
                diags.error("Superclass vtable '%s' is not exported from '%s' or its dependencies",
                            superclassVTableName.c_str(), dylibID);
                stop = true;
                return;
            }

            // Add an entry for this vtable
            VTable& vtable = vtables[classVTableLoc];
            vtable.superVTable = superclassVTableLoc;
            vtable.ma                   = ma;
            vtable.dylib                = &dylibSymbols;
            vtable.fromParentCollection = true;
            vtable.patched              = true;
            vtable.name                 = classVTableName;

            // And an entry for the superclass vtable
            VTable& supervtable = vtables[superclassVTableLoc];
            supervtable.fromParentCollection    = true;
            supervtable.patched                 = true;
            supervtable.name                    = superclassVTableName;
        };

        ma->forEachGlobalSymbol(diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                         uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitPageableKernelCollectionSymbols(symbolName, n_value);
        });

        if ( diags.hasError() ) {
            stop = true;
            return;
        }

        ma->forEachLocalSymbol(diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                        uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitPageableKernelCollectionSymbols(symbolName, n_value);
        });

        if ( diags.hasError() ) {
            stop = true;
            return;
        }
    });
}

void VTablePatcher::findVTables(uint8_t currentLevel, const dyld3::MachOAnalyzer* kernelMA,
                                std::map<std::string, DylibSymbols>& dylibsToSymbols,
                                const AppCacheBuilder::ASLR_Tracker& aslrTracker,
                                const std::map<const uint8_t*, const VTableBindSymbol>& missingBindLocations)
{
    const bool is64 = pointerSize == 8;

    uint64_t collectionBaseAddress = collections[currentLevel].baseAddress;
    const uint8_t* collectionBasePointer = collections[currentLevel].basePointer;

    // VTable patching algorithm (for each symbol...):
    // - To find the address of a class vtable:
    //  - Take symbols with '10superClassE' in their name, eg, __ZN10IOMachPort10superClassE
    //  - Work out the name of the class from that symbol name, eg, 10IOMachPort
    //  - Work out the name of the VTable from that class name, eg, __ZTV10IOMachPort
    //  - Find the address for the export with that vtable name
    // - To find the superclass for a given class
    //  - Take the symbol with '10superClassE' in their name, eg, __ZN10IOMachPort10superClassE
    //  - Take its address and dereference it as "__ZN10IOMachPort10superClassE = &__ZN8OSObject10gMetaClassE"
    //  - Find the name of the symbol at this address, eg, work out we have a symbol called __ZN8OSObject10gMetaClassE
    //  - Get the superclassic from that symbol name, eg, 8OSObject
    //  - Get the VTable name from that symbol, eg, __ZTV8OSObject
    //  - Find the superclass vtable address from that name by searching the image and dependents for __ZTV8OSObject
    for (VTableDylib& dylib : dylibs) {
        // Only process dylibs in the level we are building
        // Existing collections were handled elsewhere
        if ( dylib.cacheLevel != currentLevel )
            continue;

        const dyld3::MachOAnalyzer* ma                  = dylib.ma;
        const std::string&          dylibID             = dylib.dylibID;
        Diagnostics&                dylibDiags          = *dylib.diags;
        const std::vector<std::string>& dependencies    = dylib.dependencies;

        uint64_t loadAddress = ma->preferredLoadAddress();
        bool alreadyPatched = (ma == kernelMA);
        auto visitSymbols = ^(const char *symbolName, uint64_t n_value) {
            if ( strstr(symbolName, superMetaclassPointerToken) == nullptr )
                return;

            uint8_t* fixupLoc = (uint8_t*)ma + (n_value - loadAddress);
            logFunc("Found superclass pointer with name '%s' in '%s' at %p\n", symbolName, dylibID.c_str(), fixupLoc);

            // 2 - Derive the name of the class from the super MetaClass pointer.
            std::string_view className = extractString(symbolName, osObjPrefix, superMetaclassPointerToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( className.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Class name: '%s'\n", std::string(className).c_str());

            // 3 - Derive the name of the class's vtable from the name of the class
            // We support namespaces too which means adding an N before the class name and E after
            std::string classVTableName = std::string(vtablePrefix) + std::string(className);
            logFunc("Class vtable name: '%s'\n", classVTableName.c_str());

            uint64_t classVTableVMAddr = 0;
            const DylibSymbols& dylibSymbols = dylibsToSymbols[dylibID];
            {
                std::string namespacedVTableName;
                SymbolLocation symbolLocation = findVTablePatchingSymbol(classVTableName, dylibSymbols);
                if ( !symbolLocation.found() ) {
                    // If we didn't find a name then try again with namespaces
                    namespacedVTableName = std::string(vtablePrefix) + "N" + std::string(className) + "E";
                    logFunc("Class namespaced vtable name: '%s'\n", namespacedVTableName.c_str());
                    symbolLocation = findVTablePatchingSymbol(namespacedVTableName, dylibSymbols);
                }
                if ( symbolLocation.found() ) {
                    classVTableVMAddr = symbolLocation.vmAddr;
                } else {
                    dylibDiags.error("Class vtables '%s' or '%s' is not an exported symbol",
                                     classVTableName.c_str(), namespacedVTableName.c_str());
                    return;
                }
            }

            logFunc("Class vtable vmAddr: '0x%llx'\n", classVTableVMAddr);
            const uint8_t* classVTableLoc = (uint8_t*)ma + (classVTableVMAddr - loadAddress);

            // 4 - Follow the super MetaClass pointer to get the address of the super MetaClass's symbol
            uint64_t superMetaclassSymbolAddress = 0;
            {
                uint32_t vmAddr32 = 0;
                uint64_t vmAddr64 = 0;
                if ( aslrTracker.hasRebaseTarget32(fixupLoc, &vmAddr32) ) {
                    superMetaclassSymbolAddress = vmAddr32;
                } else if ( aslrTracker.hasRebaseTarget64(fixupLoc, &vmAddr64) ) {
                    superMetaclassSymbolAddress = vmAddr64;
                } else {
                    assert(is64);
                    superMetaclassSymbolAddress = *(uint64_t*)fixupLoc;
                }
                uint8_t highByte = 0;
                if ( aslrTracker.hasHigh8(fixupLoc, &highByte) ) {
                    uint64_t tbi = (uint64_t)highByte << 56;
                    superMetaclassSymbolAddress |= tbi;
                }
            }
            logFunc("Super MetaClass's symbol address: '0x%llx'\n", superMetaclassSymbolAddress);

            if ( superMetaclassSymbolAddress == 0 ) {
                if ( classVTableName == "__ZTV8OSObject" ) {
                    // This is the base class of all objects, so it doesn't have a super class
                    // We add it as a placeholder and set it to 'true' to show its already been processed
                    VTable& vtable = vtables[classVTableLoc];
                    vtable.ma                   = ma;
                    vtable.dylib                = &dylibSymbols;
                    vtable.fromParentCollection = false;
                    vtable.patched              = true;
                    vtable.name                 = classVTableName;
                    return;
                }
            }

            // 5 - Look up the super MetaClass symbol by address
            // FIXME: VTable patching the auxKC with the superclass in the baseKC
            uint8_t superclassFixupLevel = currentLevel;
            aslrTracker.has(fixupLoc, &superclassFixupLevel);

            auto& metaclassDefinitions = collections[superclassFixupLevel].metaclassDefinitions;
            auto metaclassIt = metaclassDefinitions.find(superMetaclassSymbolAddress);
            if ( metaclassIt == metaclassDefinitions.end() ) {
                auto bindIt = missingBindLocations.find(fixupLoc);
                if ( bindIt != missingBindLocations.end() ) {
                    dylibDiags.error("Cannot find symbol for metaclass pointed to by '%s'.  "
                                     "Expected symbol '%s' to be defined in another kext",
                                     symbolName, bindIt->second.symbolName.c_str());
                } else {
                    dylibDiags.error("Cannot find symbol for metaclass pointed to by '%s'",
                                     symbolName);
                }
                return;
            }

            // 6 - Derive the super class's name from the super MetaClass name
            std::string_view superClassName = extractString(metaclassIt->second, osObjPrefix, metaclassToken);
            // If the string isn't prefixed/suffixed appropriately, then give up on this one
            if ( superClassName.empty() ) {
                logFunc("Unsupported vtable superclass name\n");
                return;
            }
            logFunc("Superclass name: '%s'\n", std::string(superClassName).c_str());

            // 7 - Derive the super class's vtable from the super class's name
            std::string superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);

            // We support namespaces, so first try the superclass without the namespace, then again with it
            const uint8_t* superclassVTableLoc = nullptr;
            bool superVTableIsInParentCollection = false;
            for (unsigned i = 0; i != 2; ++i) {
                if ( i == 1 ) {
                    superclassVTableName = std::string(vtablePrefix) + + "N" + std::string(superClassName) + "E";
                }
                logFunc("Superclass vtable name: '%s'\n", superclassVTableName.c_str());

                {
                    // First check if the superclass vtable comes from a dependent kext
                    for (const std::string& dependencyID : dependencies) {
                        auto depIt = dylibsToSymbols.find(dependencyID);
                        if (depIt == dylibsToSymbols.end()) {
                            dylibDiags.error("Failed to bind '%s' as could not find a kext with '%s' bundle-id",
                                             symbolName, dependencyID.c_str());
                            return;
                        }

                        const DylibSymbols& dylibSymbols = depIt->second;
                        SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                        if ( !symbolLocation.found() )
                            continue;

                        uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                        logFunc("Superclass vtable vmAddr: '0x%llx'\n", superclassVTableVMAddr);
                        superclassVTableLoc = collections[dylibSymbols.dylibLevel].basePointer + (superclassVTableVMAddr - collections[dylibSymbols.dylibLevel].baseAddress);
                        superVTableIsInParentCollection = dylibSymbols.dylibLevel != currentLevel;
                        break;
                    }

                    if ( superclassVTableLoc == nullptr ) {
                        SymbolLocation symbolLocation = findVTablePatchingSymbol(superclassVTableName, dylibSymbols);
                        if ( symbolLocation.found() ) {
                            uint64_t superclassVTableVMAddr = symbolLocation.vmAddr;
                            superclassVTableLoc = (uint8_t*)collectionBasePointer + (superclassVTableVMAddr - collectionBaseAddress);
                            superVTableIsInParentCollection = false;
                        }
                    }
                }

                if ( superclassVTableLoc != nullptr )
                    break;
            }

            if ( superclassVTableLoc == nullptr ) {
                superclassVTableName = std::string(vtablePrefix) + std::string(superClassName);
                dylibDiags.error("Superclass vtable '%s' is not exported from kext or its dependencies",
                                 superclassVTableName.c_str());
                return;
            }

            // Add an entry for this vtable
            VTable& vtable = vtables[classVTableLoc];
            vtable.superVTable = superclassVTableLoc;
            vtable.ma                   = ma;
            vtable.dylib                = &dylibSymbols;
            vtable.fromParentCollection = false;
            vtable.patched              |= alreadyPatched;
            vtable.name                 = classVTableName;

            // And an entry for the superclass vtable
            VTable& supervtable = vtables[superclassVTableLoc];
            supervtable.fromParentCollection    = superVTableIsInParentCollection;
            supervtable.patched                 |= alreadyPatched;
            supervtable.name                    = superclassVTableName;

            // Also calculate the metaclass vtable name so that we can patch it
            std::string metaclassVTableName = std::string(metaclassVTablePrefix) + std::string(className) + metaclassVTableSuffix;
            logFunc("Metaclass vtable name: '%s'\n", metaclassVTableName.c_str());

            {
                // Note its safe to just ignore missing metaclass symbols if we can't find them
                // If the binary links then kxld would have let it run
                SymbolLocation symbolLocation = findVTablePatchingSymbol(metaclassVTableName, dylibSymbols);
                if ( symbolLocation.found() ) {
                    uint64_t metaclassVTableVMAddr = symbolLocation.vmAddr;

                    logFunc("Metaclass vtable vmAddr: '0x%llx'\n", metaclassVTableVMAddr);
                    uint8_t* metaclassVTableLoc = (uint8_t*)ma + (metaclassVTableVMAddr - loadAddress);

                    // Add an entry for this vtable
                    VTable& vtable = vtables[metaclassVTableLoc];
                    vtable.superVTable          = baseMetaClassVTableLoc;
                    vtable.ma                   = ma;
                    vtable.dylib                = &dylibSymbols;
                    vtable.fromParentCollection = false;
                    vtable.patched              |= alreadyPatched;
                    vtable.name                 = metaclassVTableName;
                }
            }
        };

        ma->forEachGlobalSymbol(dylibDiags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                             uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitSymbols(symbolName, n_value);
        });

        ma->forEachLocalSymbol(dylibDiags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                            uint8_t n_sect, uint16_t n_desc, bool &stop) {
            visitSymbols(symbolName, n_value);
        });
    }
}

void VTablePatcher::calculateSymbols() {
    for (VTableDylib& dylib : dylibs) {
        auto& symbolNames = collections[dylib.cacheLevel].symbolNames;
        dylib.ma->forEachGlobalSymbol(*dylib.diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                                      uint8_t n_sect, uint16_t n_desc, bool &stop) {
            symbolNames[n_value] = symbolName;
        });
        dylib.ma->forEachLocalSymbol(*dylib.diags, ^(const char *symbolName, uint64_t n_value, uint8_t n_type,
                                                     uint8_t n_sect, uint16_t n_desc, bool &stop) {
            symbolNames[n_value] = symbolName;
        });
    }
}

void VTablePatcher::patchVTables(Diagnostics& diags,
                                 std::map<const uint8_t*, const VTableBindSymbol>& missingBindLocations,
                                 AppCacheBuilder::ASLR_Tracker& aslrTracker,
                                 uint8_t currentLevel)
{
    const bool is64 = pointerSize == 8;

    // If we have vtables to patch, then make sure we found the OSMetaClass symbol to patch against
    if ( (baseMetaClassVTableLoc == nullptr) && !vtables.empty() ) {
        diags.error("Could not find OSMetaClass vtable in kernel binary");
        return;
    }

    calculateSymbols();

    auto calculateVTableEntries = ^(const uint8_t* vtableLoc, VTable& vtable) {
        assert(vtable.patched);
        logFunc("Calculating vtable: '%s'\n", vtable.name.c_str());

        // The first entry we want to patch is 2 pointers from the start of the vtable
        const uint8_t* relocLoc = vtableLoc + (2 * pointerSize);

        if ( vtable.fromParentCollection ) {
            auto it = existingCollectionFixupLocations.find(relocLoc);
            while ( it != existingCollectionFixupLocations.end() ) {
                const Fixup& fixup = it->second;
                uint64_t targetVMAddr   = fixup.targetVMAddr;
                uint16_t diversity      = fixup.diversity;
                bool     hasAddrDiv     = fixup.hasAddrDiv;
                uint8_t  key            = fixup.key;
                bool     hasPointerAuth = fixup.hasPointerAuth;
                uint32_t  cacheLevel    = fixup.cacheLevel;
                vtable.entries.push_back({ relocLoc, targetVMAddr, cacheLevel, diversity, hasAddrDiv, key, hasPointerAuth });
                relocLoc += pointerSize;
                it = existingCollectionFixupLocations.find(relocLoc);
            }
        } else {
            while ( aslrTracker.has((void*)relocLoc) ||
                   (missingBindLocations.find(relocLoc) != missingBindLocations.end()) ) {

                uint16_t diversity      = 0;
                bool     hasAddrDiv     = false;
                uint8_t  key            = 0;
                bool     hasPointerAuth = false;
                uint8_t cacheLevel     = currentLevel;

                if ( aslrTracker.has((void*)relocLoc, &cacheLevel) ) {
                    hasPointerAuth = aslrTracker.hasAuthData((void*)relocLoc, &diversity, &hasAddrDiv, &key);
                }

                uint64_t targetVMAddr = 0;
                {
                    uint32_t vmAddr32 = 0;
                    uint64_t vmAddr64 = 0;
                    if ( aslrTracker.hasRebaseTarget32((void*)relocLoc, &vmAddr32) ) {
                        targetVMAddr = vmAddr32;
                    } else if ( aslrTracker.hasRebaseTarget64((void*)relocLoc, &vmAddr64) ) {
                        targetVMAddr = vmAddr64;
                    } else {
                        assert(is64);
                        targetVMAddr = *(uint64_t*)relocLoc;
                    }
                    uint8_t highByte = 0;
                    if ( aslrTracker.hasHigh8((void*)relocLoc, &highByte) ) {
                        uint64_t tbi = (uint64_t)highByte << 56;
                        targetVMAddr |= tbi;
                    }
                }

                vtable.entries.push_back({ relocLoc, targetVMAddr, cacheLevel, diversity, hasAddrDiv, key, hasPointerAuth });
                relocLoc += pointerSize;
            }
        }

        logFunc("Found %d vtable items: '%s'\n", vtable.entries.size(), vtable.name.c_str());
    };

    // Map from MachO to diagnostics to emit for that file
    std::unordered_map<const dyld3::MachOAnalyzer*, Diagnostics*> diagsMap;
    for (VTableDylib& dylib : dylibs)
        diagsMap[dylib.ma] = dylib.diags;

    uint32_t numPatchedVTables = 0;
    for (auto& vtableEntry : vtables) {
        if ( vtableEntry.second.patched ) {
            calculateVTableEntries(vtableEntry.first, vtableEntry.second);
            ++numPatchedVTables;
        }
    }
    while ( numPatchedVTables != vtables.size() ) {
        typedef std::pair<const uint8_t*, VTable*> VTableEntry;
        std::vector<VTableEntry> toBePatched;
        for (auto& vtableEntry : vtables) {
            if ( vtableEntry.second.patched )
                continue;
            auto superIt = vtables.find(vtableEntry.second.superVTable);
            assert(superIt != vtables.end());
            if ( !superIt->second.patched )
                continue;
            logFunc("Found unpatched vtable: '%s' with patched superclass '%s'\n",
                    vtableEntry.second.name.c_str(), superIt->second.name.c_str());
            toBePatched.push_back({ vtableEntry.first, &vtableEntry.second });
        }

        if ( toBePatched.empty() ) {
            // If we can't find anything to patch, then print out what we have left
            for (const auto& vtableEntry : vtables) {
                if ( vtableEntry.second.patched )
                    continue;
                auto superIt = vtables.find(vtableEntry.second.superVTable);
                assert(superIt != vtables.end());
                diags.error("Found unpatched vtable: '%s' with unpatched superclass '%s'\n",
                            vtableEntry.second.name.c_str(), superIt->second.name.c_str());
            }
            break;
        }

        for (VTableEntry& vtableEntry : toBePatched) {
            VTable& vtable = *vtableEntry.second;

            // We can immediately mark this as patched as then calculateVTableEntries can make
            // sure we never ask for vtables which aren't ready yet
            vtable.patched = true;
            ++numPatchedVTables;

            auto superIt = vtables.find(vtable.superVTable);
            logFunc("Processing unpatched vtable: '%s' with patched superclass '%s'\n",
                    vtable.name.c_str(), superIt->second.name.c_str());

            calculateVTableEntries(vtableEntry.first, vtable);

            const VTable& supervtable = superIt->second;
            if ( vtable.entries.size() < supervtable.entries.size() ) {
                // Try emit the error to a per dylib diagnostic object if we can find one
                auto diagIt = diagsMap.find(vtable.ma);
                Diagnostics* diag = (diagIt != diagsMap.end()) ? diagIt->second : &diags;
                diag->error("Malformed vtable.  Super class '%s' has %lu entries vs subclass '%s' with %lu entries",
                            supervtable.name.c_str(), supervtable.entries.size(),
                            vtable.name.c_str(), vtable.entries.size());
                return;
            }

            const std::unordered_map<const uint8_t*, VTableBindSymbol>& resolvedBindLocations = vtable.dylib->resolvedBindLocations;
            for (uint64_t entryIndex = 0; entryIndex != supervtable.entries.size(); ++entryIndex) {
                logFuncVerbose("Processing entry %lld: super[0x%llx] vs subclass[0x%llx]\n", entryIndex,
                               *(uint64_t*)supervtable.entries[entryIndex].location,
                               *(uint64_t*)vtable.entries[entryIndex].location);

                VTable::Entry& vtableEntry = vtable.entries[entryIndex];
                const VTable::Entry& superVTableEntry = supervtable.entries[entryIndex];

                const uint8_t* patchLoc = vtableEntry.location;
                uint64_t targetVMAddr = superVTableEntry.targetVMAddr;

                // 1) If the symbol is defined locally, do not patch
                // This corresponds to a rebase not a bind, so if we have a match in our bind set
                // we were bound to another image, and should see if that bind should be overridden by a
                // better vtable patch.
                auto resolvedBindIt = resolvedBindLocations.find(patchLoc);
                auto unresolvedBindIt = missingBindLocations.find(patchLoc);
                if ( (resolvedBindIt == resolvedBindLocations.end()) && (unresolvedBindIt == missingBindLocations.end()) )
                    continue;

                // Find the child and parent symbols, if any
                const char* childSymbolName = nullptr;
                const char* parentSymbolName = nullptr;

                if ( resolvedBindIt != resolvedBindLocations.end() ) {
                    childSymbolName = resolvedBindIt->second.symbolName.c_str();
                } else {
                    assert(unresolvedBindIt != missingBindLocations.end());
                    childSymbolName = unresolvedBindIt->second.symbolName.c_str();
                }

                auto& symbolNames = collections[superVTableEntry.targetCacheLevel].symbolNames;
                auto parentNameIt = symbolNames.find(superVTableEntry.targetVMAddr);
                if ( parentNameIt != symbolNames.end() )
                    parentSymbolName = parentNameIt->second;

                // The child entry can be NULL when a locally-defined, non-external
                // symbol is stripped.  We wouldn't patch this entry anyway, so we just skip it.
                if ( childSymbolName == nullptr ) {
                    continue;
                }

                // It's possible for the patched parent entry not to have a symbol
                // (e.g. when the definition is inlined).  We can't patch this entry no
                // matter what, so we'll just skip it and die later if it's a problem
                // (which is not likely).
                if ( parentSymbolName == nullptr ) {
                    continue;
                }

                logFuncVerbose("Processing entry %lld: super[%s] vs subclass[%s]\n", entryIndex,
                               parentSymbolName, childSymbolName);

                // 2) If the child is a pure virtual function, do not patch.
                // In general, we want to proceed with patching when the symbol is
                // externally defined because pad slots fall into this category.
                // The pure virtual function symbol is special case, as the pure
                // virtual property itself overrides the parent's implementation.
                if ( !strcmp(childSymbolName, "___cxa_pure_virtual") ) {
                    continue;
                }

                // 3) If the symbols are the same, do not patch
                // Note that if the symbol was a missing bind, then we'll still patch
                // This is the case where the vtable entry itself was a local symbol
                // so we had originally failed to bind to it as it wasn't exported, but it
                // has the same name as the parent name
                if ( !strcmp(childSymbolName, parentSymbolName) && (unresolvedBindIt == missingBindLocations.end()) ) {
                    continue;
                }

#if 0
                // FIXME: Implement this

                // 4) If the parent vtable entry is a pad slot, and the child does not
                // match it, then the child was built against a newer version of the
                // libraries, so it is binary-incompatible.
                require_action(!kxld_sym_name_is_padslot(parent_entry->patched.name),
                    finish, rval = KERN_FAILURE;
                    kxld_log(kKxldLogPatching, kKxldLogErr,
                    kKxldLogParentOutOfDate,
                    kxld_demangle(super_vtable->name, &demangled_name1,
                    &demangled_length1),
                    kxld_demangle(vtable->name, &demangled_name2,
                    &demangled_length2)));
#endif

                logFunc("Patching entry '%s' in '%s' to point to '%s' in superclass '%s'\n",
                        childSymbolName, vtable.name.c_str(), parentSymbolName, supervtable.name.c_str());

                if ( is64 ) {
                    *((uint64_t*)patchLoc) = targetVMAddr;
                } else {
                    *((uint32_t*)patchLoc) = (uint32_t)targetVMAddr;
                }

                // FIXME: When we support a baseKC, pageableKC, and auxKC, the supervtable cache level
                // may no longer be correct here as we may be:
                // - patching a vtable in auxKC
                // - where the supervtable is in pageableKC
                // - but the entry slot points to baseKC
                aslrTracker.add((void*)patchLoc, superVTableEntry.targetCacheLevel);

                // Add pointer auth if the super vtable had it
                if ( superVTableEntry.hasPointerAuth )
                    aslrTracker.setAuthData((void*)patchLoc, superVTableEntry.diversity,
                                            superVTableEntry.hasAddrDiv, superVTableEntry.key);

                // Update this vtable entry in case there are any subclasses which then need to use it
                // to be patched themselves
                vtableEntry.targetVMAddr        = superVTableEntry.targetVMAddr;
                vtableEntry.targetCacheLevel    = superVTableEntry.targetCacheLevel;
                vtableEntry.diversity           = superVTableEntry.diversity;
                vtableEntry.hasAddrDiv          = superVTableEntry.hasAddrDiv;
                vtableEntry.key                 = superVTableEntry.key;
                vtableEntry.hasPointerAuth      = superVTableEntry.hasPointerAuth;

                missingBindLocations.erase(patchLoc);
            }
        }
    }
}

typedef std::pair<uint8_t, uint64_t> CacheOffset;

struct DylibSymbolLocation {
    const DylibSymbols* dylibSymbols;
    uint64_t            symbolVMAddr;
    bool                isKPI;
};

struct DylibFixups {
    void processFixups(const std::map<std::string, DylibSymbols>& dylibsToSymbols,
                       const std::unordered_map<std::string_view, std::vector<DylibSymbolLocation>>& symbolMap,
                       const std::string& kernelID, const CacheBuilder::ASLR_Tracker& aslrTracker);

    // Inputs
    const dyld3::MachOAnalyzer*     ma              = nullptr;
    DylibSymbols&                   dylibSymbols;
    Diagnostics&                    dylibDiag;
    const std::vector<std::string>& dependencies;

    // Outputs
    struct AuthData {
        uint16_t    diversity;
        bool        addrDiv;
        uint8_t     key;
    };
    struct BranchStubData {
        CacheOffset targetCacheOffset;
        const void* fixupLoc;
        uint64_t    fixupVMOffset;
    };
    std::unordered_map<const uint8_t*, VTableBindSymbol>        missingBindLocations;
    std::unordered_map<void*, uint8_t>                          fixupLocs;
    std::unordered_map<void*, uint8_t>                          fixupHigh8s;
    std::unordered_map<void*, AuthData>                         fixupAuths;
    std::vector<BranchStubData>                                 branchStubs;
};

void DylibFixups::processFixups(const std::map<std::string, DylibSymbols>& dylibsToSymbols,
                                const std::unordered_map<std::string_view, std::vector<DylibSymbolLocation>>& symbolMap,
                                const std::string& kernelID, const CacheBuilder::ASLR_Tracker& aslrTracker) {
    auto& resolvedBindLocations = dylibSymbols.resolvedBindLocations;
    const std::string& dylibID = dylibSymbols.dylibName;

    const bool _is64 = true;
    const bool isThirdPartyKext = (dylibID.find("com.apple") != 0);

    // The magic symbol for missing weak imports
    const char* missingWeakImportSymbolName = "_gOSKextUnresolved";

    struct SymbolDefinition {
        uint64_t    symbolVMAddr;
        uint32_t    kernelCollectionLevel;
    };
    auto findDependencyWithSymbol = [&symbolMap, &isThirdPartyKext](const char* symbolName,
                                                 const std::vector<std::string>& dependencies) {
        auto symbolMapIt = symbolMap.find(symbolName);
        if ( symbolMapIt == symbolMap.end() )
            return (SymbolDefinition){ ~0ULL, 0 };
        // Find the first dependency in the list
        const std::vector<DylibSymbolLocation>& dylibSymbols = symbolMapIt->second;
        // The massively common case is 1 or 2 definitions of a given symbol, so a basic searhc should be
        // fine
        for (const std::string& dependency : dependencies) {
            for (const DylibSymbolLocation& dylibSymbol : dylibSymbols) {
                if ( dependency == dylibSymbol.dylibSymbols->dylibName ) {
                    // If the Apple kext we are linking has a symbol set, and the user is a third-party kext,
                    // then only allow the  third party kext to see symbols in the kext export list, if it has one
                    const bool isAppleKext = (dependency.find("com.apple") == 0);
                    if ( isThirdPartyKext && isAppleKext && !dylibSymbol.isKPI )
                        continue;
                    return (SymbolDefinition){ dylibSymbol.symbolVMAddr, dylibSymbol.dylibSymbols->dylibLevel };
                }
            }
        }
        return (SymbolDefinition){ ~0ULL, 0 };
    };

    if (ma->hasChainedFixups()) {
        // build array of targets
        struct BindTarget {
            const VTableBindSymbol              bindSymbol;
            uint64_t                            vmAddr;
            uint32_t                            dylibLevel;
            bool                                isMissingWeakImport;
            bool                                isMissingSymbol;
        };
        __block std::vector<BindTarget> bindTargets;
        __block bool foundMissingWeakImport = false;
        ma->forEachChainedFixupTarget(dylibDiag, ^(int libOrdinal, const char* symbolName, uint64_t addend,
                                                   bool weakImport, bool& stop) {
            if ( (libOrdinal != BIND_SPECIAL_DYLIB_FLAT_LOOKUP) && (libOrdinal != BIND_SPECIAL_DYLIB_WEAK_LOOKUP) ) {
                dylibDiag.error("All chained binds should be flat namespace or weak lookups");
                stop = true;
                return;
            }

            if ( addend != 0 ) {
                dylibDiag.error("Chained bind addends are not supported right now");
                stop = true;
                return;
            }

            VTableBindSymbol bindSymbol = { dylibID, symbolName };
            bool isMissingSymbol = false;

            for (const std::string& dependencyID : dependencies) {
                auto depIt = dylibsToSymbols.find(dependencyID);
                if (depIt == dylibsToSymbols.end()) {
                    dylibDiag.error("Failed to bind '%s' as could not find a kext with '%s' bundle-id",
                                       symbolName, dependencyID.c_str());
                    stop = true;
                    return;
                }

                const DylibSymbols& dylibSymbols = depIt->second;
                auto exportIt = dylibSymbols.globals.find(symbolName);
                if ( exportIt == dylibSymbols.globals.end() )
                    continue;

                isMissingSymbol = false;
                bindTargets.push_back({ bindSymbol, exportIt->second, dylibSymbols.dylibLevel, false, isMissingSymbol });
                return;
            }

            // If the symbol is weak, and we didn't find it in our listed
            // dependencies, then use our own definition
            if ( libOrdinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP ) {
                auto dylibIt = dylibsToSymbols.find(dylibID);
                if (dylibIt == dylibsToSymbols.end()) {
                    dylibDiag.error("Failed to bind weak '%s' as could not find a define in self",
                                    symbolName);
                    stop = true;
                    return;
                }
                const DylibSymbols& dylibSymbols = dylibIt->second;
                auto exportIt = dylibSymbols.globals.find(symbolName);
                if ( exportIt != dylibSymbols.globals.end() ) {
                    isMissingSymbol = false;
                    bindTargets.push_back({ bindSymbol, exportIt->second, dylibSymbols.dylibLevel, false, isMissingSymbol });
                    return;
                }
            }

            if ( weakImport ) {
                // Find _gOSKextUnresolved in the kernel
                // Weak imports are not compared against null, but instead against the address of that symbol
                auto kernelSymbolsIt = dylibsToSymbols.find(kernelID);
                assert(kernelSymbolsIt != dylibsToSymbols.end());
                const DylibSymbols& kernelSymbols = kernelSymbolsIt->second;
                auto exportIt = kernelSymbols.globals.find(missingWeakImportSymbolName);
                if (exportIt != kernelSymbols.globals.end()) {
                    foundMissingWeakImport = true;
                    isMissingSymbol = false;
                    bindTargets.push_back({ bindSymbol, exportIt->second, kernelSymbols.dylibLevel, true, isMissingSymbol });
                    return;
                }
                dylibDiag.error("Weak bind symbol '%s' not found in kernel", missingWeakImportSymbolName);
                return;
            }

            // Store missing binds for later.  They may be fixed by vtable patching
            isMissingSymbol = true;
            bindTargets.push_back({ bindSymbol, 0, 0, false, isMissingSymbol });
        });
        if ( dylibDiag.hasError() )
            return;

        if( foundMissingWeakImport ) {
            // If we found a missing weak import, then we need to check that the user did
            // something like "if ( &foo == &gOSKextUnresolved )"
            // If they didn't use gOSKextUnresolved at all, then there's no way they could be doing that check
            auto kernelSymbolsIt = dylibsToSymbols.find(kernelID);
            assert(kernelSymbolsIt != dylibsToSymbols.end());
            const DylibSymbols& kernelSymbols = kernelSymbolsIt->second;
            auto exportIt = kernelSymbols.globals.find(missingWeakImportSymbolName);
            assert(exportIt != kernelSymbols.globals.end());
            bool foundUseOfMagicSymbol = false;
            for (const BindTarget& bindTarget : bindTargets) {
                // Skip the missing weak imports
                if ( bindTarget.isMissingWeakImport || bindTarget.isMissingSymbol )
                    continue;
                // Skip anything which isn't the symbol we are looking for
                if ( (bindTarget.dylibLevel != 0) && (bindTarget.vmAddr != exportIt->second) )
                    continue;
                foundUseOfMagicSymbol = true;
                break;
            }

            if ( !foundUseOfMagicSymbol ) {
                dylibDiag.error("Has weak references but does not test for them.  "
                                "Test for weak references with OSKextSymbolIsResolved().");
                return;
            }
        }

        // uint64_t baseAddress = ma->preferredLoadAddress();

        ma->withChainStarts(dylibDiag, 0, ^(const dyld_chained_starts_in_image* starts) {
            ma->forEachFixupInAllChains(dylibDiag, starts, false, ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
                switch (segInfo->pointer_format) {
                    case DYLD_CHAINED_PTR_64_OFFSET:
                        if ( fixupLoc->generic64.bind.bind ) {
                            uint64_t bindOrdinal = fixupLoc->generic64.bind.ordinal;
                            if ( bindOrdinal >= bindTargets.size() ) {
                                dylibDiag.error("Bind ordinal %lld out of range %lu", bindOrdinal, bindTargets.size());
                                stop = true;
                                return;
                            }

                            const BindTarget& bindTarget = bindTargets[bindOrdinal];
                            if ( bindTarget.isMissingSymbol ) {
                                // Track this missing bind for later
                                // For now we bind it to null and don't slide it.
                                fixupLoc->raw64 = 0;
                                missingBindLocations[(const uint8_t*)fixupLoc] = bindTarget.bindSymbol;
                            } else {
                                fixupLoc->raw64 = bindTarget.vmAddr;
                                fixupLocs[fixupLoc] = bindTarget.dylibLevel;
                                resolvedBindLocations[(const uint8_t*)fixupLoc] = bindTarget.bindSymbol;
                            }
                        }
                        else {
                            // convert rebase chain entry to raw pointer to target vmaddr
                            uint64_t targetVMAddr = fixupLoc->generic64.rebase.target;
                            uint64_t sideTableAddr = 0;
                            if ( aslrTracker.hasRebaseTarget64(fixupLoc, &sideTableAddr) )
                                targetVMAddr = sideTableAddr;
                            // store high8 in side table
                            if ( fixupLoc->generic64.rebase.high8 )
                                fixupHigh8s[fixupLoc] = fixupLoc->generic64.rebase.high8;
                            fixupLoc->raw64 = targetVMAddr;
                        }
                        break;
                    case DYLD_CHAINED_PTR_ARM64E_KERNEL:
                        if ( fixupLoc->arm64e.bind.bind ) {
                            uint64_t bindOrdinal = fixupLoc->arm64e.bind.ordinal;
                            if ( bindOrdinal >= bindTargets.size() ) {
                                dylibDiag.error("Bind ordinal %lld out of range %lu", bindOrdinal, bindTargets.size());
                                stop = true;
                                return;
                            }

                            const BindTarget& bindTarget = bindTargets[bindOrdinal];
                            uint64_t targetVMAddr = bindTarget.vmAddr;

                            if ( fixupLoc->arm64e.authBind.auth ) {
                                // store auth data in side table
                                fixupAuths[fixupLoc] = {
                                    (uint16_t)fixupLoc->arm64e.authBind.diversity,
                                    (bool)fixupLoc->arm64e.authBind.addrDiv,
                                    (uint8_t)fixupLoc->arm64e.authBind.key
                                };
                            }
                            else {
                                // plain binds can have addend in chain
                                targetVMAddr += fixupLoc->arm64e.bind.addend;
                            }
                            // change location from a chain ptr into a raw pointer to the target vmaddr
                            if ( bindTarget.isMissingSymbol ) {
                                // Track this missing bind for later
                                // For now we bind it to null and don't slide it.
                                fixupLoc->raw64 = 0;
                                missingBindLocations[(const uint8_t*)fixupLoc] = bindTarget.bindSymbol;
                            } else {
                                fixupLoc->raw64 = targetVMAddr;
                                fixupLocs[fixupLoc] = bindTarget.dylibLevel;
                                resolvedBindLocations[(const uint8_t*)fixupLoc] = bindTarget.bindSymbol;
                            }
                        }
                        else {
                            // convert rebase chain entry to raw pointer to target vmaddr
                            if ( fixupLoc->arm64e.rebase.auth ) {
                                // store auth data in side table
                                fixupAuths[fixupLoc] = {
                                    (uint16_t)fixupLoc->arm64e.authRebase.diversity,
                                    (bool)fixupLoc->arm64e.authRebase.addrDiv,
                                    (uint8_t)fixupLoc->arm64e.authRebase.key
                                };
                                uint64_t targetVMAddr = fixupLoc->arm64e.authRebase.target;
                                fixupLoc->raw64 = targetVMAddr;
                            }
                            else {
                                uint64_t targetVMAddr = fixupLoc->arm64e.rebase.target;
                                uint64_t sideTableAddr;
                                if ( aslrTracker.hasRebaseTarget64(fixupLoc, &sideTableAddr) )
                                    targetVMAddr = sideTableAddr;
                                // store high8 in side table
                                if ( fixupLoc->arm64e.rebase.high8 )
                                    fixupHigh8s[fixupLoc] = fixupLoc->arm64e.rebase.high8;
                                fixupLoc->raw64 = targetVMAddr;
                            }
                        }
                        break;
                    default:
                        fprintf(stderr, "unknown pointer type %d\n", segInfo->pointer_format);
                        break;
                }
             });
        });
        return;
    }

    // If we have any missing imports, then they should check for the kernel symbol
    // Grab a hold of that now if it exists so we can check it later
    __block bool foundUseOfMagicSymbol = false;
    __block bool foundMissingWeakImport = false;

    const uint64_t loadAddress = ma->preferredLoadAddress();
    ma->forEachBind(dylibDiag, ^(uint64_t runtimeOffset, int libOrdinal, uint8_t bindType,
                                 const char *symbolName, bool weakImport, bool lazyBind, uint64_t addend, bool &stop) {
        // printf("Bind at 0x%llx to '%s'\n", runtimeOffset, symbolName);
        // Kext binds are a flat namespace so walk until we find the symbol we need
        bool foundSymbol = false;
        VTableBindSymbol bindSymbol = { dylibID, symbolName };
        if (SymbolDefinition symbolDef = findDependencyWithSymbol(symbolName, dependencies); symbolDef.symbolVMAddr != ~0ULL) {
            // Set the bind to the target address since we found it
            uint8_t* fixupLoc = (uint8_t*)ma+runtimeOffset;
            if ( bindType == BIND_TYPE_POINTER ) {
                if ( _is64 )
                    *((uint64_t*)fixupLoc) = symbolDef.symbolVMAddr;
                else
                    *((uint32_t*)fixupLoc) = (uint32_t)symbolDef.symbolVMAddr;

                // Only track regular fixups for ASLR, not branch fixups
                fixupLocs[fixupLoc] = symbolDef.kernelCollectionLevel;
                resolvedBindLocations[(const uint8_t*)fixupLoc] = bindSymbol;
            } else if ( bindType == BIND_TYPE_TEXT_PCREL32 ) {
                // The value to store is the difference between the bind target
                // and the value of the PC after this instruction
                uint64_t targetAddress = 0;
                if ( dylibSymbols.dylibLevel != symbolDef.kernelCollectionLevel ) {
                    // Record this for later as we want to create stubs serially
                    CacheOffset targetCacheOffset = { symbolDef.kernelCollectionLevel, symbolDef.symbolVMAddr };
                    branchStubs.emplace_back((BranchStubData){
                        .targetCacheOffset = targetCacheOffset,
                        .fixupLoc = fixupLoc,
                        .fixupVMOffset = runtimeOffset
                    });
                } else {
                    targetAddress = symbolDef.symbolVMAddr;
                    uint64_t diffValue = targetAddress - (loadAddress + runtimeOffset + 4);
                    *((uint32_t*)fixupLoc) = (uint32_t)diffValue;
                }
            } else {
                dylibDiag.error("Unexpected bind type: %d", bindType);
                stop = true;
                return;
            }

            foundSymbol = true;
        }

        if ( foundSymbol && !foundUseOfMagicSymbol ) {
            foundUseOfMagicSymbol = (strcmp(symbolName, missingWeakImportSymbolName) == 0);
        }

        if (!foundSymbol) {
            for (const std::string& dependencyID : dependencies) {
                auto depIt = dylibsToSymbols.find(dependencyID);
                if (depIt == dylibsToSymbols.end()) {
                    dylibDiag.error("Failed to bind '%s' as could not find a kext with '%s' bundle-id",
                                    symbolName, dependencyID.c_str());
                    stop = true;
                    return;
                }

                const DylibSymbols& dylibSymbols = depIt->second;
                auto exportIt = dylibSymbols.globals.find(symbolName);
                if ( exportIt == dylibSymbols.globals.end() )
                    continue;
                findDependencyWithSymbol(symbolName, dependencies);
                break;
            }
        }

        // If the symbol is weak, and we didn't find it in our listed
        // dependencies, then use our own definition
        if ( !foundSymbol && (libOrdinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP) ) {
            auto dylibIt = dylibsToSymbols.find(dylibID);
            if (dylibIt == dylibsToSymbols.end()) {
                dylibDiag.error("Failed to bind weak '%s' as could not find a define in self",
                                symbolName);
                stop = true;
                return;
            }

            const DylibSymbols& dylibSymbols = dylibIt->second;
            auto exportIt = dylibSymbols.globals.find(symbolName);
            if ( exportIt != dylibSymbols.globals.end() ) {
                // Set the bind to the target address since we found it
                uint8_t* fixupLoc = (uint8_t*)ma+runtimeOffset;
                if ( bindType == BIND_TYPE_POINTER ) {
                    if ( _is64 )
                        *((uint64_t*)fixupLoc) = exportIt->second;
                    else
                        *((uint32_t*)fixupLoc) = (uint32_t)exportIt->second;

                    // Only track regular fixups for ASLR, not branch fixups
                    fixupLocs[fixupLoc] = dylibSymbols.dylibLevel;
                    resolvedBindLocations[(const uint8_t*)fixupLoc] = bindSymbol;
                } else if ( bindType == BIND_TYPE_TEXT_PCREL32 ) {
                    // We should never have a branch to a weak bind as we should have had a GOT for these
                    dylibDiag.error("Unexpected weak bind type: %d", bindType);
                    stop = true;
                    return;
                } else {
                    dylibDiag.error("Unexpected bind type: %d", bindType);
                    stop = true;
                    return;
                }

                foundSymbol = true;
            }
        }

        if ( !foundSymbol && weakImport ) {
            if ( bindType != BIND_TYPE_POINTER ) {
                dylibDiag.error("Unexpected bind type: %d", bindType);
                stop = true;
                return;
            }
            // Find _gOSKextUnresolved in the kernel
            // Weak imports are not compared against null, but instead against the address of that symbol
            auto kernelSymbolsIt = dylibsToSymbols.find(kernelID);
            assert(kernelSymbolsIt != dylibsToSymbols.end());
            const DylibSymbols& kernelSymbols = kernelSymbolsIt->second;
            auto exportIt = kernelSymbols.globals.find(missingWeakImportSymbolName);
            if (exportIt != kernelSymbols.globals.end()) {
                foundMissingWeakImport = true;

                uint8_t* fixupLoc = (uint8_t*)ma+runtimeOffset;
                if ( _is64 )
                    *((uint64_t*)fixupLoc) = exportIt->second;
                else
                    *((uint32_t*)fixupLoc) = (uint32_t)exportIt->second;

                // Only track regular fixups for ASLR, not branch fixups
                fixupLocs[fixupLoc] = kernelSymbols.dylibLevel;
                return;
            }
            dylibDiag.error("Weak bind symbol '%s' not found in kernel", missingWeakImportSymbolName);
            return;
        }

        if ( !foundSymbol ) {
            // Store missing binds for later.  They may be fixed by vtable patching
            const uint8_t* fixupLoc = (uint8_t*)ma+runtimeOffset;
            missingBindLocations[fixupLoc] = bindSymbol;
        }
    }, ^(const char *symbolName) {
        dylibDiag.error("Strong binds are not supported right now");
    });

    if ( foundMissingWeakImport && !foundUseOfMagicSymbol ) {
        dylibDiag.error("Has weak references but does not test for them.  "
                        "Test for weak references with OSKextSymbolIsResolved().");
        return;
    }

    ma->forEachRebase(dylibDiag, false, ^(uint64_t runtimeOffset, bool &stop) {
        uint8_t* fixupLoc = (uint8_t*)ma+runtimeOffset;
        fixupLocs[fixupLoc] = (uint8_t)~0U;
    });
}

// A helper to automatically call CFRelease when we go out of scope
struct AutoReleaseTypeRef {
    AutoReleaseTypeRef() = default;
    ~AutoReleaseTypeRef() {
        if ( ref != nullptr ) {
            CFRelease(ref);
        }
    }
    void setRef(CFTypeRef typeRef) {
        assert(ref == nullptr);
        ref = typeRef;
    }

    CFTypeRef ref = nullptr;
};

static std::unique_ptr<std::unordered_set<std::string>> getKPI(Diagnostics& diags, const dyld3::MachOAnalyzer* ma,
                                                               std::string_view dylibID) {
    bool isAppleKext = (dylibID.find("com.apple") == 0);
    if ( !isAppleKext )
        return {};

    __block std::list<std::string> nonASCIIStrings;
    auto getString = ^(Diagnostics& diags, CFStringRef symbolNameRef) {
        const char* symbolName = CFStringGetCStringPtr(symbolNameRef, kCFStringEncodingUTF8);
        if ( symbolName != nullptr )
            return symbolName;

        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(symbolNameRef), kCFStringEncodingUTF8);
        char buffer[len + 1];
        if ( !CFStringGetCString(symbolNameRef, buffer, len, kCFStringEncodingUTF8) ) {
            diags.error("Could not convert string to ASCII");
            return (const char*)nullptr;
        }
        buffer[len] = '\0';
        nonASCIIStrings.push_back(buffer);
        return nonASCIIStrings.back().c_str();
    };

    uint64_t symbolSetsSize = 0;
    const void* symbolSetsContent = ma->findSectionContent("__LINKINFO", "__symbolsets", symbolSetsSize);
    if ( symbolSetsContent == nullptr )
        return {};

    AutoReleaseTypeRef dataRefReleaser;
    AutoReleaseTypeRef plistRefReleaser;

    std::unordered_set<std::string> symbols;
    CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)symbolSetsContent, symbolSetsSize, kCFAllocatorNull);
    if ( dataRef == nullptr ) {
        diags.error("Could not create data ref for kpi");
        return {};
    }
    dataRefReleaser.setRef(dataRef);

    CFErrorRef errorRef = nullptr;
    CFPropertyListRef plistRef = CFPropertyListCreateWithData(kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nullptr, &errorRef);
    if (errorRef != nullptr) {
        CFStringRef errorString = CFErrorCopyDescription(errorRef);
        diags.error("Could not load plist because :%s", CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
        CFRelease(errorRef);
        return {};
    }
    if ( plistRef == nullptr ) {
        diags.error("Could not create plist ref for kpi");
        return {};
    }
    plistRefReleaser.setRef(plistRef);

    if ( CFGetTypeID(plistRef) != CFDictionaryGetTypeID() ) {
        diags.error("kpi plist should be a dictionary");
        return {};
    }

    CFDictionaryRef symbolSetsDictRef = (CFDictionaryRef)plistRef;

    // CFBundleIdentifier
    CFStringRef bundleIDRef = (CFStringRef)CFDictionaryGetValue(symbolSetsDictRef, CFSTR("CFBundleIdentifier"));
    if ( (bundleIDRef == nullptr) || (CFGetTypeID(bundleIDRef) != CFStringGetTypeID()) ) {
        diags.error("kpi bundle ID should be a string");
        return {};
    }

    const char* bundleID = getString(diags, bundleIDRef);
    if ( bundleID == nullptr )
        return {};

    if ( dylibID != bundleID ) {
        diags.error("kpi bundle ID doesn't match kext");
        return {};
    }

    CFArrayRef symbolsArrayRef = (CFArrayRef)CFDictionaryGetValue(symbolSetsDictRef, CFSTR("Symbols"));
    if ( symbolsArrayRef != nullptr ) {
        if ( CFGetTypeID(symbolsArrayRef) != CFArrayGetTypeID() ) {
            diags.error("Symbols value should be an array");
            return {};
        }
        for (CFIndex symbolSetIndex = 0; symbolSetIndex != CFArrayGetCount(symbolsArrayRef); ++symbolSetIndex) {
            CFStringRef symbolNameRef = (CFStringRef)CFArrayGetValueAtIndex(symbolsArrayRef, symbolSetIndex);
            if ( (symbolNameRef == nullptr) || (CFGetTypeID(symbolNameRef) != CFStringGetTypeID()) ) {
                diags.error("Symbol name should be a string");
                return {};
            }

            const char* symbolName = getString(diags, symbolNameRef);
            if ( symbolName == nullptr )
                return {};
            symbols.insert(symbolName);
        }
    }

    return std::make_unique<std::unordered_set<std::string>>(std::move(symbols));
}

void AppCacheBuilder::processFixups()
{
    auto dylibsToSymbolsOwner = std::make_unique<std::map<std::string, DylibSymbols>>();
    std::map<std::string, DylibSymbols>& dylibsToSymbols = *dylibsToSymbolsOwner.get();

    auto vtablePatcherOwner = std::make_unique<VTablePatcher>(numFixupLevels);
    VTablePatcher& vtablePatcher = *vtablePatcherOwner.get();

    const uint32_t kernelLevel = 0;
    uint8_t currentLevel = getCurrentFixupLevel();

    // Keep track of missing binds until later.  They may be "resolved" by vtable patching
    std::map<const uint8_t*, const VTableBindSymbol> missingBindLocations;

    __block std::string kernelID;
    __block const dyld3::MachOAnalyzer* kernelMA = nullptr;
    if ( appCacheOptions.cacheKind == Options::AppCacheKind::kernel ) {
        kernelMA = getKernelStaticExecutableFromCache();
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                            DylibStripMode stripMode, const std::vector<std::string> &dependencies,
                            Diagnostics& dylibDiag,
                            bool &stop) {
            if ( ma == kernelMA ) {
                kernelID = dylibID;
                stop = true;
            }
        });
        assert(!kernelID.empty());
    } else {
        assert(existingKernelCollection != nullptr);
        existingKernelCollection->forEachDylib(_diagnostics, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
            if ( ma->isStaticExecutable() ) {
                kernelMA = ma;
                kernelID = name;
            }
        });
        if ( kernelMA == nullptr ) {
            _diagnostics.error("Could not find kernel in kernel collection");
            return;
        }
    }

    auto getGlobals = [](Diagnostics& diags, const dyld3::MachOAnalyzer *ma) -> std::map<std::string_view, uint64_t> {
        // Note we don't put __block on the variable directly as then it gets copied in to the return value
        std::map<std::string_view, uint64_t> exports;
        __block std::map<std::string_view, uint64_t>& exportsRef = exports;
        ma->forEachGlobalSymbol(diags, ^(const char *symbolName, uint64_t n_value,
                                         uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
            exportsRef[symbolName] = n_value;
        });
        return exports;
    };

    auto getLocals = [](Diagnostics& diags, const dyld3::MachOAnalyzer *ma) -> std::map<std::string_view, uint64_t> {
        // Note we don't put __block on the variable directly as then it gets copied in to the return value
        std::map<std::string_view, uint64_t> exports;
        __block std::map<std::string_view, uint64_t>& exportsRef = exports;
        ma->forEachLocalSymbol(diags, ^(const char *symbolName, uint64_t n_value,
                                         uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
            exportsRef[symbolName] = n_value;
        });
        return exports;
    };

    dylibsToSymbols[kernelID] = {
        getGlobals(_diagnostics, kernelMA),
        getLocals(_diagnostics, kernelMA),
        nullptr,
        kernelLevel,
        std::string(kernelID)
    };

    // Add all the codeless kext's as kext's can list them as dependencies
    // Note we add placeholders here which can be legitimately replaced by symbol sets
    for (const InputDylib& dylib : codelessKexts) {
        dylibsToSymbols[dylib.dylibID] = { };
    }

    // Similarly, add placeholders for codeless kexts in the baseKC
    if ( existingKernelCollection != nullptr ) {
        existingKernelCollection->forEachPrelinkInfoLibrary(_diagnostics,
                                                            ^(const char *bundleName, const char* relativePath,
                                                              const std::vector<const char *> &deps) {
            dylibsToSymbols[bundleName] = { };
        });
    }

    // And placeholders for codeless kexts in the pageableKC
    if ( pageableKernelCollection != nullptr ) {
        pageableKernelCollection->forEachPrelinkInfoLibrary(_diagnostics,
                                                            ^(const char *bundleName, const char* relativePath,
                                                              const std::vector<const char *> &deps) {
            dylibsToSymbols[bundleName] = { };
        });
    }

    // Get the symbol sets
    AutoReleaseTypeRef dataRefReleaser;
    AutoReleaseTypeRef plistRefReleaser;

    __block std::list<std::string> nonASCIIStrings;
    auto getString = ^(Diagnostics& diags, CFStringRef symbolNameRef) {
        const char* symbolName = CFStringGetCStringPtr(symbolNameRef, kCFStringEncodingUTF8);
        if ( symbolName != nullptr )
            return symbolName;

        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(symbolNameRef), kCFStringEncodingUTF8);
        char buffer[len + 1];
        if ( !CFStringGetCString(symbolNameRef, buffer, len, kCFStringEncodingUTF8) ) {
            diags.error("Could not convert string to ASCII");
            return (const char*)nullptr;
        }
        buffer[len] = '\0';
        nonASCIIStrings.push_back(buffer);
        return nonASCIIStrings.back().c_str();
    };

    uint64_t symbolSetsSize = 0;
    const void* symbolSetsContent = kernelMA->findSectionContent("__LINKINFO", "__symbolsets", symbolSetsSize);
    if ( symbolSetsContent != nullptr ) {
        const DylibSymbols& kernelSymbols = dylibsToSymbols[kernelID];

        CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const uint8_t*)symbolSetsContent, symbolSetsSize, kCFAllocatorNull);
        if ( dataRef == nullptr ) {
            _diagnostics.error("Could not create data ref for symbol sets");
            return;
        }
        dataRefReleaser.setRef(dataRef);

        CFErrorRef errorRef = nullptr;
        CFPropertyListRef plistRef = CFPropertyListCreateWithData(kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nullptr, &errorRef);
        if (errorRef != nullptr) {
            CFStringRef errorString = CFErrorCopyDescription(errorRef);
            _diagnostics.error("Could not load plist because :%s",
                               CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
            CFRelease(errorRef);
            return;
        }
        if ( plistRef == nullptr ) {
            _diagnostics.error("Could not create plist ref for symbol sets");
            return;
        }
        plistRefReleaser.setRef(plistRef);

        if ( CFGetTypeID(plistRef) != CFDictionaryGetTypeID() ) {
            _diagnostics.error("Symbol set plist should be a dictionary");
            return;
        }
        CFDictionaryRef symbolSetsDictRef = (CFDictionaryRef)plistRef;
        CFArrayRef symbolSetArrayRef = (CFArrayRef)CFDictionaryGetValue(symbolSetsDictRef, CFSTR("SymbolsSets"));
        if ( symbolSetArrayRef != nullptr ) {
            if ( CFGetTypeID(symbolSetArrayRef) != CFArrayGetTypeID() ) {
                _diagnostics.error("SymbolsSets value should be an array");
                return;
            }
            for (CFIndex symbolSetIndex = 0; symbolSetIndex != CFArrayGetCount(symbolSetArrayRef); ++symbolSetIndex) {
                CFDictionaryRef symbolSetDictRef = (CFDictionaryRef)CFArrayGetValueAtIndex(symbolSetArrayRef, symbolSetIndex);
                if ( CFGetTypeID(symbolSetDictRef) != CFDictionaryGetTypeID() ) {
                    _diagnostics.error("Symbol set element should be a dictionary");
                    return;
                }

                // CFBundleIdentifier
                CFStringRef bundleIDRef = (CFStringRef)CFDictionaryGetValue(symbolSetDictRef, CFSTR("CFBundleIdentifier"));
                if ( (bundleIDRef == nullptr) || (CFGetTypeID(bundleIDRef) != CFStringGetTypeID()) ) {
                    _diagnostics.error("Symbol set bundle ID should be a string");
                    return;
                }

                // Symbols
                CFArrayRef symbolsArrayRef = (CFArrayRef)CFDictionaryGetValue(symbolSetDictRef, CFSTR("Symbols"));
                if ( (symbolsArrayRef == nullptr) || (CFGetTypeID(symbolsArrayRef) != CFArrayGetTypeID()) ) {
                    _diagnostics.error("Symbol set symbols should be an array");
                    return;
                }

                std::map<std::string_view, uint64_t> symbolSetGlobals;
                std::map<std::string_view, uint64_t> symbolSetLocals;
                for (CFIndex symbolIndex = 0; symbolIndex != CFArrayGetCount(symbolsArrayRef); ++symbolIndex) {
                    CFDictionaryRef symbolDictRef = (CFDictionaryRef)CFArrayGetValueAtIndex(symbolsArrayRef, symbolIndex);
                    if ( CFGetTypeID(symbolDictRef) != CFDictionaryGetTypeID() ) {
                        _diagnostics.error("Symbols array element should be a dictionary");
                        return;
                    }

                    // SymbolPrefix
                    CFStringRef symbolPrefixRef = (CFStringRef)CFDictionaryGetValue(symbolDictRef, CFSTR("SymbolPrefix"));
                    if ( symbolPrefixRef != nullptr ) {
                        if ( CFGetTypeID(symbolPrefixRef) != CFStringGetTypeID() ) {
                            _diagnostics.error("Symbol prefix should be a string");
                            return;
                        }

                        const char* symbolPrefix = getString(_diagnostics, symbolPrefixRef);
                        if ( symbolPrefix == nullptr )
                            return;
                        size_t symbolPrefixLen = strlen(symbolPrefix);

                        // FIXME: Brute force might not be the best thing here
                        for (std::pair<std::string_view, uint64_t> kernelGlobal : kernelSymbols.globals) {
                            if ( strncmp(kernelGlobal.first.data(), symbolPrefix, symbolPrefixLen) == 0 ) {
                                symbolSetGlobals[kernelGlobal.first] = kernelGlobal.second;
                            }
                        }
                        for (std::pair<std::string_view, uint64_t> kernelLocal : kernelSymbols.locals) {
                            if ( strncmp(kernelLocal.first.data(), symbolPrefix, symbolPrefixLen) == 0 ) {
                                symbolSetLocals[kernelLocal.first] = kernelLocal.second;
                            }
                        }
                        continue;
                    }

                    // SymbolName
                    CFStringRef symbolNameRef = (CFStringRef)CFDictionaryGetValue(symbolDictRef, CFSTR("SymbolName"));
                    if ( (symbolNameRef == nullptr) || (CFGetTypeID(symbolNameRef) != CFStringGetTypeID()) ) {
                        _diagnostics.error("Symbol name should be a string");
                        return;
                    }

                    // AliasTarget [Optional]
                    CFStringRef aliasTargetRef = (CFStringRef)CFDictionaryGetValue(symbolDictRef, CFSTR("AliasTarget"));
                    if ( aliasTargetRef == nullptr ) {
                        // No alias
                        const char* symbolName = getString(_diagnostics, symbolNameRef);
                        if ( symbolName == nullptr )
                            return;

                        // Find the symbol in xnu
                        auto globalIt = kernelSymbols.globals.find(symbolName);
                        if (globalIt != kernelSymbols.globals.end()) {
                            symbolSetGlobals[symbolName] = globalIt->second;
                        }

                        auto localIt = kernelSymbols.locals.find(symbolName);
                        if (localIt != kernelSymbols.locals.end()) {
                            symbolSetLocals[symbolName] = localIt->second;
                        }
                    } else {
                        // We have an alias
                        if ( CFGetTypeID(aliasTargetRef) != CFStringGetTypeID() ) {
                            _diagnostics.error("Alias should be a string");
                            return;
                        }

                        const char* symbolName = getString(_diagnostics, symbolNameRef);
                        if ( symbolName == nullptr )
                            return;
                        const char* aliasTargetName = getString(_diagnostics, aliasTargetRef);
                        if ( aliasTargetName == nullptr )
                            return;

                        // Find the alias symbol in xnu
                        auto globalIt = kernelSymbols.globals.find(aliasTargetName);
                        if (globalIt != kernelSymbols.globals.end()) {
                            symbolSetGlobals[symbolName] = globalIt->second;
                        } else {
                            _diagnostics.error("Alias '%s' not found in kernel", aliasTargetName);
                            return;
                        }

                        auto localIt = kernelSymbols.locals.find(aliasTargetName);
                        if (localIt != kernelSymbols.locals.end()) {
                            symbolSetLocals[symbolName] = localIt->second;
                        } else {
                            // This is not an error, as aliases from symbol sets from the kernel
                            // are only for vtable patching, not general binding
                        }
                    }
                }
                const char* dylibID = getString(_diagnostics, bundleIDRef);
                if ( dylibID == nullptr )
                    return;

                // HACK: kxld aliases __ZN15OSMetaClassBase25_RESERVEDOSMetaClassBase3Ev to __ZN15OSMetaClassBase8DispatchE5IORPC
                auto metaclassHackIt = symbolSetGlobals.find("__ZN15OSMetaClassBase8DispatchE5IORPC");
                if ( metaclassHackIt != symbolSetGlobals.end() )
                    symbolSetGlobals["__ZN15OSMetaClassBase25_RESERVEDOSMetaClassBase3Ev"] = metaclassHackIt->second;
                dylibsToSymbols[dylibID] = {
                    std::move(symbolSetGlobals),
                    std::move(symbolSetLocals),
                    nullptr,
                    kernelLevel,
                    dylibID
                };
            }
        }
    }

    auto processBinary = ^(Diagnostics& dylibDiags, const dyld3::MachOAnalyzer *ma,
                           const std::string& dylibID, uint32_t dylibLevel) {
        // We dont support export trie's for now
        uint32_t unusedExportTrieOffset = 0;
        uint32_t unusedExportTrieSize = 0;
        if (ma->hasExportTrie(unusedExportTrieOffset, unusedExportTrieSize))
            assert(false);

        // Already done the kernel before.
        if ( ma == kernelMA )
            return;

        // Regular kext.
        dylibsToSymbols[dylibID] = {
            getGlobals(dylibDiags, ma),
            getLocals(dylibDiags, ma),
            getKPI(dylibDiags, ma, dylibID),
            dylibLevel,
            dylibID };
    };

    // Process binary symbols in parallel
    {
        struct DylibData {
            const dyld3::MachOAnalyzer*     ma              = nullptr;
            Diagnostics&                    dylibDiag;
            const std::string&              dylibID;
        };

        __block std::vector<DylibData> dylibDatas;
        dylibDatas.reserve(sortedDylibs.size());
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID, DylibStripMode stripMode,
                            const std::vector<std::string> &dependencies, Diagnostics &dylibDiag, bool &stop) {
            // Already done the kernel before.
            if ( ma == kernelMA )
                return;

            // Make space for all the map entries so that we know they are there when we write their values later
            dylibsToSymbols[dylibID] = { };
            dylibDatas.emplace_back((DylibData){ ma, dylibDiag, dylibID });
        });

        dispatch_apply(dylibDatas.size(), DISPATCH_APPLY_AUTO, ^(size_t index) {
            DylibData& dylibData = dylibDatas[index];
            processBinary(dylibData.dylibDiag, dylibData.ma, dylibData.dylibID, currentLevel);
        });
    }

    // Add exports from the kernel collection if we have it
    if ( existingKernelCollection != nullptr ) {
        uint8_t fixupLevel = getFixupLevel(Options::AppCacheKind::kernel);
        existingKernelCollection->forEachDylib(_diagnostics, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
            processBinary(_diagnostics, ma, name, fixupLevel);
        });
    }

    // Add exports from the pageable collection if we have it
    if ( pageableKernelCollection != nullptr ) {
        uint8_t fixupLevel = getFixupLevel(Options::AppCacheKind::pageableKC);
        pageableKernelCollection->forEachDylib(_diagnostics, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
            processBinary(_diagnostics, ma, name, fixupLevel);
        });
    }

    // Map from an offset in to a KC to a synthesized stub which branches to that offset
    struct CacheOffsetHash
    {
        size_t operator() (const CacheOffset& cacheOffset) const
        {
            return std::hash<uint32_t>{}(cacheOffset.first) ^ std::hash<uint64_t>{}(cacheOffset.second);
        }
    };
    std::unordered_map<CacheOffset, uint64_t, CacheOffsetHash> branchStubs;

    // Clear the branch regions sizes so that we fill them up to their buffer sizes as we go
    branchStubsRegion.sizeInUse = 0;
    branchGOTsRegion.sizeInUse = 0;

    {
        // Map from each symbol to the list of dylibs which export it
        auto symbolMapOwner = std::make_unique<std::unordered_map<std::string_view, std::vector<DylibSymbolLocation>>>();
        __block auto& symbolMap = *symbolMapOwner.get();
        for (const auto& dylibNameAndSymbols : dylibsToSymbols) {
            const DylibSymbols& dylibSymbols = dylibNameAndSymbols.second;
            for (const auto& symbolNameAndAddress : dylibSymbols.globals) {
                // By default, everything i KPI, ie, can be linked by third parties.
                // If a symbol is is provided, even an empty one, then it can override this
                bool isKPI = true;
                if ( dylibSymbols.dylibName == "com.apple.kpi.private" ) {
                    // com.apple.kpi.private is always hidden from third parties.  They shouldn't even list it as a dependency
                    isKPI = false;
                } else if ( dylibSymbols.kpiSymbols ) {
                    const std::unordered_set<std::string>* kpiSymbols = dylibSymbols.kpiSymbols.get();
                    if ( kpiSymbols->count(symbolNameAndAddress.first.data()) == 0 )
                        isKPI = false;
                }
                symbolMap[symbolNameAndAddress.first].push_back({ &dylibSymbols, symbolNameAndAddress.second, isKPI });
            }
        }

        auto dylibFixupsOwner = std::make_unique<std::vector<DylibFixups>>();
        __block auto& dylibFixups = *dylibFixupsOwner.get();
        dylibFixups.reserve(sortedDylibs.size());
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID, DylibStripMode stripMode,
                            const std::vector<std::string> &dependencies, Diagnostics &dylibDiag, bool &stop) {

            auto dylibSymbolsIt = dylibsToSymbols.find(dylibID);
            assert(dylibSymbolsIt != dylibsToSymbols.end());

            dylibFixups.emplace_back((DylibFixups){
                .ma = ma,
                .dylibSymbols = dylibSymbolsIt->second,
                .dylibDiag = dylibDiag,
                .dependencies = dependencies
            });
        });

        dispatch_apply(dylibFixups.size(), DISPATCH_APPLY_AUTO, ^(size_t index) {
            DylibFixups& dylibFixup = dylibFixups[index];
            dylibFixup.processFixups(dylibsToSymbols, symbolMap, kernelID, _aslrTracker);
        });

        // Merge all the dylib results in serial
        for (DylibFixups& dylibFixup : dylibFixups) {
            // Skip bad dylibs
            if ( dylibFixup.dylibDiag.hasError() ) {
                if ( !_diagnostics.hasError() ) {
                    _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
                }
                return;
            }

            if ( !dylibFixup.missingBindLocations.empty() ) {
                missingBindLocations.insert(dylibFixup.missingBindLocations.begin(),
                                            dylibFixup.missingBindLocations.end());
            }

            if ( !dylibFixup.fixupLocs.empty() ) {
                for (auto fixupLocAndLevel : dylibFixup.fixupLocs) {
                    _aslrTracker.add(fixupLocAndLevel.first, fixupLocAndLevel.second);
                }
            }

            if ( !dylibFixup.fixupHigh8s.empty() ) {
                for (auto fixupLocAndHigh8 : dylibFixup.fixupHigh8s) {
                    _aslrTracker.setHigh8(fixupLocAndHigh8.first, fixupLocAndHigh8.second);
                }
            }

            if ( !dylibFixup.fixupAuths.empty() ) {
                for (auto fixupLocAndAuth : dylibFixup.fixupAuths) {
                    _aslrTracker.setAuthData(fixupLocAndAuth.first, fixupLocAndAuth.second.diversity,
                                             fixupLocAndAuth.second.addrDiv, fixupLocAndAuth.second.key);
                }
            }

            // Emit branch stubs
            const uint64_t loadAddress = dylibFixup.ma->preferredLoadAddress();
            for (const DylibFixups::BranchStubData& branchData : dylibFixup.branchStubs) {
                // Branching from the auxKC to baseKC.  ld64 doesn't emit a stub in x86_64 kexts
                // so we need to synthesize one now
                uint64_t targetAddress = 0;
                const CacheOffset& targetCacheOffset = branchData.targetCacheOffset;
                auto itAndInserted = branchStubs.insert({ targetCacheOffset, 0 });
                if ( itAndInserted.second ) {
                    // We inserted the branch location, so we need to create new stubs and GOTs
                    if ( branchStubsRegion.sizeInUse == branchStubsRegion.bufferSize ) {
                        _diagnostics.error("Overflow in branch stubs region");
                        return;
                    }
                    if ( branchGOTsRegion.sizeInUse == branchGOTsRegion.bufferSize ) {
                        _diagnostics.error("Overflow in branch GOTs region");
                        return;
                    }
                    uint64_t stubAddress = branchStubsRegion.unslidLoadAddress + branchStubsRegion.sizeInUse;
                    uint8_t* stubBuffer = branchStubsRegion.buffer + branchStubsRegion.sizeInUse;
                    uint64_t gotAddress = branchGOTsRegion.unslidLoadAddress + branchGOTsRegion.sizeInUse;
                    uint8_t* gotBuffer = branchGOTsRegion.buffer + branchGOTsRegion.sizeInUse;

                    // Write the stub
                    // ff 25 aa bb cc dd    jmpq    *0xddccbbaa(%rip)
                    uint64_t diffValue = gotAddress - (stubAddress + 6);
                    stubBuffer[0] = 0xFF;
                    stubBuffer[1] = 0x25;
                    memcpy(&stubBuffer[2], &diffValue, sizeof(uint32_t));

                    // And write the GOT
                    uint8_t symbolCacheLevel = targetCacheOffset.first;
                    uint64_t symbolVMAddr = targetCacheOffset.second;
                    if ( _is64 )
                        *((uint64_t*)gotBuffer) = symbolVMAddr;
                    else
                        *((uint32_t*)gotBuffer) = (uint32_t)symbolVMAddr;
                    _aslrTracker.add(gotBuffer, symbolCacheLevel);

                    branchStubsRegion.sizeInUse += 6;
                    branchGOTsRegion.sizeInUse += 8;
                    targetAddress = stubAddress;
                    itAndInserted.first->second = targetAddress;
                } else {
                    // The stub already existed, so use it
                    targetAddress = itAndInserted.first->second;
                }
                uint64_t diffValue = targetAddress - (loadAddress + branchData.fixupVMOffset + 4);
                *((uint32_t*)branchData.fixupLoc) = (uint32_t)diffValue;
            }
        }

        // FIXME: We could move symbolOwner and dylibFixupsOwner to a worker thread to be destroyed
    }

    // Now that we've processes all rebases/binds, patch all the vtables

    // Add all the collections to the vtable patcher
    if ( existingKernelCollection != nullptr ) {
        // The baseKC for x86_64 has __HIB mapped first , so we need to get either the __DATA or __TEXT depending on what is earliest
        // The kernel base address is still __TEXT, even if __DATA or __HIB is mapped prior to that.
        // The loader may have loaded something before __TEXT, but the existingKernelCollection pointer still corresponds to __TEXT
        __block uint64_t baseAddress = ~0ULL;
        existingKernelCollection->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            baseAddress = std::min(baseAddress, info.vmAddr);
        });

        // The existing collection is a pointer to the mach_header for the baseKC, but __HIB and other segments may be before that
        // Offset those here
        uint64_t basePointerOffset = existingKernelCollection->preferredLoadAddress() - baseAddress;
        const uint8_t* basePointer = (uint8_t*)existingKernelCollection - basePointerOffset;

        vtablePatcher.addKernelCollection(existingKernelCollection, Options::AppCacheKind::kernel,
                                          basePointer, baseAddress);
    }

    if ( pageableKernelCollection != nullptr ) {
        // The baseKC for x86_64 has __HIB mapped first , so we need to get either the __DATA or __TEXT depending on what is earliest
        // The kernel base address is still __TEXT, even if __DATA or __HIB is mapped prior to that.
        // The loader may have loaded something before __TEXT, but the existingKernelCollection pointer still corresponds to __TEXT
        __block uint64_t baseAddress = ~0ULL;
        pageableKernelCollection->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            baseAddress = std::min(baseAddress, info.vmAddr);
        });

        // The existing collection is a pointer to the mach_header for the baseKC, but __HIB and other segments may be before that
        // Offset those here
        uint64_t basePointerOffset = pageableKernelCollection->preferredLoadAddress() - baseAddress;
        const uint8_t* basePointer = (uint8_t*)pageableKernelCollection - basePointerOffset;

        vtablePatcher.addKernelCollection(pageableKernelCollection, Options::AppCacheKind::pageableKC,
                                          basePointer, baseAddress);
    }

    // Also add our KC
    vtablePatcher.addKernelCollection((const dyld3::MachOAppCache*)cacheHeader.header, appCacheOptions.cacheKind,
                                      (const uint8_t*)_fullAllocatedBuffer, cacheBaseAddress);

    // Add all the dylibs to the patcher
    {
        if ( existingKernelCollection != nullptr ) {
            uint8_t fixupLevel = getFixupLevel(Options::AppCacheKind::kernel);

            __block std::map<std::string, std::vector<std::string>> kextDependencies;
            kextDependencies[kernelID] = {};
            existingKernelCollection->forEachPrelinkInfoLibrary(_diagnostics,
                                                                ^(const char *bundleName, const char* relativePath,
                                                                  const std::vector<const char *> &deps) {
                std::vector<std::string>& dependencies = kextDependencies[bundleName];
                dependencies.insert(dependencies.end(), deps.begin(), deps.end());
            });

            existingKernelCollection->forEachDylib(_diagnostics, ^(const dyld3::MachOAnalyzer *ma, const char *dylibID, bool &stop) {
                auto depsIt = kextDependencies.find(dylibID);
                assert(depsIt != kextDependencies.end());
                vtablePatcher.addDylib(_diagnostics, ma, dylibID, depsIt->second, fixupLevel);
            });
        }

        if ( pageableKernelCollection != nullptr ) {
            uint8_t fixupLevel = getFixupLevel(Options::AppCacheKind::pageableKC);

            __block std::map<std::string, std::vector<std::string>> kextDependencies;
            pageableKernelCollection->forEachPrelinkInfoLibrary(_diagnostics,
                                                                ^(const char *bundleName, const char* relativePath,
                                                                  const std::vector<const char *> &deps) {
                std::vector<std::string>& dependencies = kextDependencies[bundleName];
                dependencies.insert(dependencies.end(), deps.begin(), deps.end());
            });

            pageableKernelCollection->forEachDylib(_diagnostics, ^(const dyld3::MachOAnalyzer *ma, const char *dylibID, bool &stop) {
                auto depsIt = kextDependencies.find(dylibID);
                assert(depsIt != kextDependencies.end());
                vtablePatcher.addDylib(_diagnostics, ma, dylibID, depsIt->second, fixupLevel);
            });
        }

        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID, DylibStripMode stripMode,
                            const std::vector<std::string> &dependencies, Diagnostics& dylibDiag, bool &stop) {
            vtablePatcher.addDylib(dylibDiag, ma, dylibID, dependencies, currentLevel);
        });
    }

    vtablePatcher.findMetaclassDefinitions(dylibsToSymbols, kernelID, kernelMA, appCacheOptions.cacheKind);
    vtablePatcher.findExistingFixups(_diagnostics, existingKernelCollection, pageableKernelCollection);
    if ( _diagnostics.hasError() )
        return;

    // Add vtables from the base KC if we have one
    if ( existingKernelCollection != nullptr ) {
        vtablePatcher.findBaseKernelVTables(_diagnostics, existingKernelCollection, dylibsToSymbols);
        if ( _diagnostics.hasError() )
            return;
    }

    // Add vtables from the pageable KC if we have one
    if ( pageableKernelCollection != nullptr ) {
        vtablePatcher.findPageableKernelVTables(_diagnostics, pageableKernelCollection, dylibsToSymbols);
        if ( _diagnostics.hasError() )
            return;
    }

    // Add vables from our level
    vtablePatcher.findVTables(currentLevel, kernelMA, dylibsToSymbols, _aslrTracker, missingBindLocations);

    // Don't run the patcher if we have a failure finding the vtables
    if ( vtablePatcher.hasError() ) {
        _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
        return;
    }

    // Now patch all of the vtables.
    vtablePatcher.patchVTables(_diagnostics, missingBindLocations, _aslrTracker, currentLevel);
    if ( _diagnostics.hasError() )
        return;

    if ( vtablePatcher.hasError() ) {
        _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
        return;
    }

    // FIXME: We could move vtablePatcherOwner to a worker thread to be destroyed
    vtablePatcherOwner.reset();

    // Also error out if we have an error on any of the dylib diagnostic objects

    // Log any binds which are still missing
    for (const auto& missingLocationAndBind : missingBindLocations) {
        const uint8_t* missingBindLoc = missingLocationAndBind.first;
        const VTableBindSymbol& missingBind = missingLocationAndBind.second;

        // Work out which segment and section this missing bind was in
        __block bool reportedError = false;
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID, DylibStripMode stripMode,
                            const std::vector<std::string> &dependencies, Diagnostics& dylibDiag, bool &stopDylib) {
            intptr_t slide = ma->getSlide();
            ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo,
                                 bool malformedSectionRange, bool &stopSection) {
                const uint8_t* content  = (uint8_t*)(sectInfo.sectAddr + slide);
                const uint8_t* start    = (uint8_t*)content;
                const uint8_t* end      = start + sectInfo.sectSize;
                if ( (missingBindLoc >= start) && (missingBindLoc < end) ) {
                    std::string segmentName = sectInfo.segInfo.segName;
                    std::string sectionName = sectInfo.sectName;
                    uint64_t sectionOffset = (missingBindLoc - start);

                    dylibDiag.error("Failed to bind '%s' in '%s' (at offset 0x%llx in %s, %s) as "
                                    "could not find a kext which exports this symbol",
                                    missingBind.symbolName.c_str(), missingBind.binaryID.data(),
                                    sectionOffset, segmentName.c_str(), sectionName.c_str());

                    reportedError = true;
                    stopSection = true;
                    stopDylib = true;
                }
            });
        });

        if ( !reportedError ) {
            _diagnostics.error("Failed to bind '%s' in '%s' as could not find a kext which exports this symbol",
                               missingBind.symbolName.c_str(), missingBind.binaryID.data());
        }
    }

    // If we had missing binds and reported no other errors, then generate an error to give the diagnostics something to track
    if ( !missingBindLocations.empty() && _diagnostics.noError() ) {
        _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
    }

    // FIXME: We could move dylibsToSymbolsOwner to a worker thread to be destroyed
}

namespace {

class ByteBuffer {
public:
    ByteBuffer(uint8_t* storage, uintptr_t allocCount) {
        buffer.setInitialStorage(storage, allocCount);
    }

    uint8_t* makeSpace(size_t bytesNeeded) {
        // Make space in the buffer
        for (size_t i = 0; i != bytesNeeded; ++i)
            buffer.default_constuct_back();

        // Grab a pointer to our position in the buffer
        uint8_t* data = buffer.begin();

        // Move the buffer to start after our data
        dyld3::Array<uint8_t> newBuffer(buffer.end(), buffer.freeCount(), 0);
        buffer = newBuffer;

        return data;
    };

    const uint8_t* begin() const {
        return buffer.begin();
    }

    const uint8_t* end() const {
        return buffer.end();
    }

private:
    dyld3::Array<uint8_t> buffer;
};

}

void AppCacheBuilder::writeFixups()
{
    if ( fixupsSubRegion.sizeInUse == 0 )
        return;

    __block ByteBuffer byteBuffer(fixupsSubRegion.buffer, fixupsSubRegion.bufferSize);

    // Keep track of where we put the fixups
    const uint8_t* classicRelocsBufferStart = nullptr;
    const uint8_t* classicRelocsBufferEnd = nullptr;

    // If the kernel needs classic relocs, emit those first
    CacheHeader64& header = cacheHeader;
    if ( header.dynSymbolTable != nullptr ) {
        classicRelocsBufferStart = byteBuffer.begin();

        dyld3::MachOAnalyzer* cacheMA = (dyld3::MachOAnalyzer*)header.header;
        __block uint64_t localRelocBaseAddress = 0;
        cacheMA->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
            if ( info.protections & VM_PROT_WRITE ) {
                localRelocBaseAddress = info.vmAddr;
                stop = true;
            }
        });

        const std::vector<void*> allRebaseTargets = _aslrTracker.getRebaseTargets();

        const dyld3::MachOAnalyzer* kernelMA = getKernelStaticExecutableFromCache();
        kernelMA->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
            std::vector<void*> segmentRebaseTargets;
            uint64_t segmentVMOffset = info.vmAddr - cacheBaseAddress;
            const uint8_t* segmentStartAddr = (const uint8_t*)(_fullAllocatedBuffer + segmentVMOffset);
            const uint8_t* segmentEndAddr = (const uint8_t*)(segmentStartAddr + info.vmSize);
            for (void* target : allRebaseTargets) {
                if ( (target >= segmentStartAddr) && (target < segmentEndAddr) ) {
                    segmentRebaseTargets.push_back(target);
                }
            }
            std::sort(segmentRebaseTargets.begin(), segmentRebaseTargets.end());

            for (void* target : segmentRebaseTargets) {
                uint64_t targetSegmentOffset = (uint64_t)target - (uint64_t)segmentStartAddr;
                //printf("Target: %s + 0x%llx: %p\n", info.segName, targetSegmentOffset, target);

                uint64_t offsetFromBaseAddress = (info.vmAddr + targetSegmentOffset) - localRelocBaseAddress;
                relocation_info* reloc = (relocation_info*)byteBuffer.makeSpace(sizeof(relocation_info));
                reloc->r_address = (uint32_t)offsetFromBaseAddress;
                reloc->r_symbolnum = 0;
                reloc->r_pcrel = false;
                reloc->r_length = 0;
                reloc->r_extern = 0;
                reloc->r_type = 0;

                uint32_t vmAddr32 = 0;
                uint64_t vmAddr64 = 0;
                if ( _aslrTracker.hasRebaseTarget32(target, &vmAddr32) ) {
                    reloc->r_length = 2;
                    *(uint32_t*)target = vmAddr32;
                } else if ( _aslrTracker.hasRebaseTarget64(target, &vmAddr64) ) {
                    reloc->r_length = 3;
                    *(uint64_t*)target = vmAddr64;
                }
            }

            // Remove these fixups so that we don't also emit chained fixups for them
            for (void* target : segmentRebaseTargets)
                _aslrTracker.remove(target);
        });

        classicRelocsBufferEnd = byteBuffer.begin();
    }

    // TODO: 32-bit pointer format
    assert(_is64);
    const uint8_t currentLevel = getCurrentFixupLevel();

    // We can have up to 4 levels in the fixup format.  These are the base addresses from
    // which each level starts
    BLOCK_ACCCESSIBLE_ARRAY(uint64_t, levelBaseAddresses, 4);
    for (unsigned i = 0; i != numFixupLevels; ++i)
        levelBaseAddresses[i] = 0;

    levelBaseAddresses[currentLevel] = cacheBaseAddress;
    if ( appCacheOptions.cacheKind != Options::AppCacheKind::kernel ) {
        assert(existingKernelCollection != nullptr);
        // The auxKC is mapped with __DATA first, so we need to get either the __DATA or __TEXT depending on what is earliest
        __block uint64_t baseAddress = ~0ULL;
        existingKernelCollection->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            baseAddress = std::min(baseAddress, info.vmAddr);
        });
        levelBaseAddresses[0] = baseAddress;
    }

    if ( pageableKernelCollection != nullptr ) {
        // We may have __DATA first, so we need to get either the __DATA or __TEXT depending on what is earliest
        __block uint64_t baseAddress = ~0ULL;
        pageableKernelCollection->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            baseAddress = std::min(baseAddress, info.vmAddr);
        });
        uint8_t fixupLevel = getFixupLevel(Options::AppCacheKind::pageableKC);
        levelBaseAddresses[fixupLevel] = baseAddress;
    }

    // We have a dyld_chained_starts_in_segment plus an offset for each page
    struct SegmentFixups {
        //const Region* region                            = nullptr;
        uint8_t*                        segmentBuffer       = nullptr;
        uint64_t                        segmentIndex        = 0;
        uint64_t                        unslidLoadAddress   = 0;
        uint64_t                        sizeInUse           = 0;
        dyld_chained_starts_in_segment* starts              = nullptr;
        uint64_t                        startsByteSize      = 0;
        uint64_t                        numPagesToFixup     = 0;
    };

    auto buildChainedFixups = ^(uint64_t baseAddress, uint64_t segmentCount, std::vector<SegmentFixups>& startsInSegments) {

        const uint8_t* chainedFixupsBufferStart = nullptr;
        const uint8_t* chainedFixupsBufferEnd = nullptr;

        chainedFixupsBufferStart = byteBuffer.begin();

        // Start with dyld_chained_fixups_header which is fixed size
        dyld_chained_fixups_header* fixupsHeader = (dyld_chained_fixups_header*)byteBuffer.makeSpace(sizeof(dyld_chained_fixups_header));

        // We have a dyld_chained_starts_in_image plus an offset for each segment
        dyld_chained_starts_in_image* startsInImage = (dyld_chained_starts_in_image*)byteBuffer.makeSpace(sizeof(dyld_chained_starts_in_image) + (segmentCount * sizeof(uint32_t)));

        const uint8_t* endOfStarts = nullptr;
        for (SegmentFixups& segmentFixups : startsInSegments) {
            uint64_t startsInSegmentByteSize = sizeof(dyld_chained_starts_in_segment) + (segmentFixups.numPagesToFixup * sizeof(uint16_t));
            dyld_chained_starts_in_segment* startsInSegment = (dyld_chained_starts_in_segment*)byteBuffer.makeSpace(startsInSegmentByteSize);
            endOfStarts = (const uint8_t*)startsInSegment + startsInSegmentByteSize;

            segmentFixups.starts            = startsInSegment;
            segmentFixups.startsByteSize    = startsInSegmentByteSize;
        }

        // Starts in image
        startsInImage->seg_count        = (uint32_t)segmentCount;
        for (uint32_t segmentIndex = 0; segmentIndex != segmentCount; ++segmentIndex) {
            startsInImage->seg_info_offset[segmentIndex] = 0;
        }
        for (const SegmentFixups& segmentFixups : startsInSegments) {
            dyld_chained_starts_in_segment* startsInSegment = segmentFixups.starts;
            uint64_t segmentIndex = segmentFixups.segmentIndex;
            assert(segmentIndex < segmentCount);
            assert(startsInImage->seg_info_offset[segmentIndex] == 0);
            startsInImage->seg_info_offset[segmentIndex] = (uint32_t)((uint8_t*)startsInSegment - (uint8_t*)startsInImage);
        }

        const unsigned chainedPointerStride = dyld3::MachOAnalyzer::ChainedFixupPointerOnDisk::strideSize(chainedPointerFormat);

        // Starts in segment
        for (const SegmentFixups& segmentFixups : startsInSegments) {
            dyld_chained_starts_in_segment* startsInSegment = segmentFixups.starts;
            startsInSegment->size               = (uint32_t)segmentFixups.startsByteSize;
            startsInSegment->page_size          = fixupsPageSize();
            startsInSegment->pointer_format     = chainedPointerFormat;
            startsInSegment->segment_offset     = segmentFixups.unslidLoadAddress - baseAddress;
            startsInSegment->max_valid_pointer  = 0; // FIXME: Needed in 32-bit only
            startsInSegment->page_count         = (segmentFixups.sizeInUse + startsInSegment->page_size - 1) / startsInSegment->page_size;
            for (uint64_t pageIndex = 0; pageIndex != startsInSegment->page_count; ++pageIndex) {
                startsInSegment->page_start[pageIndex] = DYLD_CHAINED_PTR_START_NONE;
                uint8_t* lastLoc = nullptr;
                // Note we always walk in 1-byte at a time as x86_64 has unaligned fixups
                for (uint64_t pageOffset = 0; pageOffset != startsInSegment->page_size; pageOffset += 1) {
                    uint8_t* fixupLoc = segmentFixups.segmentBuffer + (pageIndex * startsInSegment->page_size) + pageOffset;
                    uint8_t fixupLevel = currentLevel;
                    if ( !_aslrTracker.has(fixupLoc, &fixupLevel) )
                        continue;
                    assert((pageOffset % chainedPointerStride) == 0);
                    if ( lastLoc ) {
                        // Patch last loc to point here
                        assert(_is64);
                        dyld_chained_ptr_64_kernel_cache_rebase* lastLocBits = (dyld_chained_ptr_64_kernel_cache_rebase*)lastLoc;
                        assert(lastLocBits->next == 0);
                        uint64_t next = (fixupLoc - lastLoc) / chainedPointerStride;
                        lastLocBits->next = next;
                        assert(lastLocBits->next == next && "next location truncated");
                    } else {
                        // First fixup on this page
                        startsInSegment->page_start[pageIndex] = pageOffset;
                    }
                    lastLoc = fixupLoc;

                    uint64_t targetVMAddr = *(uint64_t*)fixupLoc;

                    uint8_t highByte = 0;
                    if ( _aslrTracker.hasHigh8(fixupLoc, &highByte) ) {
                        uint64_t tbi = (uint64_t)highByte << 56;
                        targetVMAddr |= tbi;
                    }

                    assert(fixupLevel < numFixupLevels);
                    uint64_t targetVMOffset = targetVMAddr - levelBaseAddresses[fixupLevel];

                    // Pack the vmAddr on this location in to the fixup format
                    dyld_chained_ptr_64_kernel_cache_rebase* locBits = (dyld_chained_ptr_64_kernel_cache_rebase*)fixupLoc;

                    uint16_t diversity;
                    bool     hasAddrDiv;
                    uint8_t  key;
                    if ( _aslrTracker.hasAuthData(fixupLoc, &diversity, &hasAddrDiv, &key) ) {
                        locBits->target         = targetVMOffset;
                        locBits->cacheLevel     = fixupLevel;
                        locBits->diversity      = diversity;
                        locBits->addrDiv        = hasAddrDiv;
                        locBits->key            = key;
                        locBits->next           = 0;
                        locBits->isAuth         = 1;
                        assert(locBits->target == targetVMOffset && "target truncated");
                    }
                    else {
                        locBits->target         = targetVMOffset;
                        locBits->cacheLevel     = fixupLevel;
                        locBits->diversity      = 0;
                        locBits->addrDiv        = 0;
                        locBits->key            = 0;
                        locBits->next           = 0;
                        locBits->isAuth         = 0;
                        assert(locBits->target == targetVMOffset && "target truncated");
                    }
                }
            }
        }

        chainedFixupsBufferEnd = byteBuffer.begin();

        // Header
        fixupsHeader->fixups_version    = 0;
        fixupsHeader->starts_offset     = (uint32_t)((uint8_t*)startsInImage - (uint8_t*)fixupsHeader);
        fixupsHeader->imports_offset    = (uint32_t)((uint8_t*)chainedFixupsBufferEnd - (uint8_t*)fixupsHeader);
        fixupsHeader->symbols_offset    = fixupsHeader->imports_offset;
        fixupsHeader->imports_count     = 0;
        fixupsHeader->imports_format    = DYLD_CHAINED_IMPORT; // The validate code wants a value here
        fixupsHeader->symbols_format    = 0;

        return std::make_pair(chainedFixupsBufferStart, chainedFixupsBufferEnd);
    };

    if ( fixupsArePerKext() ) {
        // The pageableKC (and sometimes auxKC) has one LC_DYLD_CHAINED_FIXUPS per kext, not 1 total
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                            DylibStripMode stripMode, const std::vector<std::string> &dependencies,
                            Diagnostics& dylibDiag, bool &stop) {
            uint64_t loadAddress = ma->preferredLoadAddress();

            __block uint64_t                    numSegments = 0;
            __block std::vector<SegmentFixups>  segmentFixups;
            ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                // Third party kexts have writable __TEXT, so we need to add starts for all segments
                // other than LINKEDIT
                bool segmentCanHaveFixups = false;
                if ( appCacheOptions.cacheKind == Options::AppCacheKind::pageableKC ) {
                    segmentCanHaveFixups = (info.protections & VM_PROT_WRITE) != 0;
                } else {
                    // auxKC
                    segmentCanHaveFixups = (strcmp(info.segName, "__LINKEDIT") != 0);
                }

                if ( segmentCanHaveFixups) {
                    SegmentFixups segmentToFixup;
                    segmentToFixup.segmentBuffer        = (uint8_t*)ma + (info.vmAddr - loadAddress);
                    segmentToFixup.segmentIndex         = info.segIndex;
                    segmentToFixup.unslidLoadAddress    = info.vmAddr;
                    segmentToFixup.sizeInUse            = info.vmSize;
                    segmentToFixup.starts               = nullptr;
                    segmentToFixup.startsByteSize       = 0;
                    segmentToFixup.numPagesToFixup      = numWritablePagesToFixup(info.vmSize);
                    segmentFixups.push_back(segmentToFixup);
                }

                ++numSegments;
            });


            std::pair<const uint8_t*, const uint8_t*> chainedFixupsRange = buildChainedFixups(loadAddress,
                                                                                              numSegments, segmentFixups);
            const uint8_t* chainedFixupsBufferStart = chainedFixupsRange.first;
            const uint8_t* chainedFixupsBufferEnd = chainedFixupsRange.second;

            if ( chainedFixupsBufferStart != chainedFixupsBufferEnd ) {
                // Add the load command to our file

                uint64_t fixupsOffset = (uint64_t)chainedFixupsBufferStart - (uint64_t)fixupsSubRegion.buffer;
                uint64_t fixupsSize = (uint64_t)chainedFixupsBufferEnd - (uint64_t)chainedFixupsBufferStart;

                // 64-bit
                assert(_is64);
                typedef Pointer64<LittleEndian> P;

                uint32_t freeSpace = ma->loadCommandsFreeSpace();
                assert(freeSpace >= sizeof(macho_linkedit_data_command<P>));
                uint8_t* endOfLoadCommands = (uint8_t*)ma + sizeof(macho_header<P>) + ma->sizeofcmds;

                // update mach_header to account for new load commands
                macho_header<P>* mh = (macho_header<P>*)ma;
                mh->set_sizeofcmds(mh->sizeofcmds() + sizeof(macho_linkedit_data_command<P>));
                mh->set_ncmds(mh->ncmds() + 1);

                // Add the new load command
                macho_linkedit_data_command<P>* cmd = (macho_linkedit_data_command<P>*)endOfLoadCommands;
                cmd->set_cmd(LC_DYLD_CHAINED_FIXUPS);
                cmd->set_cmdsize(sizeof(linkedit_data_command));
                cmd->set_dataoff((uint32_t)(_readOnlyRegion.cacheFileOffset + _readOnlyRegion.sizeInUse + fixupsOffset));
                cmd->set_datasize((uint32_t)fixupsSize);
            }
        });

        // Also build chained fixups on the top level for the branch stub GOTs
        // FIXME: We don't need numRegions() here, but instead just up to an including the RW region
        uint64_t segmentCount = numRegions();
        __block std::vector<SegmentFixups> segmentFixups;

        if ( branchGOTsRegion.sizeInUse != 0 ) {
            SegmentFixups segmentToFixup;
            segmentToFixup.segmentBuffer        = branchGOTsRegion.buffer;
            segmentToFixup.segmentIndex         = branchGOTsRegion.index;
            segmentToFixup.unslidLoadAddress    = branchGOTsRegion.unslidLoadAddress;
            segmentToFixup.sizeInUse            = branchGOTsRegion.sizeInUse;
            segmentToFixup.starts               = nullptr;
            segmentToFixup.startsByteSize       = 0;
            segmentToFixup.numPagesToFixup      = numWritablePagesToFixup(branchGOTsRegion.bufferSize);
            segmentFixups.push_back(segmentToFixup);
        }

        std::pair<const uint8_t*, const uint8_t*> chainedFixupsRange = buildChainedFixups(cacheHeaderRegion.unslidLoadAddress,
                                                                                          segmentCount, segmentFixups);
        const uint8_t* chainedFixupsBufferStart = chainedFixupsRange.first;
        const uint8_t* chainedFixupsBufferEnd = chainedFixupsRange.second;

        if ( chainedFixupsBufferStart != chainedFixupsBufferEnd ) {
            uint64_t fixupsOffset = (uint64_t)chainedFixupsBufferStart - (uint64_t)fixupsSubRegion.buffer;
            uint64_t fixupsSize = (uint64_t)chainedFixupsBufferEnd - (uint64_t)chainedFixupsBufferStart;
            header.chainedFixups->dataoff = (uint32_t)_readOnlyRegion.cacheFileOffset + (uint32_t)_readOnlyRegion.sizeInUse + (uint32_t)fixupsOffset;;
            header.chainedFixups->datasize = (uint32_t)fixupsSize;
        }
    } else {
        // Build the chained fixups for just the kernel collection itself
        // FIXME: We don't need numRegions() here, but instead just up to an including the RW region
        uint64_t segmentCount = numRegions();
        __block std::vector<SegmentFixups> segmentFixups;

        auto addSegmentStarts = ^(const Region& region) {
            SegmentFixups segmentToFixup;
            segmentToFixup.segmentBuffer        = region.buffer;
            segmentToFixup.segmentIndex         = region.index;
            segmentToFixup.unslidLoadAddress    = region.unslidLoadAddress;
            segmentToFixup.sizeInUse            = region.sizeInUse;
            segmentToFixup.starts               = nullptr;
            segmentToFixup.startsByteSize       = 0;
            segmentToFixup.numPagesToFixup      = numWritablePagesToFixup(region.bufferSize);
            segmentFixups.push_back(segmentToFixup);
        };

        if ( dataConstRegion.sizeInUse != 0 )
            addSegmentStarts(dataConstRegion);
        if ( branchGOTsRegion.sizeInUse != 0 )
            addSegmentStarts(branchGOTsRegion);
        if ( readWriteRegion.sizeInUse != 0 )
            addSegmentStarts(readWriteRegion);
        if ( hibernateRegion.sizeInUse != 0 )
            addSegmentStarts(hibernateRegion);
        for (const Region& region : nonSplitSegRegions) {
            // Assume writable regions have fixups to emit
            // Note, third party kext's have __TEXT fixups, so assume all of these have fixups
            // LINKEDIT is already elsewhere
            addSegmentStarts(region);
        }

        std::pair<const uint8_t*, const uint8_t*> chainedFixupsRange = buildChainedFixups(cacheHeaderRegion.unslidLoadAddress,
                                                                                          segmentCount, segmentFixups);
        const uint8_t* chainedFixupsBufferStart = chainedFixupsRange.first;
        const uint8_t* chainedFixupsBufferEnd = chainedFixupsRange.second;

        if ( chainedFixupsBufferStart != chainedFixupsBufferEnd ) {
            uint64_t fixupsOffset = (uint64_t)chainedFixupsBufferStart - (uint64_t)fixupsSubRegion.buffer;
            uint64_t fixupsSize = (uint64_t)chainedFixupsBufferEnd - (uint64_t)chainedFixupsBufferStart;
            header.chainedFixups->dataoff = (uint32_t)_readOnlyRegion.cacheFileOffset + (uint32_t)_readOnlyRegion.sizeInUse + (uint32_t)fixupsOffset;;
            header.chainedFixups->datasize = (uint32_t)fixupsSize;
        }
    }

    // Move the fixups to the end of __LINKEDIT
    if ( classicRelocsBufferStart != classicRelocsBufferEnd ) {
        uint64_t fixupsOffset = (uint64_t)classicRelocsBufferStart - (uint64_t)fixupsSubRegion.buffer;
        uint64_t fixupsSize = (uint64_t)classicRelocsBufferEnd - (uint64_t)classicRelocsBufferStart;
        header.dynSymbolTable->locreloff = (uint32_t)_readOnlyRegion.cacheFileOffset + (uint32_t)_readOnlyRegion.sizeInUse + (uint32_t)fixupsOffset;
        header.dynSymbolTable->nlocrel = (uint32_t)fixupsSize / sizeof(fixupsSize);
    }

    uint64_t fixupsSpace = (uint64_t)byteBuffer.end() - (uint64_t)fixupsSubRegion.buffer;

    uint8_t* linkeditEnd = _readOnlyRegion.buffer + _readOnlyRegion.sizeInUse;
    memcpy(linkeditEnd, fixupsSubRegion.buffer, fixupsSpace);
    uint8_t* fixupsEnd = linkeditEnd + fixupsSpace;

    _readOnlyRegion.sizeInUse += align(fixupsSpace, _is64 ? 3 : 2);
    _readOnlyRegion.sizeInUse = align(_readOnlyRegion.sizeInUse, 14);
    _readOnlyRegion.bufferSize = _readOnlyRegion.sizeInUse;

    // Zero the alignment gap, just in case there's any unoptimized LINKEDIT in there
    uint8_t* alignedBufferEnd = _readOnlyRegion.buffer + _readOnlyRegion.sizeInUse;
    if ( fixupsEnd != alignedBufferEnd ){
        memset(fixupsEnd, 0, alignedBufferEnd - fixupsEnd);
    }

#if 0
    dyld3::MachOAnalyzer* cacheMA = (dyld3::MachOAnalyzer*)header.header;
    uint64_t cachePreferredLoadAddress = cacheMA->preferredLoadAddress();
    cacheMA->forEachRebase(_diagnostics, false, ^(uint64_t runtimeOffset, bool &stop) {
        printf("Rebase: 0x%llx = 0x%llx\n", runtimeOffset, runtimeOffset + cachePreferredLoadAddress);
    });
#endif
}

void AppCacheBuilder::allocateBuffer()
{
    // Whether to order the regions __TEXT, __DATA, __LINKEDIT or __DATA, __TEXT, __LINKEDIT in VM address order
    bool dataRegionFirstInVMOrder = false;
    bool hibernateRegionFirstInVMOrder = false;
    switch (appCacheOptions.cacheKind) {
        case Options::AppCacheKind::none:
            assert(0 && "Cache kind should have been set");
            break;
        case Options::AppCacheKind::kernel:
            if ( hibernateAddress != 0 )
                hibernateRegionFirstInVMOrder = true;
            break;
        case Options::AppCacheKind::pageableKC:
            // There's no interesting ordering for the pageableKC
            break;
        case Options::AppCacheKind::kernelCollectionLevel2:
            assert(0 && "Unimplemented");
            break;
        case Options::AppCacheKind::auxKC:
            dataRegionFirstInVMOrder = true;
            break;
    }

    // Count how many bytes we need from all our regions
    __block uint64_t numRegionFileBytes = 0;
    __block uint64_t numRegionVMBytes = 0;

    std::vector<std::pair<Region*, uint64_t>> regions;
    std::vector<std::pair<Region*, uint64_t>> regionsVMOrder;
    std::map<const Region*, uint32_t> sectionsToAddToRegions;

    if ( hibernateRegionFirstInVMOrder ) {
        regionsVMOrder.push_back({ &hibernateRegion, numRegionVMBytes });
        // Pad out the VM offset so that the cache header starts where the base address
        // really should be
        uint64_t paddedSize = cacheBaseAddress - hibernateAddress;
        if ( hibernateRegion.bufferSize > paddedSize ) {
            _diagnostics.error("Could not lay out __HIB segment");
            return;
        }
        numRegionVMBytes = paddedSize;
        // Set the base address to the hibernate address so that we actually put the
        // hibernate segment there
        cacheBaseAddress = hibernateAddress;

        // Add a section too
        sectionsToAddToRegions[&hibernateRegion] = 1;
    } else if ( dataRegionFirstInVMOrder ) {
        if ( prelinkInfoDict != nullptr ) {
            numRegionVMBytes = align(numRegionVMBytes, 14);
            regionsVMOrder.push_back({ &prelinkInfoRegion, numRegionVMBytes });
            numRegionVMBytes += prelinkInfoRegion.bufferSize;
        }
        if ( readWriteRegion.sizeInUse != 0 ) {
            numRegionVMBytes = align(numRegionVMBytes, 14);
            regionsVMOrder.push_back({ &readWriteRegion, numRegionVMBytes });
            numRegionVMBytes += readWriteRegion.bufferSize;
        }
    }

    // Cache header
    numRegionVMBytes = align(numRegionVMBytes, 14);
    regions.push_back({ &cacheHeaderRegion, 0 });
    regionsVMOrder.push_back({ &cacheHeaderRegion, numRegionVMBytes });

    // Split seg __TEXT
    {
        // File offset
        readOnlyTextRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += readOnlyTextRegion.bufferSize;
        regions.push_back({ &readOnlyTextRegion, 0 });
        // VM offset
        numRegionVMBytes = align(numRegionVMBytes, 14);
        regionsVMOrder.push_back({ &readOnlyTextRegion, numRegionVMBytes });
        numRegionVMBytes += readOnlyTextRegion.bufferSize;

        // Add a section too
        sectionsToAddToRegions[&readOnlyTextRegion] = 1;
    }

    // Split seg __TEXT_EXEC
    if ( readExecuteRegion.sizeInUse != 0 ) {
        // File offset
        readExecuteRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += readExecuteRegion.bufferSize;
        regions.push_back({ &readExecuteRegion, 0 });
        // VM offset
        numRegionVMBytes = align(numRegionVMBytes, 14);
        regionsVMOrder.push_back({ &readExecuteRegion, numRegionVMBytes });
        numRegionVMBytes += readExecuteRegion.bufferSize;
    }

    // __BRANCH_STUBS
    if ( branchStubsRegion.bufferSize != 0 ) {
        // File offset
        branchStubsRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += branchStubsRegion.bufferSize;
        regions.push_back({ &branchStubsRegion, 0 });
        // VM offset
        numRegionVMBytes = align(numRegionVMBytes, 14);
        regionsVMOrder.push_back({ &branchStubsRegion, numRegionVMBytes });
        numRegionVMBytes += branchStubsRegion.bufferSize;
    }

    // __DATA_CONST
    if ( dataConstRegion.sizeInUse != 0 ) {
        // File offset
        dataConstRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += dataConstRegion.bufferSize;
        regions.push_back({ &dataConstRegion, 0 });
        // VM offset
        numRegionVMBytes = align(numRegionVMBytes, 14);
        regionsVMOrder.push_back({ &dataConstRegion, numRegionVMBytes });
        numRegionVMBytes += dataConstRegion.bufferSize;
    }

    // __BRANCH_GOTS
    if ( branchGOTsRegion.bufferSize != 0 ) {
        // File offset
        branchGOTsRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += branchGOTsRegion.bufferSize;
        regions.push_back({ &branchGOTsRegion, 0 });
        // VM offset
        numRegionVMBytes = align(numRegionVMBytes, 14);
        regionsVMOrder.push_back({ &branchGOTsRegion, numRegionVMBytes });
        numRegionVMBytes += branchGOTsRegion.bufferSize;
    }

    // -sectcreate
    // Align to 16k before we lay out all contiguous regions
    numRegionFileBytes = align(numRegionFileBytes, 14);
    for (CustomSegment& customSegment : customSegments) {
        Region& region = *customSegment.parentRegion;

        region.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += region.bufferSize;
        regions.push_back({ &region, 0 });
        // VM offset
        // Note we can't align the vm offset in here
        assert( (numRegionVMBytes % 4096) == 0);
        regionsVMOrder.push_back({ &region, numRegionVMBytes });
        numRegionVMBytes += region.bufferSize;

        // Maybe add sections too
        uint32_t sectionsToAdd = 0;
        if ( customSegment.sections.size() > 1 ) {
            // More than one section, so they all need names
            sectionsToAdd = (uint32_t)customSegment.sections.size();
        } else if ( !customSegment.sections.front().sectionName.empty() ) {
            // Only one section, but it has a name
            sectionsToAdd = 1;
        }
        sectionsToAddToRegions[&region] = sectionsToAdd;
    }
    numRegionVMBytes = align(numRegionVMBytes, 14);

    // __PRELINK_INFO
    // Align to 16k
    numRegionFileBytes = align(numRegionFileBytes, 14);
    if ( prelinkInfoDict != nullptr )
    {
        // File offset
        prelinkInfoRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += prelinkInfoRegion.bufferSize;
        regions.push_back({ &prelinkInfoRegion, 0 });

        if ( !dataRegionFirstInVMOrder ) {
            // VM offset
            numRegionVMBytes = align(numRegionVMBytes, 14);
            regionsVMOrder.push_back({ &prelinkInfoRegion, numRegionVMBytes });
            numRegionVMBytes += prelinkInfoRegion.bufferSize;
        }

        // Add a section too
        sectionsToAddToRegions[&prelinkInfoRegion] = 1;
    }

    // Split seg __DATA
    // Align to 16k
    numRegionFileBytes = align(numRegionFileBytes, 14);
    if ( readWriteRegion.sizeInUse != 0 ) {
        // File offset
        readWriteRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += readWriteRegion.bufferSize;
        regions.push_back({ &readWriteRegion, 0 });

        if ( !dataRegionFirstInVMOrder ) {
            // VM offset
            numRegionVMBytes = align(numRegionVMBytes, 14);
            regionsVMOrder.push_back({ &readWriteRegion, numRegionVMBytes });
            numRegionVMBytes += readWriteRegion.bufferSize;
        }
    }

    // Split seg __HIB
    // Align to 16k
    numRegionFileBytes = align(numRegionFileBytes, 14);
    if ( hibernateRegion.sizeInUse != 0 ) {
        // File offset
        hibernateRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += hibernateRegion.bufferSize;
        regions.push_back({ &hibernateRegion, 0 });

        // VM offset was already handled earlier
    }

    // Non split seg regions
    // Align to 16k before we lay out all contiguous regions
    numRegionFileBytes = align(numRegionFileBytes, 14);
    for (Region& region : nonSplitSegRegions) {
        region.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += region.bufferSize;
        regions.push_back({ &region, 0 });
        // VM offset
        // Note we can't align the vm offset in here
        assert( (numRegionVMBytes % 4096) == 0);
        regionsVMOrder.push_back({ &region, numRegionVMBytes });
        numRegionVMBytes += region.bufferSize;
    }
    numRegionVMBytes = align(numRegionVMBytes, 14);

    // __LINKEDIT
    // Align to 16k
    // File offset
    numRegionFileBytes = align(numRegionFileBytes, 14);
    _readOnlyRegion.cacheFileOffset = numRegionFileBytes;
    numRegionFileBytes += _readOnlyRegion.bufferSize;
    regions.push_back({ &_readOnlyRegion, 0 });
    // VM offset
    numRegionVMBytes = align(numRegionVMBytes, 14);
    regionsVMOrder.push_back({ &_readOnlyRegion, numRegionVMBytes });
    numRegionVMBytes += _readOnlyRegion.bufferSize;

    // __LINKEDIT fixups sub region
    // Align to 16k
    numRegionFileBytes = align(numRegionFileBytes, 14);
    if ( fixupsSubRegion.sizeInUse != 0 ) {
        fixupsSubRegion.cacheFileOffset = numRegionFileBytes;
        numRegionFileBytes += fixupsSubRegion.bufferSize;
        //regions.push_back({ &fixupsSubRegion, 0 });

        // VM offset
        regionsVMOrder.push_back({ &fixupsSubRegion, numRegionVMBytes });
        numRegionVMBytes += fixupsSubRegion.bufferSize;
    }

    const thread_command* unixThread = nullptr;
    if (const DylibInfo* dylib = getKernelStaticExecutableInputFile()) {
        unixThread = dylib->input->mappedFile.mh->unixThreadLoadCommand();
    }

    if (_is64) {

        const uint64_t cacheHeaderSize  = sizeof(mach_header_64);
        uint64_t cacheLoadCommandsSize  = 0;
        uint64_t cacheNumLoadCommands   = 0;

        // UUID
        ++cacheNumLoadCommands;
        uint64_t uuidOffset = cacheHeaderSize + cacheLoadCommandsSize;
        cacheLoadCommandsSize += sizeof(uuid_command);

        // BUILD VERSION
        ++cacheNumLoadCommands;
        uint64_t buildVersionOffset = cacheHeaderSize + cacheLoadCommandsSize;
        cacheLoadCommandsSize += sizeof(build_version_command);

        // UNIX THREAD
        uint64_t unixThreadOffset = 0;
        if ( unixThread != nullptr ) {
            ++cacheNumLoadCommands;
            unixThreadOffset = cacheHeaderSize + cacheLoadCommandsSize;
            cacheLoadCommandsSize += unixThread->cmdsize;
        }

        // SYMTAB and DYSYMTAB
        uint64_t symbolTableOffset = 0;
        uint64_t dynSymbolTableOffset = 0;
        if (const DylibInfo* dylib = getKernelStaticExecutableInputFile()) {
            if ( dylib->input->mappedFile.mh->usesClassicRelocationsInKernelCollection() ) {
                // SYMTAB
                ++cacheNumLoadCommands;
                symbolTableOffset = cacheHeaderSize + cacheLoadCommandsSize;
                cacheLoadCommandsSize += sizeof(symtab_command);

                // DYSYMTAB
                ++cacheNumLoadCommands;
                dynSymbolTableOffset = cacheHeaderSize + cacheLoadCommandsSize;
                cacheLoadCommandsSize += sizeof(dysymtab_command);
            }
        }

        // LC_DYLD_CHAINED_FIXUPS
        // The pageableKC has one LC_DYLD_CHAINED_FIXUPS per kext, and 1 more on the top-level
        // for the branch GOTs
        uint64_t chainedFixupsOffset = 0;
        if ( fixupsSubRegion.bufferSize != 0 ) {
            ++cacheNumLoadCommands;
            chainedFixupsOffset = cacheHeaderSize + cacheLoadCommandsSize;
            cacheLoadCommandsSize += sizeof(linkedit_data_command);
        }

        // Add an LC_SEGMENT_64 for each region
        for (auto& regionAndOffset : regions) {
            ++cacheNumLoadCommands;
            regionAndOffset.second = cacheHeaderSize + cacheLoadCommandsSize;
            cacheLoadCommandsSize += sizeof(segment_command_64);

            // Add space for any sections too
            auto sectionIt = sectionsToAddToRegions.find(regionAndOffset.first);
            if ( sectionIt != sectionsToAddToRegions.end() ) {
                uint32_t numSections = sectionIt->second;
                cacheLoadCommandsSize += sizeof(section_64) * numSections;
            }
        }

        // Add an LC_FILESET_ENTRY for each dylib
        std::vector<std::pair<const DylibInfo*, uint64_t>> dylibs;
        for (const auto& dylib : sortedDylibs) {
            ++cacheNumLoadCommands;
            const char* dylibID = dylib.dylibID.c_str();
            dylibs.push_back({ &dylib, cacheHeaderSize + cacheLoadCommandsSize });
            uint64_t size = align(sizeof(fileset_entry_command) + strlen(dylibID) + 1, 3);
            cacheLoadCommandsSize += size;
        }

        uint64_t cacheHeaderRegionSize = cacheHeaderSize + cacheLoadCommandsSize;

        // Align the app cache header before the rest of the bytes
        cacheHeaderRegionSize = align(cacheHeaderRegionSize, 14);

        assert(numRegionFileBytes <= numRegionVMBytes);

        _allocatedBufferSize = cacheHeaderRegionSize + numRegionVMBytes;

        // The fixup format cannot handle a KC over 1GB (64MB for arm64e auxKC).  Error out if we exceed that
        uint64_t cacheLimit = 1 << 30;
        if ( (appCacheOptions.cacheKind == Options::AppCacheKind::auxKC) && (_options.archs == &dyld3::GradedArchs::arm64e) )
            cacheLimit = 64 * (1 << 20);
        if ( _allocatedBufferSize >= cacheLimit ) {
            _diagnostics.error("kernel collection size exceeds maximum size of %lld vs actual size of %lld",
                               cacheLimit, _allocatedBufferSize);
            return;
        }

        if ( vm_allocate(mach_task_self(), &_fullAllocatedBuffer, _allocatedBufferSize, VM_FLAGS_ANYWHERE) != 0 ) {
            _diagnostics.error("could not allocate buffer");
            return;
        }

        // Assign region vm and buffer addresses now that we know the size of
        // the cache header
        {
            // All vm offsets prior to the cache header are already correct
            // All those after the cache header need to be shifted by the cache
            // header size
            bool seenCacheHeader = false;
            for (const auto& regionAndVMOffset : regionsVMOrder) {
                Region* region = regionAndVMOffset.first;
                uint64_t vmOffset = regionAndVMOffset.second;
                region->unslidLoadAddress = cacheBaseAddress + vmOffset;
                if ( seenCacheHeader ) {
                    // Shift by the cache header size
                    region->unslidLoadAddress += cacheHeaderRegionSize;
                } else {
                    // The offset is correct but add in the base address
                    seenCacheHeader = (region == &cacheHeaderRegion);
                }
                region->buffer = (uint8_t*)_fullAllocatedBuffer + (region->unslidLoadAddress - cacheBaseAddress);
            }
        }

        // Cache header
        cacheHeaderRegion.bufferSize            = cacheHeaderRegionSize;
        cacheHeaderRegion.sizeInUse             = cacheHeaderRegion.bufferSize;
        cacheHeaderRegion.cacheFileOffset       = 0;
        cacheHeaderRegion.initProt              = VM_PROT_READ;
        cacheHeaderRegion.maxProt               = VM_PROT_READ;
        cacheHeaderRegion.name                  = "__TEXT";

#if 0
        for (const auto& regionAndVMOffset : regionsVMOrder) {
            printf("0x%llx : %s\n", regionAndVMOffset.first->unslidLoadAddress, regionAndVMOffset.first->name.c_str());
        }
#endif

        CacheHeader64& header = cacheHeader;
        header.header = (mach_header_64*)cacheHeaderRegion.buffer;
        header.numLoadCommands = cacheNumLoadCommands;
        header.loadCommandsSize = cacheLoadCommandsSize;
        header.uuid = (uuid_command*)(cacheHeaderRegion.buffer + uuidOffset);
        header.buildVersion = (build_version_command*)(cacheHeaderRegion.buffer + buildVersionOffset);
        if ( unixThread != nullptr ) {
            header.unixThread = (thread_command*)(cacheHeaderRegion.buffer + unixThreadOffset);
            // Copy the contents here while we have the source pointer available
            memcpy(header.unixThread, unixThread, unixThread->cmdsize);
        }

        if ( symbolTableOffset != 0 ) {
            header.symbolTable = (symtab_command*)(cacheHeaderRegion.buffer + symbolTableOffset);
        }

        if ( dynSymbolTableOffset != 0 ) {
            header.dynSymbolTable = (dysymtab_command*)(cacheHeaderRegion.buffer + dynSymbolTableOffset);
        }

        if ( chainedFixupsOffset != 0 ) {
            header.chainedFixups = (linkedit_data_command*)(cacheHeaderRegion.buffer + chainedFixupsOffset);
        }

        for (auto& regionAndOffset : regions) {
            assert(regionAndOffset.first->initProt != 0);
            assert(regionAndOffset.first->maxProt != 0);
            segment_command_64* loadCommand = (segment_command_64*)(cacheHeaderRegion.buffer + regionAndOffset.second);
            header.segments.push_back({ loadCommand, regionAndOffset.first });
        }
        for (const auto& dylibAndOffset : dylibs) {
            fileset_entry_command* loadCommand = (fileset_entry_command*)(cacheHeaderRegion.buffer + dylibAndOffset.second);
            header.dylibs.push_back({ loadCommand, dylibAndOffset.first });
        }

        // Move the offsets of all the other regions
        // Split seg __TEXT
        readOnlyTextRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // Split seg __TEXT_EXEC
        readExecuteRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // __BRANCH_STUBS
        branchStubsRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // Split seg __DATA_CONST
        dataConstRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // __BRANCH_GOTS
        branchGOTsRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // Split seg __DATA
        readWriteRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // Split seg __HIB
        hibernateRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // -sectcreate
        for (Region& region : customDataRegions) {
            region.cacheFileOffset += cacheHeaderRegion.sizeInUse;
        }

        // Non split seg regions
        for (Region& region : nonSplitSegRegions) {
            region.cacheFileOffset += cacheHeaderRegion.sizeInUse;
        }

        // __PRELINK_INFO
        prelinkInfoRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // __LINKEDIT
        _readOnlyRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;

        // __LINKEDIT fixups sub region
        fixupsSubRegion.cacheFileOffset += cacheHeaderRegion.sizeInUse;
    } else {
        assert(false);
    }
}

void AppCacheBuilder::generateCacheHeader() {
    if ( !_is64 )
        assert(0 && "Unimplemented");

    {
        // 64-bit
        typedef Pointer64<LittleEndian> P;
        CacheHeader64& header = cacheHeader;

        // Write the header
        macho_header<P>* mh = (macho_header<P>*)header.header;
        mh->set_magic(MH_MAGIC_64);
        mh->set_cputype(_options.archs->_orderedCpuTypes[0].type);
        mh->set_cpusubtype(_options.archs->_orderedCpuTypes[0].subtype);
        mh->set_filetype(MH_FILESET);
        mh->set_ncmds((uint32_t)header.numLoadCommands);
        mh->set_sizeofcmds((uint32_t)header.loadCommandsSize);
        mh->set_flags(0);
        mh->set_reserved(0);

        // FIXME: Move this to writeAppCacheHeader
        {
            macho_uuid_command<P>* cmd = (macho_uuid_command<P>*)header.uuid;
            cmd->set_cmd(LC_UUID);
            cmd->set_cmdsize(sizeof(uuid_command));
            cmd->set_uuid((uuid_t){});
        }

        // FIXME: Move this to writeAppCacheHeader
        {
            macho_build_version_command<P>* cmd = (macho_build_version_command<P>*)header.buildVersion;
            cmd->set_cmd(LC_BUILD_VERSION);
            cmd->set_cmdsize(sizeof(build_version_command));
            cmd->set_platform((uint32_t)_options.platform);
            cmd->set_minos(0);
            cmd->set_sdk(0);
            cmd->set_ntools(0);
        }

        // FIXME: Move this to writeAppCacheHeader
        // LC_UNIXTHREAD was already memcpy()'ed from the source dylib when we allocated space for it
        // We still need to slide its PC value here before we lose the information about the slide
        if ( header.unixThread != nullptr ) {
            const DylibInfo* dylib = getKernelStaticExecutableInputFile();
            const dyld3::MachOAnalyzer* ma = dylib->input->mappedFile.mh;
            ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                uint64_t startAddress = dylib->input->mappedFile.mh->entryAddrFromThreadCmd(header.unixThread);
                if ( (startAddress < info.vmAddr) || (startAddress >= (info.vmAddr + info.vmSize)) )
                    return;

                uint64_t segSlide = dylib->cacheLocation[info.segIndex].dstCacheUnslidAddress - info.vmAddr;
                startAddress += segSlide;

                macho_thread_command<P>* cmd = (macho_thread_command<P>*)header.unixThread;
                cmd->set_thread_register(ma->entryAddrRegisterIndexForThreadCmd(), startAddress);

                stop = true;
            });
        }

        if ( header.symbolTable != nullptr ) {
            macho_symtab_command<P>* cmd = (macho_symtab_command<P>*)header.symbolTable;
            cmd->set_cmd(LC_SYMTAB);
            cmd->set_cmdsize(sizeof(symtab_command));
            cmd->set_symoff(0);
            cmd->set_nsyms(0);
            cmd->set_stroff(0);
            cmd->set_strsize(0);
        }

        if ( header.dynSymbolTable != nullptr ) {
            macho_dysymtab_command<P>* cmd = (macho_dysymtab_command<P>*)header.dynSymbolTable;
            cmd->set_cmd(LC_DYSYMTAB);
            cmd->set_cmdsize(sizeof(dysymtab_command));
            cmd->set_ilocalsym(0);
            cmd->set_nlocalsym(0);
            cmd->set_iextdefsym(0);
            cmd->set_nextdefsym(0);
            cmd->set_iundefsym(0);
            cmd->set_nundefsym(0);
            cmd->set_tocoff(0);
            cmd->set_ntoc(0);
            cmd->set_modtaboff(0);
            cmd->set_nmodtab(0);
            cmd->set_extrefsymoff(0);
            cmd->set_nextrefsyms(0);
            cmd->set_indirectsymoff(0);
            cmd->set_nindirectsyms(0);
            cmd->set_extreloff(0);
            cmd->set_nextrel(0);
            cmd->set_locreloff(0);
            cmd->set_nlocrel(0);
        }

        if ( header.chainedFixups != nullptr ) {
            macho_linkedit_data_command<P>* cmd = (macho_linkedit_data_command<P>*)header.chainedFixups;
            cmd->set_cmd(LC_DYLD_CHAINED_FIXUPS);
            cmd->set_cmdsize(sizeof(linkedit_data_command));
            cmd->set_dataoff(0);
            cmd->set_datasize(0);
        }

        // FIXME: Move this to writeAppCacheHeader
        uint64_t segmentIndex = 0;
        for (CacheHeader64::SegmentCommandAndRegion& cmdAndInfo : header.segments) {
            macho_segment_command<P>* cmd = (macho_segment_command<P>*)cmdAndInfo.first;
            Region* region = cmdAndInfo.second;
            region->index = segmentIndex;
            ++segmentIndex;

            assert(region->initProt != 0);
            assert(region->maxProt != 0);

            const char* name = region->name.c_str();

            cmd->set_cmd(LC_SEGMENT_64);
            cmd->set_cmdsize(sizeof(segment_command_64));
            cmd->set_segname(name);
            cmd->set_vmaddr(region->unslidLoadAddress);
            cmd->set_vmsize(region->sizeInUse);
            cmd->set_fileoff(region->cacheFileOffset);
            cmd->set_filesize(region->sizeInUse);
            cmd->set_maxprot(region->maxProt);
            cmd->set_initprot(region->initProt);
            cmd->set_nsects(0);
            cmd->set_flags(0);

            if ( region == &readOnlyTextRegion ) {
                // __PRELINK_TEXT should also get a section
                cmd->set_cmdsize(cmd->cmdsize() + sizeof(section_64));
                cmd->set_nsects(1);

                macho_section<P>* section = (macho_section<P>*)((uint64_t)cmd + sizeof(*cmd));
                section->set_sectname("__text");
                section->set_segname(name);
                section->set_addr(region->unslidLoadAddress);
                section->set_size(region->sizeInUse);
                section->set_offset((uint32_t)region->cacheFileOffset);
                section->set_align(0);
                section->set_reloff(0);
                section->set_nreloc(0);
                section->set_flags(S_REGULAR | S_ATTR_SOME_INSTRUCTIONS | S_ATTR_PURE_INSTRUCTIONS);
                section->set_reserved1(0);
                section->set_reserved2(0);
            } else if ( region == &prelinkInfoRegion ) {
                // __PRELINK_INFO should also get a section
                cmd->set_cmdsize(cmd->cmdsize() + sizeof(section_64));
                cmd->set_nsects(1);

                macho_section<P>* section = (macho_section<P>*)((uint64_t)cmd + sizeof(*cmd));
                section->set_sectname("__info");
                section->set_segname(name);
                section->set_addr(region->unslidLoadAddress);
                section->set_size(region->sizeInUse);
                section->set_offset((uint32_t)region->cacheFileOffset);
                section->set_align(0);
                section->set_reloff(0);
                section->set_nreloc(0);
                section->set_flags(S_REGULAR);
                section->set_reserved1(0);
                section->set_reserved2(0);
            } else if ( region == &hibernateRegion ) {
                // __HIB should also get a section
                cmd->set_cmdsize(cmd->cmdsize() + sizeof(section_64));
                cmd->set_nsects(1);

                macho_section<P>* section = (macho_section<P>*)((uint64_t)cmd + sizeof(*cmd));
                section->set_sectname("__text");
                section->set_segname(name);
                section->set_addr(region->unslidLoadAddress);
                section->set_size(region->sizeInUse);
                section->set_offset((uint32_t)region->cacheFileOffset);
                section->set_align(0);
                section->set_reloff(0);
                section->set_nreloc(0);
                section->set_flags(S_REGULAR | S_ATTR_SOME_INSTRUCTIONS);
                section->set_reserved1(0);
                section->set_reserved2(0);
            } else {
                // Custom segments may have sections
                for (CustomSegment &customSegment : customSegments) {
                    if ( region != customSegment.parentRegion )
                        continue;

                    // Found a segment for this region.  Now work out how many sections to emit
                    // Maybe add sections too
                    uint32_t sectionsToAdd = 0;
                    if ( customSegment.sections.size() > 1 ) {
                        // More than one section, so they all need names
                        sectionsToAdd = (uint32_t)customSegment.sections.size();
                    } else if ( !customSegment.sections.front().sectionName.empty() ) {
                        // Only one section, but it has a name
                        sectionsToAdd = 1;
                    } else {
                        // Only 1 section, and it has no name, so don't add a section
                        continue;
                    }

                    cmd->set_cmdsize(cmd->cmdsize() + (sizeof(section_64) * sectionsToAdd));
                    cmd->set_nsects(sectionsToAdd);
                    uint8_t* bufferPos = (uint8_t*)cmd + sizeof(*cmd);
                    for (const CustomSegment::CustomSection& customSection : customSegment.sections) {
                        macho_section<P>* section = (macho_section<P>*)bufferPos;
                        section->set_sectname(customSection.sectionName.c_str());
                        section->set_segname(name);
                        section->set_addr(region->unslidLoadAddress + customSection.offsetInRegion);
                        section->set_size(customSection.data.size());
                        section->set_offset((uint32_t)(region->cacheFileOffset + customSection.offsetInRegion));
                        section->set_align(0);
                        section->set_reloff(0);
                        section->set_nreloc(0);
                        section->set_flags(S_REGULAR);
                        section->set_reserved1(0);
                        section->set_reserved2(0);

                        bufferPos += sizeof(section_64);
                    }
                }
            }
        }

        // Write the dylibs.  These are all we need for now to be able to walk the
        // app cache
        for (CacheHeader64::DylibCommandAndInfo& cmdAndInfo : header.dylibs) {
            macho_fileset_entry_command<P>* cmd = (macho_fileset_entry_command<P>*)cmdAndInfo.first;
            const DylibInfo* dylib = cmdAndInfo.second;

            const char* dylibID = dylib->dylibID.c_str();
            uint64_t size = align(sizeof(fileset_entry_command) + strlen(dylibID) + 1, 3);

            cmd->set_cmd(LC_FILESET_ENTRY);
            cmd->set_cmdsize((uint32_t)size);
            cmd->set_vmaddr(dylib->cacheLocation[0].dstCacheUnslidAddress);
            cmd->set_fileoff(dylib->cacheLocation[0].dstCacheFileOffset);
            cmd->set_entry_id(dylibID);
        }
    }
}

void AppCacheBuilder::generatePrelinkInfo() {
    if ( prelinkInfoDict == nullptr ) {
        // The kernel doesn't need a prelink dictionary just for itself
        bool needsPrelink = true;
        if ( appCacheOptions.cacheKind == Options::AppCacheKind::kernel ) {
            if ( sortedDylibs.size() == 1 )
                needsPrelink = false;
        }
        if ( needsPrelink ) {
            _diagnostics.error("Expected prelink info dictionary");
        }
        return;
    }

    CFMutableArrayRef arrayRef = (CFMutableArrayRef)CFDictionaryGetValue(prelinkInfoDict,
                                                                         CFSTR("_PrelinkInfoDictionary"));
    if ( arrayRef == nullptr ) {
        _diagnostics.error("Expected prelink info dictionary array");
        return;
    }

    typedef std::pair<const dyld3::MachOAnalyzer*, Diagnostics*> DylibAndDiag;
    __block std::unordered_map<std::string_view, DylibAndDiag> dylibs;
    forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                        DylibStripMode stripMode, const std::vector<std::string>& dependencies,
                        Diagnostics& dylibDiag, bool& stop) {
        dylibs[dylibID] = { ma, &dylibDiag };
    });
    for (const InputDylib& dylib : codelessKexts) {
        dylibs[dylib.dylibID] = { nullptr, nullptr };
    }

    __block std::list<std::string> nonASCIIStrings;
    auto getString = ^(Diagnostics& diags, CFStringRef symbolNameRef) {
        const char* symbolName = CFStringGetCStringPtr(symbolNameRef, kCFStringEncodingUTF8);
        if ( symbolName != nullptr )
            return symbolName;

        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(symbolNameRef), kCFStringEncodingUTF8);
        char buffer[len + 1];
        if ( !CFStringGetCString(symbolNameRef, buffer, len, kCFStringEncodingUTF8) ) {
            diags.error("Could not convert string to ASCII");
            return (const char*)nullptr;
        }
        buffer[len] = '\0';
        nonASCIIStrings.push_back(buffer);
        return nonASCIIStrings.back().c_str();
    };

    bool badKext = false;
    CFIndex arrayCount = CFArrayGetCount(arrayRef);
    for (CFIndex i = 0; i != arrayCount; ++i) {
        CFMutableDictionaryRef dictRef = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(arrayRef, i);

        CFStringRef bundleIDRef = (CFStringRef)CFDictionaryGetValue(dictRef, CFSTR("CFBundleIdentifier"));
        if ( bundleIDRef == nullptr ) {
            _diagnostics.error("Cannot get bundle ID for dylib");
            return;
        }

        const char* bundleIDStr = getString(_diagnostics, bundleIDRef);
        if ( _diagnostics.hasError() )
            return;

        auto dylibIt = dylibs.find(bundleIDStr);
        if ( dylibIt == dylibs.end() ) {
            _diagnostics.error("Cannot get dylib for bundle ID %s", bundleIDStr);
            return;
        }
        const dyld3::MachOAnalyzer *ma = dylibIt->second.first;
        Diagnostics* dylibDiag = dylibIt->second.second;
        // Skip codeless kext's
        if ( ma == nullptr )
            continue;
        uint64_t loadAddress = ma->preferredLoadAddress();

        // _PrelinkExecutableLoadAddr
        CFNumberRef loadAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &loadAddress);
        CFDictionarySetValue(dictRef, CFSTR("_PrelinkExecutableLoadAddr"), loadAddrRef);
        CFRelease(loadAddrRef);

        // _PrelinkExecutableSourceAddr
        CFNumberRef sourceAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &loadAddress);
        CFDictionarySetValue(dictRef, CFSTR("_PrelinkExecutableSourceAddr"), sourceAddrRef);
        CFRelease(sourceAddrRef);

        // _PrelinkKmodInfo
        __block uint64_t kmodInfoAddress = 0;

        // Check for a global first
        __block bool found = false;
        {
            dyld3::MachOAnalyzer::FoundSymbol foundInfo;
            found = ma->findExportedSymbol(_diagnostics, "_kmod_info", true, foundInfo, nullptr);
            if ( found ) {
                kmodInfoAddress = loadAddress + foundInfo.value;
            }
        }
        // And fall back to a local if we need to
        if ( !found ) {
            ma->forEachLocalSymbol(_diagnostics, ^(const char* aSymbolName, uint64_t n_value, uint8_t n_type,
                                                   uint8_t n_sect, uint16_t n_desc, bool& stop) {
                if ( strcmp(aSymbolName, "_kmod_info") == 0 ) {
                    kmodInfoAddress = n_value;
                    found = true;
                    stop = true;
                }
            });
        }

        if ( found ) {
            CFNumberRef kmodInfoAddrRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &kmodInfoAddress);
            CFDictionarySetValue(dictRef, CFSTR("_PrelinkKmodInfo"), kmodInfoAddrRef);
            CFRelease(kmodInfoAddrRef);

            // Since we have a reference to the kmod info anyway, set its address field to the correct value
            assert(_is64);
            uint64_t kmodInfoVMOffset = kmodInfoAddress - loadAddress;
            dyld3::MachOAppCache::KModInfo64_v1* kmodInfo = (dyld3::MachOAppCache::KModInfo64_v1*)((uint8_t*)ma + kmodInfoVMOffset);
            if ( kmodInfo->info_version != 1 ) {
                dylibDiag->error("unsupported kmod_info version of %d", kmodInfo->info_version);
                badKext = true;
                continue;
            }
            __block uint64_t textSegmnentVMAddr = 0;
            __block uint64_t textSegmnentVMSize = 0;
            ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                if ( !strcmp(info.segName, "__TEXT") ) {
                    textSegmnentVMAddr = info.vmAddr;
                    textSegmnentVMSize = info.vmSize;
                    stop = true;
                }
            });
            kmodInfo->address   = textSegmnentVMAddr;
            kmodInfo->size      = textSegmnentVMSize;
        }
    }

    CFErrorRef errorRef = nullptr;
    CFDataRef xmlData = CFPropertyListCreateData(kCFAllocatorDefault, prelinkInfoDict,
                                                 kCFPropertyListXMLFormat_v1_0, 0, &errorRef);
    if (errorRef != nullptr) {
        CFStringRef errorString = CFErrorCopyDescription(errorRef);
        _diagnostics.error("Could not serialise plist because :%s",
                           CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
        CFRelease(xmlData);
        CFRelease(errorRef);
        return;
    } else {
        CFIndex xmlDataLength = CFDataGetLength(xmlData);
        if ( xmlDataLength > prelinkInfoRegion.bufferSize ) {
            _diagnostics.error("Overflow in prelink info segment.  0x%llx vs 0x%llx",
                               (uint64_t)xmlDataLength, prelinkInfoRegion.bufferSize);
            CFRelease(xmlData);
            return;
        }

        // Write the prelink info in to the buffer
        memcpy(prelinkInfoRegion.buffer, CFDataGetBytePtr(xmlData), xmlDataLength);
        CFRelease(xmlData);
    }

    if ( badKext && _diagnostics.noError() ) {
        _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
    }
}

bool AppCacheBuilder::addCustomSection(const std::string& segmentName,
                                       CustomSegment::CustomSection section) {
    for (CustomSegment& segment: customSegments) {
        if ( segment.segmentName != segmentName )
            continue;

        // Found a matching segment
        // Make sure we don't have a section with this name already
        if ( section.sectionName.empty() ) {
            // We can't add a segment only section if other sections exist
            _diagnostics.error("Cannot add empty section name with segment '%s' as other sections exist on that segment",
                               segmentName.c_str());
            return false;
        }

        for (const CustomSegment::CustomSection& existingSection : segment.sections) {
            if ( existingSection.sectionName.empty() ) {
                // We can't add a section with a name if an existing section exists with no name
                _diagnostics.error("Cannot add section named '%s' with segment '%s' as segment has existing nameless section",
                                   segmentName.c_str(), section.sectionName.c_str());
                return false;
            }
            if ( existingSection.sectionName == section.sectionName ) {
                // We can't add a section with the same name as an existing one
                _diagnostics.error("Cannot add section named '%s' with segment '%s' as section already exists",
                                   segmentName.c_str(), section.sectionName.c_str());
                return false;
            }
        }
        segment.sections.push_back(section);
        return true;
    }

    // Didn't find a segment, so add a new one
    CustomSegment segment;
    segment.segmentName = segmentName;
    segment.sections.push_back(section);
    customSegments.push_back(segment);
    return true;
}

void AppCacheBuilder::setExistingKernelCollection(const dyld3::MachOAppCache* appCacheMA) {
    existingKernelCollection = appCacheMA;
}

void AppCacheBuilder::setExistingPageableKernelCollection(const dyld3::MachOAppCache* appCacheMA) {
    pageableKernelCollection = appCacheMA;
}

void AppCacheBuilder::setExtraPrelinkInfo(CFDictionaryRef dictionary) {
    extraPrelinkInfo = dictionary;
}


inline uint32_t absolutetime_to_milliseconds(uint64_t abstime)
{
    return (uint32_t)(abstime/1000/1000);
}

void AppCacheBuilder::buildAppCache(const std::vector<InputDylib>& dylibs)
{
    uint64_t t1 = mach_absolute_time();

    // make copy of dylib list and sort
    makeSortedDylibs(dylibs);

    // Set the chained pointer format
    // x86_64 uses unaligned fixups
    if ( (_options.archs == &dyld3::GradedArchs::x86_64) || (_options.archs == &dyld3::GradedArchs::x86_64h) ) {
        chainedPointerFormat = DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE;
    } else {
        chainedPointerFormat = DYLD_CHAINED_PTR_64_KERNEL_CACHE;
    }

    // If we have only codeless kexts, then error out
    if ( sortedDylibs.empty() ) {
        if ( codelessKexts.empty() ) {
            _diagnostics.error("No binaries or codeless kexts were provided");
        } else {
            _diagnostics.error("Cannot build collection without binaries as only %lx codeless kexts provided",
                               codelessKexts.size());
        }
        return;
    }

    // assign addresses for each segment of each dylib in new cache
    assignSegmentRegionsAndOffsets();
    if ( _diagnostics.hasError() )
        return;

    // allocate space used by largest possible cache plus room for LINKEDITS before optimization
    allocateBuffer();
    if ( _diagnostics.hasError() )
        return;

    assignSegmentAddresses();

    generateCacheHeader();

     // copy all segments into cache
    uint64_t t2 = mach_absolute_time();
    copyRawSegments();

    // rebase all dylibs for new location in cache
    uint64_t t3 = mach_absolute_time();
    if ( appCacheOptions.cacheKind == Options::AppCacheKind::auxKC ) {
        // We can have text fixups in the auxKC so ASLR should just track the whole buffer
        __block const Region* firstDataRegion = nullptr;
        __block const Region* lastDataRegion = nullptr;
        forEachRegion(^(const Region &region) {
            if ( (firstDataRegion == nullptr) || (region.buffer < firstDataRegion->buffer) )
                firstDataRegion = &region;
            if ( (lastDataRegion == nullptr) || (region.buffer > lastDataRegion->buffer) )
                lastDataRegion = &region;
        });

        if ( firstDataRegion != nullptr ) {
            uint64_t size = (lastDataRegion->buffer - firstDataRegion->buffer) + lastDataRegion->bufferSize;
            _aslrTracker.setDataRegion(firstDataRegion->buffer, size);
        }
    } else {
        const Region* firstDataRegion = nullptr;
        const Region* lastDataRegion = nullptr;
        if ( hibernateRegion.sizeInUse != 0 ) {
            firstDataRegion = &hibernateRegion;
            lastDataRegion  = &hibernateRegion;
        }

        if ( dataConstRegion.sizeInUse != 0 ) {
            if ( firstDataRegion == nullptr )
                firstDataRegion = &dataConstRegion;
            if ( (lastDataRegion == nullptr) || (dataConstRegion.buffer > lastDataRegion->buffer) )
                lastDataRegion = &dataConstRegion;
        }

        if ( branchGOTsRegion.bufferSize != 0 ) {
            if ( firstDataRegion == nullptr )
                firstDataRegion = &branchGOTsRegion;
            if ( (lastDataRegion == nullptr) || (branchGOTsRegion.buffer > lastDataRegion->buffer) )
                lastDataRegion = &branchGOTsRegion;
        }

        if ( readWriteRegion.sizeInUse != 0 ) {
            // __DATA might be before __DATA_CONST in an auxKC
            if ( (firstDataRegion == nullptr) || (readWriteRegion.buffer < firstDataRegion->buffer) )
                firstDataRegion = &readWriteRegion;
            if ( (lastDataRegion == nullptr) || (readWriteRegion.buffer > lastDataRegion->buffer) )
                lastDataRegion = &readWriteRegion;
        }

        for (const Region& region : nonSplitSegRegions) {
            // Assume writable regions have fixups to emit
            // Note, third party kext's have __TEXT fixups, so assume all of these have fixups
            // LINKEDIT is already elsewhere
            if ( readWriteRegion.sizeInUse != 0 ) {
                assert(region.buffer >= readWriteRegion.buffer);
            }
            if ( firstDataRegion == nullptr )
                firstDataRegion = &region;
            if ( (lastDataRegion == nullptr) || (region.buffer > lastDataRegion->buffer) )
                lastDataRegion = &region;
        }

        if ( firstDataRegion != nullptr ) {
            uint64_t size = (lastDataRegion->buffer - firstDataRegion->buffer) + lastDataRegion->bufferSize;
            _aslrTracker.setDataRegion(firstDataRegion->buffer, size);
        }
    }
    adjustAllImagesForNewSegmentLocations(cacheBaseAddress, _aslrTracker, nullptr, nullptr);
    if ( _diagnostics.hasError() )
        return;

    // Once we have the final addresses, we can emit the prelink info segment
    generatePrelinkInfo();
    if ( _diagnostics.hasError() )
        return;

    // build ImageArray for dyld3, which has side effect of binding all cached dylibs
    uint64_t t4 = mach_absolute_time();
    processFixups();
    if ( _diagnostics.hasError() )
        return;

    uint64_t t5 = mach_absolute_time();

    // optimize away stubs
    uint64_t t6 = mach_absolute_time();
    {
        __block std::vector<std::pair<const mach_header*, const char*>> images;
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                            DylibStripMode stripMode, const std::vector<std::string>& dependencies,
                            Diagnostics& dylibDiag, bool& stop) {
            images.push_back({ ma, dylibID.c_str() });
        });
        // FIXME: Should we keep the same never stub eliminate symbols?  Eg, for gmalloc.
        const char* const neverStubEliminateSymbols[] = {
            nullptr
        };

        uint64_t cacheUnslidAddr = cacheBaseAddress;
        int64_t cacheSlide = (long)_fullAllocatedBuffer - cacheUnslidAddr;
        optimizeAwayStubs(images, cacheSlide, cacheUnslidAddr,
                          nullptr, neverStubEliminateSymbols);
    }

    // FIPS seal corecrypto, This must be done after stub elimination (so that __TEXT,__text is not changed after sealing)
    fipsSign();

    // merge and compact LINKEDIT segments
    uint64_t t7 = mach_absolute_time();
    {
        __block std::vector<std::tuple<const mach_header*, const char*, DylibStripMode>> images;
        __block std::set<const mach_header*> imagesToStrip;
        __block const dyld3::MachOAnalyzer* kernelMA = nullptr;
        forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                            DylibStripMode stripMode, const std::vector<std::string>& dependencies,
                            Diagnostics& dylibDiag, bool& stop) {
            if ( stripMode == DylibStripMode::stripNone ) {
                // If the binary didn't have a strip mode, then use the global mode
                switch (appCacheOptions.cacheKind) {
                    case Options::AppCacheKind::none:
                        assert("Unhandled kind");
                        break;
                    case Options::AppCacheKind::kernel:
                        switch (appCacheOptions.stripMode) {
                            case Options::StripMode::none:
                                break;
                            case Options::StripMode::all:
                                stripMode = CacheBuilder::DylibStripMode::stripAll;
                                break;
                            case Options::StripMode::allExceptKernel:
                                // Strip all binaries which are not the kernel
                                if ( kernelMA == nullptr ) {
                                    kernelMA = getKernelStaticExecutableFromCache();
                                }
                                if ( ma != kernelMA )
                                    stripMode = CacheBuilder::DylibStripMode::stripAll;
                                break;
                        }
                        break;
                    case Options::AppCacheKind::pageableKC:
                        assert("Unhandled kind");
                        break;
                    case Options::AppCacheKind::kernelCollectionLevel2:
                        assert("Unhandled kind");
                        break;
                    case Options::AppCacheKind::auxKC:
                        assert("Unhandled kind");
                        break;
                }
            }
            images.push_back({ ma, dylibID.c_str(), stripMode });
        });
        optimizeLinkedit(nullptr, images);

        // update final readOnly region size
        if ( !_is64 )
            assert(0 && "Unimplemented");

        {
            // 64-bit
            typedef Pointer64<LittleEndian> P;
            CacheHeader64& header = cacheHeader;

            for (CacheHeader64::SegmentCommandAndRegion& cmdAndRegion : header.segments) {
                if (cmdAndRegion.second != &_readOnlyRegion)
                    continue;
                cmdAndRegion.first->vmsize      = _readOnlyRegion.sizeInUse;
                cmdAndRegion.first->filesize    = _readOnlyRegion.sizeInUse;
                break;
            }
        }
    }

    uint64_t t8 = mach_absolute_time();

    uint64_t t9 = mach_absolute_time();

    // Add fixups to rebase/bind the app cache
    writeFixups();
    {
        if ( !_is64 )
            assert(0 && "Unimplemented");

        // update final readOnly region size
        {
            // 64-bit
            typedef Pointer64<LittleEndian> P;
            CacheHeader64& header = cacheHeader;

            for (CacheHeader64::SegmentCommandAndRegion& cmdAndRegion : header.segments) {
                if (cmdAndRegion.second != &_readOnlyRegion)
                    continue;
                cmdAndRegion.first->vmsize      = _readOnlyRegion.sizeInUse;
                cmdAndRegion.first->filesize    = _readOnlyRegion.sizeInUse;
                break;
            }
        }
    }

    // FIXME: We could move _aslrTracker to a worker thread to be destroyed as we don't need it
    // after this point

    uint64_t t10 = mach_absolute_time();

    generateUUID();
    if ( _diagnostics.hasError() )
        return;

    uint64_t t11 = mach_absolute_time();

    if ( _options.verbose ) {
        fprintf(stderr, "time to layout cache: %ums\n", absolutetime_to_milliseconds(t2-t1));
        fprintf(stderr, "time to copy cached dylibs into buffer: %ums\n", absolutetime_to_milliseconds(t3-t2));
        fprintf(stderr, "time to adjust segments for new split locations: %ums\n", absolutetime_to_milliseconds(t4-t3));
        fprintf(stderr, "time to bind all images: %ums\n", absolutetime_to_milliseconds(t5-t4));
        fprintf(stderr, "time to optimize Objective-C: %ums\n", absolutetime_to_milliseconds(t6-t5));
        fprintf(stderr, "time to do stub elimination: %ums\n", absolutetime_to_milliseconds(t7-t6));
        fprintf(stderr, "time to optimize LINKEDITs: %ums\n", absolutetime_to_milliseconds(t8-t7));
        fprintf(stderr, "time to compute slide info: %ums\n", absolutetime_to_milliseconds(t10-t9));
        fprintf(stderr, "time to compute UUID and codesign cache file: %ums\n", absolutetime_to_milliseconds(t11-t10));
    }
}

void AppCacheBuilder::fipsSign()
{
    if ( appCacheOptions.cacheKind != Options::AppCacheKind::kernel )
        return;

    // find com.apple.kec.corecrypto in collection being built
    __block const dyld3::MachOAnalyzer* kextMA = nullptr;
    forEachCacheDylib(^(const dyld3::MachOAnalyzer *ma, const std::string &dylibID,
                        DylibStripMode stripMode, const std::vector<std::string>& dependencies,
                        Diagnostics& dylibDiag, bool& stop) {
        if ( dylibID == "com.apple.kec.corecrypto" ) {
            kextMA = ma;
            stop = true;
        }
    });

    if ( kextMA == nullptr ) {
        _diagnostics.warning("Could not find com.apple.kec.corecrypto, skipping FIPS sealing");
        return;
    }

    // find location in com.apple.kec.corecrypto to store hash of __text section
    uint64_t hashStoreSize;
    const void* hashStoreLocation = kextMA->findSectionContent("__TEXT", "__fips_hmacs", hashStoreSize);
    if ( hashStoreLocation == nullptr ) {
        _diagnostics.warning("Could not find __TEXT/__fips_hmacs section in com.apple.kec.corecrypto, skipping FIPS sealing");
        return;
    }
    if ( hashStoreSize != 32 ) {
        _diagnostics.warning("__TEXT/__fips_hmacs section in com.apple.kec.corecrypto is not 32 bytes in size, skipping FIPS sealing");
        return;
    }

    // compute hmac hash of __text section.  It may be in __TEXT_EXEC or __TEXT
    uint64_t textSize;
    const void* textLocation = kextMA->findSectionContent("__TEXT", "__text", textSize);
    if ( textLocation == nullptr ) {
        textLocation = kextMA->findSectionContent("__TEXT_EXEC", "__text", textSize);
    }
    if ( textLocation == nullptr ) {
        _diagnostics.warning("Could not find __TEXT/__text section in com.apple.kec.corecrypto, skipping FIPS sealing");
        return;
    }
    unsigned char hmac_key = 0;
    CCHmac(kCCHmacAlgSHA256, &hmac_key, 1, textLocation, textSize, (void*)hashStoreLocation); // store hash directly into hashStoreLocation
}

void AppCacheBuilder::generateUUID() {
    uint8_t* uuidLoc = cacheHeader.uuid->uuid;
    assert(uuid_is_null(uuidLoc));

    CCDigestRef digestRef = CCDigestCreate(kCCDigestSHA256);
    forEachRegion(^(const Region &region) {
        if ( _diagnostics.hasError() )
            return;
        if ( region.sizeInUse == 0 )
            return;
        int result = CCDigestUpdate(digestRef, region.buffer, region.sizeInUse);
        if ( result != 0 ) {
            _diagnostics.error("Could not generate UUID: %d", result);
            return;
        }
    });
    if ( !_diagnostics.hasError() ) {
        uint8_t buffer[CCDigestGetOutputSize(kCCDigestSHA256)];
        int result = CCDigestFinal(digestRef, buffer);
        memcpy(cacheHeader.uuid->uuid, buffer, sizeof(cacheHeader.uuid->uuid));
        if ( result != 0 ) {
            _diagnostics.error("Could not finalize UUID: %d", result);
        }
    }
    CCDigestDestroy(digestRef);
    if ( _diagnostics.hasError() )
        return;

    // Update the prelink info dictionary too
    if ( prelinkInfoDict != nullptr ) {
        CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, &cacheHeader.uuid->uuid[0], sizeof(cacheHeader.uuid->uuid));
        CFDictionarySetValue(prelinkInfoDict, CFSTR("_PrelinkKCID"), dataRef);
        CFRelease(dataRef);

        CFErrorRef errorRef = nullptr;
        CFDataRef xmlData = CFPropertyListCreateData(kCFAllocatorDefault, prelinkInfoDict,
                                                     kCFPropertyListXMLFormat_v1_0, 0, &errorRef);
        if (errorRef != nullptr) {
            CFStringRef errorString = CFErrorCopyDescription(errorRef);
            _diagnostics.error("Could not serialise plist because :%s",
                               CFStringGetCStringPtr(errorString, kCFStringEncodingASCII));
            CFRelease(xmlData);
            CFRelease(errorRef);
            return;
        } else {
            CFIndex xmlDataLength = CFDataGetLength(xmlData);
            if ( xmlDataLength > prelinkInfoRegion.bufferSize ) {
                _diagnostics.error("Overflow in prelink info segment.  0x%llx vs 0x%llx",
                                   (uint64_t)xmlDataLength, prelinkInfoRegion.bufferSize);
                CFRelease(xmlData);
                return;
            }

            // Write the prelink info in to the buffer
            memcpy(prelinkInfoRegion.buffer, CFDataGetBytePtr(xmlData), xmlDataLength);
            CFRelease(xmlData);
        }
    }
}


void AppCacheBuilder::writeFile(const std::string& path)
{
    std::string pathTemplate = path + "-XXXXXX";
    size_t templateLen = strlen(pathTemplate.c_str())+2;
    BLOCK_ACCCESSIBLE_ARRAY(char, pathTemplateSpace, templateLen);
    strlcpy(pathTemplateSpace, pathTemplate.c_str(), templateLen);
    int fd = mkstemp(pathTemplateSpace);
    if ( fd == -1 ) {
        _diagnostics.error("could not open file %s", pathTemplateSpace);
        return;
    }
    uint64_t cacheFileSize = 0;
    // FIXME: Do we ever need to avoid allocating space for zero fill?
    cacheFileSize = _readOnlyRegion.cacheFileOffset + _readOnlyRegion.sizeInUse;

    // set final cache file size (may help defragment file)
    ::ftruncate(fd, cacheFileSize);

    // Write the whole buffer
    uint64_t writtenSize = pwrite(fd, (const uint8_t*)_fullAllocatedBuffer, cacheFileSize, 0);
    if (writtenSize == cacheFileSize) {
        ::fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); // mkstemp() makes file "rw-------", switch it to "rw-r--r--"
        if ( ::rename(pathTemplateSpace, path.c_str()) == 0) {
            ::close(fd);
            return; // success
        }
    } else {
        _diagnostics.error("could not write whole file.  %lld bytes out of %lld were written",
                           writtenSize, cacheFileSize);
        return;
    }
    ::close(fd);
    ::unlink(pathTemplateSpace);
}

void AppCacheBuilder::writeBuffer(uint8_t*& buffer, uint64_t& bufferSize) const {
    bufferSize = _readOnlyRegion.cacheFileOffset + _readOnlyRegion.sizeInUse;
    buffer = (uint8_t*)malloc(bufferSize);

    forEachRegion(^(const Region &region) {
        if ( region.sizeInUse == 0 )
            return;
        memcpy(buffer + region.cacheFileOffset, (const uint8_t*)region.buffer, region.sizeInUse);
    });
}
