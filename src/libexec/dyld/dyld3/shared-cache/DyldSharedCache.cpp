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


#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/dyld_priv.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>

#if BUILDING_CACHE_BUILDER
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "SharedCacheBuilder.h"
#include "FileUtils.h"
#endif

#define NO_ULEB
#include "MachOLoaded.h"
#include "ClosureFileSystemPhysical.h"
#include "DyldSharedCache.h"
#include "Trie.hpp"
#include "StringUtils.h"

#include "objc-shared-cache.h"

#if !(BUILDING_LIBDYLD || BUILDING_DYLD)
#include "JSONWriter.h"
#include <sstream>
#endif

#if (BUILDING_LIBDYLD || BUILDING_DYLD)
VIS_HIDDEN bool gEnableSharedCacheDataConst = false;
#endif


#if BUILDING_CACHE_BUILDER
DyldSharedCache::CreateResults DyldSharedCache::create(const CreateOptions&               options,
                                                       const dyld3::closure::FileSystem&  fileSystem,
                                                       const std::vector<MappedMachO>&    dylibsToCache,
                                                       const std::vector<MappedMachO>&    otherOsDylibs,
                                                       const std::vector<MappedMachO>&    osExecutables)
{
    CreateResults       results;
    SharedCacheBuilder  cache(options, fileSystem);
    if (!cache.errorMessage().empty()) {
        results.errorMessage = cache.errorMessage();
        return results;
    }

    std::vector<FileAlias> aliases;
    switch ( options.platform ) {
        case dyld3::Platform::iOS:
        case dyld3::Platform::watchOS:
        case dyld3::Platform::tvOS:
            // FIXME: embedded cache builds should be getting aliases from manifest
            aliases.push_back({"/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit", "/System/Library/Frameworks/IOKit.framework/IOKit"});
            aliases.push_back({"/usr/lib/libstdc++.6.dylib",                                  "/usr/lib/libstdc++.dylib"});
            aliases.push_back({"/usr/lib/libstdc++.6.dylib",                                  "/usr/lib/libstdc++.6.0.9.dylib"});
            aliases.push_back({"/usr/lib/libz.1.dylib",                                       "/usr/lib/libz.dylib"});
            aliases.push_back({"/usr/lib/libSystem.B.dylib",                                  "/usr/lib/libSystem.dylib"});
            aliases.push_back({"/System/Library/Frameworks/Foundation.framework/Foundation",  "/usr/lib/libextension.dylib"}); // <rdar://44315703>
            break;
        default:
            break;
    }

    cache.build(dylibsToCache, otherOsDylibs, osExecutables, aliases);

    results.agileSignature = cache.agileSignature();
    results.cdHashFirst    = cache.cdHashFirst();
    results.cdHashSecond   = cache.cdHashSecond();
    results.warnings       = cache.warnings();
    results.evictions      = cache.evictions();
    if ( cache.errorMessage().empty() ) {
        if ( !options.outputFilePath.empty() )  {
            // write cache file, if path non-empty
            cache.writeFile(options.outputFilePath);
        }
        if ( !options.outputMapFilePath.empty() ) {
            // write map file, if path non-empty
            cache.writeMapFile(options.outputMapFilePath);
        }
    }
    results.errorMessage = cache.errorMessage();
    cache.deleteBuffer();
    return results;
}

bool DyldSharedCache::verifySelfContained(std::vector<MappedMachO>& dylibsToCache,
                                          std::unordered_set<std::string>& badZippered,
                                          MappedMachO (^loader)(const std::string& runtimePath, Diagnostics& diag),
                                          std::vector<std::pair<DyldSharedCache::MappedMachO, std::set<std::string>>>& rejected)
{
    // build map of dylibs
    __block std::map<std::string, std::set<std::string>> badDylibs;
    __block std::set<std::string> knownDylibs;
    for (const DyldSharedCache::MappedMachO& dylib : dylibsToCache) {
        std::set<std::string> reasons;
        if ( dylib.mh->canBePlacedInDyldCache(dylib.runtimePath.c_str(), ^(const char* msg) { badDylibs[dylib.runtimePath].insert(msg);}) ) {
            knownDylibs.insert(dylib.runtimePath);
            knownDylibs.insert(dylib.mh->installName());
        } else {
            badDylibs[dylib.runtimePath].insert("");
        }
    }

    // check all dependencies to assure every dylib in cache only depends on other dylibs in cache
    __block std::set<std::string> missingWeakDylibs;
    __block bool doAgain = true;
    while ( doAgain ) {
        __block std::vector<DyldSharedCache::MappedMachO> foundMappings;
        doAgain = false;
        // scan dylib list making sure all dependents are in dylib list
        for (const DyldSharedCache::MappedMachO& dylib : dylibsToCache) {
            if ( badDylibs.count(dylib.runtimePath) != 0 )
                continue;
            dylib.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
                if ( isWeak && (missingWeakDylibs.count(loadPath) != 0) )
                    return;
                if ( knownDylibs.count(loadPath) == 0 ) {
                    doAgain = true;
                    if ( badZippered.count(loadPath) != 0 ) {
                        badDylibs[dylib.runtimePath].insert("");
                        knownDylibs.erase(dylib.runtimePath);
                        knownDylibs.erase(dylib.mh->installName());
                        badZippered.insert(dylib.runtimePath);
                        badZippered.insert(dylib.mh->installName());
                        return;
                    }
                    Diagnostics diag;
                    MappedMachO foundMapping;
                    if ( badDylibs.count(loadPath) == 0 )
                        foundMapping = loader(loadPath, diag);
                    if ( foundMapping.length == 0 ) {
                        // We allow weakly linked dylibs to be missing only if they are not present on disk
                        // The shared cache doesn't contain enough information to patch them in later if they are
                        // found on disk, so we don't want to pull something in to cache and cut it off from a dylib it
                        // could have used.
                        if ( isWeak ) {
                            missingWeakDylibs.insert(loadPath);
                            return;
                        }

                        if (diag.hasError())
                            badDylibs[dylib.runtimePath].insert(diag.errorMessage());
                        else
                            badDylibs[dylib.runtimePath].insert(std::string("Could not find dependency '") + loadPath +"'");
                        knownDylibs.erase(dylib.runtimePath);
                        knownDylibs.erase(dylib.mh->installName());
                    }
                    else {
                        std::set<std::string> reasons;
                        if ( foundMapping.mh->canBePlacedInDyldCache(foundMapping.runtimePath.c_str(), ^(const char* msg) { badDylibs[foundMapping.runtimePath].insert(msg);})) {
                            // see if existing mapping was returned
                            bool alreadyInVector = false;
                            for (const MappedMachO& existing : dylibsToCache) {
                                if ( existing.mh == foundMapping.mh ) {
                                    alreadyInVector = true;
                                    break;
                                }
                            }
                            if ( !alreadyInVector )
                                foundMappings.push_back(foundMapping);
                            knownDylibs.insert(loadPath);
                            knownDylibs.insert(foundMapping.runtimePath);
                            knownDylibs.insert(foundMapping.mh->installName());
                        } else {
                            badDylibs[dylib.runtimePath].insert("");
                        }
                   }
                }
            });
        }
        dylibsToCache.insert(dylibsToCache.end(), foundMappings.begin(), foundMappings.end());
        // remove bad dylibs
        const auto badDylibsCopy = badDylibs;
        dylibsToCache.erase(std::remove_if(dylibsToCache.begin(), dylibsToCache.end(), [&](const DyldSharedCache::MappedMachO& dylib) {
            auto i = badDylibsCopy.find(dylib.runtimePath);
            if ( i !=  badDylibsCopy.end()) {
                // Only add the warning if we are not a bad zippered dylib
                if ( badZippered.count(dylib.runtimePath) == 0 )
                    rejected.push_back(std::make_pair(dylib, i->second));
                return true;
             }
             else {
                return false;
             }
        }), dylibsToCache.end());
    }

    return badDylibs.empty();
}
#endif

