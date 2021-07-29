/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <mach-o/dyld_priv.h>

#include <vector>
#include <tuple>
#include <set>
#include <unordered_set>
#include <string>

#include "Array.h"
#include "MachOFile.h"
#include "MachOLoaded.h"
#include "MachOAnalyzer.h"
#include "MachOAnalyzerSet.h"
#include "ClosureFileSystemPhysical.h"
#include "DyldSharedCache.h"

typedef  dyld3::MachOLoaded::ChainedFixupPointerOnDisk   ChainedFixupPointerOnDisk;

static void versionToString(uint32_t value, char buffer[32])
{
    if ( value == 0 )
        strcpy(buffer, "n/a");
    else if ( value & 0xFF )
        sprintf(buffer, "%d.%d.%d", value >> 16, (value >> 8) & 0xFF, value & 0xFF);
    else
        sprintf(buffer, "%d.%d", value >> 16, (value >> 8) & 0xFF);
}

static void printPlatforms(const dyld3::MachOAnalyzer* ma)
{
    printf("    -platform:\n");
    printf("        platform     minOS      sdk\n");
    ma->forEachSupportedPlatform(^(dyld3::Platform platform, uint32_t minOS, uint32_t sdk) {
        char osVers[32];
        char sdkVers[32];
        versionToString(minOS, osVers);
        versionToString(sdk,   sdkVers);
       printf(" %15s     %-7s   %-7s\n", dyld3::MachOFile::platformName(platform), osVers, sdkVers);
    });
}

static void permString(uint32_t permFlags, char str[4])
{
    str[0] = (permFlags & VM_PROT_READ)    ? 'r' : '.';
    str[1] = (permFlags & VM_PROT_WRITE)   ? 'w' : '.';
    str[2] = (permFlags & VM_PROT_EXECUTE) ? 'x' : '.';
    str[3] = '\0';
}

static void printSegments(const dyld3::MachOAnalyzer* ma)
{
    if ( ma->inDyldCache() ) {
        printf("    -segments:\n");
        printf("       load-address    segment section        sect-size  seg-size perm\n");
        __block const char* lastSegName = "";
        ma->forEachSection(^(const dyld3::MachOFile::SectionInfo& sectInfo, bool malformedSectionRange, bool& stop) {
            if ( strcmp(lastSegName, sectInfo.segInfo.segName) != 0 ) {
                char permChars[8];
                permString(sectInfo.segInfo.protections, permChars);
                printf("        0x%08llX    %-16s            %16lluKB %s\n", sectInfo.segInfo.vmAddr, sectInfo.segInfo.segName, sectInfo.segInfo.vmSize/1024, permChars);
                lastSegName = sectInfo.segInfo.segName;
            }
                printf("        0x%08llX             %-16s %6llu\n", sectInfo.sectAddr, sectInfo.sectName, sectInfo.sectSize);
        });
    }
    else {
        printf("    -segments:\n");
        printf("        load-offset   segment section        sect-size  seg-size perm\n");
        __block const char* lastSegName = "";
        __block uint64_t    firstSegVmAddr = 0;
        ma->forEachSection(^(const dyld3::MachOFile::SectionInfo& sectInfo, bool malformedSectionRange, bool& stop) {
            if ( lastSegName[0] == '\0' )
                firstSegVmAddr = sectInfo.segInfo.vmAddr;
            if ( strcmp(lastSegName, sectInfo.segInfo.segName) != 0 ) {
                char permChars[8];
                permString(sectInfo.segInfo.protections, permChars);
                printf("        0x%08llX    %-16s                      %6lluKB %s\n", sectInfo.segInfo.vmAddr - firstSegVmAddr, sectInfo.segInfo.segName, sectInfo.segInfo.vmSize/1024, permChars);
                lastSegName = sectInfo.segInfo.segName;
            }
                printf("        0x%08llX             %-16s %6llu\n", sectInfo.sectAddr-firstSegVmAddr, sectInfo.sectName, sectInfo.sectSize);
        });
    }
 }


static void printDependents(const dyld3::MachOAnalyzer* ma)
{
    printf("    -dependents:\n");
    printf("        attributes     load path\n");
    ma->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
        const char* attribute = "";
        if ( isWeak )
            attribute = "weak_import";
        else if ( isReExport )
            attribute = "re-export";
        else if ( isUpward )
            attribute = "upward";
        printf("        %-12s   %s\n", attribute, loadPath);
    });
}

static bool liveMachO(const dyld3::MachOAnalyzer* ma, const DyldSharedCache* dyldCache, size_t cacheLen)
{
    if ( dyldCache == nullptr )
        return false;
    const uint8_t* cacheStart = (uint8_t*)dyldCache;
    const uint8_t* cacheEnd   = &cacheStart[cacheLen];
    if ( (uint8_t*)ma < cacheStart)
        return false;
    if ( (uint8_t*)ma > cacheEnd)
        return false;

    // only return true for live images
    return ( dyld_image_header_containing_address(ma) != nullptr );
}

