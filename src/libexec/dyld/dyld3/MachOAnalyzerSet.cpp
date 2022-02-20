/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
#include <mach/mach.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach-o/reloc.h>
#include <mach-o/nlist.h>
#include <TargetConditionals.h>

#include "MachOAnalyzerSet.h"
#include "DyldSharedCache.h"

#if BUILDING_DYLD
  namespace dyld { void log(const char*, ...); }
#endif

namespace dyld3 {

static bool hasHigh8(uint64_t addend)
{
    // distinguish negative addend from TBI
    if ( (addend >> 56) == 0 )
        return false;
    return ( (addend >> 48) != 0xFFFF );
}

void MachOAnalyzerSet::WrappedMachO::forEachBind(Diagnostics& diag, FixUpHandler fixUpHandler, CachePatchHandler patchHandler) const
{
    const bool              is64            = _mh->is64();
    __block int             lastLibOrdinal  = 256;
    __block const char*     lastSymbolName  = nullptr;
    __block uint64_t        lastAddend      = 0;
    __block FixupTarget     target;
    __block PointerMetaData pmd;
    _mh->forEachBind(diag, ^(uint64_t runtimeOffset, int libOrdinal, const char* symbolName, bool weakImport, bool lazyBind, uint64_t addend, bool& stop) {
        if ( (symbolName == lastSymbolName) && (libOrdinal == lastLibOrdinal) && (addend == lastAddend) ) {
            // same symbol lookup as last location
            fixUpHandler(runtimeOffset, pmd, target, stop);
        }
        else if ( this->findSymbolFrom(diag, libOrdinal, symbolName, weakImport, lazyBind, addend, patchHandler, target) ) {
            pmd.high8 = 0;
            if ( is64 && (target.addend != 0) ) {
                if ( hasHigh8(target.addend) ) {
                    pmd.high8  = (target.addend >> 56);
                    target.offsetInImage &= 0x00FFFFFFFFFFFFFFULL;
                    target.addend        &= 0x00FFFFFFFFFFFFFFULL;
                }
            }
            if ( !target.skippableWeakDef ) {
                fixUpHandler(runtimeOffset, pmd, target, stop);
                lastSymbolName = symbolName;
                lastLibOrdinal = libOrdinal;
                lastAddend     = addend;
            }
        }
        else {
            // call handler with missing symbol before stopping
            if ( target.kind == FixupTarget::Kind::bindMissingSymbol )
                fixUpHandler(runtimeOffset, pmd, target, stop);
            stop = true;
        }
    }, ^(const char* symbolName) {
    });
}

MachOAnalyzerSet::PointerMetaData::PointerMetaData()
{
    this->diversity           = 0;
    this->high8               = 0;
    this->authenticated       = 0;
    this->key                 = 0;
    this->usesAddrDiversity   = 0;
}

MachOAnalyzerSet::PointerMetaData::PointerMetaData(const MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, uint16_t pointer_format)
{
    this->diversity           = 0;
    this->high8               = 0;
    this->authenticated       = 0;
    this->key                 = 0;
    this->usesAddrDiversity   = 0;
    switch ( pointer_format ) {
        case DYLD_CHAINED_PTR_ARM64E:
        case DYLD_CHAINED_PTR_ARM64E_KERNEL:
        case DYLD_CHAINED_PTR_ARM64E_USERLAND:
        case DYLD_CHAINED_PTR_ARM64E_FIRMWARE:
        case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
            this->authenticated = fixupLoc->arm64e.authRebase.auth;
            if ( this->authenticated ) {
                this->key               = fixupLoc->arm64e.authRebase.key;
                this->usesAddrDiversity = fixupLoc->arm64e.authRebase.addrDiv;
                this->diversity         = fixupLoc->arm64e.authRebase.diversity;
            }
            else if ( fixupLoc->arm64e.bind.bind == 0 ) {
                this->high8             = fixupLoc->arm64e.rebase.high8;
            }
            break;
        case DYLD_CHAINED_PTR_64:
        case DYLD_CHAINED_PTR_64_OFFSET:
            if ( fixupLoc->generic64.bind.bind == 0 )
                this->high8             = fixupLoc->generic64.rebase.high8;
            break;
    }
}


void MachOAnalyzerSet::WrappedMachO::forEachFixup(Diagnostics& diag, FixUpHandler fixup, CachePatchHandler patcher) const
{
    uint16_t        fmPointerFormat;
    uint32_t        fmStartsCount;
    const uint32_t* fmStarts;
    const MachOAnalyzer* ma = _mh;
    const uint64_t prefLoadAddr = ma->preferredLoadAddress();
    if ( ma->hasChainedFixups() ) {
        // build targets table
        STACK_ALLOC_OVERFLOW_SAFE_ARRAY(FixupTarget, targets, 512);
        ma->forEachChainedFixupTarget(diag, ^(int libOrdinal, const char* symbolName, uint64_t addend, bool weakImport, bool& stop) {
            targets.default_constuct_back();
            FixupTarget& foundTarget = targets.back();
            if ( !this->findSymbolFrom(diag, libOrdinal, symbolName, weakImport, false, addend, patcher, foundTarget) ) {
                // call handler with missing symbol before stopping
                if ( foundTarget.kind == FixupTarget::Kind::bindMissingSymbol )
                    fixup(0, PointerMetaData(), foundTarget, stop);
                stop = true;
            }
        });
        if ( diag.hasError() )
            return;

        // walk all chains
        ma->withChainStarts(diag, ma->chainStartsOffset(), ^(const dyld_chained_starts_in_image* startsInfo) {
            ma->forEachFixupInAllChains(diag, startsInfo, false, ^(MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc,
                                                                    const dyld_chained_starts_in_segment* segInfo, bool& fixupsStop) {
                uint64_t fixupOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
                uint64_t targetOffset;
                uint32_t bindOrdinal;
                int64_t  embeddedAddend;
                PointerMetaData pmd(fixupLoc, segInfo->pointer_format);
                if ( fixupLoc->isBind(segInfo->pointer_format, bindOrdinal, embeddedAddend) ) {
                    if ( bindOrdinal < targets.count() ) {
                        if ( embeddedAddend == 0 ) {
                            if ( hasHigh8(targets[bindOrdinal].addend) ) {
                                FixupTarget targetWithoutHigh8 = targets[bindOrdinal];
                                pmd.high8 = (targetWithoutHigh8.addend >> 56);
                                targetWithoutHigh8.offsetInImage &= 0x00FFFFFFFFFFFFFFULL;
                                targetWithoutHigh8.addend        &= 0x00FFFFFFFFFFFFFFULL;
                                fixup(fixupOffset, pmd, targetWithoutHigh8, fixupsStop);
                            }
                            else {
                                fixup(fixupOffset, pmd, targets[bindOrdinal], fixupsStop);
                            }
                        }
                        else {
                            // pointer on disk encodes extra addend, make pseudo target for that
                            FixupTarget targetWithAddend = targets[bindOrdinal];
                            targetWithAddend.addend        += embeddedAddend;
                            targetWithAddend.offsetInImage += embeddedAddend;
                            fixup(fixupOffset, pmd, targetWithAddend, fixupsStop);
                        }
                    }
                    else {
                        diag.error("out of range bind ordinal %d (max %lu)", bindOrdinal, targets.count());
                        fixupsStop = true;
                    }
                }
                else if ( fixupLoc->isRebase(segInfo->pointer_format, prefLoadAddr, targetOffset) ) {
                    FixupTarget rebaseTarget;
                    rebaseTarget.kind               = FixupTarget::Kind::rebase;
                    rebaseTarget.foundInImage       = *this;
                    rebaseTarget.offsetInImage      = targetOffset & 0x00FFFFFFFFFFFFFFULL;
                    rebaseTarget.isLazyBindRebase   = false; // FIXME
                    fixup(fixupOffset, pmd, rebaseTarget, fixupsStop);
                }
            });
        });
    }
    else if ( ma->hasFirmwareChainStarts(&fmPointerFormat, &fmStartsCount, &fmStarts) ) {
        // This is firmware which only has rebases, the chain starts info is in a section (not LINKEDIT)
        ma->forEachFixupInAllChains(diag, fmPointerFormat, fmStartsCount, fmStarts, ^(MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, bool& stop) {
            uint64_t fixupOffset = (uint8_t*)fixupLoc - (uint8_t*)ma;
            PointerMetaData pmd(fixupLoc, fmPointerFormat);
            uint64_t targetOffset;
            fixupLoc->isRebase(fmPointerFormat, prefLoadAddr, targetOffset);
            FixupTarget rebaseTarget;
            rebaseTarget.kind               = FixupTarget::Kind::rebase;
            rebaseTarget.foundInImage       = *this;
            rebaseTarget.offsetInImage      = targetOffset & 0x00FFFFFFFFFFFFFFULL;
            rebaseTarget.isLazyBindRebase   = false;
            fixup(fixupOffset, pmd, rebaseTarget, stop);
        });
    }
    else {
        // process all rebase opcodes
        const bool is64 = ma->is64();
        ma->forEachRebase(diag, ^(uint64_t runtimeOffset, bool isLazyPointerRebase, bool& stop) {
            uint64_t* loc = (uint64_t*)((uint8_t*)ma + runtimeOffset);
            uint64_t locValue = is64 ? *loc : *((uint32_t*)loc);
            FixupTarget rebaseTarget;
            PointerMetaData pmd;
            if ( is64 )
                pmd.high8  = (locValue >> 56);
            rebaseTarget.kind               = FixupTarget::Kind::rebase;
            rebaseTarget.foundInImage       = *this;
            rebaseTarget.offsetInImage      = (locValue & 0x00FFFFFFFFFFFFFFULL) - prefLoadAddr;
            rebaseTarget.isLazyBindRebase   = isLazyPointerRebase;
            fixup(runtimeOffset, pmd, rebaseTarget, stop);
        });
        if ( diag.hasError() )
            return;

        // process all bind opcodes
        this->forEachBind(diag, fixup, patcher);
    }
    if ( diag.hasError() )
        return;

    // main executable may define operator new/delete symbols that overrides weak-defs but have no fixups
    if ( ma->isMainExecutable() && ma->hasWeakDefs() ) {
        _set->wmo_findExtraSymbolFrom(this,  patcher);
    }
}


bool MachOAnalyzerSet::wmo_findSymbolFrom(const WrappedMachO* fromWmo, Diagnostics& diag, int libOrdinal, const char* symbolName, bool weakImport,
                                           bool lazyBind, uint64_t addend, CachePatchHandler patcher, FixupTarget& target) const
{
    target.libOrdinal = libOrdinal;
    if ( libOrdinal == BIND_SPECIAL_DYLIB_FLAT_LOOKUP ) {
        __block bool found = false;
        this->mas_forEachImage(^(const WrappedMachO& anImage, bool hidden, bool& stop) {
            // when an image is hidden (RTLD_LOCAL) it can still look up symbols in itself
            if ( hidden && (fromWmo->_mh != anImage._mh) )
                return;
            if ( anImage.findSymbolIn(diag, symbolName, addend, target) ) {
                stop = true;
                found = true;
            }
        });
        if ( found )
            return true;
        // see if missing symbol resolver can find something
        if ( fromWmo->missingSymbolResolver(weakImport, lazyBind, symbolName, "flat namespace", fromWmo->path(), target) )
            return true;
        // fill out target info about missing symbol
        target.kind                 = FixupTarget::Kind::bindMissingSymbol;
        target.requestedSymbolName  = symbolName;
        target.foundSymbolName      = nullptr;
        target.foundInImage         = WrappedMachO();   // no image it should be in

        diag.error("symbol '%s' not found, expected in flat namespace by '%s'", symbolName, fromWmo->path());
        return false;
    }
    else if ( libOrdinal == BIND_SPECIAL_DYLIB_WEAK_LOOKUP ) {
        if ( this->mas_fromImageWeakDefLookup(*fromWmo, symbolName, addend, patcher, target) ) {
            target.weakCoalesced = true;
            return true;
        }

        if ( !fromWmo->_mh->hasChainedFixups() ) {
            // support old binaries where symbols have been stripped and have weak_bind to itself
            target.skippableWeakDef = true;
            return true;
        }
        // see if missing symbol resolver can find something
        if ( fromWmo->missingSymbolResolver(weakImport, lazyBind, symbolName, "flat namespace", fromWmo->path(), target) )
            return true;
        // fill out target info about missing symbol
        target.kind                 = FixupTarget::Kind::bindMissingSymbol;
        target.requestedSymbolName  = symbolName;
        target.foundSymbolName      = nullptr;
        target.foundInImage         = WrappedMachO();   // no image it should be in

        diag.error("symbol '%s' not found, expected to be weak-def coalesced in '%s'", symbolName, fromWmo->path());
        return false;
    }
    else {
        int depIndex = libOrdinal - 1;
        bool missingWeakDylib = false;
        WrappedMachO depHelper;
        const WrappedMachO* targetImage = nullptr;
        if ( libOrdinal == BIND_SPECIAL_DYLIB_SELF ) {
            targetImage = fromWmo;
        }
        else if ( libOrdinal == BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE ) {
            this->mas_mainExecutable(depHelper);
            targetImage = &depHelper;
        }
        else if ( fromWmo->dependent(depIndex, depHelper, missingWeakDylib) ) {
            targetImage = &depHelper;
        }
        else {
            diag.error("unknown library ordinal %d in %s", libOrdinal, fromWmo->path());
            return false;
        }
        // use two-level namespace target image
        if ( !missingWeakDylib && targetImage->findSymbolIn(diag, symbolName, addend, target) )
            return true;

        // see if missing symbol resolver can find something
        const char* expectedInPath = missingWeakDylib ? "missing dylib" : targetImage->path();
        if ( fromWmo->missingSymbolResolver(weakImport, lazyBind, symbolName, expectedInPath, fromWmo->path(), target) )
            return true;

        // fill out target info about missing symbol
        target.kind                 = FixupTarget::Kind::bindMissingSymbol;
        target.requestedSymbolName  = symbolName;
        target.foundSymbolName      = nullptr;
        target.foundInImage         = *targetImage;    // no image it is expected to be in

        // symbol not found and not weak or lazy so error out
        diag.error("symbol '%s' not found, expected in '%s', needed by '%s'", symbolName, expectedInPath, fromWmo->path());
        return false;
    }
    return false;
}

// These are mangled symbols for all the variants of operator new and delete
// which a main executable can define (non-weak) and override the
// weak-def implementation in the OS.
static const char* const sTreatAsWeak[] = {
    "__Znwm", "__ZnwmRKSt9nothrow_t",
    "__Znam", "__ZnamRKSt9nothrow_t",
    "__ZdlPv", "__ZdlPvRKSt9nothrow_t", "__ZdlPvm",
    "__ZdaPv", "__ZdaPvRKSt9nothrow_t", "__ZdaPvm",
    "__ZnwmSt11align_val_t", "__ZnwmSt11align_val_tRKSt9nothrow_t",
    "__ZnamSt11align_val_t", "__ZnamSt11align_val_tRKSt9nothrow_t",
    "__ZdlPvSt11align_val_t", "__ZdlPvSt11align_val_tRKSt9nothrow_t", "__ZdlPvmSt11align_val_t",
    "__ZdaPvSt11align_val_t", "__ZdaPvSt11align_val_tRKSt9nothrow_t", "__ZdaPvmSt11align_val_t"
};

void MachOAnalyzerSet::wmo_findExtraSymbolFrom(const WrappedMachO* fromWmo, CachePatchHandler patcher) const
{
    for (const char* weakSymbolName : sTreatAsWeak) {
        Diagnostics exportDiag;
        FixupTarget dummyTarget;
        // pretend main executable does have a use of this operator new/delete and look up the impl
        // this has the side effect of adding a cache patch if there is an impl outside the cache
        wmo_findSymbolFrom(fromWmo, exportDiag, -3, weakSymbolName, true, false, 0, patcher, dummyTarget);
    }

}

bool MachOAnalyzerSet::WrappedMachO::findSymbolIn(Diagnostics& diag, const char* symbolName, uint64_t addend, FixupTarget& target) const
{
    const MachOAnalyzer* ma  = _mh;

    // if exports trie location not computed yet, do it now
    ExportsTrie exportsTrie = this->getExportsTrie();

    target.foundSymbolName = nullptr;
    if ( exportsTrie.start ) {
        if ( const uint8_t* node = this->_mh->trieWalk(diag, exportsTrie.start, exportsTrie.end, symbolName)) {
            const uint8_t* p = node;
            const uint64_t flags = this->_mh->read_uleb128(diag, p, exportsTrie.end);
            if ( flags & EXPORT_SYMBOL_FLAGS_REEXPORT ) {
                // re-export from another dylib, lookup there
                const uint64_t libOrdinal = ma->read_uleb128(diag, p, exportsTrie.end);
                const char* importedName = (char*)p;
                if ( importedName[0] == '\0' )
                    importedName = symbolName;
                const int depIndex = (int)(libOrdinal - 1);
                bool missingWeakDylib;
                WrappedMachO depHelper;
                if ( this->dependent(depIndex, depHelper, missingWeakDylib) && !missingWeakDylib ) {
                    if ( depHelper.findSymbolIn(diag, importedName, addend, target) ) {
                        target.requestedSymbolName = symbolName;
                        return true;
                    }
                }
                if ( !missingWeakDylib )
                    diag.error("re-export ordinal %lld out of range for %s", libOrdinal, symbolName);
                return false;
            }
            target.kind                 = FixupTarget::Kind::bindToImage;
            target.requestedSymbolName  = symbolName;
            target.foundSymbolName      = symbolName;
            target.foundInImage         = *this;
            target.isWeakDef            = false;
            target.addend               = addend;
            uint64_t trieValue = ma->read_uleb128(diag, p, exportsTrie.end);
            switch ( flags & EXPORT_SYMBOL_FLAGS_KIND_MASK ) {
                case EXPORT_SYMBOL_FLAGS_KIND_REGULAR:
                    target.offsetInImage = trieValue + addend;
                    if ( flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER ) {
                        // for now, just return address of resolver helper stub
                        // FIXME handle running resolver
                        (void)this->_mh->read_uleb128(diag, p, exportsTrie.end);
                    }
                    if ( flags & EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION )
                        target.isWeakDef = true;
                    break;
                case EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL:
                    // no type checking that client expected TLV yet
                    target.offsetInImage = trieValue;
                    break;
                case EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE:
                    target.kind          = FixupTarget::Kind::bindAbsolute;
                    target.offsetInImage = trieValue + addend;
                    break;
                default:
                    diag.error("unsupported exported symbol kind. flags=%llu at node offset=0x%0lX", flags, (long)(node-exportsTrie.start));
                    return false;
            }
            return true;
        }
    }
    else {
        ma->forEachGlobalSymbol(diag, ^(const char* n_name, uint64_t n_value, uint8_t n_type, uint8_t n_sect, uint16_t n_desc, bool& stop) {
            if ( strcmp(n_name, symbolName) == 0 ) {
                target.kind                 = FixupTarget::Kind::bindToImage;
                target.foundSymbolName      = symbolName;
                target.requestedSymbolName  = symbolName;
                target.foundInImage         = *this;
                target.offsetInImage        = n_value - ma->preferredLoadAddress() + addend;
                target.addend               = addend;
                stop = true;
            }
        });
        if ( target.foundSymbolName )
            return true;
    }

    // symbol not exported from this image
    // if this is a dylib and has re-exported dylibs, search those too
    if ( (ma->filetype == MH_DYLIB) && ((ma->flags & MH_NO_REEXPORTED_DYLIBS) == 0) ) {
        __block unsigned depIndex = 0;
        ma->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
            if ( isReExport ) {
                bool missingWeakDylib;
                WrappedMachO child;
                 if ( this->dependent(depIndex, child, missingWeakDylib) && !missingWeakDylib ) {
                    if ( child.findSymbolIn(diag, symbolName, addend, target) )
                        stop = true;
                }
            }
            ++depIndex;
        });
    }

    return (target.foundSymbolName != nullptr);
}