template<typename T>
const T DyldSharedCache::getAddrField(uint64_t addr) const {
    uint64_t slide = (uint64_t)this - unslidLoadAddress();
    return (const T)(addr + slide);
}

uint64_t DyldSharedCache::getCodeSignAddress() const
{
    auto mappings = (const dyld_cache_mapping_info*)((uint8_t*)this + header.mappingOffset);
    return mappings[header.mappingCount-1].address + mappings[header.mappingCount-1].size;
}

void DyldSharedCache::forEachRegion(void (^handler)(const void* content, uint64_t vmAddr, uint64_t size,
                                                    uint32_t initProt, uint32_t maxProt, uint64_t flags)) const
{
    // <rdar://problem/49875993> sanity check cache header
    if ( strncmp(header.magic, "dyld_v1", 7) != 0 )
        return;
    if ( header.mappingOffset > 1024 )
        return;
    if ( header.mappingCount > 20 )
        return;
    if ( header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset) ) {
        const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
        const dyld_cache_mapping_info* mappingsEnd = &mappings[header.mappingCount];
        for (const dyld_cache_mapping_info* m=mappings; m < mappingsEnd; ++m) {
            handler((char*)this + m->fileOffset, m->address, m->size, m->initProt, m->maxProt, 0);
        }
    } else {
        const dyld_cache_mapping_and_slide_info* mappings = (const dyld_cache_mapping_and_slide_info*)((char*)this + header.mappingWithSlideOffset);
        const dyld_cache_mapping_and_slide_info* mappingsEnd = &mappings[header.mappingCount];
        for (const dyld_cache_mapping_and_slide_info* m=mappings; m < mappingsEnd; ++m) {
            handler((char*)this + m->fileOffset, m->address, m->size, m->initProt, m->maxProt, m->flags);
        }
    }
}

bool DyldSharedCache::inCache(const void* addr, size_t length, bool& readOnly) const
{
    // quick out if before start of cache
    if ( addr < this )
        return false;

    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    uintptr_t unslidStart = (uintptr_t)addr - slide;

    // quick out if after end of cache
    const dyld_cache_mapping_info* lastMapping = &mappings[header.mappingCount - 1];
    if ( unslidStart > (lastMapping->address + lastMapping->size) )
        return false;

    // walk cache regions
    const dyld_cache_mapping_info* mappingsEnd = &mappings[header.mappingCount];
    uintptr_t unslidEnd = unslidStart + length;
    for (const dyld_cache_mapping_info* m=mappings; m < mappingsEnd; ++m) {
        if ( (unslidStart >= m->address) && (unslidEnd < (m->address+m->size)) ) {
            readOnly = ((m->initProt & VM_PROT_WRITE) == 0);
            return true;
        }
    }

    return false;
}

bool DyldSharedCache::isAlias(const char* path) const {
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    // paths for aliases are store between cache header and first segment
    return path < ((char*)mappings[0].address + slide);
}

void DyldSharedCache::forEachImage(void (^handler)(const mach_header* mh, const char* installName)) const
{
    const dyld_cache_image_info*   dylibs   = (dyld_cache_image_info*)((char*)this + header.imagesOffset);
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    if ( mappings[0].fileOffset != 0 )
        return;
    uint64_t firstImageOffset = 0;
    uint64_t firstRegionAddress = mappings[0].address;
    for (uint32_t i=0; i < header.imagesCount; ++i) {
        const char* dylibPath  = (char*)this + dylibs[i].pathFileOffset;
        uint64_t offset = dylibs[i].address - firstRegionAddress;
        if ( firstImageOffset == 0 )
            firstImageOffset = offset;
        // skip over aliases
        if ( dylibs[i].pathFileOffset < firstImageOffset)
            continue;
        const mach_header* mh = (mach_header*)((char*)this + offset);
        handler(mh, dylibPath);
    }
}

void DyldSharedCache::forEachImageEntry(void (^handler)(const char* path, uint64_t mTime, uint64_t inode)) const
{
    const dyld_cache_image_info*   dylibs   = (dyld_cache_image_info*)((char*)this + header.imagesOffset);
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    if ( mappings[0].fileOffset != 0 )
        return;
    uint64_t firstImageOffset = 0;
    uint64_t firstRegionAddress = mappings[0].address;
    for (uint32_t i=0; i < header.imagesCount; ++i) {
        const char* dylibPath  = (char*)this + dylibs[i].pathFileOffset;
        uint64_t offset = dylibs[i].address - firstRegionAddress;
        if ( firstImageOffset == 0 )
            firstImageOffset = offset;
        // skip over aliases
        if ( dylibs[i].pathFileOffset < firstImageOffset)
            continue;
        handler(dylibPath, dylibs[i].modTime, dylibs[i].inode);
    }
}

const bool DyldSharedCache::hasLocalSymbolsInfo() const
{
    return (header.localSymbolsOffset != 0 && header.mappingOffset > offsetof(dyld_cache_header,localSymbolsSize));
}

const void* DyldSharedCache::getLocalNlistEntries() const
{
    // check for cache without local symbols info
    if (!this->hasLocalSymbolsInfo())
        return nullptr;
    const auto localInfo = (dyld_cache_local_symbols_info*)((uint8_t*)this + header.localSymbolsOffset);
    return (uint8_t*)localInfo + localInfo->nlistOffset;
}