static void printInitializers(const dyld3::MachOAnalyzer* ma, const DyldSharedCache* dyldCache, size_t cacheLen)
{
    printf("    -inits:\n");
    Diagnostics diag;
    const dyld3::MachOAnalyzer::VMAddrConverter vmAddrConverter = (ma->inDyldCache() ? dyldCache->makeVMAddrConverter(true) : ma->makeVMAddrConverter(false));
    ma->forEachInitializer(diag, vmAddrConverter, ^(uint32_t offset) {
        uint64_t    targetLoadAddr = (uint64_t)ma+offset;
        const char* symbolName;
        uint64_t    symbolLoadAddr;
        if ( ma->findClosestSymbol(targetLoadAddr, &symbolName, &symbolLoadAddr) ) {
            uint64_t delta = targetLoadAddr - symbolLoadAddr;
            if ( delta == 0 )
                printf("        0x%08X  %s\n", offset, symbolName);
            else
                printf("        0x%08X  %s + 0x%llX\n", offset, symbolName, delta);
        }
        else
            printf("        0x%08X\n", offset);
    });
    if ( ma->hasPlusLoadMethod(diag) ) {
        // can't inspect ObjC of a live dylib
        if ( liveMachO(ma, dyldCache, cacheLen) ) {
            printf("         <<<cannot print objc data on live dylib>>>\n");
            return;
        }
        const uint32_t pointerSize = ma->pointerSize();
        uint64_t prefLoadAddress = ma->preferredLoadAddress();
        // print all +load methods on classes in this image
        ma->forEachObjCClass(diag, vmAddrConverter, ^(Diagnostics& diag, uint64_t classVMAddr,
                             uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                             const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
            if (!isMetaClass)
                return;
            dyld3::MachOAnalyzer::PrintableStringResult classNameResult;
            const char* className = ma->getPrintableString(objcClass.nameVMAddr(pointerSize), classNameResult);
            if ( classNameResult == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint ) {
                ma->forEachObjCMethod(objcClass.baseMethodsVMAddr(pointerSize), vmAddrConverter, ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    dyld3::MachOAnalyzer::PrintableStringResult methodNameResult;
                    const char* methodName = ma->getPrintableString(method.nameVMAddr, methodNameResult);
                    if ( methodNameResult == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint ) {
                        if ( strcmp(methodName, "load") == 0 )
                            printf("        0x%08llX  +[%s %s]\n", methodVMAddr-prefLoadAddress, className, methodName);
                    }
                });
            }
        });
        // print all +load methods on categories in this image
        ma->forEachObjCCategory(diag, vmAddrConverter, ^(Diagnostics& diag, uint64_t categoryVMAddr,
                                                         const dyld3::MachOAnalyzer::ObjCCategory& objcCategory) {
            dyld3::MachOAnalyzer::PrintableStringResult categoryNameResult;
            const char* categoryName = ma->getPrintableString(objcCategory.nameVMAddr, categoryNameResult);
            if ( categoryNameResult == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint ) {
                ma->forEachObjCMethod(objcCategory.classMethodsVMAddr, vmAddrConverter, ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
                    dyld3::MachOAnalyzer::PrintableStringResult methodNameResult;
                    const char* methodName = ma->getPrintableString(method.nameVMAddr, methodNameResult);
                    if ( methodNameResult == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint ) {
                        if ( strcmp(methodName, "load") == 0 ) {
                            // FIXME: if category is on class in another image, forEachObjCCategory returns null for objcCategory.clsVMAddr, need way to get name
                            __block const char* catOnClassName = "";
                            ma->forEachObjCClass(diag, vmAddrConverter, ^(Diagnostics& diag, uint64_t classVMAddr,
                                                uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                                const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                                if ( objcCategory.clsVMAddr == classVMAddr ) {
                                    dyld3::MachOAnalyzer::PrintableStringResult classNameResult;
                                    const char* className = ma->getPrintableString(objcClass.nameVMAddr(pointerSize), classNameResult);
                                    if ( classNameResult == dyld3::MachOAnalyzer::PrintableStringResult::CanPrint ) {
                                        catOnClassName = className;
                                    }
                                }
                            });
                            printf("        0x%08llX  +[%s(%s) %s]\n", methodVMAddr-prefLoadAddress, catOnClassName, categoryName, methodName);
                        }
                    }
                });
            }
        });
    }
}


static const char* pointerFormat(uint16_t format)
{
    switch (format) {
        case DYLD_CHAINED_PTR_ARM64E:
            return "authenticated arm64e, 8-byte stride, target vmadddr";
        case DYLD_CHAINED_PTR_ARM64E_USERLAND:
            return "authenticated arm64e, 8-byte stride, target vmoffset";
        case DYLD_CHAINED_PTR_ARM64E_FIRMWARE:
            return "authenticated arm64e, 4-byte stride, target vmadddr";
        case DYLD_CHAINED_PTR_ARM64E_KERNEL:
            return "authenticated arm64e, 4-byte stride, target vmoffset";
        case DYLD_CHAINED_PTR_64:
            return "generic 64-bit, 4-byte stride, target vmadddr";
        case DYLD_CHAINED_PTR_64_OFFSET:
            return "generic 64-bit, 4-byte stride, target vmoffset ";
        case DYLD_CHAINED_PTR_32:
            return "generic 32-bit";
        case DYLD_CHAINED_PTR_32_CACHE:
            return "32-bit for dyld cache";
        case DYLD_CHAINED_PTR_64_KERNEL_CACHE:
            return "64-bit for kernel cache";
        case DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE:
            return "64-bit for x86_64 kernel cache";
        case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
            return "authenticated arm64e, 8-byte stride, target vmoffset, 24-bit bind ordinals";
    }
    return "unknown";
}

static void printChains(const dyld3::MachOAnalyzer* ma)
{
    Diagnostics diag;
    ma->withChainStarts(diag, 0, ^(const dyld_chained_starts_in_image* starts) {
        for (int i=0; i < starts->seg_count; ++i) {
            if ( starts->seg_info_offset[i] == 0 )
                continue;
            const dyld_chained_starts_in_segment* seg = (dyld_chained_starts_in_segment*)((uint8_t*)starts + starts->seg_info_offset[i]);
            if ( seg->page_count == 0 )
                continue;
            printf("seg[%d]:\n", i);
            printf("  page_size:       0x%04X\n",     seg->page_size);
            printf("  pointer_format:  %d (%s)\n",    seg->pointer_format, pointerFormat(seg->pointer_format));
            printf("  segment_offset:  0x%08llX\n",   seg->segment_offset);
            printf("  max_pointer:     0x%08X\n",     seg->max_valid_pointer);
            printf("  pages:         %d\n",           seg->page_count);
            for (int pageIndex=0; pageIndex < seg->page_count; ++pageIndex) {
                uint16_t offsetInPage = seg->page_start[pageIndex];
                if ( offsetInPage == DYLD_CHAINED_PTR_START_NONE )
                    continue;
                if ( offsetInPage & DYLD_CHAINED_PTR_START_MULTI ) {
                    // 32-bit chains which may need multiple starts per page
                    uint32_t overflowIndex = offsetInPage & ~DYLD_CHAINED_PTR_START_MULTI;
                    bool chainEnd = false;
                    while (!chainEnd) {
                        chainEnd = (seg->page_start[overflowIndex] & DYLD_CHAINED_PTR_START_LAST);
                        offsetInPage = (seg->page_start[overflowIndex] & ~DYLD_CHAINED_PTR_START_LAST);
                        printf("    start[% 2d]:  0x%04X\n",   pageIndex, offsetInPage);
                        ++overflowIndex;
                    }
                }
                else {
                    // one chain per page
                    printf("    start[% 2d]:  0x%04X\n",   pageIndex, offsetInPage);
                }
            }
        }
   });
}