MachOAnalyzerSet::ExportsTrie MachOAnalyzerSet::wmo_getExportsTrie(const WrappedMachO* wmo) const
{
    const uint8_t* start = nullptr;
    const uint8_t* end   = nullptr;
    uint32_t runtimeOffset;
    uint32_t size;
    if ( wmo->_mh->hasExportTrie(runtimeOffset, size) ) {
        start = (uint8_t*)wmo->_mh + runtimeOffset;
        end   = start + size;
    }
    return { start, end };
}


// scan all weak-def images in load order
// return first non-weak defintion found
// otherwise first weak definition found
bool MachOAnalyzerSet::mas_fromImageWeakDefLookup(const WrappedMachO& fromWmo, const char* symbolName, uint64_t addend, CachePatchHandler patcher, FixupTarget& target) const
{
    // walk all images in load order, looking only at ones with weak-defs
    const DyldSharedCache* dyldCache = (DyldSharedCache*)mas_dyldCache();
    __block bool foundImpl = false;
    this->mas_forEachImage(^(const WrappedMachO& anImage, bool hidden, bool& stop) {
        if ( !anImage._mh->hasWeakDefs() )
            return;
        // when an image is hidden (RTLD_LOCAL) it can still look up symbols in itself
        if ( hidden && (fromWmo._mh != anImage._mh) )
            return;
        FixupTarget tempTarget;
        Diagnostics diag;
        if ( anImage.findSymbolIn(diag, symbolName, addend, tempTarget) ) {
            // ignore symbol re-exports, we will find the real definition later in forEachImage()
            if ( anImage._mh != tempTarget.foundInImage._mh )
                return;
            if ( foundImpl && anImage._mh->inDyldCache() && (anImage._mh != target.foundInImage._mh) ) {
                // we have already found the target, but now we see something in the dyld cache
                // that also implements this symbol, so we need to change all caches uses of that
                // to use the found one instead
                uint32_t cachedDylibIndex = 0;
                if ( dyldCache->findMachHeaderImageIndex(anImage._mh, cachedDylibIndex) ) {
                    uintptr_t exportCacheOffset = (uint8_t*)tempTarget.foundInImage._mh + tempTarget.offsetInImage - (uint8_t*)dyldCache;
                    patcher(cachedDylibIndex, (uint32_t)exportCacheOffset, target);
                }
            }
            if ( !foundImpl ) {
                // this is first found, so copy this to result
                target        = tempTarget;
                foundImpl     = true;
            }
            else if ( target.isWeakDef && !tempTarget.isWeakDef ) {
                // we found a non-weak impl later on, switch to it
                target        = tempTarget;
            }
        }
    });
    return foundImpl;
}


} // dyld3