const uint32_t DyldSharedCache::getLocalNlistCount() const
{
    // check for cache without local symbols info
     if (!this->hasLocalSymbolsInfo())
        return 0;
    const auto localInfo = (dyld_cache_local_symbols_info*)((uint8_t*)this + header.localSymbolsOffset);
    return localInfo->nlistCount;
}

const char* DyldSharedCache::getLocalStrings() const
{
    // check for cache without local symbols info
     if (!this->hasLocalSymbolsInfo())
        return nullptr;
    const auto localInfo = (dyld_cache_local_symbols_info*)((uint8_t*)this + header.localSymbolsOffset);
    return (char*)localInfo + localInfo->stringsOffset;
}

const uint32_t DyldSharedCache::getLocalStringsSize() const
{
    // check for cache without local symbols info
     if (!this->hasLocalSymbolsInfo())
        return 0;
    const auto localInfo = (dyld_cache_local_symbols_info*)((uint8_t*)this + header.localSymbolsOffset);
    return localInfo->stringsSize;
}

 void DyldSharedCache::forEachLocalSymbolEntry(void (^handler)(uint32_t dylibOffset, uint32_t nlistStartIndex, uint32_t nlistCount, bool& stop)) const
{
    // check for cache without local symbols info
     if (!this->hasLocalSymbolsInfo())
        return;
    const auto localInfo = (dyld_cache_local_symbols_info*)((uint8_t*)this + header.localSymbolsOffset);
    const auto localEntries = (dyld_cache_local_symbols_entry*)((uint8_t*)localInfo + localInfo->entriesOffset);
    bool stop = false;
    for (uint32_t i = 0; i < localInfo->entriesCount; i++) {
        dyld_cache_local_symbols_entry localEntry = localEntries[i];
        handler(localEntry.dylibOffset, localEntry.nlistStartIndex, localEntry.nlistCount, stop);
    }
}


const mach_header* DyldSharedCache::getIndexedImageEntry(uint32_t index, uint64_t& mTime, uint64_t& inode) const
{
    const dyld_cache_image_info*   dylibs   = (dyld_cache_image_info*)((char*)this + header.imagesOffset);
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    mTime = dylibs[index].modTime;
    inode = dylibs[index].inode;
    return (mach_header*)((uint8_t*)this + dylibs[index].address - mappings[0].address);
}


 const char* DyldSharedCache::getIndexedImagePath(uint32_t index) const
{
    auto dylibs = (const dyld_cache_image_info*)((char*)this + header.imagesOffset);
    return (char*)this + dylibs[index].pathFileOffset;
}

void DyldSharedCache::forEachImageTextSegment(void (^handler)(uint64_t loadAddressUnslid, uint64_t textSegmentSize, const uuid_t dylibUUID, const char* installName, bool& stop)) const
{
    // check for old cache without imagesText array
    if ( (header.mappingOffset <= __offsetof(dyld_cache_header, imagesTextOffset)) || (header.imagesTextCount == 0) )
        return;

    // walk imageText table and call callback for each entry
    const dyld_cache_image_text_info* imagesText = (dyld_cache_image_text_info*)((char*)this + header.imagesTextOffset);
    const dyld_cache_image_text_info* imagesTextEnd = &imagesText[header.imagesTextCount];
    bool stop = false;
    for (const dyld_cache_image_text_info* p=imagesText; p < imagesTextEnd && !stop; ++p) {
        handler(p->loadAddress, p->textSegmentSize, p->uuid, (char*)this + p->pathOffset, stop);
    }
}

bool DyldSharedCache::addressInText(uint32_t cacheOffset, uint32_t* imageIndex) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    if ( cacheOffset > mappings[0].size )
        return false;
    uint64_t targetAddr = mappings[0].address + cacheOffset;
    // walk imageText table and call callback for each entry
    const dyld_cache_image_text_info* imagesText = (dyld_cache_image_text_info*)((char*)this + header.imagesTextOffset);
    const dyld_cache_image_text_info* imagesTextEnd = &imagesText[header.imagesTextCount];
    for (const dyld_cache_image_text_info* p=imagesText; p < imagesTextEnd; ++p) {
        if ( (p->loadAddress <= targetAddr) && (targetAddr < p->loadAddress+p->textSegmentSize) ) {
            *imageIndex = (uint32_t)(p-imagesText);
            return true;
        }
    }
    return false;
}

const char* DyldSharedCache::archName() const
{
    const char* archSubString = ((char*)this) + 7;
    while (*archSubString == ' ')
        ++archSubString;
    return archSubString;
}


dyld3::Platform DyldSharedCache::platform() const
{
    return (dyld3::Platform)header.platform;
}

#if BUILDING_CACHE_BUILDER
std::string DyldSharedCache::mapFile() const
{
    __block std::string             result;
    __block std::vector<uint64_t>   regionStartAddresses;
    __block std::vector<uint64_t>   regionSizes;
    __block std::vector<uint64_t>   regionFileOffsets;

    result.reserve(256*1024);
    forEachRegion(^(const void* content, uint64_t vmAddr, uint64_t size,
                    uint32_t initProt, uint32_t maxProt, uint64_t flags) {
        regionStartAddresses.push_back(vmAddr);
        regionSizes.push_back(size);
        regionFileOffsets.push_back((uint8_t*)content - (uint8_t*)this);
        char lineBuffer[256];
        const char* prot = "RW";
        if ( maxProt == (VM_PROT_EXECUTE|VM_PROT_READ) )
            prot = "EX";
        else if ( maxProt == VM_PROT_READ )
            prot = "RO";
        if ( size > 1024*1024 )
            sprintf(lineBuffer, "mapping  %s %4lluMB 0x%0llX -> 0x%0llX\n", prot, size/(1024*1024), vmAddr, vmAddr+size);
        else
            sprintf(lineBuffer, "mapping  %s %4lluKB 0x%0llX -> 0x%0llX\n", prot, size/1024,        vmAddr, vmAddr+size);
        result += lineBuffer;
    });

    // TODO:  add linkedit breakdown
    result += "\n\n";

    forEachImage(^(const mach_header* mh, const char* installName) {
        result += std::string(installName) + "\n";
        const dyld3::MachOFile* mf = (dyld3::MachOFile*)mh;
        mf->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& info, bool& stop) {
            char lineBuffer[256];
            sprintf(lineBuffer, "\t%16s 0x%08llX -> 0x%08llX\n", info.segName, info.vmAddr, info.vmAddr+info.vmSize);
            result += lineBuffer;
        });
        result += "\n";
    });

    return result;
}
#endif


uint64_t DyldSharedCache::unslidLoadAddress() const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    return mappings[0].address;
}

void DyldSharedCache::getUUID(uuid_t uuid) const
{
    memcpy(uuid, header.uuid, sizeof(uuid_t));
}

