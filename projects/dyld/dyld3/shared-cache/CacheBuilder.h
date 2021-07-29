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


#ifndef CacheBuilder_h
#define CacheBuilder_h

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "ClosureFileSystem.h"
#include "DyldSharedCache.h"
#include "Diagnostics.h"
#include "MachOAnalyzer.h"
#include "IMPCaches.hpp"

template <typename P> class LinkeditOptimizer;


class CacheBuilder {
public:
    CacheBuilder(const DyldSharedCache::CreateOptions& options, const dyld3::closure::FileSystem& fileSystem);
    virtual ~CacheBuilder();

    struct InputFile {
        enum State {
            Unset,
            MustBeIncluded,
            MustBeIncludedForDependent,
            MustBeExcludedIfUnused
        };
        InputFile(const char* path, State state) : path(path), state(state) { }
        const char*     path;
        State           state = Unset;
        Diagnostics     diag;

        bool mustBeIncluded() const {
            return (state == MustBeIncluded) || (state == MustBeIncludedForDependent);
        }
    };

    // Contains a MachO which has been loaded from the file system and may potentially need to be unloaded later.
    struct LoadedMachO {
        DyldSharedCache::MappedMachO    mappedFile;
        dyld3::closure::LoadedFileInfo  loadedFileInfo;
        InputFile*                      inputFile;
    };

    std::string                                 errorMessage();

    struct Region
    {
        uint8_t*    buffer                          = nullptr;
        uint64_t    bufferSize                      = 0;
        uint64_t    sizeInUse                       = 0;
        uint64_t    unslidLoadAddress               = 0;
        uint64_t    cacheFileOffset                 = 0;
        uint8_t     initProt                        = 0;
        uint8_t     maxProt                         = 0;
        std::string name;
        uint64_t    index                  = ~0ULL; // The index of this region in the final binary

        // Each region can optionally have its own slide info
        uint8_t*    slideInfoBuffer                 = nullptr;
        uint64_t    slideInfoBufferSizeAllocated    = 0;
        uint64_t    slideInfoFileOffset             = 0;
        uint64_t    slideInfoFileSize               = 0;
    };

    struct SegmentMappingInfo {
        const void*     srcSegment;
        const char*     segName;
        void*           dstSegment;
        uint64_t        dstCacheUnslidAddress;
        uint32_t        dstCacheFileOffset;
        uint32_t        dstCacheSegmentSize;
        uint32_t        dstCacheFileSize;
        uint32_t        copySegmentSize;
        uint32_t        srcSegmentIndex;
        // Used by the AppCacheBuilder to work out which one of the regions this segment is in
        const Region*   parentRegion            = nullptr;
    };

    struct DylibTextCoalescer {

        typedef std::map<uint32_t, uint32_t> DylibSectionOffsetToCacheSectionOffset;

        DylibSectionOffsetToCacheSectionOffset objcClassNames;
        DylibSectionOffsetToCacheSectionOffset objcMethNames;
        DylibSectionOffsetToCacheSectionOffset objcMethTypes;

        DylibSectionOffsetToCacheSectionOffset cfStrings;

        bool segmentWasCoalesced(std::string_view segmentName) const;
        bool sectionWasCoalesced(std::string_view segmentName, std::string_view sectionName) const;
        DylibSectionOffsetToCacheSectionOffset& getSectionCoalescer(std::string_view segmentName, std::string_view sectionName);
        const DylibSectionOffsetToCacheSectionOffset& getSectionCoalescer(std::string_view segmentName, std::string_view sectionName) const;
    };

    struct CacheCoalescedText {
        static const char* SupportedSections[3];
        struct StringSection {
            // Map from class name strings to offsets in to the class names buffer
            std::map<std::string_view, uint32_t> stringsToOffsets;
            uint8_t*                             bufferAddr       = nullptr;
            uint32_t                             bufferSize       = 0;
            uint64_t                             bufferVMAddr     = 0;

            // Note this is for debugging only
            uint64_t                             savedSpace       = 0;
        };

