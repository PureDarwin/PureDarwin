/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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



#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/ldsyms.h>
#include <mach/shared_region.h>
#include <mach/mach.h>
#include <Availability.h>
#include <TargetConditionals.h>

#include "dyld_cache_format.h"
#include "SharedCacheRuntime.h"
#include "Loading.h"
#include "BootArgs.h"

#define ENABLE_DYLIBS_TO_OVERRIDE_CACHE_SIZE 1024

// should be in mach/shared_region.h
extern "C" int __shared_region_check_np(uint64_t* startaddress);
extern "C" int __shared_region_map_and_slide_np(int fd, uint32_t count, const shared_file_mapping_np mappings[], long slide, const dyld_cache_slide_info2* slideInfo, size_t slideInfoSize);
extern "C" int __shared_region_map_and_slide_2_np(uint32_t files_count, const shared_file_np files[], uint32_t mappings_count, const shared_file_mapping_slide_np mappings[]);

#ifndef VM_PROT_NOAUTH
#define VM_PROT_NOAUTH  0x40  /* must not interfere with normal prot assignments */
#endif

extern bool gEnableSharedCacheDataConst;

namespace dyld {
    extern void log(const char*, ...);
    extern void logToConsole(const char* format, ...);
#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
    bool isTranslated();
#endif
}


namespace dyld3 {


struct CacheInfo
{
    shared_file_mapping_slide_np            mappings[DyldSharedCache::MaxMappings];
    uint32_t                                mappingsCount;
    // All mappings come from the same file
    int                                     fd               = 0;
    uint64_t                                sharedRegionStart;
    uint64_t                                sharedRegionSize;
    uint64_t                                maxSlide;
};




#if __i386__
    #define ARCH_NAME            "i386"
    #define ARCH_CACHE_MAGIC     "dyld_v1    i386"
#elif __x86_64__
    #define ARCH_NAME            "x86_64"
    #define ARCH_CACHE_MAGIC     "dyld_v1  x86_64"
    #define ARCH_NAME_H          "x86_64h"
    #define ARCH_CACHE_MAGIC_H   "dyld_v1 x86_64h"
#elif __ARM_ARCH_7K__
    #define ARCH_NAME            "armv7k"
    #define ARCH_CACHE_MAGIC     "dyld_v1  armv7k"
#elif __ARM_ARCH_7A__
    #define ARCH_NAME            "armv7"
    #define ARCH_CACHE_MAGIC     "dyld_v1   armv7"
#elif __ARM_ARCH_7S__
    #define ARCH_NAME            "armv7s"
    #define ARCH_CACHE_MAGIC     "dyld_v1  armv7s"
#elif __arm64e__
    #define ARCH_NAME            "arm64e"
    #define ARCH_CACHE_MAGIC     "dyld_v1  arm64e"
#elif __arm64__
    #if __LP64__
        #define ARCH_NAME            "arm64"
        #define ARCH_CACHE_MAGIC     "dyld_v1   arm64"
    #else
        #define ARCH_NAME            "arm64_32"
        #define ARCH_CACHE_MAGIC     "dyld_v1arm64_32"
    #endif
#endif


#if !TARGET_OS_SIMULATOR
static void rebaseChainV2(uint8_t* pageContent, uint16_t startOffset, uintptr_t slideAmount, const dyld_cache_slide_info2* slideInfo)
{
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
        *((uintptr_t*)loc) = value;
        //dyld::log("         pageOffset=0x%03X, loc=%p, org value=0x%08llX, new value=0x%08llX, delta=0x%X\n", pageOffset, loc, (uint64_t)rawValue, (uint64_t)value, delta);
        pageOffset += delta;
    }
}
#endif

#if !__LP64__ && !TARGET_OS_SIMULATOR
static void rebaseChainV4(uint8_t* pageContent, uint16_t startOffset, uintptr_t slideAmount, const dyld_cache_slide_info4* slideInfo)
{
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
        if ( (value & 0xFFFF8000) == 0 ) {
           // small positive non-pointer, use as-is
        }
        else if ( (value & 0x3FFF8000) == 0x3FFF8000 ) {
           // small negative non-pointer
           value |= 0xC0000000;
        }
        else {
            value += valueAdd;
            value += slideAmount;
        }
        *((uintptr_t*)loc) = value;
        //dyld::log("         pageOffset=0x%03X, loc=%p, org value=0x%08llX, new value=0x%08llX, delta=0x%X\n", pageOffset, loc, (uint64_t)rawValue, (uint64_t)value, delta);
        pageOffset += delta;
    }
}
#endif

#if TARGET_OS_OSX
bool getMacOSCachePath(char pathBuffer[], size_t pathBufferSize,
                       const char* cacheDir, bool useHaswell) {
    // Clear old attempts at finding a cache, if any
    pathBuffer[0] = '\0';

    // set cache dir
    strlcpy(pathBuffer, cacheDir, pathBufferSize);

    // append file component of cache file
    if ( pathBuffer[strlen(pathBuffer)-1] != '/' )
        strlcat(pathBuffer, "/", pathBufferSize);

#if __x86_64__
    if ( useHaswell ) {
        size_t len = strlen(pathBuffer);
        struct stat haswellStatBuf;
        strlcat(pathBuffer, DYLD_SHARED_CACHE_BASE_NAME ARCH_NAME_H, pathBufferSize);
        if ( dyld3::stat(pathBuffer, &haswellStatBuf) == 0 )
            return true;
        // no haswell cache file, use regular x86_64 cache
        pathBuffer[len] = '\0';
    }
#endif

    struct stat statBuf;
    strlcat(pathBuffer, DYLD_SHARED_CACHE_BASE_NAME ARCH_NAME, pathBufferSize);
    if ( dyld3::stat(pathBuffer, &statBuf) == 0 )
        return true;

    return false;
}
#endif // TARGET_OS_OSX