static void printChainDetails(const dyld3::MachOAnalyzer* ma)
{
    __block Diagnostics diag;
    ma->withChainStarts(diag, 0, ^(const dyld_chained_starts_in_image* starts) {
        ma->forEachFixupInAllChains(diag, starts, true, ^(ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
            uint64_t vmOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
            switch (segInfo->pointer_format) {
                case DYLD_CHAINED_PTR_ARM64E:
                case DYLD_CHAINED_PTR_ARM64E_KERNEL:
                case DYLD_CHAINED_PTR_ARM64E_USERLAND:
                case DYLD_CHAINED_PTR_ARM64E_FIRMWARE:
                case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
                   if ( fixupLoc->arm64e.authRebase.auth ) {
                        uint32_t bindOrdinal = (segInfo->pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND24) ? fixupLoc->arm64e.authBind24.ordinal : fixupLoc->arm64e.authBind.ordinal;
                        if ( fixupLoc->arm64e.authBind.bind ) {
                             printf("  0x%08llX:  raw: 0x%016llX    auth-bind: (next: %03d, key: %s, addrDiv: %d, diversity: 0x%04X, ordinal: %04X)\n", vmOffset, fixupLoc->raw64,
                                   fixupLoc->arm64e.authBind.next, fixupLoc->arm64e.keyName(),
                                   fixupLoc->arm64e.authBind.addrDiv, fixupLoc->arm64e.authBind.diversity, bindOrdinal);
                        }
                        else {
                            printf("  0x%08llX:  raw: 0x%016llX  auth-rebase: (next: %03d, key: %s, addrDiv: %d, diversity: 0x%04X, target: 0x%08X)\n", vmOffset, fixupLoc->raw64,
                                  fixupLoc->arm64e.authRebase.next,  fixupLoc->arm64e.keyName(),
                                  fixupLoc->arm64e.authBind.addrDiv, fixupLoc->arm64e.authBind.diversity, fixupLoc->arm64e.authRebase.target);
                        }
                    }
                    else {
                        uint32_t bindOrdinal = (segInfo->pointer_format == DYLD_CHAINED_PTR_ARM64E_USERLAND24) ? fixupLoc->arm64e.bind24.ordinal : fixupLoc->arm64e.bind.ordinal;
                        if ( fixupLoc->arm64e.rebase.bind ) {
                            printf("  0x%08llX:  raw: 0x%016llX         bind: (next: %03d, ordinal: %04X, addend: %d)\n", vmOffset, fixupLoc->raw64,
                                  fixupLoc->arm64e.bind.next, bindOrdinal, fixupLoc->arm64e.bind.addend);
                        }
                        else {
                            printf("  0x%08llX:  raw: 0x%016llX       rebase: (next: %03d, target: 0x%011llX, high8: 0x%02X)\n", vmOffset, fixupLoc->raw64,
                                  fixupLoc->arm64e.rebase.next, fixupLoc->arm64e.rebase.target, fixupLoc->arm64e.rebase.high8);
                        }
                    }
                    break;
                case DYLD_CHAINED_PTR_64:
                case DYLD_CHAINED_PTR_64_OFFSET:
                    if ( fixupLoc->generic64.rebase.bind ) {
                        printf("  0x%08llX:  raw: 0x%016llX         bind: (next: %03d, ordinal: %06X, addend: %d)\n", vmOffset, fixupLoc->raw64,
                              fixupLoc->generic64.bind.next, fixupLoc->generic64.bind.ordinal, fixupLoc->generic64.bind.addend);
                    }
                    else {
                         printf("  0x%08llX:  raw: 0x%016llX       rebase: (next: %03d, target: 0x%011llX, high8: 0x%02X)\n", vmOffset, fixupLoc->raw64,
                               fixupLoc->generic64.rebase.next, fixupLoc->generic64.rebase.target, fixupLoc->generic64.rebase.high8);
                    }
                    break;
                case DYLD_CHAINED_PTR_32:
                    if ( fixupLoc->generic32.bind.bind ) {
                        printf("  0x%08llX:  raw: 0x%08X    bind: (next:%02d ordinal:%05X addend:%d)\n", vmOffset, fixupLoc->raw32,
                              fixupLoc->generic32.bind.next, fixupLoc->generic32.bind.ordinal, fixupLoc->generic32.bind.addend);
                    }
                    else if ( fixupLoc->generic32.rebase.target > segInfo->max_valid_pointer ) {
                        uint32_t bias  = (0x04000000 + segInfo->max_valid_pointer)/2;
                        uint32_t value = fixupLoc->generic32.rebase.target - bias;
                        printf("  0x%08llX:  raw: 0x%08X  nonptr: (next:%02d value: 0x%08X)\n", vmOffset, fixupLoc->raw32,
                              fixupLoc->generic32.rebase.next, value);
                    }
                    else {
                        printf("  0x%08llX:  raw: 0x%08X  rebase: (next:%02d target: 0x%07X)\n", vmOffset, fixupLoc->raw32,
                              fixupLoc->generic32.rebase.next, fixupLoc->generic32.rebase.target);
                    }
                    break;
                default:
                    fprintf(stderr, "unknown pointer type %d\n", segInfo->pointer_format);
                    break;
            }
         });
    });
    if ( diag.hasError() )
        fprintf(stderr, "dyldinfo: %s\n", diag.errorMessage());
}


struct FixupInfo
{
    std::string segName;
    std::string sectName;
    uint64_t    address;
    dyld3::MachOAnalyzerSet::PointerMetaData pmd;
    const char* type;
    uint64_t    targetValue;
    const char* targetDylib;
    const char* targetSymbolName;
    uint64_t    targetAddend;
    bool        targetWeakImport;
};


struct SymbolicFixupInfo
{
    uint64_t    address;
    const char* kind;
    std::string target;
};



