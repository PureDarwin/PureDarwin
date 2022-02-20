/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2009-2012 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/syslimits.h>
#include <mach-o/arch.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_priv.h>
#include <bootstrap.h>
#include <mach/mach.h>
#include <dispatch/dispatch.h>
#include <uuid/uuid.h>

#include <TargetConditionals.h>

#include <map>
#include <vector>
#include <iostream>
#include <optional>

#include "ClosureBuilder.h"
#include "DyldSharedCache.h"
#include "ClosureFileSystemPhysical.h"
#include "JSONWriter.h"
#include "Trie.hpp"
#include "dsc_extractor.h"

#include "objc-shared-cache.h"

#if TARGET_OS_OSX
#define DSC_BUNDLE_REL_PATH "../../lib/dsc_extractor.bundle"
#else
#define DSC_BUNDLE_REL_PATH "../lib/dsc_extractor.bundle"
#endif

using dyld3::closure::ClosureBuilder;
using dyld3::closure::FileSystemPhysical;

// mmap() an shared cache file read/only but laid out like it would be at runtime
static const DyldSharedCache* mapCacheFile(const char* path)
{
    struct stat statbuf;
    if ( ::stat(path, &statbuf) ) {
        fprintf(stderr, "Error: stat failed for dyld shared cache at %s\n", path);
        return nullptr;
    }
    
    int cache_fd = ::open(path, O_RDONLY);
    if (cache_fd < 0) {
        fprintf(stderr, "Error: failed to open shared cache file at %s\n", path);
        return nullptr;
    }
    
    uint8_t  firstPage[4096];
    if ( ::pread(cache_fd, firstPage, 4096, 0) != 4096 ) {
        fprintf(stderr, "Error: failed to read shared cache file at %s\n", path);
        return nullptr;
    }
    const dyld_cache_header*       header   = (dyld_cache_header*)firstPage;
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)(firstPage + header->mappingOffset);
    const dyld_cache_mapping_info* lastMapping = &mappings[header->mappingCount - 1];

    size_t vmSize = (size_t)(lastMapping->address + lastMapping->size - mappings[0].address);
    vm_address_t result;
    kern_return_t r = ::vm_allocate(mach_task_self(), &result, vmSize, VM_FLAGS_ANYWHERE);
    if ( r != KERN_SUCCESS ) {
        fprintf(stderr, "Error: failed to allocate space to load shared cache file at %s\n", path);
        return nullptr;
    }
    for (int i=0; i < header->mappingCount; ++i) {
        void* mapped_cache = ::mmap((void*)(result + mappings[i].address - mappings[0].address), (size_t)mappings[i].size,
                                    PROT_READ, MAP_FIXED | MAP_PRIVATE, cache_fd, mappings[i].fileOffset);
        if (mapped_cache == MAP_FAILED) {
            fprintf(stderr, "Error: mmap() for shared cache at %s failed, errno=%d\n", path, errno);
            return nullptr;
        }
    }
    ::close(cache_fd);
    
    return (DyldSharedCache*)result;
}

enum Mode {
    modeNone,
    modeList,
    modeMap,
    modeDependencies,
    modeSlideInfo,
    modeVerboseSlideInfo,
    modeTextInfo,
    modeLinkEdit,
    modeLocalSymbols,
    modeJSONMap,
    modeJSONDependents,
    modeSectionSizes,
    modeStrings,
    modeInfo,
    modeSize,
    modeObjCProtocols,
    modeObjCImpCaches,
    modeObjCClasses,
    modeObjCSelectors,
    modeExtract,
    modePatchTable,
    modeListDylibsWithSection
};

struct Options {
    Mode            mode;
    const char*     dependentsOfPath;
    const char*     extractionDir;
    const char*     segmentName;
    const char*     sectionName;
    bool            printUUIDs;
    bool            printVMAddrs;
    bool            printDylibVersions;
    bool            printInodes;
};


void usage() {
    fprintf(stderr, "Usage: dyld_shared_cache_util -list [ -uuid ] [-vmaddr] | -dependents <dylib-path> [ -versions ] | -linkedit | -map | -slide_info | -verbose_slide_info | -info | -extract <dylib-dir>  [ shared-cache-file ] \n");
}

static void checkMode(Mode mode) {
    if ( mode != modeNone ) {
        fprintf(stderr, "Error: select one of: -list, -dependents, -info, -slide_info, -verbose_slide_info, -linkedit, -map, -extract, or -size\n");
        usage();
        exit(1);
    }
}

struct SegmentInfo
{
    uint64_t    vmAddr;
    uint64_t    vmSize;
    const char* installName;
    const char* segName;
};

static void buildSegmentInfo(const DyldSharedCache* dyldCache, std::vector<SegmentInfo>& segInfos)
{
    dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
        dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
        ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo& info, bool& stop) {
            segInfos.push_back({info.vmAddr, info.vmSize, installName, info.segName});
        });
    });

    std::sort(segInfos.begin(), segInfos.end(), [](const SegmentInfo& l, const SegmentInfo& r) -> bool {
        return l.vmAddr < r.vmAddr;
    });
}