        struct CFSection {
            uint8_t*                             bufferAddr         = nullptr;
            uint32_t                             bufferSize         = 0;
            uint64_t                             bufferVMAddr       = 0;
            uint64_t                             cacheFileOffset    = 0;

            // The install name of the dylib for the ISA
            const char*                          isaInstallName     = nullptr;
            const char*                          isaClassName       = "___CFConstantStringClassReference";
            uint64_t                             isaVMOffset        = 0;
        };

        StringSection objcClassNames;
        StringSection objcMethNames;
        StringSection objcMethTypes;

        CFSection     cfStrings;

        void parseCoalescableText(const dyld3::MachOAnalyzer* ma,
                                  DylibTextCoalescer& textCoalescer,
                                  const IMPCaches::SelectorMap& selectors,
                                  IMPCaches::HoleMap& selectorHoleMap);
        void parseCFConstants(const dyld3::MachOAnalyzer* ma,
                              DylibTextCoalescer& textCoalescer);
        void clear();

        StringSection& getSectionData(std::string_view sectionName);
        const StringSection& getSectionData(std::string_view sectionName) const;
        uint64_t getSectionVMAddr(std::string_view segmentName, std::string_view sectionName) const;
        uint8_t* getSectionBufferAddr(std::string_view segmentName, std::string_view sectionName) const;
        uint64_t getSectionObjcTag(std::string_view segmentName, std::string_view sectionName) const;
    };

    class ASLR_Tracker
    {
    public:
        ASLR_Tracker() = default;
        ~ASLR_Tracker();

        ASLR_Tracker(ASLR_Tracker&&) = delete;
        ASLR_Tracker(const ASLR_Tracker&) = delete;
        ASLR_Tracker& operator=(ASLR_Tracker&& other) = delete;
        ASLR_Tracker& operator=(const ASLR_Tracker& other) = delete;

        void        setDataRegion(const void* rwRegionStart, size_t rwRegionSize);
        void        add(void* loc, uint8_t level = (uint8_t)~0);
        void        setHigh8(void* p, uint8_t high8);
        void        setAuthData(void* p, uint16_t diversity, bool hasAddrDiv, uint8_t key);
        void        setRebaseTarget32(void*p, uint32_t targetVMAddr);
        void        setRebaseTarget64(void*p, uint64_t targetVMAddr);
        void        remove(void* p);
        bool        has(void* loc, uint8_t* level = nullptr) const;
        const bool* bitmap()        { return _bitmap; }
        unsigned    dataPageCount() { return _pageCount; }
        unsigned    pageSize() const { return _pageSize; }
        void        disable()       { _enabled = false; };
        bool        hasHigh8(void* p, uint8_t* highByte) const;
        bool        hasAuthData(void* p, uint16_t* diversity, bool* hasAddrDiv, uint8_t* key) const;
        bool        hasRebaseTarget32(void* p, uint32_t* vmAddr) const;
        bool        hasRebaseTarget64(void* p, uint64_t* vmAddr) const;

        // Get all the out of band rebase targets.  Used for the kernel collection builder
        // to emit the classic relocations
        std::vector<void*> getRebaseTargets() const;

    private:

        enum {
#if BUILDING_APP_CACHE_UTIL
            // The x86_64 kernel collection needs 1-byte aligned fixups
            kMinimumFixupAlignment = 1
#else
            // Shared cache fixups must be at least 4-byte aligned
            kMinimumFixupAlignment = 4
#endif
        };

        uint8_t*     _regionStart    = nullptr;
        uint8_t*     _regionEnd      = nullptr;
        bool*        _bitmap         = nullptr;
        unsigned     _pageCount      = 0;
        unsigned     _pageSize       = 4096;
        bool         _enabled        = true;

        struct AuthData {
            uint16_t    diversity;
            bool        addrDiv;
            uint8_t     key;
        };
        std::unordered_map<void*, uint8_t>  _high8Map;
        std::unordered_map<void*, AuthData> _authDataMap;
        std::unordered_map<void*, uint32_t> _rebaseTarget32;
        std::unordered_map<void*, uint64_t> _rebaseTarget64;