static const char* ordinalName(const dyld3::MachOAnalyzer* ma, int libraryOrdinal)
{
    static int          sLastOrdinal = -100;
    static const char*  sLastString = nullptr;
    if ( libraryOrdinal > 0 ) {
        if ( libraryOrdinal != sLastOrdinal ) {
            const char* path = ma->dependentDylibLoadPath(libraryOrdinal-1);
            if ( path == nullptr )
                return "ordinal-too-large";
            const char* leafName = path;
            if ( const char* lastSlash = strrchr(path, '/') )
                leafName = lastSlash+1;
            sLastOrdinal = libraryOrdinal;
            sLastString = leafName;
        }
        return sLastString;
    }
    else {
        switch ( libraryOrdinal) {
            case BIND_SPECIAL_DYLIB_SELF:
                return "this-image";
            case BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE:
                return "main-executable";
            case BIND_SPECIAL_DYLIB_FLAT_LOOKUP:
                return "flat-namespace";
            case BIND_SPECIAL_DYLIB_WEAK_LOOKUP:
                return "weak-coalesce";
        }
    }
    return "unknown-ordinal";
}


class SectionFinder
{
public:
                     SectionFinder(const dyld3::MachOAnalyzer* ma);
    const char*      segmentName(uint64_t vmOffset) const;
    const char*      sectionName(uint64_t vmOffset) const;
    uint64_t         baseAddress() const { return _baseAddress; }
    uint64_t         currentSectionAddress() const { return _lastSection.sectAddr; }
    bool             isNewSection(uint64_t vmOffset) const;

private:
    void             updateLastSection(uint64_t vmOffset) const;

    const dyld3::MachOAnalyzer*           _ma;
    uint64_t                              _baseAddress;
    mutable dyld3::MachOFile::SectionInfo _lastSection;
    mutable char                          _lastSegName[20];
    mutable char                          _lastSectName[20];
};

SectionFinder::SectionFinder(const dyld3::MachOAnalyzer* ma)
    : _ma(ma)
{
    _baseAddress = ma->preferredLoadAddress();
    _lastSection.sectAddr = 0;
    _lastSection.sectSize = 0;
}

bool SectionFinder::isNewSection(uint64_t vmOffset) const
{
    uint64_t vmAddr = _baseAddress + vmOffset;
    return ( (vmAddr < _lastSection.sectAddr) || (vmAddr >= _lastSection.sectAddr+_lastSection.sectSize) );
}

void SectionFinder::updateLastSection(uint64_t vmOffset) const
{
    if ( isNewSection(vmOffset) ) {
        _lastSegName[0] = '\0';
        _lastSectName[0] = '\0';
        uint64_t vmAddr = _baseAddress + vmOffset;
        _ma->forEachSection(^(const dyld3::MachOFile::SectionInfo& sectInfo, bool malformedSectionRange, bool& sectStop) {
            if ( (sectInfo.sectAddr <= vmAddr) && (vmAddr < sectInfo.sectAddr+sectInfo.sectSize) ) {
                _lastSection = sectInfo;
                strcpy(_lastSegName, _lastSection.segInfo.segName);
                strcpy(_lastSectName, _lastSection.sectName);
                sectStop = true;
            }
        });
    }
}

const char* SectionFinder::segmentName(uint64_t vmOffset) const
{
    updateLastSection(vmOffset);
    return _lastSegName;
}

const char* SectionFinder::sectionName(uint64_t vmOffset) const
{
    updateLastSection(vmOffset);
    return _lastSectName;
}


static inline std::string decimal(int64_t value) {
    char buff[64];
    sprintf(buff, "%lld", value);
    return buff;
}

static inline std::string hex(int64_t value) {
    char buff[64];
    sprintf(buff, "0x%llX", value);
    return buff;
}

static std::string rebaseTargetString(const dyld3::MachOAnalyzer* ma, uint64_t vmAddr)
{
    uint64_t    targetLoadAddr = (uint64_t)ma+vmAddr;
    const char* targetSymbolName;
    uint64_t    targetSymbolLoadAddr;
    if ( ma->findClosestSymbol(targetLoadAddr, &targetSymbolName, &targetSymbolLoadAddr) ) {
        uint64_t delta = targetLoadAddr - targetSymbolLoadAddr;
        if ( delta == 0 ) {
            return targetSymbolName;
        }
        else {
            if ( (delta == 1) && (ma->cputype == CPU_TYPE_ARM) )
                return std::string(targetSymbolName) + std::string(" [thumb]");
            else
                return std::string(targetSymbolName) + std::string("+") + decimal(delta);
        }
    }
    else {
        __block std::string result;
        ma->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo& sectInfo, bool malformedSectionRange, bool& stop) {
            if ( (sectInfo.sectAddr <= vmAddr) && (vmAddr < sectInfo.sectAddr+sectInfo.sectSize) ) {
                if ( (sectInfo.sectFlags & SECTION_TYPE) == S_CSTRING_LITERALS ) {
                    const char* cstring = (char*)ma + (vmAddr-ma->preferredLoadAddress());
                    result = std::string("\"") + cstring + std::string("\"");
                }
                else {
                    result = std::string(sectInfo.segInfo.segName) + "/" + sectInfo.sectName + "+" + decimal(vmAddr - sectInfo.sectAddr);
                }
            }
        });
       return result;
    }
}



typedef dyld3::MachOAnalyzerSet                 MachOAnalyzerSet;
typedef dyld3::MachOAnalyzerSet::WrappedMachO   WrappedMachO;

struct InfoAnalyzerSet : public MachOAnalyzerSet
{
    void            mas_forEachImage(void (^handler)(const WrappedMachO& anImage, bool hidden, bool& stop)) const override;
    void            mas_mainExecutable(WrappedMachO& anImage) const override;
    void*           mas_dyldCache() const override;
    bool            wmo_dependent(const WrappedMachO* image, uint32_t depIndex, WrappedMachO& childObj, bool& missingWeakDylib) const override;
    const char*     wmo_path(const WrappedMachO* image) const override;
    ExportsTrie     wmo_getExportsTrie(const WrappedMachO* image) const override;
    bool            wmo_findSymbolFrom(const WrappedMachO* fromWmo, Diagnostics& diag, int libOrdinal, const char* symbolName, bool weakImport,
                                       bool lazyBind, uint64_t addend, CachePatchHandler ph, FixupTarget& target) const override;
    bool            wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const override;
};