uint64_t DyldSharedCache::mappedSize() const
{
    __block uint64_t startAddr = 0;
    __block uint64_t endAddr = 0;
    forEachRegion(^(const void* content, uint64_t vmAddr, uint64_t size,
                    uint32_t initProt, uint32_t maxProt, uint64_t flags) {
        if ( startAddr == 0 )
            startAddr = vmAddr;
        uint64_t end = vmAddr+size;
        if ( end > endAddr )
            endAddr = end;
    });
    return (endAddr - startAddr);
}

bool DyldSharedCache::findMachHeaderImageIndex(const mach_header* mh, uint32_t& imageIndex) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    uint64_t unslidMh = (uintptr_t)mh - slide;
    const dyld_cache_image_info* dylibs = (dyld_cache_image_info*)((char*)this + header.imagesOffset);
    for (uint32_t i=0; i < header.imagesCount; ++i) {
        if ( dylibs[i].address == unslidMh ) {
            imageIndex = i;
            return true;
        }
    }
    return false;
}

bool DyldSharedCache::hasImagePath(const char* dylibPath, uint32_t& imageIndex) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    if ( mappings[0].fileOffset != 0 )
        return false;
    if ( header.mappingOffset >= 0x118 ) {
        uintptr_t      slide           = (uintptr_t)this - (uintptr_t)(mappings[0].address);
        const uint8_t* dylibTrieStart  = (uint8_t*)(this->header.dylibsTrieAddr + slide);
        const uint8_t* dylibTrieEnd    = dylibTrieStart + this->header.dylibsTrieSize;

        Diagnostics diag;
        const uint8_t* imageNode = dyld3::MachOLoaded::trieWalk(diag, dylibTrieStart, dylibTrieEnd, dylibPath);
        if ( imageNode != NULL ) {
            imageIndex = (uint32_t)dyld3::MachOFile::read_uleb128(diag, imageNode, dylibTrieEnd);
            return true;
        }
    }
    else {
        const dyld_cache_image_info* dylibs = (dyld_cache_image_info*)((char*)this + header.imagesOffset);
        uint64_t firstImageOffset = 0;
        uint64_t firstRegionAddress = mappings[0].address;
        for (uint32_t i=0; i < header.imagesCount; ++i) {
            const char* aPath  = (char*)this + dylibs[i].pathFileOffset;
            if ( strcmp(aPath, dylibPath) == 0 ) {
                imageIndex = i;
                return true;
            }
            uint64_t offset = dylibs[i].address - firstRegionAddress;
            if ( firstImageOffset == 0 )
                firstImageOffset = offset;
            // skip over aliases
            if ( dylibs[i].pathFileOffset < firstImageOffset)
                continue;
        }
    }

    return false;
}

bool DyldSharedCache::isOverridablePath(const char* dylibPath) const
{
    // all dylibs in customer dyld cache cannot be overridden except libdispatch.dylib
    if ( header.cacheType == kDyldSharedCacheTypeProduction ) {
        return (strcmp(dylibPath, "/usr/lib/system/libdispatch.dylib") == 0);
    }
    // in dev caches we can override all paths
    return true;
}

bool DyldSharedCache::hasNonOverridablePath(const char* dylibPath) const
{
    // all dylibs in customer dyld cache cannot be overridden except libdispatch.dylib
    bool pathIsInDyldCacheWhichCannotBeOverridden = false;
    if ( header.cacheType == kDyldSharedCacheTypeProduction ) {
        uint32_t imageIndex;
        pathIsInDyldCacheWhichCannotBeOverridden = this->hasImagePath(dylibPath, imageIndex);
        if ( pathIsInDyldCacheWhichCannotBeOverridden && isOverridablePath(dylibPath) )
            pathIsInDyldCacheWhichCannotBeOverridden = false;
    }
    return pathIsInDyldCacheWhichCannotBeOverridden;
}

#if !BUILDING_LIBDSC
const dyld3::closure::Image* DyldSharedCache::findDlopenOtherImage(const char* path) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    if ( mappings[0].fileOffset != 0 )
        return nullptr;
    if ( header.mappingOffset < __offsetof(dyld_cache_header, otherImageArrayAddr) )
        return nullptr;
    if ( header.otherImageArrayAddr == 0 )
        return nullptr;
    uintptr_t      slide           = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    const uint8_t* dylibTrieStart  = (uint8_t*)(this->header.otherTrieAddr + slide);
    const uint8_t* dylibTrieEnd    = dylibTrieStart + this->header.otherTrieSize;

    Diagnostics diag;
    const uint8_t* imageNode = dyld3::MachOLoaded::trieWalk(diag, dylibTrieStart, dylibTrieEnd, path);
    if ( imageNode != NULL ) {
        dyld3::closure::ImageNum imageNum = (uint32_t)dyld3::MachOFile::read_uleb128(diag, imageNode, dylibTrieEnd);
        uint64_t arrayAddrOffset = header.otherImageArrayAddr - mappings[0].address;
        const dyld3::closure::ImageArray* otherImageArray = (dyld3::closure::ImageArray*)((char*)this + arrayAddrOffset);
        return otherImageArray->imageForNum(imageNum);
    }

    return nullptr;
}

const dyld3::closure::LaunchClosure* DyldSharedCache::findClosure(const char* executablePath) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t      slide                = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    const uint8_t* executableTrieStart  = (uint8_t*)(this->header.progClosuresTrieAddr + slide);
    const uint8_t* executableTrieEnd    = executableTrieStart + this->header.progClosuresTrieSize;
    const uint8_t* closuresStart        = (uint8_t*)(this->header.progClosuresAddr + slide);

    Diagnostics diag;
    const uint8_t* imageNode = dyld3::MachOLoaded::trieWalk(diag, executableTrieStart, executableTrieEnd, executablePath);
    if ( (imageNode == NULL) && (strncmp(executablePath, "/System/", 8) == 0) ) {
        // anything in /System/ should have a closure.  Perhaps it was launched via symlink path
        char realPath[PATH_MAX];
        if ( realpath(executablePath, realPath) != NULL )
            imageNode = dyld3::MachOLoaded::trieWalk(diag, executableTrieStart, executableTrieEnd, realPath);
    }
    if ( imageNode != NULL ) {
        uint32_t closureOffset = (uint32_t)dyld3::MachOFile::read_uleb128(diag, imageNode, executableTrieEnd);
        if ( closureOffset < this->header.progClosuresSize )
            return (dyld3::closure::LaunchClosure*)((uint8_t*)closuresStart + closureOffset);
    }

    return nullptr;
}

