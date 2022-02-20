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

#include <assert.h>

#include "MachOFileAbstraction.hpp"
#include "DyldSharedCache.h"
#include "CacheBuilder.h"
#include "Diagnostics.h"
#include "IMPCaches.hpp"

CacheBuilder::CacheBuilder(const DyldSharedCache::CreateOptions& options, const dyld3::closure::FileSystem& fileSystem)
    : _options(options)
    , _fileSystem(fileSystem)
    , _fullAllocatedBuffer(0)
    , _diagnostics(options.loggingPrefix, options.verbose)
    , _allocatedBufferSize(0)
{
}

CacheBuilder::~CacheBuilder() {
}


std::string CacheBuilder::errorMessage()
{
    return _diagnostics.errorMessage();
}

void CacheBuilder::copyRawSegments()
{
    const bool log = false;
    const bool logCFConstants = false;

    forEachDylibInfo(^(const DylibInfo& dylib, Diagnostics& dylibDiag) {
        for (const SegmentMappingInfo& info : dylib.cacheLocation) {
            if (log) fprintf(stderr, "copy %s segment %s (0x%08X bytes) from %p to %p (logical addr 0x%llX) for %s\n",
                             _options.archs->name(), info.segName, info.copySegmentSize, info.srcSegment, info.dstSegment, info.dstCacheUnslidAddress, dylib.input->mappedFile.runtimePath.c_str());
            ::memcpy(info.dstSegment, info.srcSegment, info.copySegmentSize);
        }
    });

    // Copy the coalesced __TEXT sections
    const uint64_t numCoalescedSections = sizeof(CacheCoalescedText::SupportedSections) / sizeof(*CacheCoalescedText::SupportedSections);
    dispatch_apply(numCoalescedSections, DISPATCH_APPLY_AUTO, ^(size_t index) {
        const CacheCoalescedText::StringSection& cacheStringSection = _coalescedText.getSectionData(CacheCoalescedText::SupportedSections[index]);
        if (log) fprintf(stderr, "copy %s __TEXT_COAL section %s (0x%08X bytes) to %p (logical addr 0x%llX)\n",
                         _options.archs->name(), CacheCoalescedText::SupportedSections[index],
                         cacheStringSection.bufferSize, cacheStringSection.bufferAddr, cacheStringSection.bufferVMAddr);
        for (const auto& stringAndOffset : cacheStringSection.stringsToOffsets)
            ::memcpy(cacheStringSection.bufferAddr + stringAndOffset.second, stringAndOffset.first.data(), stringAndOffset.first.size() + 1);
    });

    // Copy the coalesced CF sections
    if ( _coalescedText.cfStrings.bufferSize != 0 ) {
        uint8_t* dstBuffer = _coalescedText.cfStrings.bufferAddr;
        uint64_t dstBufferVMAddr = _coalescedText.cfStrings.bufferVMAddr;
        forEachDylibInfo(^(const DylibInfo& dylib, Diagnostics& dylibDiag) {
            const char* segmentName = "__OBJC_CONST";
            const char* sectionName = "__cfstring";
            const DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset& sectionData = dylib.textCoalescer.getSectionCoalescer(segmentName, sectionName);
            if ( sectionData.empty() )
                return;

            uint64_t sectionContentSize = 0;
            const void* sectionContent = dylib.input->mappedFile.mh->findSectionContent(segmentName, sectionName, sectionContentSize);
            assert(sectionContent != nullptr);
            assert(sectionContentSize != 0);
            for (const auto& dylibOffsetAndCacheOffset : sectionData) {
                uint64_t dylibOffset = dylibOffsetAndCacheOffset.first;
                uint64_t cacheOffset = dylibOffsetAndCacheOffset.second;
                if (logCFConstants) fprintf(stderr, "copy %s %s section %s (0x%08X bytes) to %p (logical addr 0x%llX)\n",
                                            _options.archs->name(), segmentName, sectionName,
                                            (uint32_t)DyldSharedCache::ConstantClasses::cfStringAtomSize, dstBuffer + cacheOffset, dstBufferVMAddr + cacheOffset);
                ::memcpy(dstBuffer + cacheOffset, (const uint8_t*)sectionContent + dylibOffset, (size_t)DyldSharedCache::ConstantClasses::cfStringAtomSize);
            }
        });
    }
}