bool InfoAnalyzerSet::wmo_dependent(const WrappedMachO* image, uint32_t depIndex, WrappedMachO& childObj, bool& missingWeakDylib) const
{
    const char* depPath = image->_mh->dependentDylibLoadPath(depIndex);
    childObj = WrappedMachO(nullptr, image->_set, (void*)depPath);
    return true;
}

const char* InfoAnalyzerSet::wmo_path(const WrappedMachO* image) const
{
    return (const char*)image->_other;
}

MachOAnalyzerSet::ExportsTrie InfoAnalyzerSet::wmo_getExportsTrie(const WrappedMachO* obj) const
{
    return { nullptr, nullptr };
}

bool InfoAnalyzerSet::wmo_findSymbolFrom(const WrappedMachO* obj, Diagnostics& diag, int libOrdinal, const char* symbolName, bool weakImport,
                                         bool lazyBind, uint64_t addend, CachePatchHandler ph, MachOAnalyzerSet::FixupTarget& target) const
{
    target.requestedSymbolName      = symbolName;
    target.foundSymbolName          = symbolName;
    target.addend                   = addend;
    target.kind                     = weakImport ? MachOAnalyzerSet::FixupTarget::Kind::bindMissingSymbol : MachOAnalyzerSet::FixupTarget::Kind::bindToImage;
    if ( libOrdinal == BIND_SPECIAL_DYLIB_SELF ) {
        target.foundInImage = WrappedMachO(nullptr, obj->_set, (void*)"self");
    }
    else if ( libOrdinal == BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE ) {
        target.foundInImage = WrappedMachO(nullptr, obj->_set, (void*)"main-executable");
    }
    else if ( libOrdinal == BIND_SPECIAL_DYLIB_FLAT_LOOKUP ) {
        target.foundInImage = WrappedMachO(nullptr, obj->_set, (void*)"flat-namespace");
    }
    else if ( libOrdinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP ) {
        target.foundInImage = WrappedMachO(nullptr, obj->_set, (void*)"weak-coalesce");
    }
    else {
        int depIndex = libOrdinal - 1;
        const char* depPath = obj->_mh->dependentDylibLoadPath(depIndex);
        const char* lastSlash = strrchr(depPath, '/');
        if (lastSlash)
            ++lastSlash;
        else
            lastSlash = depPath;
        target.foundInImage = WrappedMachO(nullptr, obj->_set, (void*)lastSlash);
    }
    return true;
}

void InfoAnalyzerSet::mas_forEachImage(void (^handler)(const WrappedMachO& anImage, bool hidden, bool& stop)) const
{
    WrappedMachO anImage(nullptr, this, (void*)"flat-namespace");
    bool stop = false;
    handler(anImage, false, stop);
}


bool InfoAnalyzerSet::wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const
{
    return false;
}

void InfoAnalyzerSet::mas_mainExecutable(WrappedMachO& anImage) const
{
    anImage = WrappedMachO(nullptr, this, (void*)"main-executable");
}

void*  InfoAnalyzerSet::mas_dyldCache() const
{
    return nullptr;
}

static void printFixups(const dyld3::MachOAnalyzer* ma, const char* path)
{
    printf("    -fixups:\n");
    Diagnostics diag;
    __block std::vector<FixupInfo> fixups;
    uint64_t prefLoadAddr = ma->preferredLoadAddress();
    SectionFinder namer(ma);
    InfoAnalyzerSet ias;
    WrappedMachO wm(ma, &ias, (void*)path);
    wm.forEachFixup(diag, ^(uint64_t fixupLocRuntimeOffset, MachOAnalyzerSet::PointerMetaData pmd, const MachOAnalyzerSet::FixupTarget& target, bool& stop) {
        FixupInfo fixup;
        fixup.segName           = namer.segmentName(fixupLocRuntimeOffset);
        fixup.sectName          = namer.sectionName(fixupLocRuntimeOffset);
        fixup.address           = prefLoadAddr + fixupLocRuntimeOffset;
        fixup.pmd               = pmd;
        fixup.targetWeakImport  = false;
        switch ( target.kind ) {
            case MachOAnalyzerSet::FixupTarget::Kind::bindAbsolute:
                fixup.type      = "absolute";
                break;
            case MachOAnalyzerSet::FixupTarget::Kind::rebase:
                fixup.type      = "rebase";
                fixup.targetSymbolName  = nullptr;
                fixup.targetDylib       = nullptr;
                break;
            case MachOAnalyzerSet::FixupTarget::Kind::bindToImage:
                fixup.type      = "bind";
                fixup.targetSymbolName  = target.requestedSymbolName;
                fixup.targetDylib       = target.foundInImage.path();
                break;
            case MachOAnalyzerSet::FixupTarget::Kind::bindMissingSymbol:
                fixup.type      = "bind";
                fixup.targetSymbolName  = target.requestedSymbolName;
                fixup.targetDylib       = target.foundInImage.path();
                fixup.targetWeakImport  = true;
                break;
        }
        fixup.targetValue       = target.offsetInImage;
        fixup.targetAddend      = target.addend;
        if ( pmd.high8 )
            fixup.targetAddend += ((uint64_t)pmd.high8 << 56);
        fixups.push_back(fixup);
    },
    ^(uint32_t, uint32_t, const MachOAnalyzerSet::FixupTarget&) {
    });

    std::sort(fixups.begin(), fixups.end(), [](const FixupInfo& l, const FixupInfo& r) {
        if ( &l == &r )
            return false;
        if ( l.address == r.address )
            return (l.targetSymbolName == nullptr);
        return ( l.address < r.address );
    });

    printf("        segment      section          address         type          target\n");
    for (const FixupInfo& fixup : fixups) {
        char authInfo[128];
        authInfo[0] = '\0';
        if ( fixup.pmd.authenticated ) {
            sprintf(authInfo, " (div=0x%04X ad=%d key=%s)", fixup.pmd.diversity, fixup.pmd.usesAddrDiversity, ChainedFixupPointerOnDisk::Arm64e::keyName(fixup.pmd.key));
        }
        if ( fixup.targetSymbolName == nullptr )
            printf("        %-12s %-16s 0x%08llX  %16s  0x%08llX%s\n", fixup.segName.c_str(), fixup.sectName.c_str(), fixup.address, fixup.type, fixup.targetValue, authInfo);
        else if ( fixup.targetAddend != 0 )
            printf("        %-12s %-16s 0x%08llX  %16s  %s/%s + 0x%llX%s\n", fixup.segName.c_str(), fixup.sectName.c_str(), fixup.address, fixup.type, fixup.targetDylib, fixup.targetSymbolName, fixup.targetAddend, authInfo);
        else if ( fixup.targetWeakImport )
            printf("        %-12s %-16s 0x%08llX  %16s  %s/%s [weak-import]%s\n", fixup.segName.c_str(), fixup.sectName.c_str(), fixup.address, fixup.type, fixup.targetDylib, fixup.targetSymbolName, authInfo);
        else
            printf("        %-12s %-16s 0x%08llX  %16s  %s/%s%s\n", fixup.segName.c_str(), fixup.sectName.c_str(), fixup.address, fixup.type, fixup.targetDylib, fixup.targetSymbolName, authInfo);
   }
}