#if !BUILDING_LIBDYLD && !BUILDING_DYLD
void DyldSharedCache::forEachLaunchClosure(void (^handler)(const char* executableRuntimePath, const dyld3::closure::LaunchClosure* closure)) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t      slide                = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    const uint8_t* executableTrieStart  = (uint8_t*)(this->header.progClosuresTrieAddr + slide);
    const uint8_t* executableTrieEnd    = executableTrieStart + this->header.progClosuresTrieSize;
    const uint8_t* closuresStart        = (uint8_t*)(this->header.progClosuresAddr + slide);

    std::vector<DylibIndexTrie::Entry> closureEntries;
    if ( Trie<DylibIndex>::parseTrie(executableTrieStart, executableTrieEnd, closureEntries) ) {
        for (DylibIndexTrie::Entry& entry : closureEntries ) {
            uint32_t offset = entry.info.index;
            if ( offset < this->header.progClosuresSize )
                handler(entry.name.c_str(), (const dyld3::closure::LaunchClosure*)(closuresStart+offset));
        }
    }
}

void DyldSharedCache::forEachDlopenImage(void (^handler)(const char* runtimePath, const dyld3::closure::Image* image)) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t      slide           = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    const uint8_t* otherTrieStart  = (uint8_t*)(this->header.otherTrieAddr + slide);
    const uint8_t* otherTrieEnd    = otherTrieStart + this->header.otherTrieSize;

    std::vector<DylibIndexTrie::Entry> otherEntries;
    if ( Trie<DylibIndex>::parseTrie(otherTrieStart, otherTrieEnd, otherEntries) ) {
        for (const DylibIndexTrie::Entry& entry : otherEntries ) {
            dyld3::closure::ImageNum imageNum = entry.info.index;
            uint64_t arrayAddrOffset = header.otherImageArrayAddr - mappings[0].address;
            const dyld3::closure::ImageArray* otherImageArray = (dyld3::closure::ImageArray*)((char*)this + arrayAddrOffset);
            handler(entry.name.c_str(), otherImageArray->imageForNum(imageNum));
        }
    }
}

void DyldSharedCache::forEachDylibPath(void (^handler)(const char* dylibPath, uint32_t index)) const
{
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t      slide                = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    const uint8_t* dylibTrieStart       = (uint8_t*)(this->header.dylibsTrieAddr + slide);
    const uint8_t* dylibTrieEnd         = dylibTrieStart + this->header.dylibsTrieSize;

   std::vector<DylibIndexTrie::Entry> dylibEntries;
    if ( Trie<DylibIndex>::parseTrie(dylibTrieStart, dylibTrieEnd, dylibEntries) ) {
        for (DylibIndexTrie::Entry& entry : dylibEntries ) {
            handler(entry.name.c_str(), entry.info.index);
        }
    }
}
#endif // !BUILDING_LIBDYLD && !BUILDING_DYLD
#endif // !BUILDING_LIBDSC

const dyld3::closure::ImageArray* DyldSharedCache::cachedDylibsImageArray() const
{
    // check for old cache without imagesArray
    if ( header.mappingOffset < 0x100 )
        return nullptr;

    if ( header.dylibsImageArrayAddr == 0 )
        return nullptr;
        
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uint64_t arrayAddrOffset = header.dylibsImageArrayAddr - mappings[0].address;
    return (dyld3::closure::ImageArray*)((char*)this + arrayAddrOffset);
}

const dyld3::closure::ImageArray* DyldSharedCache::otherOSImageArray() const
{
    // check for old cache without imagesArray
    if ( header.mappingOffset < __offsetof(dyld_cache_header, otherImageArrayAddr) )
        return nullptr;

    if ( header.otherImageArrayAddr == 0 )
        return nullptr;

    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uint64_t arrayAddrOffset = header.otherImageArrayAddr - mappings[0].address;
    return (dyld3::closure::ImageArray*)((char*)this + arrayAddrOffset);
}


uint32_t DyldSharedCache::patchableExportCount(uint32_t imageIndex) const {
    if ( header.patchInfoAddr == 0 )
        return 0;

    const dyld_cache_patch_info* patchInfo = getAddrField<dyld_cache_patch_info*>(header.patchInfoAddr);
    const dyld_cache_image_patches* patchArray = getAddrField<dyld_cache_image_patches*>(patchInfo->patchTableArrayAddr);
    if (imageIndex > patchInfo->patchTableArrayCount)
        return 0;
    return patchArray[imageIndex].patchExportsCount;
}

void DyldSharedCache::forEachPatchableExport(uint32_t imageIndex, void (^handler)(uint32_t cacheOffsetOfImpl, const char* exportName)) const {
    if ( header.patchInfoAddr == 0 )
        return;

    const dyld_cache_patch_info* patchInfo = getAddrField<dyld_cache_patch_info*>(header.patchInfoAddr);
    const dyld_cache_image_patches* patchArray = getAddrField<dyld_cache_image_patches*>(patchInfo->patchTableArrayAddr);
    if (imageIndex > patchInfo->patchTableArrayCount)
        return;
    const dyld_cache_image_patches& patch = patchArray[imageIndex];
    if ( (patch.patchExportsStartIndex + patch.patchExportsCount) > patchInfo->patchExportArrayCount )
        return;
    const dyld_cache_patchable_export* patchExports = getAddrField<dyld_cache_patchable_export*>(patchInfo->patchExportArrayAddr);
    const char* exportNames = getAddrField<char*>(patchInfo->patchExportNamesAddr);
    for (uint64_t exportIndex = 0; exportIndex != patch.patchExportsCount; ++exportIndex) {
        const dyld_cache_patchable_export& patchExport = patchExports[patch.patchExportsStartIndex + exportIndex];
        const char* exportName = ( patchExport.exportNameOffset < patchInfo->patchExportNamesSize ) ? &exportNames[patchExport.exportNameOffset] : "";
        handler(patchExport.cacheOffsetOfImpl, exportName);
    }
}