void CacheBuilder::adjustAllImagesForNewSegmentLocations(uint64_t cacheBaseAddress,
                                                         ASLR_Tracker& aslrTracker, LOH_Tracker* lohTracker,
                                                         const CacheBuilder::CacheCoalescedText* coalescedText)
{
    // Note this cannot to be done in parallel because the LOH Tracker and aslr tracker are not thread safe
    __block bool badDylib = false;
    forEachDylibInfo(^(const DylibInfo& dylib, Diagnostics& dylibDiag) {
        if ( dylibDiag.hasError() )
            return;
        adjustDylibSegments(dylib, dylibDiag, cacheBaseAddress, aslrTracker,
                            lohTracker, coalescedText);
        if ( dylibDiag.hasError() )
            badDylib = true;
    });

    if ( badDylib && !_diagnostics.hasError() ) {
        _diagnostics.error("One or more binaries has an error which prevented linking.  See other errors.");
    }
}


CacheBuilder::ASLR_Tracker::~ASLR_Tracker()
{
    if ( _bitmap != nullptr )
        ::free(_bitmap);
#if BUILDING_APP_CACHE_UTIL
    if ( _cacheLevels != nullptr )
        ::free(_cacheLevels);
#endif
}

void CacheBuilder::ASLR_Tracker::setDataRegion(const void* rwRegionStart, size_t rwRegionSize)
{
    _pageCount   = (unsigned)(rwRegionSize+_pageSize-1)/_pageSize;
    _regionStart = (uint8_t*)rwRegionStart;
    _regionEnd   = (uint8_t*)rwRegionStart + rwRegionSize;
    _bitmap      = (bool*)calloc(_pageCount*(_pageSize/kMinimumFixupAlignment)*sizeof(bool), 1);
#if BUILDING_APP_CACHE_UTIL
    size_t cacheLevelsSize = (_pageCount*(_pageSize/kMinimumFixupAlignment)*sizeof(uint8_t));
    _cacheLevels = (uint8_t*)malloc(cacheLevelsSize);
    memset(_cacheLevels, (int)~0U, cacheLevelsSize);
#endif
}

void CacheBuilder::ASLR_Tracker::add(void* loc, uint8_t level)
{
    if (!_enabled)
        return;
    uint8_t* p = (uint8_t*)loc;
    assert(p >= _regionStart);
    assert(p < _regionEnd);
    _bitmap[(p-_regionStart)/kMinimumFixupAlignment] = true;

#if BUILDING_APP_CACHE_UTIL
    if ( level != (uint8_t)~0U ) {
        _cacheLevels[(p-_regionStart)/kMinimumFixupAlignment] = level;
    }
#endif
}

void CacheBuilder::ASLR_Tracker::remove(void* loc)
{
    if (!_enabled)
        return;
    uint8_t* p = (uint8_t*)loc;
    assert(p >= _regionStart);
    assert(p < _regionEnd);
    _bitmap[(p-_regionStart)/kMinimumFixupAlignment] = false;
}

bool CacheBuilder::ASLR_Tracker::has(void* loc, uint8_t* level) const
{
    if (!_enabled)
        return true;
    uint8_t* p = (uint8_t*)loc;
    assert(p >= _regionStart);
    assert(p < _regionEnd);

    if ( _bitmap[(p-_regionStart)/kMinimumFixupAlignment] ) {
#if BUILDING_APP_CACHE_UTIL
        if ( level != nullptr ) {
            uint8_t levelValue = _cacheLevels[(p-_regionStart)/kMinimumFixupAlignment];
            if ( levelValue != (uint8_t)~0U )
                *level = levelValue;
        }
#endif
        return true;
    }
    return false;
}

void CacheBuilder::ASLR_Tracker::setHigh8(void* p, uint8_t high8)
{
    _high8Map[p] = high8;
}

void CacheBuilder::ASLR_Tracker::setAuthData(void* p, uint16_t diversity, bool hasAddrDiv, uint8_t key)
{
    _authDataMap[p] = {diversity, hasAddrDiv, key};
}