static void printSymbolicFixups(const dyld3::MachOAnalyzer* ma, const char* path)
{
    printf("    -symbolic_fixups:\n");
    Diagnostics diag;
    __block std::vector<SymbolicFixupInfo> fixups;
    uint64_t prefLoadAddr = ma->preferredLoadAddress();
    InfoAnalyzerSet ias;
    WrappedMachO wm(ma, &ias, (void*)path);
    wm.forEachFixup(diag, ^(uint64_t fixupLocRuntimeOffset, MachOAnalyzerSet::PointerMetaData pmd, const MachOAnalyzerSet::FixupTarget& target, bool& stop) {
        SymbolicFixupInfo fixup;
        fixup.address = prefLoadAddr + fixupLocRuntimeOffset;
        switch ( target.kind ) {
            case MachOAnalyzerSet::FixupTarget::Kind::bindAbsolute:
                fixup.kind         = "absolute";
                fixup.target       = "";
                break;
            case MachOAnalyzerSet::FixupTarget::Kind::rebase:
                fixup.kind         = "rebase pointer";
                fixup.target       = rebaseTargetString(ma, target.offsetInImage);
                break;
            case MachOAnalyzerSet::FixupTarget::Kind::bindToImage:
            case MachOAnalyzerSet::FixupTarget::Kind::bindMissingSymbol:
                fixup.kind         = "bind pointer";
                fixup.target       = std::string(target.foundInImage.path()) + "/" + target.requestedSymbolName;
                if ( target.addend != 0 )
                    fixup.target += std::string("+") + decimal(target.addend);
                if ( pmd.high8 )
                    fixup.target += std::string("+") + hex(((uint64_t)pmd.high8 << 56));
                //if ( target.weakImport )
                //    fixup.target += " [weak-import]";
                break;
        }
        if ( pmd.authenticated ) {
            char authInfo[256];
            sprintf(authInfo, " (div=0x%04X ad=%d key=%s)", pmd.diversity, pmd.usesAddrDiversity, ChainedFixupPointerOnDisk::Arm64e::keyName(pmd.key));
            fixup.target += authInfo;
        }
        fixups.push_back(fixup);
    },
    ^(uint32_t, uint32_t, const MachOAnalyzerSet::FixupTarget&) {
    });


    std::sort(fixups.begin(), fixups.end(), [](const SymbolicFixupInfo& l, const SymbolicFixupInfo& r) {
        if ( &l == &r )
          return false;
        return ( l.address < r.address );
    });

    SectionFinder sectionTracker(ma);
    uint64_t lastSymbolVmOffset = 0;
    for (const SymbolicFixupInfo& fixup : fixups) {
        uint64_t vmAddr   = fixup.address;
        uint64_t vmOffset = vmAddr - prefLoadAddr;
        if ( sectionTracker.isNewSection(vmOffset) ) {
            printf("        0x%08llX %-12s %-16s \n", vmAddr, sectionTracker.segmentName(vmOffset), sectionTracker.sectionName(vmOffset));
        }
        const char* symbolName;
        uint64_t    symbolLoadAddr = 0;
        if ( ma->findClosestSymbol((uint64_t)ma+vmOffset, &symbolName, &symbolLoadAddr) ) {
            uint64_t symbolVmOffset = symbolLoadAddr - (uint64_t)ma;
            if ( symbolVmOffset != lastSymbolVmOffset ) {
                printf("        %s:\n", symbolName);
                lastSymbolVmOffset = symbolVmOffset;
            }
        }
        printf("           +0x%04llX  %16s   %s\n", vmOffset - lastSymbolVmOffset, fixup.kind, fixup.target.c_str());
    }
}


static void printExports(const dyld3::MachOAnalyzer* ma)
{
    printf("    -exports:\n");
    printf("        offset      symbol\n");
    Diagnostics diag;
    ma->forEachExportedSymbol(diag, ^(const char* symbolName, uint64_t imageOffset, uint64_t flags, uint64_t other, const char* importName, bool& stop) {
        //printf("0x%08llX %s\n", imageOffset, symbolName);
        const bool reExport     = (flags & EXPORT_SYMBOL_FLAGS_REEXPORT);
        const bool weakDef      = (flags & EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION);
        const bool resolver     = (flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER);
        const bool threadLocal  = ((flags & EXPORT_SYMBOL_FLAGS_KIND_MASK) == EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL);
        const bool abs          = ((flags & EXPORT_SYMBOL_FLAGS_KIND_MASK) == EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE);
        if ( reExport )
            printf("        [re-export] ");
        else
            printf("        0x%08llX  ", imageOffset);
        printf("%s", symbolName);
        if ( weakDef || threadLocal || resolver || abs ) {
            bool needComma = false;
            printf(" [");
            if ( weakDef ) {
                printf("weak_def");
                needComma = true;
            }
            if ( threadLocal ) {
                if ( needComma )
                    printf(", ");
                printf("per-thread");
                needComma = true;
            }
            if ( abs ) {
                if ( needComma )
                    printf(", ");
                printf("absolute");
                needComma = true;
            }
            if ( resolver ) {
                if ( needComma )
                    printf(", ");
                printf("resolver=0x%08llX", other);
                needComma = true;
            }
            printf("]");
        }
        if ( reExport ) {
            if ( importName[0] == '\0' )
                printf(" (from %s)", ordinalName(ma, (int)other));
            else
                printf(" (%s from %s)", importName, ordinalName(ma, (int)other));
        }
        printf("\n");
    });

}