        // For kernel collections to work out which other collection a given
        // fixup is relative to
#if BUILDING_APP_CACHE_UTIL
        uint8_t*                            _cacheLevels = nullptr;
#endif
    };

    typedef std::map<uint64_t, std::set<void*>> LOH_Tracker;

    // For use by the LinkeditOptimizer to work out which symbols to strip on each binary
    enum class DylibStripMode {
        stripNone,
        stripLocals,
        stripExports,
        stripAll
    };

    struct DylibInfo
    {
        const LoadedMachO*              input;
        std::string                     dylibID;
        std::vector<SegmentMappingInfo> cacheLocation;
        DylibTextCoalescer              textCoalescer;

        // <class name, metaclass> -> pointer
        std::unordered_map<IMPCaches::ClassKey, std::unique_ptr<IMPCaches::ClassData>, IMPCaches::ClassKeyHasher> impCachesClassData;
    };

protected:
    template <typename P>
    friend class LinkeditOptimizer;

    struct UnmappedRegion
    {
        uint8_t*    buffer                 = nullptr;
        uint64_t    bufferSize             = 0;
        uint64_t    sizeInUse              = 0;
    };

    // Virtual methods overridden by the shared cache builder and app cache builder
    virtual void forEachDylibInfo(void (^callback)(const DylibInfo& dylib, Diagnostics& dylibDiag)) = 0;

    void        copyRawSegments();
    void        adjustAllImagesForNewSegmentLocations(uint64_t cacheBaseAddress,
                                                      ASLR_Tracker& aslrTracker, LOH_Tracker* lohTracker,
                                                      const CacheBuilder::CacheCoalescedText* coalescedText);

    // implemented in AdjustDylibSegemnts.cpp
    void        adjustDylibSegments(const DylibInfo& dylib, Diagnostics& diag,
                                    uint64_t cacheBaseAddress,
                                    CacheBuilder::ASLR_Tracker& aslrTracker,
                                    CacheBuilder::LOH_Tracker* lohTracker,
                                    const CacheBuilder::CacheCoalescedText* coalescedText) const;

    // implemented in OptimizerLinkedit.cpp
    void        optimizeLinkedit(UnmappedRegion* localSymbolsRegion,
                                 const std::vector<std::tuple<const mach_header*, const char*, DylibStripMode>>& images);

    // implemented in OptimizerBranches.cpp
    void        optimizeAwayStubs(const std::vector<std::pair<const mach_header*, const char*>>& images,
                                  int64_t cacheSlide, uint64_t cacheUnslidAddr,
                                  const DyldSharedCache* dyldCache,
                                  const char* const neverStubEliminateSymbols[]);

    const DyldSharedCache::CreateOptions&       _options;
    const dyld3::closure::FileSystem&           _fileSystem;
    Region                                      _readExecuteRegion;
    Region                                      _readOnlyRegion;
    UnmappedRegion                              _localSymbolsRegion;
    vm_address_t                                _fullAllocatedBuffer;
    uint64_t                                    _nonLinkEditReadOnlySize;
    Diagnostics                                 _diagnostics;
    TimeRecorder                                _timeRecorder;
    uint64_t                                    _allocatedBufferSize;
    CacheCoalescedText                          _coalescedText;
    bool                                        _is64                       = false;
    // Note this is mutable as the only parallel writes to it are done atomically to the bitmap
    mutable ASLR_Tracker                        _aslrTracker;
    mutable LOH_Tracker                         _lohTracker;
};

inline uint64_t align(uint64_t addr, uint8_t p2)
{
    uint64_t mask = (1 << p2);
    return (addr + mask - 1) & (-mask);
}

inline uint8_t* align_buffer(uint8_t* addr, uint8_t p2)
{
    return (uint8_t *)align((uintptr_t)addr, p2);
}


#endif /* CacheBuilder_h */