void CacheBuilder::ASLR_Tracker::setRebaseTarget32(void*p, uint32_t targetVMAddr)
{
    _rebaseTarget32[p] = targetVMAddr;
}

void CacheBuilder::ASLR_Tracker::setRebaseTarget64(void*p, uint64_t targetVMAddr)
{
    _rebaseTarget64[p] = targetVMAddr;
}

bool CacheBuilder::ASLR_Tracker::hasHigh8(void* p, uint8_t* highByte) const
{
    auto pos = _high8Map.find(p);
    if ( pos == _high8Map.end() )
        return false;
    *highByte = pos->second;
    return true;
}

bool CacheBuilder::ASLR_Tracker::hasAuthData(void* p, uint16_t* diversity, bool* hasAddrDiv, uint8_t* key) const
{
    auto pos = _authDataMap.find(p);
    if ( pos == _authDataMap.end() )
        return false;
    *diversity  = pos->second.diversity;
    *hasAddrDiv = pos->second.addrDiv;
    *key        = pos->second.key;
    return true;
}

bool CacheBuilder::ASLR_Tracker::hasRebaseTarget32(void* p, uint32_t* vmAddr) const
{
    auto pos = _rebaseTarget32.find(p);
    if ( pos == _rebaseTarget32.end() )
        return false;
    *vmAddr = pos->second;
    return true;
}

bool CacheBuilder::ASLR_Tracker::hasRebaseTarget64(void* p, uint64_t* vmAddr) const
{
    auto pos = _rebaseTarget64.find(p);
    if ( pos == _rebaseTarget64.end() )
        return false;
    *vmAddr = pos->second;
    return true;
}

std::vector<void*> CacheBuilder::ASLR_Tracker::getRebaseTargets() const {
    std::vector<void*> targets;
    for (const auto& target : _rebaseTarget32)
        targets.push_back(target.first);
    for (const auto& target : _rebaseTarget64)
        targets.push_back(target.first);
    return targets;
}

////////////////////////////  DylibTextCoalescer ////////////////////////////////////

bool CacheBuilder::DylibTextCoalescer::segmentWasCoalesced(std::string_view segmentName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);

    if ( segmentName == "__OBJC_CONST" ) {
        return !cfStrings.empty();
    }

    return false;
}

bool CacheBuilder::DylibTextCoalescer::sectionWasCoalesced(std::string_view segmentName,
                                                           std::string_view sectionName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        std::map<std::string_view, const DylibSectionOffsetToCacheSectionOffset*> supportedSections = {
            { "__objc_classname", &objcClassNames },
            { "__objc_methname", &objcMethNames },
            { "__objc_methtype", &objcMethTypes }
        };
        auto it = supportedSections.find(sectionName);
        if (it == supportedSections.end())
            return false;
        return !it->second->empty();
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            return !cfStrings.empty();
        }
    }

    return false;
}

CacheBuilder::DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset&
CacheBuilder::DylibTextCoalescer::getSectionCoalescer(std::string_view segmentName, std::string_view sectionName) {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        std::map<std::string_view, DylibSectionOffsetToCacheSectionOffset*> supportedSections = {
            { "__objc_classname", &objcClassNames },
            { "__objc_methname", &objcMethNames },
            { "__objc_methtype", &objcMethTypes }
        };
        auto it = supportedSections.find(sectionName);
        assert(it != supportedSections.end());
        return *it->second;
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            return cfStrings;
        }
    }

    assert(false);
}

const CacheBuilder::DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset&
CacheBuilder::DylibTextCoalescer::getSectionCoalescer(std::string_view segmentName, std::string_view sectionName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        std::map<std::string_view, const DylibSectionOffsetToCacheSectionOffset*> supportedSections = {
            { "__objc_classname", &objcClassNames },
            { "__objc_methname", &objcMethNames },
            { "__objc_methtype", &objcMethTypes }
        };
        auto it = supportedSections.find(sectionName);
        assert(it != supportedSections.end());
        return *it->second;
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            return cfStrings;
        }
    }

    assert(false);
}