static void printSlideInfoForDataRegion(const DyldSharedCache* dyldCache, uint64_t dataStartAddress, uint64_t dataSize,
                                        const uint8_t* dataPagesStart,
                                        const dyld_cache_slide_info* slideInfoHeader, bool verboseSlideInfo) {

    printf("slide info version=%d\n", slideInfoHeader->version);
    if ( slideInfoHeader->version == 1 ) {
        printf("toc_count=%d, data page count=%lld\n", slideInfoHeader->toc_count, dataSize/4096);
        const dyld_cache_slide_info_entry* entries = (dyld_cache_slide_info_entry*)((char*)slideInfoHeader + slideInfoHeader->entries_offset);
        const uint16_t* tocs = (uint16_t*)((char*)slideInfoHeader + slideInfoHeader->toc_offset);
        for(int i=0; i < slideInfoHeader->toc_count; ++i) {
            printf("0x%08llX: [% 5d,% 5d] ", dataStartAddress + i*4096, i, tocs[i]);
            const dyld_cache_slide_info_entry* entry = &entries[tocs[i]];
            for(int j=0; j < slideInfoHeader->entries_size; ++j)
                printf("%02X", entry->bits[j]);
            printf("\n");
        }
    }
    else if ( slideInfoHeader->version == 2 ) {
        const dyld_cache_slide_info2* slideInfo = (dyld_cache_slide_info2*)(slideInfoHeader);
        printf("page_size=%d\n", slideInfo->page_size);
        printf("delta_mask=0x%016llX\n", slideInfo->delta_mask);
        printf("value_add=0x%016llX\n", slideInfo->value_add);
        printf("page_starts_count=%d, page_extras_count=%d\n", slideInfo->page_starts_count, slideInfo->page_extras_count);
        const uint16_t* starts = (uint16_t* )((char*)slideInfo + slideInfo->page_starts_offset);
        const uint16_t* extras = (uint16_t* )((char*)slideInfo + slideInfo->page_extras_offset);
        for (int i=0; i < slideInfo->page_starts_count; ++i) {
            const uint16_t start = starts[i];
            auto rebaseChain = [&](uint8_t* pageContent, uint16_t startOffset)
            {
                uintptr_t slideAmount = 0;
                const uintptr_t   deltaMask    = (uintptr_t)(slideInfo->delta_mask);
                const uintptr_t   valueMask    = ~deltaMask;
                const uintptr_t   valueAdd     = (uintptr_t)(slideInfo->value_add);
                const unsigned    deltaShift   = __builtin_ctzll(deltaMask) - 2;

                uint32_t pageOffset = startOffset;
                uint32_t delta = 1;
                while ( delta != 0 ) {
                    uint8_t* loc = pageContent + pageOffset;
                    uintptr_t rawValue = *((uintptr_t*)loc);
                    delta = (uint32_t)((rawValue & deltaMask) >> deltaShift);
                    uintptr_t value = (rawValue & valueMask);
                    if ( value != 0 ) {
                        value += valueAdd;
                        value += slideAmount;
                    }
                    printf("    [% 5d + 0x%04llX]: 0x%016llX = 0x%016llX\n", i, (uint64_t)(pageOffset), (uint64_t)rawValue, (uint64_t)value);
                    pageOffset += delta;
                }
            };
            if ( start == DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE ) {
                printf("page[% 5d]: no rebasing\n", i);
            }
            else if ( start & DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA ) {
                printf("page[% 5d]: ", i);
                int j=(start & 0x3FFF);
                bool done = false;
                do {
                    uint16_t aStart = extras[j];
                    printf("start=0x%04X ", aStart & 0x3FFF);
                    if ( verboseSlideInfo ) {
                        uint8_t* page = (uint8_t*)(long)(dataPagesStart + (slideInfo->page_size*i));
                        uint16_t pageStartOffset = (aStart & 0x3FFF)*4;
                        rebaseChain(page, pageStartOffset);
                    }
                    done = (extras[j] & DYLD_CACHE_SLIDE_PAGE_ATTR_END);
                    ++j;
                } while ( !done );
                printf("\n");
            }
            else {
                printf("page[% 5d]: start=0x%04X\n", i, starts[i]);
                if ( verboseSlideInfo ) {
                    uint8_t* page = (uint8_t*)(long)(dataPagesStart + (slideInfo->page_size*i));
                    uint16_t pageStartOffset = start*4;
                    rebaseChain(page, pageStartOffset);
                }
            }
        }
    }
    else if ( slideInfoHeader->version == 3 ) {
        const dyld_cache_slide_info3* slideInfo = (dyld_cache_slide_info3*)(slideInfoHeader);
        printf("page_size=%d\n", slideInfo->page_size);
        printf("page_starts_count=%d\n", slideInfo->page_starts_count);
        printf("auth_value_add=0x%016llX\n", slideInfo->auth_value_add);
        const uintptr_t authValueAdd = (uintptr_t)(slideInfo->auth_value_add);
        for (int i=0; i < slideInfo->page_starts_count; ++i) {
            uint16_t delta = slideInfo->page_starts[i];
            if ( delta == DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE ) {
                printf("page[% 5d]: no rebasing\n", i);
                continue;
            }

            printf("page[% 5d]: start=0x%04X\n", i, delta);
            if ( !verboseSlideInfo )
                continue;

            delta = delta/sizeof(uint64_t); // initial offset is byte based
            const uint8_t* pageStart = dataPagesStart + (i * slideInfo->page_size);
            const dyld_cache_slide_pointer3* loc = (dyld_cache_slide_pointer3*)pageStart;
            do {
                loc += delta;
                delta = loc->plain.offsetToNextPointer;
                dyld3::MachOLoaded::ChainedFixupPointerOnDisk ptr;
                ptr.raw64 = *((uint64_t*)loc);
                if ( loc->auth.authenticated ) {
                    uint64_t target = authValueAdd + loc->auth.offsetFromSharedCacheBase;
                    uint64_t targetValue = ptr.arm64e.signPointer((void*)loc, target);
                    printf("    [% 5d + 0x%04llX]: 0x%016llX (JOP: diversity %d, address %s, %s)\n",
                           i, (uint64_t)((const uint8_t*)loc - pageStart), targetValue,
                           ptr.arm64e.authBind.diversity, ptr.arm64e.authBind.addrDiv ? "true" : "false",
                           ptr.arm64e.keyName());
                }
                else {
                    uint64_t targetValue = ptr.arm64e.unpackTarget();
                    printf("    [% 5d + 0x%04llX]: 0x%016llX\n", i, (uint64_t)((const uint8_t*)loc - pageStart), targetValue);
                }
            } while (delta != 0);
        }
    }
    else if ( slideInfoHeader->version == 4 ) {
        const dyld_cache_slide_info4* slideInfo = (dyld_cache_slide_info4*)(slideInfoHeader);
        printf("page_size=%d\n", slideInfo->page_size);
        printf("delta_mask=0x%016llX\n", slideInfo->delta_mask);
        printf("value_add=0x%016llX\n", slideInfo->value_add);
        printf("page_starts_count=%d, page_extras_count=%d\n", slideInfo->page_starts_count, slideInfo->page_extras_count);
        const uint16_t* starts = (uint16_t* )((char*)slideInfo + slideInfo->page_starts_offset);
        const uint16_t* extras = (uint16_t* )((char*)slideInfo + slideInfo->page_extras_offset);
        for (int i=0; i < slideInfo->page_starts_count; ++i) {
            const uint16_t start = starts[i];
            auto rebaseChainV4 = [&](uint8_t* pageContent, uint16_t startOffset)
            {
                uintptr_t slideAmount = 0;
                const uintptr_t   deltaMask    = (uintptr_t)(slideInfo->delta_mask);
                const uintptr_t   valueMask    = ~deltaMask;
                const uintptr_t   valueAdd     = (uintptr_t)(slideInfo->value_add);
                const unsigned    deltaShift   = __builtin_ctzll(deltaMask) - 2;

                uint32_t pageOffset = startOffset;
                uint32_t delta = 1;
                while ( delta != 0 ) {
                    uint8_t* loc = pageContent + pageOffset;
                    uint32_t rawValue = *((uint32_t*)loc);
                    delta = (uint32_t)((rawValue & deltaMask) >> deltaShift);
                    uintptr_t value = (rawValue & valueMask);
                    if ( (value & 0xFFFF8000) == 0 ) {
                        // small positive non-pointer, use as-is
                    }
                    else if ( (value & 0x3FFF8000) == 0x3FFF8000 ) {
                        // small negative non-pointer
                        value |= 0xC0000000;
                    }
                    else  {
                        value += valueAdd;
                        value += slideAmount;
                    }
                    printf("    [% 5d + 0x%04X]: 0x%08X\n", i, pageOffset, rawValue);
                    pageOffset += delta;
                }
            };
            if ( start == DYLD_CACHE_SLIDE4_PAGE_NO_REBASE ) {
                printf("page[% 5d]: no rebasing\n", i);
            }
            else if ( start & DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA ) {
                printf("page[% 5d]: ", i);
                int j=(start & DYLD_CACHE_SLIDE4_PAGE_INDEX);
                bool done = false;
                do {
                    uint16_t aStart = extras[j];
                    printf("start=0x%04X ", aStart & DYLD_CACHE_SLIDE4_PAGE_INDEX);
                    if ( verboseSlideInfo ) {
                        uint8_t* page = (uint8_t*)(long)(dataPagesStart + (slideInfo->page_size*i));
                        uint16_t pageStartOffset = (aStart & DYLD_CACHE_SLIDE4_PAGE_INDEX)*4;
                        rebaseChainV4(page, pageStartOffset);
                    }
                    done = (extras[j] & DYLD_CACHE_SLIDE4_PAGE_EXTRA_END);
                    ++j;
                } while ( !done );
                printf("\n");
            }
            else {
                printf("page[% 5d]: start=0x%04X\n", i, starts[i]);
                if ( verboseSlideInfo ) {
                    uint8_t* page = (uint8_t*)(long)(dataPagesStart + (slideInfo->page_size*i));
                    uint16_t pageStartOffset = start*4;
                    rebaseChainV4(page, pageStartOffset);
                }
            }
        }
    }
}


static void findImageAndSegment(const DyldSharedCache* dyldCache, const std::vector<SegmentInfo>& segInfos, uint32_t cacheOffset, SegmentInfo* found)
{
    const uint64_t locVmAddr = dyldCache->unslidLoadAddress() + cacheOffset;
    const SegmentInfo target = { locVmAddr, 0, NULL, NULL };
    const auto lowIt = std::lower_bound(segInfos.begin(), segInfos.end(), target,
                                                                        [](const SegmentInfo& l, const SegmentInfo& r) -> bool {
                                                                            return l.vmAddr+l.vmSize < r.vmAddr+r.vmSize;
                                                                    });
    *found = *lowIt;
}