void DyldSharedCache::forEachPatchableUseOfExport(uint32_t imageIndex, uint32_t cacheOffsetOfImpl,
                                                  void (^handler)(dyld_cache_patchable_location patchLocation)) const {
    if ( header.patchInfoAddr == 0 )
        return;

    // Loading a new cache so get the data from the cache header
    const dyld_cache_patch_info* patchInfo = getAddrField<dyld_cache_patch_info*>(header.patchInfoAddr);
    const dyld_cache_image_patches* patchArray = getAddrField<dyld_cache_image_patches*>(patchInfo->patchTableArrayAddr);
    if (imageIndex > patchInfo->patchTableArrayCount)
        return;
    const dyld_cache_image_patches& patch = patchArray[imageIndex];
    if ( (patch.patchExportsStartIndex + patch.patchExportsCount) > patchInfo->patchExportArrayCount )
        return;
    const dyld_cache_patchable_export* patchExports = getAddrField<dyld_cache_patchable_export*>(patchInfo->patchExportArrayAddr);
    const dyld_cache_patchable_location* patchLocations = getAddrField<dyld_cache_patchable_location*>(patchInfo->patchLocationArrayAddr);
    for (uint64_t exportIndex = 0; exportIndex != patch.patchExportsCount; ++exportIndex) {
        const dyld_cache_patchable_export& patchExport = patchExports[patch.patchExportsStartIndex + exportIndex];
        if ( patchExport.cacheOffsetOfImpl != cacheOffsetOfImpl )
            continue;
        if ( (patchExport.patchLocationsStartIndex + patchExport.patchLocationsCount) > patchInfo->patchLocationArrayCount )
            return;
        for (uint64_t locationIndex = 0; locationIndex != patchExport.patchLocationsCount; ++locationIndex) {
            const dyld_cache_patchable_location& patchLocation = patchLocations[patchExport.patchLocationsStartIndex + locationIndex];
            handler(patchLocation);
        }
    }
}

#if (BUILDING_LIBDYLD || BUILDING_DYLD)
void DyldSharedCache::changeDataConstPermissions(mach_port_t machTask, uint32_t permissions,
                                                 DataConstLogFunc logFunc) const {

    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);

    if ( (permissions & VM_PROT_WRITE) != 0 )
        permissions |= VM_PROT_COPY;

    forEachRegion(^(const void *, uint64_t vmAddr, uint64_t size,
                    uint32_t initProt, uint32_t maxProt, uint64_t flags) {
        void* content = (void*)(vmAddr + slide);
        if ( ( flags & DYLD_CACHE_MAPPING_CONST_DATA) == 0 )
            return;
        if ( logFunc != nullptr ) {
            logFunc("dyld: marking shared cache range 0x%x permissions: 0x%09lX -> 0x%09lX\n",
                    permissions, (long)content, (long)content + size);
        }
        kern_return_t result = vm_protect(machTask, (vm_address_t)content, (vm_size_t)size, false, permissions);
        if ( result != KERN_SUCCESS ) {
            if ( logFunc != nullptr )
                logFunc("dyld: failed to mprotect shared cache due to: %d\n", result);
        }
    });
}

DyldSharedCache::DataConstLazyScopedWriter::DataConstLazyScopedWriter(const DyldSharedCache* cache, mach_port_t machTask, DataConstLogFunc logFunc)
    : cache(cache), machTask(machTask), logFunc(logFunc) {
}

DyldSharedCache::DataConstLazyScopedWriter::~DataConstLazyScopedWriter() {
    if ( wasMadeWritable )
        cache->changeDataConstPermissions(machTask, VM_PROT_READ, logFunc);
}

void DyldSharedCache::DataConstLazyScopedWriter::makeWriteable() {
    if ( wasMadeWritable )
        return;
    if ( !gEnableSharedCacheDataConst )
        return;
    if ( cache == nullptr )
        return;
    wasMadeWritable = true;
    cache->changeDataConstPermissions(machTask, VM_PROT_READ | VM_PROT_WRITE, logFunc);
}

DyldSharedCache::DataConstScopedWriter::DataConstScopedWriter(const DyldSharedCache* cache, mach_port_t machTask, DataConstLogFunc logFunc)
    : writer(cache, machTask, logFunc) {
    writer.makeWriteable();
}
#endif

#if !(BUILDING_LIBDYLD || BUILDING_DYLD)
// MRM map file generator
std::string DyldSharedCache::generateJSONMap(const char* disposition) const {
    dyld3::json::Node cacheNode;

    cacheNode.map["version"].value = "1";
    cacheNode.map["disposition"].value = disposition;
    cacheNode.map["base-address"].value = dyld3::json::hex(unslidLoadAddress());
    uuid_t cache_uuid;
    getUUID(cache_uuid);
    uuid_string_t cache_uuidStr;
    uuid_unparse(cache_uuid, cache_uuidStr);
    cacheNode.map["uuid"].value = cache_uuidStr;

    __block dyld3::json::Node imagesNode;
    forEachImage(^(const mach_header *mh, const char *installName) {
        dyld3::json::Node imageNode;
        imageNode.map["path"].value = installName;
        dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)mh;
        uuid_t uuid;
        if (ma->getUuid(uuid)) {
            uuid_string_t uuidStr;
            uuid_unparse(uuid, uuidStr);
            imageNode.map["uuid"].value = uuidStr;
        }

        __block dyld3::json::Node segmentsNode;
        ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
            dyld3::json::Node segmentNode;
            segmentNode.map["name"].value = info.segName;
            segmentNode.map["start-vmaddr"].value = dyld3::json::hex(info.vmAddr);
            segmentNode.map["end-vmaddr"].value = dyld3::json::hex(info.vmAddr + info.vmSize);
            segmentsNode.array.push_back(segmentNode);
        });
        imageNode.map["segments"] = segmentsNode;
        imagesNode.array.push_back(imageNode);
    });

    cacheNode.map["images"] = imagesNode;

    std::stringstream stream;
    printJSON(cacheNode, 0, stream);

    return stream.str();
}

std::string DyldSharedCache::generateJSONDependents() const {
    std::unordered_map<std::string, std::set<std::string>> dependents;
    computeTransitiveDependents(dependents);

    std::stringstream stream;

    stream << "{";
    bool first = true;
    for (auto p : dependents) {
        if (!first) stream << "," << std::endl;
        first = false;

        stream << "\"" << p.first << "\" : [" << std::endl;
        bool firstDependent = true;
        for (const std::string & dependent : p.second) {
            if (!firstDependent) stream << "," << std::endl;
            firstDependent = false;
            stream << "  \"" << dependent << "\"";
        }
        stream << "]" <<  std::endl;
    }
    stream << "}" << std::endl;
    return stream.str();
}

#endif