////////////////////////////  CacheCoalescedText ////////////////////////////////////
const char* CacheBuilder::CacheCoalescedText::SupportedSections[] = {
    "__objc_classname",
    "__objc_methname",
    "__objc_methtype",
};

void CacheBuilder::CacheCoalescedText::
     parseCoalescableText(const dyld3::MachOAnalyzer* ma,
                          DylibTextCoalescer& textCoalescer,
                          const IMPCaches::SelectorMap& selectors,
                          IMPCaches::HoleMap& selectorsHoleMap) {
    static const bool log = false;

    // We can only remove sections if we know we have split seg v2 to point to it
    // Otherwise, a PC relative load in the __TEXT segment wouldn't know how to point to the new strings
    // which are no longer in the same segment
    uint32_t splitSegSize = 0;
    const void* splitSegStart = ma->getSplitSeg(splitSegSize);
    if (!splitSegStart)
        return;

    if ((*(const uint8_t*)splitSegStart) != DYLD_CACHE_ADJ_V2_FORMAT)
        return;

    // We can only remove sections from the end of a segment, so cache them all and walk backwards.
    __block std::vector<std::pair<std::string, dyld3::MachOAnalyzer::SectionInfo>> textSectionInfos;
    ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stop) {
        if (strcmp(sectInfo.segInfo.segName, "__TEXT") != 0)
            return;
        assert(!malformedSectionRange);
        textSectionInfos.push_back({ sectInfo.sectName, sectInfo });
    });

    const std::set<std::string_view> supportedSections(std::begin(SupportedSections), std::end(SupportedSections));
    int64_t slide = ma->getSlide();

    bool isSelectorsSection = false;
    for (auto sectionInfoIt = textSectionInfos.rbegin(); sectionInfoIt != textSectionInfos.rend(); ++sectionInfoIt) {
        const std::string& sectionName = sectionInfoIt->first;
        const dyld3::MachOAnalyzer::SectionInfo& sectInfo = sectionInfoIt->second;

        isSelectorsSection = (sectionName == "__objc_methname");

        // If we find a section we can't handle then stop here.  Hopefully we coalesced some from the end.
        if (supportedSections.find(sectionName) == supportedSections.end())
            break;

        StringSection& cacheStringSection = getSectionData(sectionName);

        DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset& sectionStringData = textCoalescer.getSectionCoalescer("__TEXT", sectionName);

        // Walk the strings in this section
        const uint8_t* content = (uint8_t*)(sectInfo.sectAddr + slide);
        const char* s   = (char*)content;
        const char* end = s + sectInfo.sectSize;
        while ( s < end ) {
            std::string_view str = s;
            int cacheSectionOffset = 0;

            auto it = cacheStringSection.stringsToOffsets.find(str);
            if (it != cacheStringSection.stringsToOffsets.end()) {
                // Debugging only.  If we didn't include the string then we saved that many bytes
                cacheStringSection.savedSpace += str.size() + 1;
                cacheSectionOffset = it->second;
            } else if (isSelectorsSection) {
                // If we are in the selectors section, we need to move
                // the selectors in the selector map to their correct addresses,
                // and fill the holes with the rest

#if BUILDING_APP_CACHE_UTIL
                cacheSectionOffset = cacheStringSection.bufferSize;
#else
                const IMPCaches::SelectorMap::UnderlyingMap & map = selectors.map;
                IMPCaches::SelectorMap::UnderlyingMap::const_iterator selectorsIterator = map.find(str);
                if (selectorsIterator != map.end()) {
                    cacheSectionOffset = selectorsIterator->second->offset;
                } else {
                    cacheSectionOffset = selectorsHoleMap.addStringOfSize((unsigned)str.size() + 1);
                }
#endif
                cacheStringSection.stringsToOffsets[str] = cacheSectionOffset;
                uint32_t sizeAtLeast = cacheSectionOffset + (uint32_t)str.size() + 1;
                if (cacheStringSection.bufferSize < sizeAtLeast) {
                    cacheStringSection.bufferSize = sizeAtLeast;
                }
            } else {
                auto itAndInserted = cacheStringSection.stringsToOffsets.insert({ str, cacheStringSection.bufferSize });
                cacheSectionOffset = itAndInserted.first->second;
                assert(itAndInserted.second);

                cacheStringSection.bufferSize += str.size() + 1;
                if (log) {
                    printf("Selector: %s -> %s\n", ma->installName(), s);
                }
            }

            // Now keep track of this offset in our source dylib as pointing to this offset
            uint32_t sourceSectionOffset = (uint32_t)((uint64_t)s - (uint64_t)content);
            sectionStringData[sourceSectionOffset] = cacheSectionOffset;
            s += str.size() + 1;
        }
    }
}