static void printObjC(const dyld3::MachOAnalyzer* ma, const DyldSharedCache* dyldCache, size_t cacheLen)
{
    Diagnostics diag;
    const uint32_t pointerSize = ma->pointerSize();
    const dyld3::MachOAnalyzer::VMAddrConverter vmAddrConverter = (ma->inDyldCache() ? dyldCache->makeVMAddrConverter(true) : ma->makeVMAddrConverter(false));

    auto printMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) {
        const char* type = "method";
        dyld3::MachOAnalyzer::PrintableStringResult methodNameResult;
        const char* methodName = ma->getPrintableString(method.nameVMAddr, methodNameResult);
        switch (methodNameResult) {
            case dyld3::MachOAnalyzer::PrintableStringResult::CanPrint:
                // The string is already valid
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::FairPlayEncrypted:
                methodName = "### fairplay encrypted";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::ProtectedSection:
                methodName = "### protected section";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::UnknownSection:
                methodName = "### unknown section";
                break;
        }
        printf("        %10s   0x%08llX                 %s\n",
               type, methodVMAddr, methodName);
    };

    printf("    -objc:\n");
    // can't inspect ObjC of a live dylib
    if ( liveMachO(ma, dyldCache, cacheLen) ) {
        printf("         <<<cannot print objc data on live dylib>>>\n");
        return;
    }
    printf("              type       vmaddr   data-vmaddr   name\n");
    auto printClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                        uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                        const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
        const char* type = "class";
        if (isMetaClass)
            type = "meta-class";
        dyld3::MachOAnalyzer::PrintableStringResult classNameResult;
        const char* className = ma->getPrintableString(objcClass.nameVMAddr(pointerSize), classNameResult);
        switch (classNameResult) {
            case dyld3::MachOAnalyzer::PrintableStringResult::CanPrint:
                // The string is already valid
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::FairPlayEncrypted:
                className = "### fairplay encrypted";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::ProtectedSection:
                className = "### protected section";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::UnknownSection:
                className = "### unknown section";
                break;
        }
        printf("        %10s   0x%08llX    0x%08llX   %s\n",
               type, classVMAddr, objcClass.dataVMAddr, className);

        // Now print the methods on this class
        ma->forEachObjCMethod(objcClass.baseMethodsVMAddr(pointerSize), vmAddrConverter, printMethod);
    };
    auto printCategory = ^(Diagnostics& diag, uint64_t categoryVMAddr,
                           const dyld3::MachOAnalyzer::ObjCCategory& objcCategory) {
        const char* type = "category";
        dyld3::MachOAnalyzer::PrintableStringResult categoryNameResult;
        const char* categoryName = ma->getPrintableString(objcCategory.nameVMAddr, categoryNameResult);
        switch (categoryNameResult) {
            case dyld3::MachOAnalyzer::PrintableStringResult::CanPrint:
                // The string is already valid
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::FairPlayEncrypted:
                categoryName = "### fairplay encrypted";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::ProtectedSection:
                categoryName = "### protected section";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::UnknownSection:
                categoryName = "### unknown section";
                break;
        }
        printf("        %10s   0x%08llX                 %s\n",
               type, categoryVMAddr, categoryName);

        // Now print the methods on this category
        ma->forEachObjCMethod(objcCategory.instanceMethodsVMAddr, vmAddrConverter,
                              printMethod);
        ma->forEachObjCMethod(objcCategory.classMethodsVMAddr, vmAddrConverter,
                              printMethod);
    };
    auto printProtocol = ^(Diagnostics& diag, uint64_t protocolVMAddr,
                           const dyld3::MachOAnalyzer::ObjCProtocol& objCProtocol) {
        const char* type = "protocol";
        dyld3::MachOAnalyzer::PrintableStringResult protocolNameResult;
        const char* protocolName = ma->getPrintableString(objCProtocol.nameVMAddr, protocolNameResult);
        switch (protocolNameResult) {
            case dyld3::MachOAnalyzer::PrintableStringResult::CanPrint:
                // The string is already valid
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::FairPlayEncrypted:
                protocolName = "### fairplay encrypted";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::ProtectedSection:
                protocolName = "### protected section";
                break;
            case dyld3::MachOAnalyzer::PrintableStringResult::UnknownSection:
                protocolName = "### unknown section";
                break;
        }
        printf("        %10s   0x%08llX                 %s\n",
               type, protocolVMAddr, protocolName);

        // Now print the methods on this protocol
        ma->forEachObjCMethod(objCProtocol.instanceMethodsVMAddr, vmAddrConverter,
                              printMethod);
        ma->forEachObjCMethod(objCProtocol.classMethodsVMAddr, vmAddrConverter,
                              printMethod);
        ma->forEachObjCMethod(objCProtocol.optionalInstanceMethodsVMAddr, vmAddrConverter,
                              printMethod);
        ma->forEachObjCMethod(objCProtocol.optionalClassMethodsVMAddr, vmAddrConverter,
                              printMethod);
    };
    ma->forEachObjCClass(diag, vmAddrConverter, printClass);
    ma->forEachObjCCategory(diag, vmAddrConverter, printCategory);
    ma->forEachObjCProtocol(diag, vmAddrConverter, printProtocol);
}

static void usage()
{
	fprintf(stderr, "Usage: dyldinfo [-arch <arch>]* <options>* <mach-o file>+\n"
            "\t-segments         print segments\n"
			"\t-dependents       print dependent dylibs\n"
            "\t-fixups           print locations dyld will rebase/bind\n"
            "\t-exports          print addresses of all symbols this file exports\n"
            "\t-objc             print objc classes, categories, etc\n"
		);
}

static bool inStringVector(const std::vector<const char*>& vect, const char* target)
{
    for (const char* str : vect) {
        if ( strcmp(str, target) == 0 )
            return true;
    }
    return false;
}