int main (int argc, const char* argv[]) {

    const char* sharedCachePath = nullptr;

    Options options;
    options.mode = modeNone;
    options.printUUIDs = false;
    options.printVMAddrs = false;
    options.printDylibVersions = false;
    options.printInodes = false;
    options.dependentsOfPath = NULL;
    options.extractionDir = NULL;

    bool printStrings = false;
    bool printExports = false;

    for (uint32_t i = 1; i < argc; i++) {
        const char* opt = argv[i];
        if (opt[0] == '-') {
            if (strcmp(opt, "-list") == 0) {
                checkMode(options.mode);
                options.mode = modeList;
            }
            else if (strcmp(opt, "-dependents") == 0) {
                checkMode(options.mode);
                options.mode = modeDependencies;
                options.dependentsOfPath = argv[++i];
                if ( i >= argc ) {
                    fprintf(stderr, "Error: option -depdendents requires an argument\n");
                    usage();
                    exit(1);
                }
            }
            else if (strcmp(opt, "-linkedit") == 0) {
                checkMode(options.mode);
                options.mode = modeLinkEdit;
            }
            else if (strcmp(opt, "-info") == 0) {
                checkMode(options.mode);
                options.mode = modeInfo;
            }
            else if (strcmp(opt, "-slide_info") == 0) {
                checkMode(options.mode);
                options.mode = modeSlideInfo;
            }
            else if (strcmp(opt, "-verbose_slide_info") == 0) {
                checkMode(options.mode);
                options.mode = modeVerboseSlideInfo;
            }
            else if (strcmp(opt, "-text_info") == 0) {
                checkMode(options.mode);
                options.mode = modeTextInfo;
            }
            else if (strcmp(opt, "-local_symbols") == 0) {
                checkMode(options.mode);
                options.mode = modeLocalSymbols;
            }
            else if (strcmp(opt, "-strings") == 0) {
                if (options.mode != modeStrings)
                    checkMode(options.mode);
                options.mode = modeStrings;
                printStrings = true;
            }
            else if (strcmp(opt, "-sections") == 0) {
                checkMode(options.mode);
                options.mode = modeSectionSizes;
            }
            else if (strcmp(opt, "-exports") == 0) {
                if (options.mode != modeStrings)
                    checkMode(options.mode);
                options.mode = modeStrings;
                printExports = true;
            }
            else if (strcmp(opt, "-map") == 0) {
                checkMode(options.mode);
                options.mode = modeMap;
            }
            else if (strcmp(opt, "-json-map") == 0) {
                checkMode(options.mode);
                options.mode = modeJSONMap;
            }
            else if (strcmp(opt, "-json-dependents") == 0) {
                checkMode(options.mode);
                options.mode = modeJSONDependents;
            }
            else if (strcmp(opt, "-size") == 0) {
                checkMode(options.mode);
                options.mode = modeSize;
            }
            else if (strcmp(opt, "-objc-protocols") == 0) {
                checkMode(options.mode);
                options.mode = modeObjCProtocols;
            }
            else if (strcmp(opt, "-objc-imp-caches") == 0) {
                checkMode(options.mode);
                options.mode = modeObjCImpCaches;
            }
            else if (strcmp(opt, "-objc-classes") == 0) {
                checkMode(options.mode);
                options.mode = modeObjCClasses;
            }
            else if (strcmp(opt, "-objc-selectors") == 0) {
                checkMode(options.mode);
                options.mode = modeObjCSelectors;
            }
            else if (strcmp(opt, "-extract") == 0) {
                checkMode(options.mode);
                options.mode = modeExtract;
                options.extractionDir = argv[++i];
                if ( i >= argc ) {
                    fprintf(stderr, "Error: option -extract requires a directory argument\n");
                    usage();
                    exit(1);
                }
            }
            else if (strcmp(opt, "-uuid") == 0) {
                options.printUUIDs = true;
            }
            else if (strcmp(opt, "-inode") == 0) {
                options.printInodes = true;
            }
            else if (strcmp(opt, "-versions") == 0) {
                options.printDylibVersions = true;
            }
            else if (strcmp(opt, "-vmaddr") == 0) {
                options.printVMAddrs = true;
            }
            else if (strcmp(opt, "-patch_table") == 0) {
                options.mode = modePatchTable;
            }
            else if (strcmp(opt, "-list_dylibs_with_section") == 0) {
                options.mode = modeListDylibsWithSection;
                options.segmentName = argv[++i];
                options.sectionName = argv[++i];
                if ( i >= argc ) {
                    fprintf(stderr, "Error: option -list_dylibs_with_section requires a segment and section name\n");
                    usage();
                    exit(1);
                }
            }
            else {
                fprintf(stderr, "Error: unrecognized option %s\n", opt);
                usage();
                exit(1);
            }
        }
        else {
            sharedCachePath = opt;
        }
    }

    if ( options.mode == modeNone ) {
        fprintf(stderr, "Error: select one of -list, -dependents, -info, -linkedit, or -map\n");
        usage();
        exit(1);
    }

    if ( options.mode != modeSlideInfo && options.mode != modeVerboseSlideInfo ) {
        if ( options.printUUIDs && (options.mode != modeList) )
            fprintf(stderr, "Warning: -uuid option ignored outside of -list mode\n");

        if ( options.printVMAddrs && (options.mode != modeList) )
            fprintf(stderr, "Warning: -vmaddr option ignored outside of -list mode\n");

        if ( options.printDylibVersions && (options.mode != modeDependencies) )
            fprintf(stderr, "Warning: -versions option ignored outside of -dependents mode\n");

        if ( (options.mode == modeDependencies) && (options.dependentsOfPath == NULL) ) {
            fprintf(stderr, "Error: -dependents given, but no dylib path specified\n");
            usage();
            exit(1);
        }
    }

    const DyldSharedCache* dyldCache = nullptr;
    if ( sharedCachePath != nullptr ) {
        dyldCache = mapCacheFile(sharedCachePath);
        // mapCacheFile prints an error if something goes wrong, so just return in that case.
        if ( dyldCache == nullptr )
            return 1;
    }
    else {
        size_t cacheLength;
        dyldCache = (DyldSharedCache*)_dyld_get_shared_cache_range(&cacheLength);
        if (dyldCache == nullptr) {
            fprintf(stderr, "Could not get in-memory shared cache\n");
            return 1;
        }
        if ( options.mode == modeObjCClasses ) {
            fprintf(stderr, "Cannot use -objc-classes with a live cache.  Please run with a path to an on-disk cache file\n");
            return 1;
        }
    }

    if ( options.mode == modeSlideInfo || options.mode == modeVerboseSlideInfo ) {
        if ( !dyldCache->hasSlideInfo() ) {
            fprintf(stderr, "Error: dyld shared cache does not contain slide info\n");
            exit(1);
        }

        const bool verboseSlideInfo = (options.mode == modeVerboseSlideInfo);
        dyldCache->forEachSlideInfo(^(uint64_t mappingStartAddress, uint64_t mappingSize, const uint8_t *mappingPagesStart,
                                      uint64_t slideInfoOffset, uint64_t slideInfoSize, const dyld_cache_slide_info *slideInfoHeader) {
            printSlideInfoForDataRegion(dyldCache, mappingStartAddress, mappingSize, mappingPagesStart,
                                        slideInfoHeader, verboseSlideInfo);
        });
        return 0;
    }
    else if ( options.mode == modeInfo ) {
        const dyld_cache_header* header = &dyldCache->header;
        uuid_string_t uuidString;
        uuid_unparse_upper(header->uuid, uuidString);
        printf("uuid: %s\n", uuidString);

        dyld3::Platform platform = dyldCache->platform();
        printf("platform: %s\n", dyld3::MachOFile::platformName(platform));
        printf("built by: %s\n", header->locallyBuiltCache ? "local machine" : "B&I");
        printf("cache type: %s\n", header->cacheType ? "production" : "development");
        printf("image count: %u\n", header->imagesCount);
        if ( (header->mappingOffset >= 0x78) && (header->branchPoolsOffset != 0) ) {
            printf("branch pool count:  %u\n", header->branchPoolsCount);
        }
        if ( dyldCache->hasSlideInfo() ) {
            uint32_t pageSize            = 0x4000; // fix me for intel
            uint32_t possibleSlideValues = (uint32_t)(header->maxSlide/pageSize);
            uint32_t entropyBits = 0;
            if ( possibleSlideValues > 1 )
                entropyBits = __builtin_clz(possibleSlideValues - 1);
            printf("ASLR entropy: %u-bits\n", entropyBits);
        }
        printf("mappings:\n");
        dyldCache->forEachRegion(^(const void *content, uint64_t vmAddr, uint64_t size,
                                   uint32_t initProt, uint32_t maxProt, uint64_t flags) {
            std::string mappingName = "";
            if ( maxProt & VM_PROT_EXECUTE ) {
                mappingName = "__TEXT";
            } else if ( maxProt & VM_PROT_WRITE ) {
                // Start off with __DATA or __AUTH
                if ( flags & DYLD_CACHE_MAPPING_AUTH_DATA )
                    mappingName = "__AUTH";
                else
                    mappingName = "__DATA";
                // Then add one of "", _DIRTY, or _CONST
                if ( flags & DYLD_CACHE_MAPPING_DIRTY_DATA )
                    mappingName += "_DIRTY";
                else if ( flags & DYLD_CACHE_MAPPING_CONST_DATA )
                    mappingName += "_CONST";
            }
            else if ( maxProt & VM_PROT_READ ) {
                mappingName = "__LINKEDIT";
            } else {
                mappingName = "*unknown*";
            }
            uint64_t fileOffset = (uint8_t*)content - (uint8_t*)dyldCache;
            printf("%16s %4lluMB,  file offset: 0x%08llX -> 0x%08llX,  address: 0x%08llX -> 0x%08llX\n",
                   mappingName.c_str(), size / (1024*1024), fileOffset, fileOffset + size, vmAddr, vmAddr + size);
        });
        if ( header->codeSignatureOffset != 0 ) {
            uint64_t size = header->codeSignatureSize;
            uint64_t csAddr = dyldCache->getCodeSignAddress();
            if ( size != 0 )
                printf("%16s %4lluMB,  file offset: 0x%08llX -> 0x%08llX,  address: 0x%08llX -> 0x%08llX\n",
                       "code sign", size/(1024*1024), header->codeSignatureOffset, header->codeSignatureOffset + size, csAddr, csAddr + size);
        }
        dyldCache->forEachSlideInfo(^(uint64_t mappingStartAddress, uint64_t mappingSize, const uint8_t *mappingPagesStart,
                                      uint64_t slideInfoOffset, uint64_t slideInfoSize, const dyld_cache_slide_info *slideInfoHeader) {

            printf("slide info:      %4lluKB,  file offset: 0x%08llX -> 0x%08llX\n",
                   slideInfoSize/1024, slideInfoOffset, slideInfoOffset + slideInfoSize);
        });
        if ( header->localSymbolsOffset != 0 )
            printf("local symbols:    %3lluMB,  file offset: 0x%08llX -> 0x%08llX\n",
                   header->localSymbolsSize/(1024*1024), header->localSymbolsOffset, header->localSymbolsOffset + header->localSymbolsSize);
    }
    else if ( options.mode == modeTextInfo ) {
        const dyld_cache_header* header = &dyldCache->header;
        printf("dylib text infos (count=%llu):\n", header->imagesTextCount);
        dyldCache->forEachImageTextSegment(^(uint64_t loadAddressUnslid, uint64_t textSegmentSize, const unsigned char *dylibUUID, const char *installName, bool &stop) {
            uuid_string_t uuidString;
            uuid_unparse_upper(dylibUUID, uuidString);
            printf("   0x%09llX -> 0x%09llX  <%s>  %s\n", loadAddressUnslid, loadAddressUnslid + textSegmentSize, uuidString, installName);
        });
    }
    else if ( options.mode == modeLocalSymbols ) {
        if ( !dyldCache->hasLocalSymbolsInfo() ) {
            fprintf(stderr, "Error: dyld shared cache does not contain local symbols info\n");
            exit(1);
        }
        const bool is64 = (strstr(dyldCache->archName(), "64") != NULL);
        const uint32_t nlistFileOffset = (uint32_t)((uint8_t*)dyldCache->getLocalNlistEntries() - (uint8_t*)dyldCache);
        const uint32_t nlistCount = dyldCache->getLocalNlistCount();
        const uint32_t nlistByteSize = is64 ? nlistCount*16 : nlistCount*12;
        const char* localStrings = dyldCache->getLocalStrings();
        const uint32_t stringsFileOffset = (uint32_t)((uint8_t*)localStrings - (uint8_t*)dyldCache);
        const uint32_t stringsSize = dyldCache->getLocalStringsSize();

        printf("local symbols nlist array:  %3uMB,  file offset: 0x%08X -> 0x%08X\n", nlistByteSize/(1024*1024), nlistFileOffset, nlistFileOffset+nlistByteSize);
        printf("local symbols string pool:  %3uMB,  file offset: 0x%08X -> 0x%08X\n", stringsSize/(1024*1024), stringsFileOffset, stringsFileOffset+stringsSize);

        __block uint32_t entriesCount = 0;
        dyldCache->forEachLocalSymbolEntry(^(uint32_t dylibOffset, uint32_t nlistStartIndex, uint32_t nlistCount, bool &stop) {
            const char* imageName = dyldCache->getIndexedImagePath(entriesCount);
            printf("   nlistStartIndex=%5d, nlistCount=%5d, image=%s\n", nlistStartIndex, nlistCount, imageName);
#if 0
            if ( is64 ) {
                const nlist_64* symTab = (nlist_64*)((char*)dyldCache + nlistFileOffset);
                for (int e = 0; e < nlistCount; ++e) {
                    const nlist_64* entry = &symTab[nlistStartIndex + e];
                    printf("     nlist[%d].str=%d, %s\n", e, entry->n_un.n_strx, &localStrings[entry->n_un.n_strx]);
                    printf("     nlist[%d].value=0x%0llX\n", e, entry->n_value);
                }
            }
#endif
            entriesCount++;
        });
        printf("local symbols by dylib (count=%d):\n", entriesCount);
    }
    else if ( options.mode == modeJSONMap ) {
        std::string buffer = dyldCache->generateJSONMap("unknown");
        printf("%s\n", buffer.c_str());
    }
    else if ( options.mode == modeJSONDependents ) {
        std::cout <<  dyldCache->generateJSONDependents();
    }
    else if ( options.mode == modeStrings ) {
        if (printStrings) {
            dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
                const dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
                int64_t slide = ma->getSlide();
                ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo& info, bool malformedSectionRange, bool& stop) {
                    if ( ( (info.sectFlags & SECTION_TYPE) == S_CSTRING_LITERALS ) ) {
                        if ( malformedSectionRange ) {
                            stop = true;
                            return;
                        }
                        const uint8_t* content = (uint8_t*)(info.sectAddr + slide);
                        const char* s   = (char*)content;
                        const char* end = s + info.sectSize;
                        while ( s < end ) {
                            printf("%s: %s\n", ma->installName(), s);
                            while (*s != '\0' )
                                ++s;
                            ++s;
                        }
                    }
                });
            });
        }

        if (printExports) {
            dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
                const dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
                uint32_t exportTrieRuntimeOffset;
                uint32_t exportTrieSize;
                if ( ma->hasExportTrie(exportTrieRuntimeOffset, exportTrieSize) ) {
                    const uint8_t* start = (uint8_t*)mh + exportTrieRuntimeOffset;
                    const uint8_t* end = start + exportTrieSize;
                    std::vector<ExportInfoTrie::Entry> exports;
                    if ( !ExportInfoTrie::parseTrie(start, end, exports) ) {
                        return;
                    }

                    for (const ExportInfoTrie::Entry& entry: exports) {
                        printf("%s: %s\n", ma->installName(), entry.name.c_str());
                    }
                }
            });
        }
    }
    else if ( options.mode == modeSectionSizes ) {
        __block std::map<std::string, uint64_t> sectionSizes;
        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;
            ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stop) {
                std::string section = std::string(sectInfo.segInfo.segName) + " " + sectInfo.sectName;
                sectionSizes[section] += sectInfo.sectSize;
            });
        });
        for (const auto& keyAndValue : sectionSizes) {
            printf("%lld %s\n", keyAndValue.second, keyAndValue.first.c_str());
        }
    }
    else if ( options.mode == modeObjCProtocols ) {
        if ( dyldCache->objcOpt() == nullptr ) {
            fprintf(stderr, "Error: could not get optimized objc\n");
            return 1;
        }
        objc_opt::objc_protocolopt2_t* protocols = dyldCache->objcOpt()->protocolopt2();
        if ( protocols == nullptr ) {
            fprintf(stderr, "Error: could not get optimized objc protocols\n");
            return 1;
        }

        for (uint64_t index = 0; index != protocols->capacity; ++index) {
            const objc_opt::objc_classheader_t& clshi = protocols->classOffsets()[index];
            if ( clshi.clsOffset == 0 ) {
                fprintf(stderr, "[% 5lld]\n", index);
                continue;
            }
            const char* name = (const char*)(((const uint8_t*)protocols) + protocols->offsets()[index]);
            if ( !clshi.isDuplicate() ) {
                fprintf(stderr, "[% 5lld] -> (% 8d, % 8d) = %s\n", index, clshi.clsOffset, clshi.hiOffset, name);
                continue;
            }

            // class appears in more than one header
            uint32_t count = clshi.duplicateCount();
            fprintf(stderr, "[% 5lld] -> duplicates [% 5d..% 5d] = %s\n",
                    index, clshi.duplicateIndex(), clshi.duplicateIndex() + clshi.duplicateCount() - 1, name);

            const objc_opt::objc_classheader_t *list = &protocols->duplicateOffsets()[clshi.duplicateIndex()];
            for (uint32_t i = 0; i < count; i++) {
                fprintf(stderr, "  - [% 5lld] -> (% 8d, % 8d)\n", (uint64_t)(clshi.duplicateIndex() + i), list[i].clsOffset, list[i].hiOffset);
            }
        }
    }
    else if ( options.mode == modeObjCClasses ) {
        using dyld3::json::Node;
        using dyld3::json::NodeValueType;
        using ObjCClassInfo = dyld3::MachOAnalyzer::ObjCClassInfo;
        const bool rebased = false;

        std::string instancePrefix("-");
        std::string classPrefix("+");

        auto getString = ^const char *(const dyld3::MachOAnalyzer* ma, uint64_t nameVMAddr){
            dyld3::MachOAnalyzer::PrintableStringResult result;
            const char* name = ma->getPrintableString(nameVMAddr, result);
            if (result == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint)
                return name;
            return nullptr;
        };

        // Build a map of class vm addrs to their names so that categories know the
        // name of the class they are attaching to
        __block std::unordered_map<uint64_t, const char*> classVMAddrToName;
        __block std::unordered_map<uint64_t, const char*> metaclassVMAddrToName;
        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;
            const uint32_t pointerSize = ma->pointerSize();

            auto visitClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                                uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                if (auto className = getString(ma, objcClass.nameVMAddr(pointerSize))) {
                    if (isMetaClass)
                        metaclassVMAddrToName[classVMAddr] = className;
                    else
                        classVMAddrToName[classVMAddr] = className;
                }
            };

            Diagnostics diag;

            dyld3::MachOAnalyzer::VMAddrConverter vmAddrConverter = dyldCache->makeVMAddrConverter(rebased);
            ma->forEachObjCClass(diag, vmAddrConverter, visitClass);
        });

        // These are used only for the on-disk binaries we analyze
        __block std::vector<const char*>        onDiskChainedFixupBindTargets;
        __block std::unordered_map<uint64_t, const char*> onDiskClassVMAddrToName;
        __block std::unordered_map<uint64_t, const char*> onDiskMetaclassVMAddrToName;

        auto getProperties = ^(const dyld3::MachOAnalyzer* ma, uint64_t propertiesVMAddr,
                               const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            __block Node propertiesNode;
            auto visitProperty = ^(uint64_t propertyVMAddr, const dyld3::MachOAnalyzer::ObjCProperty& property) {
                // Get the name && attributes
                auto propertyName = getString(ma, property.nameVMAddr);
                auto propertyAttributes = getString(ma, property.attributesVMAddr);

                if (!propertyName || !propertyAttributes)
                    return;

                Node propertyNode;
                propertyNode.map["name"] = Node{propertyName};
                propertyNode.map["attributes"] = Node{propertyAttributes};
                propertiesNode.array.push_back(propertyNode);
            };
            ma->forEachObjCProperty(propertiesVMAddr, vmAddrConverter, visitProperty);
            return propertiesNode.array.empty() ? std::optional<Node>() : propertiesNode;
        };

        auto getClassProtocols = ^(const dyld3::MachOAnalyzer* ma, uint64_t protocolsVMAddr,
                                   const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            __block Node protocolsNode;

            auto visitProtocol = ^(uint64_t protocolVMAddr, const dyld3::MachOAnalyzer::ObjCProtocol& protocol) {
                if (const char *name = getString(ma, protocol.nameVMAddr)) {
                    protocolsNode.array.push_back(Node{name});
                }
            };

            ma->forEachObjCProtocol(protocolsVMAddr, vmAddrConverter, visitProtocol);

            return protocolsNode.array.empty() ? std::optional<Node>() : protocolsNode;
        };

        auto getProtocols = ^(const dyld3::MachOAnalyzer* ma,
                              const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            __block Node protocols;

            auto getMethods = ^(const dyld3::MachOAnalyzer* ma, uint64_t methodListVMAddr, const std::string &prefix, Node &node){
                auto visitMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    if (auto name = getString(ma, method.nameVMAddr)) {
                        node.array.push_back(Node{prefix + name});
                    }
                };

                ma->forEachObjCMethod(methodListVMAddr, vmAddrConverter, visitMethod);
            };

            auto visitProtocol = ^(Diagnostics& diag, uint64_t protoVMAddr,
                                   const dyld3::MachOAnalyzer::ObjCProtocol& objcProto) {
                const char* protoName = getString(ma, objcProto.nameVMAddr);
                if (!protoName)
                    return;

                Node entry;
                entry.map["protocolName"] = Node{protoName};

                if ( objcProto.protocolsVMAddr != 0 ) {
                    __block Node protocols;

                    auto visitProtocol = ^(uint64_t protocolRefVMAddr, const dyld3::MachOAnalyzer::ObjCProtocol &protocol) {
                        if (auto name = getString(ma, protocol.nameVMAddr)) {
                            protocols.array.push_back(Node{name});
                        }
                    };

                    ma->forEachObjCProtocol(objcProto.protocolsVMAddr, vmAddrConverter, visitProtocol);
                    if (!protocols.array.empty()) {
                        entry.map["protocols"] = protocols;
                    }
                }

                Node methods;
                getMethods(ma, objcProto.instanceMethodsVMAddr, instancePrefix, methods);
                getMethods(ma, objcProto.classMethodsVMAddr, classPrefix, methods);
                if (!methods.array.empty()) {
                    entry.map["methods"] = methods;
                }

                Node optMethods;
                getMethods(ma, objcProto.optionalInstanceMethodsVMAddr, instancePrefix, optMethods);
                getMethods(ma, objcProto.optionalClassMethodsVMAddr, classPrefix, optMethods);
                if (!optMethods.array.empty()) {
                    entry.map["optionalMethods"] = optMethods;
                }

                protocols.array.push_back(entry);
            };

            Diagnostics diag;
            ma->forEachObjCProtocol(diag, vmAddrConverter, visitProtocol);

            return protocols.array.empty() ? std::optional<Node>() : protocols;
        };

        auto getSelRefs = ^(const dyld3::MachOAnalyzer* ma,
                            const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            __block std::vector<const char *> selNames;

            auto visitSelRef = ^(uint64_t selRefVMAddr, uint64_t selRefTargetVMAddr) {
                if (auto selValue = getString(ma, selRefTargetVMAddr)) {
                    selNames.push_back(selValue);
                }
            };

            Diagnostics diag;
            ma->forEachObjCSelectorReference(diag, vmAddrConverter, visitSelRef);

            std::sort(selNames.begin(), selNames.end(),
                      [](const char* a, const char* b) {
                return strcasecmp(a, b) < 0;
            });

            Node selrefs;
            for (auto s: selNames) {
                selrefs.array.push_back(Node{s});
            }

            return selrefs.array.empty() ? std::optional<Node>() : selrefs;
        };

        auto getClasses = ^(const dyld3::MachOAnalyzer* ma,
                            const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            Diagnostics diag;
            const uint32_t pointerSize = ma->pointerSize();

            // Get the vmAddrs for all exported symbols as we want to know if classes
            // are exported
            std::set<uint64_t> exportedSymbolVMAddrs;
            {
                uint64_t loadAddress = ma->preferredLoadAddress();

                uint32_t exportTrieRuntimeOffset;
                uint32_t exportTrieSize;
                if ( ma->hasExportTrie(exportTrieRuntimeOffset, exportTrieSize) ) {
                    const uint8_t* start = (uint8_t*)ma + exportTrieRuntimeOffset;
                    const uint8_t* end = start + exportTrieSize;
                    std::vector<ExportInfoTrie::Entry> exports;
                    if ( ExportInfoTrie::parseTrie(start, end, exports) ) {
                        for (const ExportInfoTrie::Entry& entry: exports) {
                            exportedSymbolVMAddrs.insert(loadAddress + entry.info.address);
                        }
                    }
                }
            }

            __block Node classesNode;
            __block bool skippedPreviousClass = false;
            auto visitClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                                uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                if (isMetaClass) {
                    if (skippedPreviousClass) {
                        // If the class was bad, then skip the meta class too
                        skippedPreviousClass = false;
                        return;
                    }
                } else {
                    skippedPreviousClass = true;
                }

                std::string classType = "-";
                if (isMetaClass)
                    classType = "+";
                dyld3::MachOAnalyzer::PrintableStringResult classNameResult;
                const char* className = ma->getPrintableString(objcClass.nameVMAddr(pointerSize), classNameResult);
                if (classNameResult != dyld3::MachOAnalyzer::PrintableStringResult::CanPrint) {
                    return;
                }

                const char* superClassName = nullptr;
                if ( ma->inDyldCache() ) {
                    if ( objcClass.superclassVMAddr != 0 ) {
                        if (isMetaClass) {
                            // If we are root class, then our superclass should actually point to our own class
                            const uint32_t RO_ROOT = (1<<1);
                            if ( objcClass.flags(pointerSize) & RO_ROOT ) {
                                auto it = classVMAddrToName.find(objcClass.superclassVMAddr);
                                assert(it != classVMAddrToName.end());
                                superClassName = it->second;
                            } else {
                                auto it = metaclassVMAddrToName.find(objcClass.superclassVMAddr);
                                assert(it != metaclassVMAddrToName.end());
                                superClassName = it->second;
                            }
                        } else {
                            auto it = classVMAddrToName.find(objcClass.superclassVMAddr);
                            assert(it != classVMAddrToName.end());
                            superClassName = it->second;
                        }
                    }
                } else {
                    // On-disk binary.  Lets crack the chain to work out what we are pointing at
                    dyld3::MachOAnalyzer::ChainedFixupPointerOnDisk fixup;
                    fixup.raw64 = objcClass.superclassVMAddr;
                    if (fixup.arm64e.bind.bind) {
                        uint64_t bindOrdinal = fixup.arm64e.authBind.auth ? fixup.arm64e.authBind.ordinal : fixup.arm64e.bind.ordinal;
                        // Bind to another image.  Use the bind table to work out which name to bind to
                        const char* symbolName = onDiskChainedFixupBindTargets[bindOrdinal];
                        if (isMetaClass) {
                            if ( strstr(symbolName, "_OBJC_METACLASS_$_") == symbolName ) {
                                superClassName = symbolName + strlen("_OBJC_METACLASS_$_");
                            } else {
                                // Swift classes don't start with these prefixes so just skip them
                                if (objcClass.isSwiftLegacy || objcClass.isSwiftStable)
                                    return;
                            }
                        } else {
                            if ( strstr(symbolName, "_OBJC_CLASS_$_") == symbolName ) {
                                superClassName = symbolName + strlen("_OBJC_CLASS_$_");
                            } else {
                                // Swift classes don't start with these prefixes so just skip them
                                if (objcClass.isSwiftLegacy || objcClass.isSwiftStable)
                                    return;
                            }
                        }
                    } else {
                        // Rebase within this image.
                        if (isMetaClass) {
                            auto it = onDiskMetaclassVMAddrToName.find(objcClass.superclassVMAddr);
                            assert(it != onDiskMetaclassVMAddrToName.end());
                            superClassName = it->second;
                        } else {
                            auto it = onDiskClassVMAddrToName.find(objcClass.superclassVMAddr);
                            assert(it != onDiskClassVMAddrToName.end());
                            superClassName = it->second;
                        }
                    }
                }

                // Print the methods on this class
                __block Node methodsNode;
                auto visitMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    dyld3::MachOAnalyzer::PrintableStringResult methodNameResult;
                    const char* methodName = ma->getPrintableString(method.nameVMAddr, methodNameResult);
                    if (methodNameResult != dyld3::MachOAnalyzer::PrintableStringResult::CanPrint)
                        return;
                    methodsNode.array.push_back(Node{classType + methodName});
                };
                ma->forEachObjCMethod(objcClass.baseMethodsVMAddr(pointerSize), vmAddrConverter,
                                      visitMethod);

                std::optional<Node> properties = getProperties(ma, objcClass.basePropertiesVMAddr(pointerSize), vmAddrConverter);

                if (isMetaClass) {
                    assert(!classesNode.array.empty());
                    Node& currentClassNode = classesNode.array.back();
                    assert(currentClassNode.map["className"].value == className);
                    if (!methodsNode.array.empty()) {
                        Node& currentMethodsNode = currentClassNode.map["methods"];
                        currentMethodsNode.array.insert(currentMethodsNode.array.end(),
                                                        methodsNode.array.begin(),
                                                        methodsNode.array.end());
                    }
                    if (properties.has_value()) {
                        Node& currentPropertiesNode = currentClassNode.map["properties"];
                        currentPropertiesNode.array.insert(currentPropertiesNode.array.end(),
                                                           properties->array.begin(),
                                                           properties->array.end());
                    }
                    return;
                }

                Node currentClassNode;
                currentClassNode.map["className"] = Node{className};
                if ( superClassName != nullptr )
                    currentClassNode.map["superClassName"] = Node{superClassName};
                if (!methodsNode.array.empty())
                    currentClassNode.map["methods"] = methodsNode;
                if (properties.has_value())
                    currentClassNode.map["properties"] = properties.value();
                if (std::optional<Node> protocols = getClassProtocols(ma, objcClass.baseProtocolsVMAddr(pointerSize), vmAddrConverter))
                    currentClassNode.map["protocols"] = protocols.value();

                currentClassNode.map["exported"] = Node{exportedSymbolVMAddrs.count(classVMAddr) != 0};

                // We didn't skip this class so mark it as such
                skippedPreviousClass = false;

                classesNode.array.push_back(currentClassNode);
            };

            ma->forEachObjCClass(diag, vmAddrConverter, visitClass);
            return classesNode.array.empty() ? std::optional<Node>() : classesNode;
        };

        auto getCategories = ^(const dyld3::MachOAnalyzer* ma,
                               const dyld3::MachOAnalyzer::VMAddrConverter& vmAddrConverter) {
            Diagnostics diag;

            __block Node categoriesNode;
            auto visitCategory = ^(Diagnostics& diag, uint64_t categoryVMAddr,
                                   const dyld3::MachOAnalyzer::ObjCCategory& objcCategory) {
                dyld3::MachOAnalyzer::PrintableStringResult categoryNameResult;
                const char* categoryName = ma->getPrintableString(objcCategory.nameVMAddr, categoryNameResult);
                if (categoryNameResult != dyld3::MachOAnalyzer::PrintableStringResult::CanPrint)
                    return;

                    const char* className = nullptr;
                    if ( ma->inDyldCache() ) {
                        auto it = classVMAddrToName.find(objcCategory.clsVMAddr);
                        assert(it != classVMAddrToName.end());
                        className = it->second;
                    } else {
                        // On-disk binary.  Lets crack the chain to work out what we are pointing at
                        dyld3::MachOAnalyzer::ChainedFixupPointerOnDisk fixup;
                        fixup.raw64 = objcCategory.clsVMAddr;
                        if (fixup.arm64e.bind.bind) {
                            uint64_t bindOrdinal = fixup.arm64e.authBind.auth ? fixup.arm64e.authBind.ordinal : fixup.arm64e.bind.ordinal;
                            // Bind to another image.  Use the bind table to work out which name to bind to
                            const char* symbolName = onDiskChainedFixupBindTargets[bindOrdinal];
                            if ( strstr(symbolName, "_OBJC_CLASS_$_") == symbolName ) {
                                className = symbolName + strlen("_OBJC_CLASS_$_");
                            } else {
                                // Swift classes don't start with these prefixes so just skip them
                                // We don't know that this is a Swift class/category though, but skip it anyway
                                return;
                            }
                        } else {
                            auto it = onDiskClassVMAddrToName.find(objcCategory.clsVMAddr);
                            if (it == onDiskClassVMAddrToName.end()) {
                                // This is an odd binary with perhaps a Swift class.  Just skip this entry
                                return;
                            }
                            className = it->second;
                        }
                    }

                // Print the instance methods on this category
                __block Node methodsNode;
                auto visitInstanceMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    if (auto methodName = getString(ma, method.nameVMAddr))
                        methodsNode.array.push_back(Node{instancePrefix + methodName});
                };
                ma->forEachObjCMethod(objcCategory.instanceMethodsVMAddr, vmAddrConverter,
                                      visitInstanceMethod);

                // Print the instance methods on this category
                __block Node classMethodsNode;
                auto visitClassMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    if (auto methodName = getString(ma, method.nameVMAddr))
                        methodsNode.array.push_back(Node{classPrefix + methodName});
                };
                ma->forEachObjCMethod(objcCategory.classMethodsVMAddr, vmAddrConverter,
                                      visitClassMethod);

                Node currentCategoryNode;
                currentCategoryNode.map["categoryName"] = Node{categoryName};
                currentCategoryNode.map["className"] = Node{className};
                if (!methodsNode.array.empty())
                    currentCategoryNode.map["methods"] = methodsNode;
                if (std::optional<Node> properties = getProperties(ma, objcCategory.instancePropertiesVMAddr, vmAddrConverter))
                    currentCategoryNode.map["properties"] = properties.value();
                if (std::optional<Node> protocols = getClassProtocols(ma, objcCategory.protocolsVMAddr, vmAddrConverter))
                    currentCategoryNode.map["protocols"] = protocols.value();

                categoriesNode.array.push_back(currentCategoryNode);
            };

            ma->forEachObjCCategory(diag, vmAddrConverter, visitCategory);
            return categoriesNode.array.empty() ? std::optional<Node>() : categoriesNode;
        };

        __block bool needsComma = false;

        dyld3::json::streamArrayBegin(needsComma);

        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            dyld3::MachOAnalyzer::VMAddrConverter vmAddrConverter = dyldCache->makeVMAddrConverter(rebased);
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;

            Node imageRecord;
            imageRecord.map["imagePath"] = Node{installName};
            imageRecord.map["imageType"] = Node{"cache-dylib"};
            std::optional<Node> classes = getClasses(ma, vmAddrConverter);
            std::optional<Node> categories = getCategories(ma, vmAddrConverter);
            std::optional<Node> protocols = getProtocols(ma, vmAddrConverter);
            std::optional<Node> selrefs = getSelRefs(ma, vmAddrConverter);

            // Skip emitting images with no objc data
            if (!classes.has_value() && !categories.has_value() && !protocols.has_value() && !selrefs.has_value())
                return;
            if (classes.has_value())
                imageRecord.map["classes"] = classes.value();
            if (categories.has_value())
                imageRecord.map["categories"] = categories.value();
            if (protocols.has_value())
                imageRecord.map["protocols"] = protocols.value();
            if (selrefs.has_value())
                imageRecord.map["selrefs"] = selrefs.value();

            dyld3::json::streamArrayNode(needsComma, imageRecord);
        });

        FileSystemPhysical fileSystem;
        dyld3::Platform            platform = dyldCache->platform();
        const dyld3::GradedArchs&  archs    = dyld3::GradedArchs::forName(dyldCache->archName(), true);

        dyldCache->forEachLaunchClosure(^(const char *executableRuntimePath, const dyld3::closure::LaunchClosure *closure) {
            Diagnostics diag;
            char realerPath[MAXPATHLEN];
            dyld3::closure::LoadedFileInfo loadedFileInfo = dyld3::MachOAnalyzer::load(diag, fileSystem, executableRuntimePath, archs, platform, realerPath);
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)loadedFileInfo.fileContent;
            uint32_t pointerSize = ma->pointerSize();

            // Populate the bind targets for classes from other images
            onDiskChainedFixupBindTargets.clear();
            ma->forEachChainedFixupTarget(diag, ^(int libOrdinal, const char* symbolName, uint64_t addend, bool weakImport, bool& stop) {
                onDiskChainedFixupBindTargets.push_back(symbolName);
            });
            if ( diag.hasError() )
                return;

            // Populate the rebase targets for class names
            onDiskMetaclassVMAddrToName.clear();
            onDiskClassVMAddrToName.clear();
            auto visitClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                                uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                if (auto className = getString(ma, objcClass.nameVMAddr(pointerSize))) {
                    if (isMetaClass)
                        onDiskMetaclassVMAddrToName[classVMAddr] = className;
                    else
                        onDiskClassVMAddrToName[classVMAddr] = className;
                }
            };

            // Get a vmAddrConverter for this on-disk binary.  We can't use the shared cache one
            dyld3::MachOAnalyzer::VMAddrConverter onDiskVMAddrConverter = ma->makeVMAddrConverter(rebased);

            ma->forEachObjCClass(diag, onDiskVMAddrConverter, visitClass);

            Node imageRecord;
            imageRecord.map["imagePath"] = Node{executableRuntimePath};
            imageRecord.map["imageType"] = Node{"executable"};
            std::optional<Node> classes = getClasses(ma, onDiskVMAddrConverter);
            std::optional<Node> categories = getCategories(ma, onDiskVMAddrConverter);
            // TODO: protocols
            std::optional<Node> selrefs = getSelRefs(ma, onDiskVMAddrConverter);

            // Skip emitting images with no objc data
            if (!classes.has_value() && !categories.has_value() && !selrefs.has_value())
                return;
            if (classes.has_value())
                imageRecord.map["classes"] = classes.value();
            if (categories.has_value())
                imageRecord.map["categories"] = categories.value();
            if (selrefs.has_value())
                imageRecord.map["selrefs"] = selrefs.value();

            dyld3::json::streamArrayNode(needsComma, imageRecord);
        });

        dyldCache->forEachDlopenImage(^(const char *runtimePath, const dyld3::closure::Image *image) {
            Diagnostics diag;
            char realerPath[MAXPATHLEN];
            dyld3::closure::LoadedFileInfo loadedFileInfo = dyld3::MachOAnalyzer::load(diag, fileSystem, runtimePath, archs, platform, realerPath);
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)loadedFileInfo.fileContent;
            uint32_t pointerSize = ma->pointerSize();

            // Populate the bind targets for classes from other images
            onDiskChainedFixupBindTargets.clear();
            ma->forEachChainedFixupTarget(diag, ^(int libOrdinal, const char* symbolName, uint64_t addend, bool weakImport, bool& stop) {
                onDiskChainedFixupBindTargets.push_back(symbolName);
            });
            if ( diag.hasError() )
                return;

            // Populate the rebase targets for class names
            onDiskMetaclassVMAddrToName.clear();
            onDiskClassVMAddrToName.clear();
            auto visitClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                                uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                if (auto className = getString(ma, objcClass.nameVMAddr(pointerSize))) {
                    if (isMetaClass)
                        onDiskMetaclassVMAddrToName[classVMAddr] = className;
                    else
                        onDiskClassVMAddrToName[classVMAddr] = className;
                }
            };

            // Get a vmAddrConverter for this on-disk binary.  We can't use the shared cache one
            dyld3::MachOAnalyzer::VMAddrConverter onDiskVMAddrConverter = ma->makeVMAddrConverter(rebased);

            ma->forEachObjCClass(diag, onDiskVMAddrConverter, visitClass);

            Node imageRecord;
            imageRecord.map["imagePath"] = Node{runtimePath};
            imageRecord.map["imageType"] = Node{"non-cache-dylib"};
            std::optional<Node> classes = getClasses(ma, onDiskVMAddrConverter);
            std::optional<Node> categories = getCategories(ma, onDiskVMAddrConverter);
            // TODO: protocols
            std::optional<Node> selrefs = getSelRefs(ma, onDiskVMAddrConverter);

            // Skip emitting images with no objc data
            if (!classes.has_value() && !categories.has_value() && !selrefs.has_value())
                return;
            if (classes.has_value())
                imageRecord.map["classes"] = classes.value();
            if (categories.has_value())
                imageRecord.map["categories"] = categories.value();
            if (selrefs.has_value())
                imageRecord.map["selrefs"] = selrefs.value();

            dyld3::json::streamArrayNode(needsComma, imageRecord);
        });

        dyld3::json::streamArrayEnd(needsComma);
    }
    else if ( options.mode == modeObjCSelectors ) {
        if ( dyldCache->objcOpt() == nullptr ) {
            fprintf(stderr, "Error: could not get optimized objc\n");
            return 1;
        }
        const objc_opt::objc_selopt_t* selectors = dyldCache->objcOpt()->selopt();
        if ( selectors == nullptr ) {
            fprintf(stderr, "Error: could not get optimized objc selectors\n");
            return 1;
        }

        std::vector<const char*> selNames;
        for (uint64_t index = 0; index != selectors->capacity; ++index) {
            objc_opt::objc_stringhash_offset_t offset = selectors->offsets()[index];
            if ( offset == 0 )
                continue;
            const char* selName = selectors->getEntryForIndex((uint32_t)index);
            selNames.push_back(selName);
        }

        std::sort(selNames.begin(), selNames.end(),
                  [](const char* a, const char* b) {
            // Sort by offset, not string value
            return a < b;
        });

        dyld3::json::Node root;
        for (const char* selName : selNames) {
            dyld3::json::Node selNode;
            selNode.map["selectorName"] = dyld3::json::Node{selName};
            selNode.map["offset"] = dyld3::json::Node{(int64_t)selName - (int64_t)dyldCache};

            root.array.push_back(selNode);
        }

        dyld3::json::printJSON(root, 0, std::cout);
    }
    else if ( options.mode == modeExtract ) {
        return dyld_shared_cache_extract_dylibs(sharedCachePath, options.extractionDir);
    }
    else if ( options.mode == modeObjCImpCaches ) {
        if (sharedCachePath == nullptr) {
            fprintf(stderr, "Cannot emit imp caches with live cache.  Run again with the path to the cache file\n");
            return 1;
        }
        __block std::map<uint64_t, const char*> methodToClassMap;
        __block std::map<uint64_t, const char*> classVMAddrToNameMap;
        const bool contentRebased = false;
        const uint32_t pointerSize = 8;

        // Get the base pointers from the magic section in objc
        __block uint64_t objcCacheOffsetsSize = 0;
        __block const void* objcCacheOffsets = nullptr;
        __block Diagnostics diag;
        dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
            if ( !strcmp(installName, "/usr/lib/libobjc.A.dylib") ) {
                const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;
                objcCacheOffsets = ma->findSectionContent("__DATA_CONST", "__objc_scoffs", objcCacheOffsetsSize);
            }
        });

        if ( objcCacheOffsets == nullptr ) {
            fprintf(stderr, "Unable to print imp-caches as cannot find __DATA_CONST __objc_scoffs inside /usr/lib/libobjc.A.dylib\n");
            return 1;
        }

        if ( objcCacheOffsetsSize < (4 * pointerSize) ) {
            fprintf(stderr, "Unable to print imp-caches as __DATA_CONST __objc_scoffs is too small (%lld vs required %u)\n", objcCacheOffsetsSize, (4 * pointerSize));
            return 1;
        }

        dyld3::MachOAnalyzer::VMAddrConverter vmAddrConverter = dyldCache->makeVMAddrConverter(contentRebased);

        uint64_t selectorStringVMAddrStart  = vmAddrConverter.convertToVMAddr(((uint64_t*)objcCacheOffsets)[0]);
        uint64_t selectorStringVMAddrEnd    = vmAddrConverter.convertToVMAddr(((uint64_t*)objcCacheOffsets)[1]);

        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            if (diag.hasError())
                return;

            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;
            intptr_t slide = ma->getSlide();

            ma->forEachObjCClass(diag, vmAddrConverter, ^(Diagnostics& diag,
                                                          uint64_t classVMAddr,
                                                          uint64_t classSuperclassVMAddr,
                                                          uint64_t classDataVMAddr,
                                                          const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass,
                                                          bool isMetaClass) {
                const char* className = (const char*)objcClass.nameVMAddr(pointerSize) + slide;
                classVMAddrToNameMap[classVMAddr] = className;
                ma->forEachObjCMethod(objcClass.baseMethodsVMAddr(pointerSize), vmAddrConverter,
                                      ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    // const char* methodName = (const char*)(method.nameVMAddr + slide);
                    methodToClassMap[method.impVMAddr] = className;
                });
            });

            ma->forEachObjCCategory(diag, vmAddrConverter, ^(Diagnostics& diag, uint64_t categoryVMAddr, const dyld3::MachOAnalyzer::ObjCCategory& objcCategory) {
                ma->forEachObjCMethod(objcCategory.instanceMethodsVMAddr, vmAddrConverter, ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    const char* catName = (const char*)objcCategory.nameVMAddr + slide;
                    // const char* methodName = (const char*)(method.nameVMAddr + slide);
                    methodToClassMap[method.impVMAddr] = catName;
                });

                ma->forEachObjCMethod(objcCategory.classMethodsVMAddr, vmAddrConverter, ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    const char* catName = (const char*)objcCategory.nameVMAddr + slide;
                    // const char* methodName = (const char*)(method.nameVMAddr + slide);
                    methodToClassMap[method.impVMAddr] = catName;
                });
            });
        });
        if (diag.hasError())
            return 1;

        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            if (diag.hasError())
                return;

            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;
            intptr_t slide = ma->getSlide();

            ma->forEachObjCClass(diag, vmAddrConverter, ^(Diagnostics& diag,
                                                          uint64_t classVMAddr,
                                                          uint64_t classSuperclassVMAddr,
                                                          uint64_t classDataVMAddr,
                                                          const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass,
                                                          bool isMetaClass) {
                const char* type = "class";
                if (isMetaClass)
                    type = "meta-class";
                const char* className = (const char*)objcClass.nameVMAddr(pointerSize) + slide;

                if (objcClass.methodCacheVMAddr == 0) {
                    printf("%s (%s): empty\n", className, type);
                    return;
                }

                struct Bucket {
                    uint32_t selOffset;
                    uint32_t impOffset;
                };
                struct ImpCache {
                    int32_t  fallback_class_offset;
                    uint32_t cache_shift :  5;
                    uint32_t cache_mask  : 11;
                    uint32_t occupied    : 14;
                    uint32_t has_inlines :  1;
                    uint32_t bit_one     :  1;
                    struct Bucket buckets[];
                };

                const ImpCache* impCache = (const ImpCache*)(objcClass.methodCacheVMAddr + slide);
                printf("%s (%s): %d buckets\n", className, type, impCache->cache_mask + 1);

                if ((classVMAddr + impCache->fallback_class_offset) != objcClass.superclassVMAddr) {
                    printf("Flattening fallback: %s\n", classVMAddrToNameMap[classVMAddr + impCache->fallback_class_offset]);
                }
                // Buckets are a 32-bit offset from the impcache itself
                for (uint32_t i = 0; i <= impCache->cache_mask ; ++i) {
                    const Bucket& b = impCache->buckets[i];
                    uint64_t sel = (uint64_t)b.selOffset + selectorStringVMAddrStart;
                    uint64_t imp = classVMAddr - (uint64_t)b.impOffset;
                    if (b.selOffset == 0xFFFFFFFF) {
                        // Empty bucket
                        printf("  - 0x%016llx: %s\n", 0ULL, "");
                    } else {
                        assert(sel < selectorStringVMAddrEnd);

                        auto it = methodToClassMap.find(imp);
                        if (it == methodToClassMap.end()) {
                            fprintf(stderr, "Could not find IMP %llx (for %s)\n", imp, (const char*)(sel + slide));
                        }
                        assert(it != methodToClassMap.end());
                        printf("  - 0x%016llx: %s (from %s)\n", imp, (const char*)(sel + slide), it->second);
                    }
                }
           });
        });
    } else {
        switch ( options.mode ) {
            case modeList: {
                // list all dylibs, including their aliases (symlinks to them) with option vmaddr
                __block std::vector<std::unordered_set<std::string>> indexToPaths;
                __block std::vector<uint64_t> indexToAddr;
                __block std::vector<std::string> indexToUUID;
                dyldCache->forEachImageTextSegment(^(uint64_t loadAddressUnslid, uint64_t textSegmentSize, const unsigned char* dylibUUID, const char* installName, bool& stop) {
                    std::unordered_set<std::string> empty;
                    if ( options.printVMAddrs )
                        indexToAddr.push_back(loadAddressUnslid);
                    if ( options.printUUIDs ) {
                        uuid_string_t uuidString;
                        uuid_unparse_upper(dylibUUID, uuidString);
                        indexToUUID.push_back(uuidString);
                    }
                    indexToPaths.push_back(empty);
                    indexToPaths.back().insert(installName);
                });
                dyldCache->forEachDylibPath(^(const char* dylibPath, uint32_t index) {
                    indexToPaths[index].insert(dylibPath);
                });
                int index = 0;
                for (const std::unordered_set<std::string>& paths : indexToPaths) {
                    for (const std::string& path: paths) {
                        if ( options.printVMAddrs )
                            printf("0x%08llX ", indexToAddr[index]);
                        if ( options.printUUIDs )
                             printf("<%s> ", indexToUUID[index].c_str());
                       printf("%s\n", path.c_str());
                    }
                    ++index;
                }
                break;
            }
            case modeListDylibsWithSection: {
                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
                    dyld3::MachOFile* mf = (dyld3::MachOFile*)mh;
                    mf->forEachSection(^(const dyld3::MachOFile::SectionInfo& sectInfo, bool malformedSectionRange, bool& stop) {
                        if ( (strcmp(sectInfo.sectName, options.sectionName) == 0) && (strcmp(sectInfo.segInfo.segName, options.segmentName) == 0) ) {
                            printf("%s\n", installName);
                            stop = true;
                        }
                    });
                });
                break;
            }
            case modeMap: {
                __block std::map<uint64_t, const char*> dataSegNames;
                __block std::map<uint64_t, uint64_t>    dataSegEnds;
                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
                    dyld3::MachOFile* mf = (dyld3::MachOFile*)mh;
                    mf->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                        printf("0x%08llX - 0x%08llX %s %s\n", info.vmAddr, info.vmAddr + info.vmSize, info.segName, installName);
                        if ( strncmp(info.segName, "__DATA", 6) == 0 ) {
                            dataSegNames[info.vmAddr] = installName;
                            dataSegEnds[info.vmAddr] = info.vmAddr + info.vmSize;
                        }
                    });
                });
                // <rdar://problem/51084507> Enhance dyld_shared_cache_util to show where section alignment added padding
                uint64_t lastEnd = 0;
                for (const auto& entry : dataSegEnds) {
                    uint64_t padding = entry.first - lastEnd;
                    if ( (padding > 32) && (lastEnd != 0) ) {
                        printf("0x%08llX - 0x%08llX PADDING %lluKB\n", lastEnd, entry.first, padding/1024);
                    }
                    lastEnd = entry.second;
                }
                break;
            }
            case modeDependencies: {
                __block bool dependentTargetFound = false;
                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
                    if ( strcmp(options.dependentsOfPath, installName) != 0 )
                        return;
                    dependentTargetFound = true;

                    auto printDep = [&options](const char *loadPath, uint32_t compatVersion, uint32_t curVersion) {
                        if ( options.printDylibVersions ) {
                            uint32_t compat_vers = compatVersion;
                            uint32_t current_vers = curVersion;
                            printf("\t%s", loadPath);
                            if ( compat_vers != 0xFFFFFFFF ) {
                                printf("(compatibility version %u.%u.%u, current version %u.%u.%u)\n",
                                       (compat_vers >> 16),
                                       (compat_vers >> 8) & 0xff,
                                       (compat_vers) & 0xff,
                                       (current_vers >> 16),
                                       (current_vers >> 8) & 0xff,
                                       (current_vers) & 0xff);
                            }
                            else {
                                printf("\n");
                            }
                        }
                        else {
                            printf("\t%s\n", loadPath);
                        }
                    };

                    dyld3::MachOFile* mf = (dyld3::MachOFile*)mh;

                    // First print out our dylib and version.
                    const char* dylibInstallName;
                    uint32_t currentVersion;
                    uint32_t compatVersion;
                    if ( mf->getDylibInstallName(&dylibInstallName, &compatVersion, &currentVersion) ) {
                        printDep(dylibInstallName, compatVersion, currentVersion);
                    }

                    // Then the dependent dylibs.
                    mf->forEachDependentDylib(^(const char *loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
                        printDep(loadPath, compatVersion, curVersion);
                    });
                });
                if (options.dependentsOfPath && !dependentTargetFound) {
                    fprintf(stderr, "Error: could not find '%s' in the shared cache at\n  %s\n", options.dependentsOfPath, sharedCachePath);
                    exit(1);
                }
                break;
            }
            case modeLinkEdit: {
                std::map<uint32_t, const char*> pageToContent;
                auto add_linkedit = [&pageToContent](uint32_t pageStart, uint32_t pageEnd, const char* message) {
                    for (uint32_t p = pageStart; p <= pageEnd; p += 4096) {
                        std::map<uint32_t, const char*>::iterator pos = pageToContent.find(p);
                        if ( pos == pageToContent.end() ) {
                            pageToContent[p] = strdup(message);
                        }
                        else {
                            const char* oldMessage = pos->second;
                            char* newMesssage;
                            asprintf(&newMesssage, "%s, %s", oldMessage, message);
                            pageToContent[p] = newMesssage;
                            ::free((void*)oldMessage);
                        }
                    }
                };

                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
                    dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
                    Diagnostics diag;
                    dyld3::MachOAnalyzer::LinkEditInfo leInfo;
                    ma->getLinkEditPointers(diag, leInfo);

                    if (diag.hasError())
                        return;

                    char message[1000];
                    const char* shortName = strrchr(installName, '/') + 1;
                    // add export trie info
                    if ( leInfo.dyldInfo->export_size != 0 ) {
                        //printf("export_off=0x%X\n", leInfo.dyldInfo->export_off());
                        uint32_t exportPageOffsetStart = leInfo.dyldInfo->export_off & (-4096);
                        uint32_t exportPageOffsetEnd = (leInfo.dyldInfo->export_off + leInfo.dyldInfo->export_size) & (-4096);
                        sprintf(message, "exports from %s", shortName);
                        add_linkedit(exportPageOffsetStart, exportPageOffsetEnd, message);
                    }
                    // add binding info
                    if ( leInfo.dyldInfo->bind_size != 0 ) {
                        uint32_t bindPageOffsetStart = leInfo.dyldInfo->bind_off & (-4096);
                        uint32_t bindPageOffsetEnd = (leInfo.dyldInfo->bind_off + leInfo.dyldInfo->bind_size) & (-4096);
                        sprintf(message, "bindings from %s", shortName);
                        add_linkedit(bindPageOffsetStart, bindPageOffsetEnd, message);
                    }
                    // add lazy binding info
                    if ( leInfo.dyldInfo->lazy_bind_size != 0 ) {
                        uint32_t lazybindPageOffsetStart = leInfo.dyldInfo->lazy_bind_off & (-4096);
                        uint32_t lazybindPageOffsetEnd = (leInfo.dyldInfo->lazy_bind_off + leInfo.dyldInfo->lazy_bind_size) & (-4096);
                        sprintf(message, "lazy bindings from %s", shortName);
                        add_linkedit(lazybindPageOffsetStart, lazybindPageOffsetEnd, message);
                    }
                    // add weak binding info
                    if ( leInfo.dyldInfo->weak_bind_size != 0 ) {
                        uint32_t weakbindPageOffsetStart = leInfo.dyldInfo->weak_bind_off & (-4096);
                        uint32_t weakbindPageOffsetEnd = (leInfo.dyldInfo->weak_bind_off + leInfo.dyldInfo->weak_bind_size) & (-4096);
                        sprintf(message, "weak bindings from %s", shortName);
                        add_linkedit(weakbindPageOffsetStart, weakbindPageOffsetEnd, message);
                    }
                });

                for (std::map<uint32_t, const char*>::iterator it = pageToContent.begin(); it != pageToContent.end(); ++it) {
                    printf("0x%08X %s\n", it->first, it->second);
                }
                break;
            }
            case modeSize: {
                struct TextInfo {
                    uint64_t    textSize;
                    const char* path;
                };
                __block std::vector<TextInfo> textSegments;
                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {

                    dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
                    ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                        if ( strcmp(info.segName, "__TEXT") != 0 )
                            return;
                        textSegments.push_back({ info.fileSize, installName });
                    });
                });
                std::sort(textSegments.begin(), textSegments.end(), [](const TextInfo& left, const TextInfo& right) {
                    return (left.textSize > right.textSize);
                });
                for (std::vector<TextInfo>::iterator it = textSegments.begin(); it != textSegments.end(); ++it) {
                    printf(" 0x%08llX  %s\n", it->textSize, it->path);
                }
                break;
            }
            case modePatchTable: {
                std::vector<SegmentInfo> segInfos;
                buildSegmentInfo(dyldCache, segInfos);
                __block uint32_t imageIndex = 0;
                dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
                    printf("%s:\n", installName);
                    dyldCache->forEachPatchableExport(imageIndex, ^(uint32_t cacheOffsetOfImpl, const char* exportName) {
                        printf("    export: 0x%08X  %s\n", cacheOffsetOfImpl, exportName);
                        dyldCache->forEachPatchableUseOfExport(imageIndex, cacheOffsetOfImpl, ^(dyld_cache_patchable_location patchLocation) {
                            SegmentInfo usageAt;
                            const uint64_t patchLocVmAddr = dyldCache->unslidLoadAddress() + patchLocation.cacheOffset;
                            findImageAndSegment(dyldCache, segInfos, patchLocation.cacheOffset, &usageAt);
                            if ( patchLocation.addend == 0 )
                                printf("        used by: %s+0x%04llX in %s\n", usageAt.segName, patchLocVmAddr-usageAt.vmAddr, usageAt.installName);
                            else
                                printf("        used by: %s+0x%04llX (addend=%d) in %s\n", usageAt.segName, patchLocVmAddr-usageAt.vmAddr, patchLocation.addend, usageAt.installName);
                        });
                    });
                    ++imageIndex;
                });
                break;
            }
            case modeNone:
            case modeInfo:
            case modeSlideInfo:
            case modeVerboseSlideInfo:
            case modeTextInfo:
            case modeLocalSymbols:
            case modeJSONMap:
            case modeJSONDependents:
            case modeSectionSizes:
            case modeStrings:
            case modeObjCProtocols:
            case modeObjCImpCaches:
            case modeObjCClasses:
            case modeObjCSelectors:
            case modeExtract:
                break;
        }
    }
    return 0;
}