void CacheBuilder::CacheCoalescedText::parseCFConstants(const dyld3::MachOAnalyzer *ma,
                                                        DylibTextCoalescer &textCoalescer) {
    static const bool log = false;

    // FIXME: Re-enable this once we can correctly patch the shared cache
    if ( ma != nullptr )
        return;

    if ( !ma->is64() )
        return;

    // We only support chained fixups as we need to rewrite binds/rebases after applying split seg
    // and that is much easier with chained fixups than opcodes
    if ( !ma->hasChainedFixupsLoadCommand() )
        return;

    // FIXME: Support DYLD_CHAINED_PTR_ARM64E_USERLAND once ld64 moves to it.
    const uint16_t pointerFormat = ma->chainedPointerFormat();
    if ( pointerFormat != DYLD_CHAINED_PTR_ARM64E )
        return;

    // We can only remove sections if we know we have split seg v2 to point to it
    // Otherwise, a PC relative load in the __TEXT segment wouldn't know how to point to the new constants
    // which are no longer in the same segment
    if ( !ma->isSplitSegV2() )
        return;

    // We can only remove sections from the end of a segment, so cache them all and walk backwards.
    __block std::vector<std::pair<std::string, dyld3::MachOAnalyzer::SectionInfo>> dataSectionInfos;
    __block uint64_t cstringStartVMAddr = 0;
    __block uint64_t cstringEndVMAddr = 0;
    ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stop) {
        if ( malformedSectionRange )
            return;
        if ( strcmp(sectInfo.segInfo.segName, "__OBJC_CONST") == 0 ) {
            dataSectionInfos.push_back({ sectInfo.sectName, sectInfo });
            return;
        }
        if ( strcmp(sectInfo.segInfo.segName, "__TEXT") == 0 ) {
            if ( strcmp(sectInfo.sectName, "__cstring") == 0 ) {
                if ( ( (sectInfo.sectFlags & SECTION_TYPE) == S_CSTRING_LITERALS ) ) {
                    cstringStartVMAddr = sectInfo.sectAddr;
                    cstringEndVMAddr = cstringStartVMAddr + sectInfo.sectSize;
                }
            }
        }
    });

    // We need to clear the chained pointer fixups for the whole segment, so can only
    // process any type of CF object if we can process them all
    if ( dataSectionInfos.size() != 1 )
        return;

    if ( dataSectionInfos.front().first != "__cfstring" )
        return;

    if ( cstringStartVMAddr == 0 )
        return;

    const dyld3::MachOAnalyzer::SectionInfo& cfStringsSection = dataSectionInfos.back().second;

    // A CFString is layed out in memory as
    // {
    //     uintptr_t   isa;
    //     uint32_t    encoding;
    //     uint32_t    padding;
    //     uintptr_t   cstringData;
    //     uintptr_t   cstringLength;
    // }
    const uint64_t cstringDataOffset = 16;
    const char* className = cfStrings.isaClassName;
    if ( cfStringsSection.sectSize % (uint32_t)DyldSharedCache::ConstantClasses::cfStringAtomSize ) {
        // We don't support padding or any kind on the section
        return;
    }

    uint64_t baseAddress = ma->preferredLoadAddress();

    uint64_t startVMOffset = cfStringsSection.sectAddr - baseAddress;
    uint64_t endVMOffset = startVMOffset + cfStringsSection.sectSize;

    __block Diagnostics diag;

    // Make sure no symbols are pointing in to this section
    __block bool hasSymbols = false;
    ma->forEachGlobalSymbol(diag, ^(const char *symbolName, uint64_t n_value, uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
        uint64_t vmOffset = n_value - baseAddress;
        if ( vmOffset < startVMOffset )
            return;
        if ( vmOffset >= endVMOffset )
            return;
        // In range of our section
        hasSymbols = true;
        stop = true;
    });
    if ( diag.hasError() )
        return;
    if ( hasSymbols )
        return;

    ma->forEachLocalSymbol(diag, ^(const char *symbolName, uint64_t n_value, uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool &stop) {
        uint64_t vmOffset = n_value - baseAddress;
        if ( vmOffset < startVMOffset )
            return;
        if ( vmOffset >= endVMOffset )
            return;
        // In range of our section
        hasSymbols = true;
        stop = true;
    });
    if ( diag.hasError() )
        return;
    if ( hasSymbols )
        return;

    ma->forEachExportedSymbol(diag, ^(const char *symbolName, uint64_t imageOffset, uint64_t flags, uint64_t other, const char *importName, bool &stop) {
        if ( imageOffset < startVMOffset )
            return;
        if ( imageOffset >= endVMOffset )
            return;
        // In range of our section
        hasSymbols = true;
        stop = true;
    });
    if ( diag.hasError() )
        return;
    if ( hasSymbols )
        return;

    __block std::vector<const char*> dependentPaths;
    ma->forEachDependentDylib(^(const char *loadPath, bool isWeak, bool isReExport,
                                bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
        dependentPaths.push_back(loadPath);
    });

    // Find all the binds to the ISA class.  These delineate the atoms
    // In CoreFoundation itself, we are looking for rebases to the ISA
    __block std::vector<uint64_t> atomOffsets;

    bool dylibExportsISA = strcmp(ma->installName(), cfStrings.isaInstallName) == 0;
    if ( !dylibExportsISA ) {
        // This dylib doens't export the class, so look for binds to the ISA
        __block std::vector<std::pair<const char*, int>> bindTargetSymbols;
        ma->forEachChainedFixupTarget(diag, ^(int libraryOrdinal, const char* symbolName, uint64_t addend, bool weakImport, bool& stop) {
            bindTargetSymbols.push_back({ symbolName, libraryOrdinal });
        });

        __block bool foundBadBind = false;
        ma->withChainStarts(diag, ma->chainStartsOffset(), ^(const dyld_chained_starts_in_image* startsInfo) {
            if ( foundBadBind )
                return;
            ma->forEachFixupInAllChains(diag, startsInfo, false, ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc,
                                                                   const dyld_chained_starts_in_segment* segInfo, bool& stopFixups) {
                // Skip anything not in this section
                uint64_t vmOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
                if ( vmOffset < startVMOffset )
                    return;
                if ( vmOffset >= endVMOffset )
                    return;

                uint32_t bindOrdinal;
                int64_t  ptrAddend;
                if ( fixupLoc->isBind(pointerFormat, bindOrdinal, ptrAddend) ) {
                    if ( ptrAddend != 0 ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    if ( bindOrdinal >= bindTargetSymbols.size() ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    if ( strcmp(bindTargetSymbols[bindOrdinal].first, className) != 0 ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    int libOrdinal = bindTargetSymbols[bindOrdinal].second;
                    if ( libOrdinal <= 0 ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    int depIndex = libOrdinal - 1;
                    if ( depIndex >= dependentPaths.size() ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    const char* depLoadPath = dependentPaths[depIndex];
                    // All dylibs must find the ISA in the same place
                    if ( strcmp(cfStrings.isaInstallName, depLoadPath) != 0 ) {
                        foundBadBind = true;
                        stopFixups = true;
                        return;
                    }
                    atomOffsets.push_back(vmOffset);
                }
            });
        });
        if ( foundBadBind )
            return;

        if ( atomOffsets.empty() )
            return;
    }

    if ( diag.hasError() )
        return;

    // Find all the rebases in the atoms, which correpond to pointers strings
    __block std::map<uint64_t, uint64_t> sectionRebases;
    ma->withChainStarts(diag, ma->chainStartsOffset(), ^(const dyld_chained_starts_in_image* startsInfo) {
        ma->forEachFixupInAllChains(diag, startsInfo, false, ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stopFixups) {
            // Skip anything not in this section
            uint64_t vmOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
            if ( vmOffset < startVMOffset )
                return;
            if ( vmOffset >= endVMOffset )
                return;

            uint64_t rebaseTargetRuntimeOffset;
            if ( fixupLoc->isRebase(pointerFormat, 0, rebaseTargetRuntimeOffset) ) {
                if ( dylibExportsISA && (rebaseTargetRuntimeOffset == cfStrings.isaVMOffset) ) {
                    atomOffsets.push_back(vmOffset);
                } else {
                    sectionRebases[vmOffset] = rebaseTargetRuntimeOffset;
                }
            }
        });
    });
    if ( diag.hasError() )
        return;

    // Every atom should have a single rebase to a cstring
    if ( sectionRebases.size() != atomOffsets.size() )
        return;

    std::sort(atomOffsets.begin(), atomOffsets.end());
    for (uint64_t atomOffset : atomOffsets) {
        auto it = sectionRebases.find(atomOffset + cstringDataOffset);
        if ( it == sectionRebases.end() )
            return;
    }

    CFSection& stringSection = this->cfStrings;
    DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset& sectionData = textCoalescer.getSectionCoalescer("__OBJC_CONST", "__cfstring");
    for (uint64_t atomOffset : atomOffsets) {
        if ( log )
            printf("%s: found __cfstring at: 0x%llx\n", ma->installName(), atomOffset);

        // Now keep track of this offset in our source dylib as pointing to this offset
        uint32_t sourceSectionOffset = (uint32_t)(atomOffset - startVMOffset);
        uint32_t cacheSectionOffset = stringSection.bufferSize;
        sectionData[sourceSectionOffset] = cacheSectionOffset;
        stringSection.bufferSize += (uint32_t)DyldSharedCache::ConstantClasses::cfStringAtomSize;
    }
}

void CacheBuilder::CacheCoalescedText::clear() {
    *this = CacheBuilder::CacheCoalescedText();
}


CacheBuilder::CacheCoalescedText::StringSection& CacheBuilder::CacheCoalescedText::getSectionData(std::string_view sectionName) {
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);
    std::map<std::string_view, StringSection*> supportedSections = {
        { "__objc_classname", &objcClassNames },
        { "__objc_methname", &objcMethNames },
        { "__objc_methtype", &objcMethTypes }
    };
    auto it = supportedSections.find(sectionName);
    assert(it != supportedSections.end());
    return *it->second;
}


const CacheBuilder::CacheCoalescedText::StringSection& CacheBuilder::CacheCoalescedText::getSectionData(std::string_view sectionName) const {
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);
    std::map<std::string_view, const StringSection*> supportedSections = {
        { "__objc_classname", &objcClassNames },
        { "__objc_methname", &objcMethNames },
        { "__objc_methtype", &objcMethTypes }
    };
    auto it = supportedSections.find(sectionName);
    assert(it != supportedSections.end());
    return *it->second;
}

