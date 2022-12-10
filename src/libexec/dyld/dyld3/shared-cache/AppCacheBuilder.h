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

#ifndef AppCacheBuilder_h
#define AppCacheBuilder_h

#include "CacheBuilder.h"
#include "MachOFileAbstraction.hpp"
#include "MachOAppCache.h"

#include <list>

#include <CoreFoundation/CFDictionary.h>

class AppCacheBuilder final : public CacheBuilder {
public:
    struct Options {
        enum class AppCacheKind {
            none,
            kernel,                 // The base first party kernel collection with xnu and boot time kexts
            pageableKC,              // Other first party kexts which are usually only for some HW, eg, different GPUs
            kernelCollectionLevel2, // Placeholder until we find a use for this
            auxKC,                  // Third party kexts, or Apple kexts updated out of band with the OS, if any
        };

        enum class StripMode {
            none,           // Don't strip anything
            all,            // Strip everything
            allExceptKernel // Strip everything other than the static kernel
        };

        AppCacheKind    cacheKind = AppCacheKind::none;
        StripMode       stripMode = StripMode::none;
    };
    AppCacheBuilder(const DyldSharedCache::CreateOptions& dyldCacheOptions, const Options& appCacheOptions,
                    const dyld3::closure::FileSystem& fileSystem);
    ~AppCacheBuilder() override final;

    // Wraps up a loaded macho with the other information required by kext's, eg, the list of dependencies
    struct InputDylib {
        LoadedMachO                     dylib;
        std::string                     dylibID;
        std::vector<std::string>        dylibDeps;
        CFDictionaryRef                 infoPlist   = nullptr;
        std::string                     bundlePath;
        Diagnostics*                    errors          = nullptr;
        CacheBuilder::DylibStripMode    stripMode = CacheBuilder::DylibStripMode::stripNone;
    };

    // The payload for -sectcreate
    struct CustomSegment {
        struct CustomSection {
            std::string                     sectionName;
            std::vector<uint8_t>            data;
            uint64_t                        offsetInRegion = 0;
        };

        std::string                 segmentName;
        std::vector<CustomSection>  sections;
        Region*                     parentRegion = nullptr;
    };

    bool                    addCustomSection(const std::string& segmentName,
                                             CustomSegment::CustomSection section);
    void                    setExistingKernelCollection(const dyld3::MachOAppCache* appCacheMA);
    void                    setExistingPageableKernelCollection(const dyld3::MachOAppCache* appCacheMA);
    void                    setExtraPrelinkInfo(CFDictionaryRef dictionary);
    void                    buildAppCache(const std::vector<InputDylib>& dylibs);
    void                    writeFile(const std::string& path);
    void                    writeBuffer(uint8_t*& buffer, uint64_t& bufferSize) const;

private:

    enum {
        // The fixup format can't support more than 3 levels right now
        numFixupLevels = 4
    };

    struct AppCacheDylibInfo : CacheBuilder::DylibInfo
    {
        // From CacheBuilder::DylibInfo
        // const LoadedMachO*              input;
        // std::string                     dylibID;
        // std::vector<SegmentMappingInfo> cacheLocation;

        DylibStripMode                  stripMode       = DylibStripMode::stripNone;
        std::vector<std::string>        dependencies;
        CFDictionaryRef                 infoPlist       = nullptr;
        Diagnostics*                    errors          = nullptr;
        std::string                     bundlePath;
    };

    static_assert(std::is_move_constructible<AppCacheDylibInfo>::value);

    void                    forEachDylibInfo(void (^callback)(const DylibInfo& dylib, Diagnostics& dylibDiag)) override final;

    void                    forEachCacheDylib(void (^callback)(const dyld3::MachOAnalyzer* ma,
                                                               const std::string& dylibID,
                                                               DylibStripMode stripMode,
                                                               const std::vector<std::string>& dependencies,
                                                               Diagnostics& dylibDiag,
                                                               bool& stop)) const;

    const DylibInfo*                    getKernelStaticExecutableInputFile() const;
    const dyld3::MachOAnalyzer*         getKernelStaticExecutableFromCache() const;

    void                                makeSortedDylibs(const std::vector<InputDylib>& dylibs);
    void                                allocateBuffer();
    void                                assignSegmentRegionsAndOffsets();
    void                                copyRawSegments();
    void                                assignSegmentAddresses();
    void                                generateCacheHeader();
    void                                generatePrelinkInfo();
    uint32_t                            getCurrentFixupLevel() const;
    void                                processFixups();
    void                                writeFixups();
    void                                fipsSign();
    void                                generateUUID();
    uint64_t                            numRegions() const;
    uint64_t                            numBranchRelocationTargets();
    uint64_t                            fixupsPageSize() const;
    uint64_t                            numWritablePagesToFixup(uint64_t numBytesToFixup) const;
    bool                                fixupsArePerKext() const;
    void                                forEachRegion(void (^callback)(const Region& region)) const;

    Options                             appCacheOptions;
    const dyld3::MachOAppCache*         existingKernelCollection = nullptr;
    const dyld3::MachOAppCache*         pageableKernelCollection = nullptr;
    CFDictionaryRef                     extraPrelinkInfo         = nullptr;

    std::vector<AppCacheDylibInfo>      sortedDylibs;
    std::vector<InputDylib>             codelessKexts;
    std::vector<CustomSegment>          customSegments;
    Region                              cacheHeaderRegion;
    Region                              readExecuteRegion;
    Region                              branchStubsRegion;
    Region                              dataConstRegion;
    Region                              branchGOTsRegion;
    Region                              readWriteRegion;
    Region                              hibernateRegion;
    Region                              readOnlyTextRegion;
    std::list<Region>                   customDataRegions;       // -sectcreate
    Region                              prelinkInfoRegion;
    std::list<Region>                   nonSplitSegRegions;
    // Region                           _readOnlyRegion;        // This comes from the base class
    Region                              fixupsSubRegion;        // This will be in the __LINKEDIT when we write the file

    // This is the base address from the statically linked kernel, or 0 for other caches
    uint64_t                            cacheBaseAddress = 0;
    // For x86_64 only, we want to keep the address of the hibernate segment from xnu
    // We'll ensure this is mapped lower than the cache base address
    uint64_t                            hibernateAddress = 0;

    uint16_t                            chainedPointerFormat = 0;

    // The dictionary we ultimately store in the __PRELINK_INFO region
    CFMutableDictionaryRef              prelinkInfoDict = nullptr;

    // Cache header fields
    // FIXME: 32-bit

    struct CacheHeader64 {
        typedef std::pair<segment_command_64*, Region*> SegmentCommandAndRegion;
        typedef std::pair<fileset_entry_command*, const DylibInfo*> DylibCommandAndInfo;

        mach_header_64*                         header              = nullptr;

        uint64_t                                numLoadCommands     = 0;
        uint64_t                                loadCommandsSize    = 0;

        uuid_command*                           uuid                = nullptr;
        build_version_command*                  buildVersion        = nullptr;
        thread_command*                         unixThread          = nullptr;
        symtab_command*                         symbolTable         = nullptr;
        dysymtab_command*                       dynSymbolTable      = nullptr;
        linkedit_data_command*                  chainedFixups       = nullptr;
        std::vector<SegmentCommandAndRegion>    segments;
        std::vector<DylibCommandAndInfo>        dylibs;
    };

    CacheHeader64 cacheHeader;
};

#endif /* AppCacheBuilder_h */