static void getCachePath(const SharedCacheOptions& options, size_t pathBufferSize, char pathBuffer[])
{
#if TARGET_OS_OSX

    if ( options.cacheDirOverride != nullptr ) {
        getMacOSCachePath(pathBuffer, pathBufferSize, options.cacheDirOverride, options.useHaswell);
    } else {
        getMacOSCachePath(pathBuffer, pathBufferSize, MACOSX_MRM_DYLD_SHARED_CACHE_DIR, options.useHaswell);
    }

#else // TARGET_OS_OSX

    // Non-macOS path
    if ( options.cacheDirOverride != nullptr ) {
        strlcpy(pathBuffer, options.cacheDirOverride, pathBufferSize);
    } else {
        strlcpy(pathBuffer, IPHONE_DYLD_SHARED_CACHE_DIR, sizeof(IPHONE_DYLD_SHARED_CACHE_DIR));
    }

    // append file component of cache file
    if ( pathBuffer[strlen(pathBuffer)-1] != '/' )
        strlcat(pathBuffer, "/", pathBufferSize);

    strlcat(pathBuffer, DYLD_SHARED_CACHE_BASE_NAME ARCH_NAME, pathBufferSize);

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    // use .development cache if it exists
    if ( BootArgs::forceCustomerCache() ) {
        // The boot-arg always wins.  Use the customer cache if we are told to
        return;
    }
    if ( !dyld3::internalInstall() ) {
        // We can't use the development cache on customer installs
        return;
    }
    if ( BootArgs::forceDevelopmentCache() ) {
        // The boot-arg always wins.  Use the development cache if we are told to
        strlcat(pathBuffer, DYLD_SHARED_CACHE_DEVELOPMENT_EXT, pathBufferSize);
        return;
    }

    // If only one or the other caches exists, then use the one we have
    struct stat devCacheStatBuf;
    struct stat optCacheStatBuf;
    bool devCacheExists = (dyld3::stat(IPHONE_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME ARCH_NAME DYLD_SHARED_CACHE_DEVELOPMENT_EXT, &devCacheStatBuf) == 0);
    bool optCacheExists = (dyld3::stat(IPHONE_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME ARCH_NAME, &optCacheStatBuf) == 0);
    if ( !devCacheExists ) {
        // If the dev cache doesn't exist, then use the customer cache
        return;
    }
    if ( !optCacheExists ) {
        // If the customer cache doesn't exist, then use the development cache
        strlcat(pathBuffer, DYLD_SHARED_CACHE_DEVELOPMENT_EXT, pathBufferSize);
        return;
    }

    // Finally, check for the sentinels
    struct stat enableStatBuf;
    //struct stat sentinelStatBuf;
    bool enableFileExists = (dyld3::stat(IPHONE_DYLD_SHARED_CACHE_DIR "enable-dylibs-to-override-cache", &enableStatBuf) == 0);
    // FIXME: rdar://problem/59813537 Re-enable once automation is updated to use boot-arg
    bool sentinelFileExists = false;
    //bool sentinelFileExists = (dyld3::stat(MACOSX_MRM_DYLD_SHARED_CACHE_DIR "enable_development_mode", &sentinelStatBuf) == 0);
    if ( enableFileExists && (enableStatBuf.st_size < ENABLE_DYLIBS_TO_OVERRIDE_CACHE_SIZE) ) {
        // if the old enable file exists, use the development cache
        strlcat(pathBuffer, DYLD_SHARED_CACHE_DEVELOPMENT_EXT, pathBufferSize);
        return;
    }
    if ( sentinelFileExists ) {
        // If the new sentinel exists, then use the development cache
        strlcat(pathBuffer, DYLD_SHARED_CACHE_DEVELOPMENT_EXT, pathBufferSize);
        return;
    }
#endif

#endif //!TARGET_OS_OSX
}


int openSharedCacheFile(const SharedCacheOptions& options, SharedCacheLoadInfo* results)
{
    getCachePath(options, sizeof(results->path), results->path);
    return dyld3::open(results->path, O_RDONLY, 0);
}

static bool validMagic(const SharedCacheOptions& options, const DyldSharedCache* cache)
{
    if ( strcmp(cache->header.magic, ARCH_CACHE_MAGIC) == 0 )
        return true;

#if __x86_64__
    if ( options.useHaswell ) {
        if ( strcmp(cache->header.magic, ARCH_CACHE_MAGIC_H) == 0 )
            return true;
    }
#endif
    return false;
}


static bool validPlatform(const SharedCacheOptions& options, const DyldSharedCache* cache)
{
    // grandfather in old cache that does not have platform in header
    if ( cache->header.mappingOffset < 0xE0 )
        return true;

    if ( cache->header.platform != (uint32_t)MachOFile::currentPlatform() )
        return false;

#if TARGET_OS_SIMULATOR
    if ( cache->header.simulator == 0 )
        return false;
#else
    if ( cache->header.simulator != 0 )
        return false;
#endif

    return true;
}