uint64_t CacheBuilder::CacheCoalescedText::getSectionVMAddr(std::string_view segmentName,
                                                            std::string_view sectionName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        return getSectionData(sectionName).bufferVMAddr;
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            return cfStrings.bufferVMAddr;
        }
    }

    assert(false);
}

uint8_t* CacheBuilder::CacheCoalescedText::getSectionBufferAddr(std::string_view segmentName,
                                                                std::string_view sectionName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        return getSectionData(sectionName).bufferAddr;
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            return cfStrings.bufferAddr;
        }
    }

    assert(false);
}

uint64_t CacheBuilder::CacheCoalescedText::getSectionObjcTag(std::string_view segmentName,
                                                             std::string_view sectionName) const {
    if (segmentName.size() > 16)
        segmentName = segmentName.substr(0, 16);
    if (sectionName.size() > 16)
        sectionName = sectionName.substr(0, 16);

    if ( segmentName == "__TEXT" ) {
        // Nothing has a tag in __TEXT
        return 0;
    }

    if ( segmentName == "__OBJC_CONST" ) {
        if ( sectionName == "__cfstring" ) {
            // This is defined by objc as the tag we put in the high bits
            // FIXME: Get a tag from objc
            // return 1ULL << 63;
            return 0;
        }
    }

    assert(false);
}