#if !(BUILDING_LIBDYLD || BUILDING_DYLD)
dyld3::MachOAnalyzer::VMAddrConverter DyldSharedCache::makeVMAddrConverter(bool contentRebased) const {
    typedef dyld3::MachOAnalyzer::VMAddrConverter VMAddrConverter;

    __block VMAddrConverter::SharedCacheFormat pointerFormat = VMAddrConverter::SharedCacheFormat::none;
    __block uint64_t pointerValueAdd = 0;;
    forEachSlideInfo(^(uint64_t mappingStartAddress, uint64_t mappingSize, const uint8_t *mappingPagesStart, uint64_t slideInfoOffset, uint64_t slideInfoSize, const dyld_cache_slide_info *slideInfoHeader) {
        assert(slideInfoHeader->version >= 2);
        if ( slideInfoHeader->version == 2 ) {
            const dyld_cache_slide_info2* slideInfo = (dyld_cache_slide_info2*)(slideInfoHeader);
            assert(slideInfo->delta_mask == 0x00FFFF0000000000);
            pointerFormat   = VMAddrConverter::SharedCacheFormat::v2_x86_64_tbi;
            pointerValueAdd = slideInfo->value_add;
        } else if ( slideInfoHeader->version == 3 ) {
            pointerFormat   = VMAddrConverter::SharedCacheFormat::v3;
            pointerValueAdd = unslidLoadAddress();
        } else {
            assert(false);
        }
    });

    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);

    VMAddrConverter vmAddrConverter;
    vmAddrConverter.preferredLoadAddress            = pointerValueAdd;
    vmAddrConverter.slide                           = slide;
    vmAddrConverter.chainedPointerFormat            = 0;
    vmAddrConverter.sharedCacheChainedPointerFormat = pointerFormat;
    vmAddrConverter.contentRebased                  = contentRebased;

    return vmAddrConverter;
}
#endif

const dyld_cache_slide_info* DyldSharedCache::legacyCacheSlideInfo() const
{
    assert(header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset));
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);

    uint64_t offsetInLinkEditRegion = (header.slideInfoOffsetUnused - mappings[2].fileOffset);
    return (dyld_cache_slide_info*)((uint8_t*)(mappings[2].address) + slide + offsetInLinkEditRegion);
}

const dyld_cache_mapping_info* DyldSharedCache::legacyCacheDataRegionMapping() const
{
    assert(header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset));
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    return &mappings[1];
}

const uint8_t* DyldSharedCache::legacyCacheDataRegionBuffer() const
{
    assert(header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset));
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)this + header.mappingOffset);
    uintptr_t slide = (uintptr_t)this - (uintptr_t)(mappings[0].address);
    
    return (uint8_t*)(legacyCacheDataRegionMapping()->address) + slide;
}

#if !BUILDING_LIBDSC
const objc_opt::objc_opt_t* DyldSharedCache::objcOpt() const {
    // Find the objc image
    const dyld3::MachOAnalyzer* objcMA = nullptr;

    uint32_t imageIndex;
    if ( hasImagePath("/usr/lib/libobjc.A.dylib", imageIndex) ) {
        const dyld3::closure::ImageArray* images = cachedDylibsImageArray();
        const dyld3::closure::Image* image = images->imageForNum(imageIndex+1);
        objcMA = (const dyld3::MachOAnalyzer*)((uintptr_t)this + image->cacheOffset());
    } else {
        return nullptr;
    }

    // If we found the objc image, then try to find the read-only data inside.
    __block const uint8_t* objcROContent = nullptr;
    int64_t slide = objcMA->getSlide();
    objcMA->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo& info, bool malformedSectionRange, bool& stop) {
        if (strcmp(info.segInfo.segName, "__TEXT") != 0)
            return;
        if (strcmp(info.sectName, "__objc_opt_ro") != 0)
            return;
        if ( malformedSectionRange ) {
            stop = true;
            return;
        }
        objcROContent = (uint8_t*)(info.sectAddr + slide);
    });

    if (objcROContent == nullptr)
        return nullptr;

    const objc_opt::objc_opt_t* optObjCHeader = (const objc_opt::objc_opt_t*)objcROContent;
    return optObjCHeader->version == objc_opt::VERSION ? optObjCHeader : nullptr;
}

const void* DyldSharedCache::objcOptPtrs() const {
    // Find the objc image
    const dyld3::MachOAnalyzer* objcMA = nullptr;

    uint32_t imageIndex;
    if ( hasImagePath("/usr/lib/libobjc.A.dylib", imageIndex) ) {
        const dyld3::closure::ImageArray* images = cachedDylibsImageArray();
        const dyld3::closure::Image* image = images->imageForNum(imageIndex+1);
        objcMA = (const dyld3::MachOAnalyzer*)((uintptr_t)this + image->cacheOffset());
    } else {
        return nullptr;
    }

    // If we found the objc image, then try to find the read-only data inside.
    __block const void* objcPointersContent = nullptr;
    int64_t slide = objcMA->getSlide();
    uint32_t pointerSize = objcMA->pointerSize();
    objcMA->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo& info, bool malformedSectionRange, bool& stop) {
        if ( (strncmp(info.segInfo.segName, "__DATA", 6) != 0) && (strncmp(info.segInfo.segName, "__AUTH", 6) != 0) )
            return;
        if (strcmp(info.sectName, "__objc_opt_ptrs") != 0)
            return;
        if ( info.sectSize != pointerSize ) {
            stop = true;
            return;
        }
        if ( malformedSectionRange ) {
            stop = true;
            return;
        }
        objcPointersContent = (uint8_t*)(info.sectAddr + slide);
    });

    return objcPointersContent;
}
#endif

std::pair<const void*, uint64_t> DyldSharedCache::getObjCConstantRange() const {
    const dyld3::MachOAnalyzer* libDyldMA = nullptr;
    uint32_t imageIndex;
    if ( hasImagePath("/usr/lib/system/libdyld.dylib", imageIndex) ) {
        const dyld3::closure::ImageArray* images = cachedDylibsImageArray();
        const dyld3::closure::Image* image = images->imageForNum(imageIndex+1);
        libDyldMA = (const dyld3::MachOAnalyzer*)((uintptr_t)this + image->cacheOffset());

        std::pair<const void*, uint64_t> ranges = { nullptr, 0 };
#if TARGET_OS_OSX
        ranges.first = libDyldMA->findSectionContent("__DATA", "__objc_ranges", ranges.second);
#else
        ranges.first = libDyldMA->findSectionContent("__DATA_CONST", "__objc_ranges", ranges.second);
#endif
        return ranges;
    }

    return { nullptr, 0 };
}

bool DyldSharedCache::hasSlideInfo() const {
    if ( header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset) ) {
        return header.slideInfoSizeUnused != 0;
    } else {
        const dyld_cache_mapping_and_slide_info* slidableMappings = (const dyld_cache_mapping_and_slide_info*)((char*)this + header.mappingWithSlideOffset);
        for (uint32_t i = 0; i != header.mappingWithSlideCount; ++i) {
            if ( slidableMappings[i].slideInfoFileSize != 0 ) {
                return true;
            }
        }
    }
    return false;
}