#if !TARGET_OS_SIMULATOR
static void verboseSharedCacheMappings(const shared_file_mapping_slide_np mappings[DyldSharedCache::MaxMappings],
                                       uint32_t mappingsCount)
{
    for (int i=0; i < mappingsCount; ++i) {
        const char* mappingName = "";
        if ( mappings[i].sms_max_prot & VM_PROT_WRITE ) {
            if ( mappings[i].sms_max_prot & VM_PROT_NOAUTH ) {
                // __DATA*
                mappingName = "data";
            } else {
                // __AUTH*
                mappingName = "auth";
            }
        }
        uint32_t init_prot = mappings[i].sms_init_prot & (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
        uint32_t max_prot = mappings[i].sms_max_prot & (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
        dyld::log("        0x%08llX->0x%08llX init=%x, max=%x %s%s%s%s\n",
            mappings[i].sms_address, mappings[i].sms_address+mappings[i].sms_size-1,
            init_prot, max_prot,
            ((mappings[i].sms_init_prot & VM_PROT_READ) ? "read " : ""),
            ((mappings[i].sms_init_prot & VM_PROT_WRITE) ? "write " : ""),
            ((mappings[i].sms_init_prot & VM_PROT_EXECUTE) ? "execute " : ""),
            mappingName);
    }
}


static void verboseSharedCacheMappingsToConsole(const shared_file_mapping_slide_np mappings[DyldSharedCache::MaxMappings],
                                                uint32_t mappingsCount)
{
    for (int i=0; i < mappingsCount; ++i) {
        const char* mappingName = "";
        if ( mappings[i].sms_max_prot & VM_PROT_WRITE ) {
            if ( mappings[i].sms_max_prot & VM_PROT_NOAUTH ) {
                // __DATA*
                mappingName = "data";
            } else {
                // __AUTH*
                mappingName = "auth";
            }
        }
        uint32_t init_prot = mappings[i].sms_init_prot & (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
        uint32_t max_prot = mappings[i].sms_max_prot & (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
        dyld::logToConsole("dyld: mapping 0x%08llX->0x%08llX init=%x, max=%x %s%s%s%s\n",
                           mappings[i].sms_address, mappings[i].sms_address+mappings[i].sms_size-1,
                           init_prot, max_prot,
                           ((mappings[i].sms_init_prot & VM_PROT_READ) ? "read " : ""),
                           ((mappings[i].sms_init_prot & VM_PROT_WRITE) ? "write " : ""),
                           ((mappings[i].sms_init_prot & VM_PROT_EXECUTE) ? "execute " : ""),
                           mappingName);
    }
}
#endif

static bool preflightCacheFile(const SharedCacheOptions& options, SharedCacheLoadInfo* results, CacheInfo* info)
{
    
    // find and open shared cache file
    int fd = openSharedCacheFile(options, results);
    if ( fd == -1 ) {
        results->errorMessage = "shared cache file open() failed";
        return false;
    }

    struct stat cacheStatBuf;
    if ( dyld3::stat(results->path, &cacheStatBuf) != 0 ) {
        results->errorMessage = "shared cache file stat() failed";
        ::close(fd);
        return false;
    }
    size_t cacheFileLength = (size_t)(cacheStatBuf.st_size);

    // sanity check header and mappings
    uint8_t firstPage[0x4000];
    if ( ::pread(fd, firstPage, sizeof(firstPage), 0) != sizeof(firstPage) ) {
        results->errorMessage = "shared cache file pread() failed";
        ::close(fd);
        return false;
    }
    const DyldSharedCache* cache = (DyldSharedCache*)firstPage;
    if ( !validMagic(options, cache) ) {
        results->errorMessage = "shared cache file has wrong magic";
        ::close(fd);
        return false;
    }
    if ( !validPlatform(options, cache) ) {
        results->errorMessage = "shared cache file is for a different platform";
        ::close(fd);
        return false;
    }
    if ( (cache->header.mappingCount < 3) || (cache->header.mappingCount > DyldSharedCache::MaxMappings) || (cache->header.mappingOffset > 0x168) ) {
        results->errorMessage = "shared cache file mappings are invalid";
        ::close(fd);
        return false;
    }
    const dyld_cache_mapping_info* const fileMappings = (dyld_cache_mapping_info*)&firstPage[cache->header.mappingOffset];
    const dyld_cache_mapping_info* textMapping = &fileMappings[0];
    const dyld_cache_mapping_info* firstDataMapping = &fileMappings[1];
    const dyld_cache_mapping_info* linkeditMapping = &fileMappings[cache->header.mappingCount - 1];
    if (  (textMapping->fileOffset != 0)
      || ((fileMappings[0].address + fileMappings[0].size) > firstDataMapping->address)
      || ((fileMappings[0].fileOffset + fileMappings[0].size) != firstDataMapping->fileOffset)
      || ((cache->header.codeSignatureOffset + cache->header.codeSignatureSize) != cacheFileLength)
      || (textMapping->maxProt != (VM_PROT_READ|VM_PROT_EXECUTE))
      || (linkeditMapping->maxProt != VM_PROT_READ) ) {
        results->errorMessage = "shared cache text/linkedit mappings are invalid";
        ::close(fd);
        return false;
    }

    // Check the __DATA mappings
    for (unsigned i = 1; i != (cache->header.mappingCount - 1); ++i) {
        if ( ((fileMappings[i].address + fileMappings[i].size) > fileMappings[i + 1].address)
          || ((fileMappings[i].fileOffset + fileMappings[i].size) != fileMappings[i + 1].fileOffset)
          || (fileMappings[i].maxProt != (VM_PROT_READ|VM_PROT_WRITE)) ) {
            results->errorMessage = "shared cache data mappings are invalid";
            ::close(fd);
            return false;
        }
    }

    if ( (textMapping->address != cache->header.sharedRegionStart) || ((linkeditMapping->address + linkeditMapping->size) > (cache->header.sharedRegionStart+cache->header.sharedRegionSize)) ) {
        results->errorMessage = "shared cache file mapping addressses invalid";
        ::close(fd);
        return false;
    }

    // register code signature of cache file
    fsignatures_t siginfo;
    siginfo.fs_file_start = 0;  // cache always starts at beginning of file
    siginfo.fs_blob_start = (void*)cache->header.codeSignatureOffset;
    siginfo.fs_blob_size  = (size_t)(cache->header.codeSignatureSize);
    int result = fcntl(fd, F_ADDFILESIGS_RETURN, &siginfo);
    if ( result == -1 ) {
        results->errorMessage = "code signature registration for shared cache failed";
        ::close(fd);
        return false;
    }

    // <rdar://problem/23188073> validate code signature covers entire shared cache
    uint64_t codeSignedLength = siginfo.fs_file_start;
    if ( codeSignedLength < cache->header.codeSignatureOffset ) {
        results->errorMessage = "code signature does not cover entire shared cache file";
        ::close(fd);
        return false;
    }
    void* mappedData = ::mmap(NULL, sizeof(firstPage), PROT_READ|PROT_EXEC, MAP_PRIVATE, fd, 0);
    if ( mappedData == MAP_FAILED ) {
        results->errorMessage = "first page of shared cache not mmap()able";
        ::close(fd);
        return false;
    }
    if ( memcmp(mappedData, firstPage, sizeof(firstPage)) != 0 ) {
        results->errorMessage = "first page of mmap()ed shared cache not valid";
        ::close(fd);
        return false;
    }
    ::munmap(mappedData, sizeof(firstPage));

    // fill out results
    info->mappingsCount = cache->header.mappingCount;
    // We have to emit the mapping for the __LINKEDIT before the slid mappings
    // This is so that the kernel has already mapped __LINKEDIT in to its address space
    // for when it copies the slid info for each __DATA mapping
    for (int i=0; i < cache->header.mappingCount; ++i) {
        uint64_t    slideInfoFileOffset = 0;
        uint64_t    slideInfoFileSize   = 0;
        vm_prot_t   authProt            = 0;
        vm_prot_t   initProt            = fileMappings[i].initProt;
        if ( cache->header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset) ) {
            // Old cache without the new slid mappings
            if ( i == 1 ) {
                // Add slide info to the __DATA mapping
                slideInfoFileOffset = cache->header.slideInfoOffsetUnused;
                slideInfoFileSize   = cache->header.slideInfoSizeUnused;
                // Don't set auth prot to anything interseting on the old mapppings
                authProt = 0;
            }
        } else {
            // New cache where each mapping has a corresponding slid mapping
            const dyld_cache_mapping_and_slide_info* slidableMappings = (const dyld_cache_mapping_and_slide_info*)&firstPage[cache->header.mappingWithSlideOffset];
            slideInfoFileOffset = slidableMappings[i].slideInfoFileOffset;
            slideInfoFileSize   = slidableMappings[i].slideInfoFileSize;
            if ( (slidableMappings[i].flags & DYLD_CACHE_MAPPING_AUTH_DATA) == 0 )
                authProt = VM_PROT_NOAUTH;
            if ( (slidableMappings[i].flags & DYLD_CACHE_MAPPING_CONST_DATA) != 0 ) {
                // The cache was built with __DATA_CONST being read-only.  We can override that
                // with a boot-arg
                if ( !gEnableSharedCacheDataConst )
                    initProt |= VM_PROT_WRITE;
            }
        }

        // Add a file for each mapping
        info->fd                        = fd;
        info->mappings[i].sms_address               = fileMappings[i].address;
        info->mappings[i].sms_size                  = fileMappings[i].size;
        info->mappings[i].sms_file_offset           = fileMappings[i].fileOffset;
        info->mappings[i].sms_slide_size            = 0;
        info->mappings[i].sms_slide_start           = 0;
        info->mappings[i].sms_max_prot              = fileMappings[i].maxProt;
        info->mappings[i].sms_init_prot             = initProt;
        if ( slideInfoFileSize != 0 ) {
            uint64_t offsetInLinkEditRegion = (slideInfoFileOffset - linkeditMapping->fileOffset);
            info->mappings[i].sms_slide_start   = (user_addr_t)(linkeditMapping->address + offsetInLinkEditRegion);
            info->mappings[i].sms_slide_size    = (user_addr_t)slideInfoFileSize;
            info->mappings[i].sms_init_prot    |= (VM_PROT_SLIDE | authProt);
            info->mappings[i].sms_max_prot     |= (VM_PROT_SLIDE | authProt);
        }
    }
    info->sharedRegionStart = cache->header.sharedRegionStart;
    info->sharedRegionSize  = cache->header.sharedRegionSize;
    info->maxSlide          = cache->header.maxSlide;
    return true;
}


#if !TARGET_OS_SIMULATOR

// update all __DATA pages with slide info
static bool rebaseDataPages(bool isVerbose, const dyld_cache_slide_info* slideInfo, const uint8_t *dataPagesStart,
                            uint64_t sharedRegionStart, SharedCacheLoadInfo* results)
{
    const dyld_cache_slide_info* slideInfoHeader = slideInfo;
    if ( slideInfoHeader != nullptr ) {
        if ( slideInfoHeader->version == 2 ) {
            const dyld_cache_slide_info2* slideHeader = (dyld_cache_slide_info2*)slideInfo;
            const uint32_t  page_size = slideHeader->page_size;
            const uint16_t* page_starts = (uint16_t*)((long)(slideInfo) + slideHeader->page_starts_offset);
            const uint16_t* page_extras = (uint16_t*)((long)(slideInfo) + slideHeader->page_extras_offset);
            for (int i=0; i < slideHeader->page_starts_count; ++i) {
                uint8_t* page = (uint8_t*)(long)(dataPagesStart + (page_size*i));
                uint16_t pageEntry = page_starts[i];
                //dyld::log("page[%d]: page_starts[i]=0x%04X\n", i, pageEntry);
                if ( pageEntry == DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE )
                    continue;
                if ( pageEntry & DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA ) {
                    uint16_t chainIndex = (pageEntry & 0x3FFF);
                    bool done = false;
                    while ( !done ) {
                        uint16_t pInfo = page_extras[chainIndex];
                        uint16_t pageStartOffset = (pInfo & 0x3FFF)*4;
                        //dyld::log("     chain[%d] pageOffset=0x%03X\n", chainIndex, pageStartOffset);
                        rebaseChainV2(page, pageStartOffset, results->slide, slideHeader);
                        done = (pInfo & DYLD_CACHE_SLIDE_PAGE_ATTR_END);
                        ++chainIndex;
                    }
                }
                else {
                    uint32_t pageOffset = pageEntry * 4;
                    //dyld::log("     start pageOffset=0x%03X\n", pageOffset);
                    rebaseChainV2(page, pageOffset, results->slide, slideHeader);
                }
            }
        }
#if __LP64__
        else if ( slideInfoHeader->version == 3 ) {
             const dyld_cache_slide_info3* slideHeader = (dyld_cache_slide_info3*)slideInfo;
             const uint32_t                pageSize    = slideHeader->page_size;
             for (int i=0; i < slideHeader->page_starts_count; ++i) {
                 uint8_t* page = (uint8_t*)(dataPagesStart + (pageSize*i));
                 uint64_t delta = slideHeader->page_starts[i];
                 //dyld::log("page[%d]: page_starts[i]=0x%04X\n", i, delta);
                 if ( delta == DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE )
                     continue;
                 delta = delta/sizeof(uint64_t); // initial offset is byte based
                 dyld_cache_slide_pointer3* loc = (dyld_cache_slide_pointer3*)page;
                 do {
                     loc += delta;
                     delta = loc->plain.offsetToNextPointer;
                     if ( loc->auth.authenticated ) {
#if __has_feature(ptrauth_calls)
                        uint64_t target = sharedRegionStart + loc->auth.offsetFromSharedCacheBase + results->slide;
                        MachOLoaded::ChainedFixupPointerOnDisk ptr;
                        ptr.raw64 = *((uint64_t*)loc);
                        loc->raw = ptr.arm64e.signPointer(loc, target);
#else
                        results->errorMessage = "invalid pointer kind in cache file";
                        return false;
#endif
                     }
                     else {
                        MachOLoaded::ChainedFixupPointerOnDisk ptr;
                        ptr.raw64 = *((uint64_t*)loc);
                        loc->raw = ptr.arm64e.unpackTarget() + results->slide;
                     }
                } while (delta != 0);
            }
        }
#else
        else if ( slideInfoHeader->version == 4 ) {
            const dyld_cache_slide_info4* slideHeader = (dyld_cache_slide_info4*)slideInfo;
            const uint32_t  page_size = slideHeader->page_size;
            const uint16_t* page_starts = (uint16_t*)((long)(slideInfo) + slideHeader->page_starts_offset);
            const uint16_t* page_extras = (uint16_t*)((long)(slideInfo) + slideHeader->page_extras_offset);
            for (int i=0; i < slideHeader->page_starts_count; ++i) {
                uint8_t* page = (uint8_t*)(long)(dataPagesStart + (page_size*i));
                uint16_t pageEntry = page_starts[i];
                //dyld::log("page[%d]: page_starts[i]=0x%04X\n", i, pageEntry);
                if ( pageEntry == DYLD_CACHE_SLIDE4_PAGE_NO_REBASE )
                    continue;
                if ( pageEntry & DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA ) {
                    uint16_t chainIndex = (pageEntry & DYLD_CACHE_SLIDE4_PAGE_INDEX);
                    bool done = false;
                    while ( !done ) {
                        uint16_t pInfo = page_extras[chainIndex];
                        uint16_t pageStartOffset = (pInfo & DYLD_CACHE_SLIDE4_PAGE_INDEX)*4;
                        //dyld::log("     chain[%d] pageOffset=0x%03X\n", chainIndex, pageStartOffset);
                        rebaseChainV4(page, pageStartOffset, results->slide, slideHeader);
                        done = (pInfo & DYLD_CACHE_SLIDE4_PAGE_EXTRA_END);
                        ++chainIndex;
                    }
                }
                else {
                    uint32_t pageOffset = pageEntry * 4;
                    //dyld::log("     start pageOffset=0x%03X\n", pageOffset);
                    rebaseChainV4(page, pageOffset, results->slide, slideHeader);
                }
            }
        }
#endif // LP64
        else {
            results->errorMessage = "invalid slide info in cache file";
            return false;
        }
    }
    return true;
}

static bool reuseExistingCache(const SharedCacheOptions& options, SharedCacheLoadInfo* results)
{
    uint64_t cacheBaseAddress;
#if __i386__
    if ( syscall(294, &cacheBaseAddress) == 0 ) {
#else
    if ( __shared_region_check_np(&cacheBaseAddress) == 0 ) {
#endif
        const DyldSharedCache* existingCache = (DyldSharedCache*)cacheBaseAddress;
        if ( validMagic(options, existingCache) ) {
            const dyld_cache_mapping_info* const fileMappings = (dyld_cache_mapping_info*)(cacheBaseAddress + existingCache->header.mappingOffset);
            results->loadAddress = existingCache;
            results->slide = (long)(cacheBaseAddress - fileMappings[0].address);
            // we don't know the path this cache was previously loaded from, assume default
            getCachePath(options, sizeof(results->path), results->path);
            if ( options.verbose ) {
                const dyld_cache_mapping_and_slide_info* const mappings = (const dyld_cache_mapping_and_slide_info*)(cacheBaseAddress + existingCache->header.mappingWithSlideOffset);
                dyld::log("re-using existing shared cache (%s):\n", results->path);
                shared_file_mapping_slide_np slidMappings[DyldSharedCache::MaxMappings];
                for (int i=0; i < DyldSharedCache::MaxMappings; ++i) {
                    slidMappings[i].sms_address = mappings[i].address;
                    slidMappings[i].sms_size = mappings[i].size;
                    slidMappings[i].sms_file_offset = mappings[i].fileOffset;
                    slidMappings[i].sms_max_prot = mappings[i].maxProt;
                    slidMappings[i].sms_init_prot = mappings[i].initProt;
                    slidMappings[i].sms_address += results->slide;
                    if ( existingCache->header.mappingOffset > __offsetof(dyld_cache_header, mappingWithSlideOffset) ) {
                        // New caches have slide info on each new mapping
                        const dyld_cache_mapping_and_slide_info* const slidableMappings = (dyld_cache_mapping_and_slide_info*)(cacheBaseAddress + existingCache->header.mappingWithSlideOffset);
                        assert(existingCache->header.mappingWithSlideCount <= DyldSharedCache::MaxMappings);
                        if ( !(slidableMappings[i].flags & DYLD_CACHE_MAPPING_AUTH_DATA) ) {
                            slidMappings[i].sms_max_prot  |= VM_PROT_NOAUTH;
                            slidMappings[i].sms_init_prot |= VM_PROT_NOAUTH;
                        }
                        if ( (slidableMappings[i].flags & DYLD_CACHE_MAPPING_CONST_DATA) != 0 ) {
                            // The cache was built with __DATA_CONST being read-only.  We can override that
                            // with a boot-arg
                            if ( !gEnableSharedCacheDataConst )
                                slidMappings[i].sms_init_prot |= VM_PROT_WRITE;
                        }
                    }
                }
                verboseSharedCacheMappings(slidMappings, existingCache->header.mappingCount);
            }
        }
        else {
            results->errorMessage = "existing shared cache in memory is not compatible";
        }

        return true;
    }
    return false;
}

static long pickCacheASLRSlide(CacheInfo& info)
{
    // choose new random slide
#if TARGET_OS_IPHONE || (TARGET_OS_OSX && TARGET_CPU_ARM64)
    // <rdar://problem/20848977> change shared cache slide for 32-bit arm to always be 16k aligned
    long slide;
    if (info.maxSlide == 0)
        slide = 0;
    else
        slide = ((arc4random() % info.maxSlide) & (-16384));
#else
    long slide;
    if (info.maxSlide == 0)
        slide = 0;
    else
        slide = ((arc4random() % info.maxSlide) & (-4096));
#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
    if (dyld::isTranslated()) {
        slide &= (-16384);
    }
#endif
#endif
    
    return slide;
}

static bool mapCacheSystemWide(const SharedCacheOptions& options, SharedCacheLoadInfo* results)
{
    CacheInfo info;
    if ( !preflightCacheFile(options, results, &info) )
        return false;

    int result = 0;
    if ( info.mappingsCount != 3 ) {
        uint32_t maxSlide = options.disableASLR ? 0 : (uint32_t)info.maxSlide;

        shared_file_np file;
        file.sf_fd = info.fd;
        file.sf_mappings_count = info.mappingsCount;
        // For the new syscall, this is actually the max slide.  The kernel now owns the actual slide
        file.sf_slide = maxSlide;
        result = __shared_region_map_and_slide_2_np(1, &file, info.mappingsCount, info.mappings);
    } else {
        // With the old syscall, dyld has to choose the slide
        results->slide = options.disableASLR ? 0 : pickCacheASLRSlide(info);

        // update mappings based on the slide we choose
        for (uint32_t i=0; i < info.mappingsCount; ++i) {
            info.mappings[i].sms_address += results->slide;
            if ( info.mappings[i].sms_slide_size != 0 )
                info.mappings[i].sms_slide_start += (uint32_t)results->slide;
        }

        // If we get here then we don't have the new kernel function, so use the old one
        const dyld_cache_slide_info2*   slideInfo       = nullptr;
        size_t                          slideInfoSize   = 0;
        shared_file_mapping_np mappings[3];
        for (unsigned i = 0; i != 3; ++i) {
            mappings[i].sfm_address         = info.mappings[i].sms_address;
            mappings[i].sfm_size            = info.mappings[i].sms_size;
            mappings[i].sfm_file_offset     = info.mappings[i].sms_file_offset;
            mappings[i].sfm_max_prot        = info.mappings[i].sms_max_prot;
            mappings[i].sfm_init_prot       = info.mappings[i].sms_init_prot;
            if ( info.mappings[i].sms_slide_size != 0 ) {
                slideInfo       = (dyld_cache_slide_info2*)info.mappings[i].sms_slide_start;
                slideInfoSize   = (size_t)info.mappings[i].sms_slide_size;
            }
        }
        result = __shared_region_map_and_slide_np(info.fd, 3, mappings, results->slide, slideInfo, slideInfoSize);
    }

    ::close(info.fd);
    if ( result == 0 ) {
        results->loadAddress = (const DyldSharedCache*)(info.mappings[0].sms_address);
        if ( info.mappingsCount != 3 ) {
            // We don't know our own slide any more as the kernel owns it, so ask for it again now
            if ( reuseExistingCache(options, results) ) {

                // update mappings based on the slide the kernel chose
                for (uint32_t i=0; i < info.mappingsCount; ++i) {
                    info.mappings[i].sms_address += results->slide;
                    if ( info.mappings[i].sms_slide_size != 0 )
                        info.mappings[i].sms_slide_start += (uint32_t)results->slide;
                }

                if ( options.verbose )
                    verboseSharedCacheMappingsToConsole(info.mappings, info.mappingsCount);
                return true;
            }
            // Uh oh, we mapped the kernel, but we didn't find the slide
            if ( options.verbose )
                dyld::logToConsole("dyld: error finding shared cache slide for system wide mapping\n");
            return false;
        }
    }
    else {
        // could be another process beat us to it
        if ( reuseExistingCache(options, results) )
            return true;
        // if cache does not exist, then really is an error
        if ( results->errorMessage == nullptr )
            results->errorMessage = "syscall to map cache into shared region failed";
        return false;
    }

    if ( options.verbose ) {
        dyld::log("mapped dyld cache file system wide: %s\n", results->path);
        verboseSharedCacheMappings(info.mappings, info.mappingsCount);
    }
    return true;
}
#endif // TARGET_OS_SIMULATOR

static bool mapCachePrivate(const SharedCacheOptions& options, SharedCacheLoadInfo* results)
{
    // open and validate cache file
    CacheInfo info;
    if ( !preflightCacheFile(options, results, &info) )
        return false;

    // compute ALSR slide
    results->slide = 0;
#if !TARGET_OS_SIMULATOR
    results->slide = options.disableASLR ? 0 : pickCacheASLRSlide(info);
#endif

    // update mappings
    for (uint32_t i=0; i < info.mappingsCount; ++i) {
        info.mappings[i].sms_address += (uint32_t)results->slide;
        if ( info.mappings[i].sms_slide_size != 0 )
            info.mappings[i].sms_slide_start += (uint32_t)results->slide;
    }

    results->loadAddress = (const DyldSharedCache*)(info.mappings[0].sms_address);

    // deallocate any existing system wide shared cache
    deallocateExistingSharedCache();

#if TARGET_OS_SIMULATOR && TARGET_OS_WATCH
    // <rdar://problem/50887685> watchOS 32-bit cache does not overlap macOS dyld cache address range
    // mmap() of a file needs a vm_allocation behind it, so make one
    vm_address_t loadAddress = 0x40000000;
    ::vm_allocate(mach_task_self(), &loadAddress, 0x40000000, VM_FLAGS_FIXED);
#endif

    // map cache just for this process with mmap()
    for (int i=0; i < info.mappingsCount; ++i) {
        void* mmapAddress = (void*)(uintptr_t)(info.mappings[i].sms_address);
        size_t size = (size_t)(info.mappings[i].sms_size);
        //dyld::log("dyld: mapping address %p with size 0x%08lX\n", mmapAddress, size);
        int protection = 0;
        if ( info.mappings[i].sms_init_prot & VM_PROT_EXECUTE )
            protection   |= PROT_EXEC;
        if ( info.mappings[i].sms_init_prot & VM_PROT_READ )
            protection   |= PROT_READ;
        if ( info.mappings[i].sms_init_prot & VM_PROT_WRITE )
            protection   |= PROT_WRITE;
        off_t offset = info.mappings[i].sms_file_offset;
        if ( ::mmap(mmapAddress, size, protection, MAP_FIXED | MAP_PRIVATE, info.fd, offset) != mmapAddress ) {
            // failed to map some chunk of this shared cache file
            // clear shared region
            ::mmap((void*)((long)SHARED_REGION_BASE), SHARED_REGION_SIZE, PROT_NONE, MAP_FIXED | MAP_PRIVATE| MAP_ANON, 0, 0);
            // return failure
            results->loadAddress        = nullptr;
            results->errorMessage       = "could not mmap() part of dyld cache";
            ::close(info.fd);
            return false;
        }
    }
    ::close(info.fd);

#if TARGET_OS_SIMULATOR // simulator caches do not support sliding
    return true;
#else

    // Change __DATA_CONST to read-write for this block
    DyldSharedCache::DataConstScopedWriter patcher(results->loadAddress, mach_task_self(), options.verbose ? &dyld::log : nullptr);

    __block bool success = true;
    for (int i=0; i < info.mappingsCount; ++i) {
        if ( info.mappings[i].sms_slide_size == 0 )
            continue;
        const dyld_cache_slide_info* slideInfoHeader = (const dyld_cache_slide_info*)info.mappings[i].sms_slide_start;
        const uint8_t* mappingPagesStart = (const uint8_t*)info.mappings[i].sms_address;
        success &= rebaseDataPages(options.verbose, slideInfoHeader, mappingPagesStart, info.sharedRegionStart, results);
    }

    if ( options.verbose ) {
        dyld::log("mapped dyld cache file private to process (%s):\n", results->path);
        verboseSharedCacheMappings(info.mappings, info.mappingsCount);
    }
    return success;
#endif
}



bool loadDyldCache(const SharedCacheOptions& options, SharedCacheLoadInfo* results)
{
    results->loadAddress        = 0;
    results->slide              = 0;
    results->errorMessage       = nullptr;

#if TARGET_OS_SIMULATOR
    // simulator only supports mmap()ing cache privately into process
    return mapCachePrivate(options, results);
#else
    if ( options.forcePrivate ) {
        // mmap cache into this process only
        return mapCachePrivate(options, results);
    }
    else {
        // fast path: when cache is already mapped into shared region
        bool hasError = false;
        if ( reuseExistingCache(options, results) ) {
            hasError = (results->errorMessage != nullptr);
        } else {
            // slow path: this is first process to load cache
            hasError = mapCacheSystemWide(options, results);
        }
        return hasError;
    }
#endif
}


bool findInSharedCacheImage(const SharedCacheLoadInfo& loadInfo, const char* dylibPathToFind, SharedCacheFindDylibResults* results)
{
    if ( loadInfo.loadAddress == nullptr )
        return false;

    if ( loadInfo.loadAddress->header.formatVersion != dyld3::closure::kFormatVersion ) {
        // support for older cache with a different Image* format
#if TARGET_OS_IPHONE
        uint64_t hash = 0;
        for (const char* s=dylibPathToFind; *s != '\0'; ++s)
                hash += hash*4 + *s;
#endif
        const dyld_cache_image_info* const start = (dyld_cache_image_info*)((uint8_t*)loadInfo.loadAddress + loadInfo.loadAddress->header.imagesOffset);
        const dyld_cache_image_info* const end = &start[loadInfo.loadAddress->header.imagesCount];
        for (const dyld_cache_image_info* p = start; p != end; ++p) {
#if TARGET_OS_IPHONE
            // on iOS, inode is used to hold hash of path
            if ( (p->modTime == 0) && (p->inode != hash) )
                continue;
#endif
            const char* aPath = (char*)loadInfo.loadAddress + p->pathFileOffset;
            if ( strcmp(aPath, dylibPathToFind) == 0 ) {
                results->mhInCache    = (const mach_header*)(p->address+loadInfo.slide);
                results->pathInCache  = aPath;
                results->slideInCache = loadInfo.slide;
                results->image        = nullptr;
                return true;
            }
        }
        return false;
    }

    const dyld3::closure::ImageArray* images = loadInfo.loadAddress->cachedDylibsImageArray();
    results->image = nullptr;
    uint32_t imageIndex;
    if ( loadInfo.loadAddress->hasImagePath(dylibPathToFind, imageIndex) ) {
        results->image = images->imageForNum(imageIndex+1);
    }

    if ( results->image == nullptr )
        return false;

    results->mhInCache    = (const mach_header*)((uintptr_t)loadInfo.loadAddress + results->image->cacheOffset());
    results->pathInCache  = results->image->path();
    results->slideInCache = loadInfo.slide;
    return true;
}


bool pathIsInSharedCacheImage(const SharedCacheLoadInfo& loadInfo, const char* dylibPathToFind)
{
    if ( (loadInfo.loadAddress == nullptr) )
        return false;

    uint32_t imageIndex;
    return loadInfo.loadAddress->hasImagePath(dylibPathToFind, imageIndex);
}

void deallocateExistingSharedCache()
{
#if TARGET_OS_SIMULATOR
    // dyld deallocated macOS shared cache before jumping into dyld_sim
#else
    // <rdar://problem/50773474> remove the shared region sub-map
    uint64_t existingCacheAddress = 0;
    if ( __shared_region_check_np(&existingCacheAddress) == 0 ) {
        ::mmap((void*)((long)SHARED_REGION_BASE), SHARED_REGION_SIZE, PROT_NONE, MAP_FIXED | MAP_PRIVATE| MAP_ANON, 0, 0);
    }
#endif

}

} // namespace dyld3