int main(int argc, const char* argv[])
{
	if ( argc == 1 ) {
		usage();
		return 0;
	}

    std::vector<const char*> files;
    std::vector<const char*> cmdLineArchs;
    for (int i=1; i < argc; ++i) {
        const char* arg = argv[i];
        if ( arg[0] == '-' ) {
            if ( strcmp(arg, "-arch") == 0 ) {
                if ( ++i < argc ) {
                    cmdLineArchs.push_back(argv[i]);
                }
                else {
                    fprintf(stderr, "-arch missing architecture name");
                    return 1;
                }
            }
        }
        else {
            files.push_back(arg);
        }
    }
    if ( files.size() == 0 ) {
        usage();
        return 0;
    }

    size_t cacheLen;
    const DyldSharedCache* cache = (DyldSharedCache*)_dyld_get_shared_cache_range(&cacheLen);
    for (const char* path : files) {
        Diagnostics diag;
        bool fromSharedCache = false;
        dyld3::closure::FileSystemPhysical fileSystem;
        dyld3::closure::LoadedFileInfo info;
        __block std::vector<const char*> archesForFile;
        char realerPath[MAXPATHLEN];
        __block bool printedError = false;
        if (!fileSystem.loadFile(path, info, realerPath, ^(const char* format, ...) {
            fprintf(stderr, "dyldinfo: ");
            va_list list;
            va_start(list, format);
            vfprintf(stderr, format, list);
            va_end(list);
            printedError = true;
        })) {
            if ( printedError )
                return 1;
            // see if path is in current dyld shared cache
            info.fileContent = nullptr;
            if ( cache != nullptr ) {
                uint32_t imageIndex;
                if ( cache->hasImagePath(path, imageIndex) ) {
                    uint64_t mTime;
                    uint64_t inode;
                    const mach_header* mh = cache->getIndexedImageEntry(imageIndex, mTime, inode);
                    info.fileContent = mh;
                    info.path = path;
                    fromSharedCache = true;
                    archesForFile.push_back(cache->archName());
                }
            }
            if ( !fromSharedCache ) {
                fprintf(stderr, "dyldinfo: %s: file not found\n", path);
                return 1;
            }
        }
        __block dyld3::Platform platform = dyld3::Platform::unknown;
        if ( dyld3::FatFile::isFatFile(info.fileContent) ) {
            const dyld3::FatFile* ff = (dyld3::FatFile*)info.fileContent;
            ff->forEachSlice(diag, info.fileContentLen, ^(uint32_t sliceCpuType, uint32_t sliceCpuSubType, const void* sliceStart, uint64_t sliceSize, bool& stop) {
                const char* sliceArchName = dyld3::MachOFile::archName(sliceCpuType, sliceCpuSubType);
                if ( cmdLineArchs.empty() || inStringVector(cmdLineArchs, sliceArchName) ) {
                    archesForFile.push_back(sliceArchName);
                    const dyld3::MachOFile* mf = (dyld3::MachOFile*)sliceStart;
                    mf->forEachSupportedPlatform(^(dyld3::Platform plat, uint32_t minOS, uint32_t sdk) {
                        if ( platform == dyld3::Platform::unknown)
                            platform = plat;
                    });
                }
             });
        }
        else if ( !fromSharedCache ) {
            const dyld3::MachOFile* mo = (dyld3::MachOFile*)info.fileContent;
            if ( mo->isMachO(diag, info.sliceLen) ) {
                archesForFile.push_back(mo->archName());
                mo->forEachSupportedPlatform(^(dyld3::Platform plat, uint32_t minOS, uint32_t sdk) {
                    if ( platform == dyld3::Platform::unknown)
                        platform = plat;
                });
            }
            else {
                fprintf(stderr, "dyldinfo: %s: %s\n", path, diag.errorMessage());
                return 1;
            }
        }
        if ( archesForFile.empty() ) {
            fprintf(stderr, "dyldinfo: %s: does not contain specified arch(s)\n", path);
            return 1;
        }
        char loadedPath[MAXPATHLEN];
        for (const char* sliceArch : archesForFile) {
            if ( !fromSharedCache )
                info = dyld3::MachOAnalyzer::load(diag, fileSystem, path, dyld3::GradedArchs::forName(sliceArch), platform, loadedPath);
            if ( diag.hasError() ) {
                fprintf(stderr, "dyldinfo: %s\n", diag.errorMessage());
                return 1;
            }
            const dyld3::MachOAnalyzer* ma = (dyld3::MachOAnalyzer*)info.fileContent;
            printf("%s [%s]:\n", path, sliceArch);

            bool somethingPrinted = false;
            for (int i=1; i < argc; ++i) {
                const char* arg = argv[i];
                if ( arg[0] != '-' )
                    continue;
                if ( strcmp(arg, "-arch") == 0 ) {
                    // handled previously
                    ++i;
                }
                else if ( strcmp(arg, "-platform") == 0 ) {
                    printPlatforms(ma);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-segments") == 0 ) {
                    printSegments(ma);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-dependents") == 0 ) {
                    printDependents(ma);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-inits") == 0 ) {
                    printInitializers(ma, cache, cacheLen);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-fixups") == 0 ) {
                    printFixups(ma, path);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-exports") == 0 ) {
                    printExports(ma);
                     somethingPrinted = true;
                }
                else if ( strcmp(arg, "-fixup_chains") == 0 ) {
                    printChains(ma);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-fixup_chain_details") == 0 ) {
                    printChainDetails(ma);
                    somethingPrinted = true;
                }
                else if ( strcmp(arg, "-symbolic_fixups") == 0 ) {
                     printSymbolicFixups(ma, path);
                     somethingPrinted = true;
                }
                else if ( strcmp(arg, "-opcodes") == 0 ) {
                }
                else if ( strcmp(arg, "-shared_region") == 0 ) {
                }
                else if ( strcmp(arg, "-function_starts") == 0 ) {
                }
                else if ( strcmp(arg, "-data_in_code") == 0 ) {
                }
                else if ( strcmp(arg, "-objc") == 0 ) {
                    printObjC(ma, cache, cacheLen);
                    somethingPrinted = true;
                }
                else {
                    fprintf(stderr, "unknown option: %s\n", arg);
                    return 1;
                }
            }
            if ( !somethingPrinted ) {
                printPlatforms(ma);
                printSegments(ma);
                printDependents(ma);
                printInitializers(ma, cache, cacheLen);
                printFixups(ma, path);
                printExports(ma);
                printObjC(ma, cache, cacheLen);
            }

        }
    }

	return 0;
}