void DyldSharedCache::forEachSlideInfo(void (^handler)(uint64_t mappingStartAddress, uint64_t mappingSize,
                                                       const uint8_t* mappingPagesStart,
                                                       uint64_t slideInfoOffset, uint64_t slideInfoSize,
                                                       const dyld_cache_slide_info* slideInfoHeader)) const {
    if ( header.mappingOffset <= __offsetof(dyld_cache_header, mappingWithSlideOffset) ) {
        // Old caches should get the slide info from the cache header and assume a single data region.
        const dyld_cache_mapping_info* dataMapping = legacyCacheDataRegionMapping();
        uint64_t dataStartAddress = dataMapping->address;
        uint64_t dataSize = dataMapping->size;
        const uint8_t* dataPagesStart = legacyCacheDataRegionBuffer();
        const dyld_cache_slide_info* slideInfoHeader = legacyCacheSlideInfo();

        handler(dataStartAddress, dataSize, dataPagesStart,
                header.slideInfoOffsetUnused, header.slideInfoSizeUnused, slideInfoHeader);
    } else {
        const dyld_cache_mapping_and_slide_info* slidableMappings = (const dyld_cache_mapping_and_slide_info*)((char*)this + header.mappingWithSlideOffset);
        const dyld_cache_mapping_and_slide_info* linkeditMapping = &slidableMappings[header.mappingWithSlideCount - 1];
        uint64_t sharedCacheSlide = (uint64_t)this - unslidLoadAddress();

        for (uint32_t i = 0; i != header.mappingWithSlideCount; ++i) {
            if ( slidableMappings[i].slideInfoFileOffset != 0 ) {
                // Get the data pages
                uint64_t dataStartAddress = slidableMappings[i].address;
                uint64_t dataSize = slidableMappings[i].size;
                const uint8_t* dataPagesStart = (uint8_t*)dataStartAddress + sharedCacheSlide;

                // Get the slide info
                uint64_t offsetInLinkEditRegion = (slidableMappings[i].slideInfoFileOffset - linkeditMapping->fileOffset);
                const dyld_cache_slide_info* slideInfoHeader = (dyld_cache_slide_info*)((uint8_t*)(linkeditMapping->address) + sharedCacheSlide + offsetInLinkEditRegion);
                handler(dataStartAddress, dataSize, dataPagesStart,
                        slidableMappings[i].slideInfoFileOffset, slidableMappings[i].slideInfoFileSize, slideInfoHeader);
            }
        }
    }
}

#if BUILDING_LIBDYLD
const char* DyldSharedCache::getCanonicalPath(const char *path) const {
    uint32_t dyldCacheImageIndex;
    if ( hasImagePath(path, dyldCacheImageIndex) )
        return getIndexedImagePath(dyldCacheImageIndex);
#if TARGET_OS_OSX
    // on macOS support "Foo.framework/Foo" symlink
    char resolvedPath[PATH_MAX];
    realpath(path, resolvedPath);
    int realpathErrno = errno;
    // If realpath() resolves to a path which does not exist on disk, errno is set to ENOENT
    if ( (realpathErrno == ENOENT) || (realpathErrno == 0) ) {
        if ( hasImagePath(resolvedPath, dyldCacheImageIndex) )
            return getIndexedImagePath(dyldCacheImageIndex);
    }
#endif
    return nullptr;
}
#endif

#if !(BUILDING_LIBDYLD || BUILDING_DYLD)
void DyldSharedCache::fillMachOAnalyzersMap(std::unordered_map<std::string,dyld3::MachOAnalyzer*> & dylibAnalyzers) const {
    forEachImage(^(const mach_header *mh, const char *iteratedInstallName) {
        dylibAnalyzers[std::string(iteratedInstallName)] = (dyld3::MachOAnalyzer*)mh;
    });
}

void DyldSharedCache::computeReverseDependencyMapForDylib(std::unordered_map<std::string, std::set<std::string>> &reverseDependencyMap, const std::unordered_map<std::string,dyld3::MachOAnalyzer*> & dylibAnalyzers, const std::string &loadPath) const {
    dyld3::MachOAnalyzer *ma = dylibAnalyzers.at(loadPath);
    if (reverseDependencyMap.find(loadPath) != reverseDependencyMap.end()) return;
    reverseDependencyMap[loadPath] = std::set<std::string>();

    ma->forEachDependentDylib(^(const char *dependencyLoadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
        if (isUpward) return;
        std::string dependencyLoadPathString = std::string(dependencyLoadPath);
        computeReverseDependencyMapForDylib(reverseDependencyMap, dylibAnalyzers, dependencyLoadPathString);
        reverseDependencyMap[dependencyLoadPathString].insert(loadPath);
    });
}

// Walks the shared cache and construct the reverse dependency graph (if dylib A depends on B,
// constructs the graph with B -> A edges)
void DyldSharedCache::computeReverseDependencyMap(std::unordered_map<std::string, std::set<std::string>> &reverseDependencyMap) const {
    std::unordered_map<std::string,dyld3::MachOAnalyzer*> dylibAnalyzers;

    fillMachOAnalyzersMap(dylibAnalyzers);
    forEachImage(^(const mach_header *mh, const char *installName) {
        computeReverseDependencyMapForDylib(reverseDependencyMap, dylibAnalyzers, std::string(installName));
    });
}

// uses the reverse dependency graph constructed above to find the recursive set of dependents for each dylib
void DyldSharedCache::findDependentsRecursively(std::unordered_map<std::string, std::set<std::string>> &transitiveDependents, const std::unordered_map<std::string, std::set<std::string>> &reverseDependencyMap, std::set<std::string> & visited, const std::string &loadPath) const {

    if (transitiveDependents.find(loadPath) != transitiveDependents.end()) {
        return;
    }

    if (visited.find(loadPath) != visited.end()) {
        return;
    }

    visited.insert(loadPath);

    std::set<std::string> dependents;

    for (const std::string & dependent : reverseDependencyMap.at(loadPath)) {
        findDependentsRecursively(transitiveDependents, reverseDependencyMap, visited, dependent);
        if (transitiveDependents.find(dependent) != transitiveDependents.end()) {
            std::set<std::string> & theseTransitiveDependents = transitiveDependents.at(dependent);
            dependents.insert(theseTransitiveDependents.begin(), theseTransitiveDependents.end());
        }
        dependents.insert(dependent);
    }

    transitiveDependents[loadPath] = dependents;
}

// Fills a map from each install name N to the set of install names depending on N
void DyldSharedCache::computeTransitiveDependents(std::unordered_map<std::string, std::set<std::string>> & transitiveDependents) const {
    std::unordered_map<std::string, std::set<std::string>> reverseDependencyMap;
    computeReverseDependencyMap(reverseDependencyMap);
    forEachImage(^(const mach_header *mh, const char *installName) {
        std::set<std::string> visited;
        findDependentsRecursively(transitiveDependents, reverseDependencyMap, visited, std::string(installName));
    });
}
#endif
