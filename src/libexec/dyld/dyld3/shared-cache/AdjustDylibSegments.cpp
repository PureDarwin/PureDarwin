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
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <assert.h>

#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "CacheBuilder.h"
#include "Diagnostics.h"
#include "DyldSharedCache.h"
#include "Trie.hpp"
#include "MachOFileAbstraction.hpp"
#include "MachOLoaded.h"
#include "MachOAnalyzer.h"
#include "mach-o/fixup-chains.h"


#ifndef EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE
    #define EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE 0x02
#endif

namespace {

template <typename P>
class Adjustor {
public:
                    Adjustor(uint64_t cacheBaseAddress, dyld3::MachOAnalyzer* mh, const char* dylibID,
                             const std::vector<CacheBuilder::SegmentMappingInfo>& mappingInfo, Diagnostics& diag);
    void            adjustImageForNewSegmentLocations(CacheBuilder::ASLR_Tracker& aslrTracker,
                                                      CacheBuilder::LOH_Tracker* lohTracker,
                                                      const CacheBuilder::CacheCoalescedText* coalescedText,
                                                      const CacheBuilder::DylibTextCoalescer& textCoalescer);
 
private:
    void            adjustReferencesUsingInfoV2(CacheBuilder::ASLR_Tracker& aslrTracker,
                                                CacheBuilder::LOH_Tracker* lohTracker,
                                                const CacheBuilder::CacheCoalescedText* coalescedText,
                                                const CacheBuilder::DylibTextCoalescer& textCoalescer);
    void            adjustReference(uint32_t kind, uint8_t* mappedAddr, uint64_t fromNewAddress, uint64_t toNewAddress, int64_t adjust, int64_t targetSlide,
                                    uint64_t imageStartAddress, uint64_t imageEndAddress, bool convertRebaseChains,
                                    CacheBuilder::ASLR_Tracker& aslrTracker, CacheBuilder::LOH_Tracker* lohTracker,
                                    uint32_t*& lastMappedAddr32, uint32_t& lastKind, uint64_t& lastToNewAddress);
    void            adjustDataPointers(CacheBuilder::ASLR_Tracker& aslrTracker);
    void            adjustRebaseChains(CacheBuilder::ASLR_Tracker& aslrTracker);
    void            slidePointer(int segIndex, uint64_t segOffset, uint8_t type, CacheBuilder::ASLR_Tracker& aslrTracker);
    void            adjustSymbolTable();
    void            adjustChainedFixups(const CacheBuilder::DylibTextCoalescer& textCoalescer);
    void            adjustExternalRelocations();
    void            adjustExportsTrie(std::vector<uint8_t>& newTrieBytes);
    void            rebuildLinkEdit();
    void            adjustCode();
    void            adjustInstruction(uint8_t kind, uint8_t* textLoc, uint64_t codeToDataDelta);
    void            rebuildLinkEditAndLoadCommands(const CacheBuilder::DylibTextCoalescer& textCoalescer);
    uint64_t        slideForOrigAddress(uint64_t addr);
    void            convertGeneric64RebaseToIntermediate(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* chainPtr, CacheBuilder::ASLR_Tracker& aslrTracker, uint64_t targetSlide);
    void            convertArm64eRebaseToIntermediate(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* chainPtr, CacheBuilder::ASLR_Tracker& aslrTracker,
                                                      uint64_t targetSlide, bool convertRebaseChains);


    typedef typename P::uint_t pint_t;
    typedef typename P::E E;

    uint64_t                                                _cacheBaseAddress   = 0;
    dyld3::MachOAnalyzer*                                   _mh;
    Diagnostics&                                            _diagnostics;
    const uint8_t*                                          _linkeditBias       = nullptr;
    unsigned                                                _linkeditSegIndex   = 0;
    bool                                                    _maskPointers       = false;
    bool                                                    _splitSegInfoV2     = false;
    const char*                                             _dylibID            = nullptr;
    symtab_command*                                         _symTabCmd          = nullptr;
    dysymtab_command*                                       _dynSymTabCmd       = nullptr;
    dyld_info_command*                                      _dyldInfo           = nullptr;
    linkedit_data_command*                                  _splitSegInfoCmd    = nullptr;
    linkedit_data_command*                                  _functionStartsCmd  = nullptr;
    linkedit_data_command*                                  _dataInCodeCmd      = nullptr;
    linkedit_data_command*                                  _exportTrieCmd      = nullptr;
    linkedit_data_command*                                  _chainedFixupsCmd   = nullptr;
    uint16_t                                                _chainedFixupsFormat = 0;
    std::vector<uint64_t>                                   _segOrigStartAddresses;
    std::vector<uint64_t>                                   _segSizes;
    std::vector<uint64_t>                                   _segSlides;
    std::vector<macho_segment_command<P>*>                  _segCmds;
    const std::vector<CacheBuilder::SegmentMappingInfo>&    _mappingInfo;
};

template <typename P>
Adjustor<P>::Adjustor(uint64_t cacheBaseAddress, dyld3::MachOAnalyzer* mh, const char* dylibID,
                      const std::vector<CacheBuilder::SegmentMappingInfo>& mappingInfo, Diagnostics& diag)
    : _cacheBaseAddress(cacheBaseAddress), _mh(mh), _diagnostics(diag), _dylibID(dylibID), _mappingInfo(mappingInfo)
{
    assert((_mh->magic == MH_MAGIC) || (_mh->magic == MH_MAGIC_64));

    __block unsigned segIndex = 0;
    mh->forEachLoadCommand(diag, ^(const load_command *cmd, bool &stop) {
        switch ( cmd->cmd ) {
            case LC_SYMTAB:
                _symTabCmd = (symtab_command*)cmd;
                break;
            case LC_DYSYMTAB:
                _dynSymTabCmd = (dysymtab_command*)cmd;
                break;
            case LC_DYLD_INFO:
            case LC_DYLD_INFO_ONLY:
                _dyldInfo = (dyld_info_command*)cmd;
                break;
            case LC_SEGMENT_SPLIT_INFO:
                _splitSegInfoCmd = (linkedit_data_command*)cmd;
                break;
            case LC_FUNCTION_STARTS:
                _functionStartsCmd = (linkedit_data_command*)cmd;
                break;
            case LC_DATA_IN_CODE:
                _dataInCodeCmd = (linkedit_data_command*)cmd;
                break;
            case LC_DYLD_CHAINED_FIXUPS:
                _chainedFixupsCmd = (linkedit_data_command*)cmd;
                _chainedFixupsFormat = dyld3::MachOAnalyzer::chainedPointerFormat((dyld_chained_fixups_header*)&_linkeditBias[_chainedFixupsCmd->dataoff]);
                break;
            case LC_DYLD_EXPORTS_TRIE:
                _exportTrieCmd = (linkedit_data_command*)cmd;
                break;
            case macho_segment_command<P>::CMD:
                macho_segment_command<P>* segCmd = (macho_segment_command<P>*)cmd;
                _segCmds.push_back(segCmd);
                _segOrigStartAddresses.push_back(segCmd->vmaddr());
                _segSizes.push_back(segCmd->vmsize());
                _segSlides.push_back(_mappingInfo[segIndex].dstCacheUnslidAddress - segCmd->vmaddr());
                if ( strcmp(segCmd->segname(), "__LINKEDIT") == 0 ) {
                    _linkeditBias = (uint8_t*)_mappingInfo[segIndex].dstSegment - segCmd->fileoff();
                    _linkeditSegIndex = segIndex;
                }
                ++segIndex;
                break;
        }
    });

    _maskPointers = (mh->cputype == CPU_TYPE_ARM64) || (mh->cputype == CPU_TYPE_ARM64_32);
    if ( _splitSegInfoCmd != NULL ) {
        const uint8_t* infoStart = &_linkeditBias[_splitSegInfoCmd->dataoff];
        _splitSegInfoV2 = (*infoStart == DYLD_CACHE_ADJ_V2_FORMAT);
    }
    else {
        bool canHaveMissingSplitSeg = false;
#if BUILDING_APP_CACHE_UTIL
        if ( mh->isKextBundle() ) {
            if ( mh->isArch("x86_64") || mh->isArch("x86_64h") )
                canHaveMissingSplitSeg = true;
        }
#endif
        if ( !canHaveMissingSplitSeg )
            _diagnostics.error("missing LC_SEGMENT_SPLIT_INFO in %s", _dylibID);
    }

    // Set the chained pointer format on old arm64e binaries using threaded rebase, and
    // which don't have LC_DYLD_CHAINED_FIXUPS
    if ( (_chainedFixupsCmd == nullptr) && mh->isArch("arm64e") ) {
        _chainedFixupsFormat = DYLD_CHAINED_PTR_ARM64E;
    }
}

template <typename P>
void Adjustor<P>::adjustImageForNewSegmentLocations(CacheBuilder::ASLR_Tracker& aslrTracker,
                                                    CacheBuilder::LOH_Tracker* lohTracker,
                                                    const CacheBuilder::CacheCoalescedText* coalescedText,
                                                    const CacheBuilder::DylibTextCoalescer& textCoalescer)
{
    if ( _diagnostics.hasError() )
        return;
    if ( _splitSegInfoV2 ) {
        adjustReferencesUsingInfoV2(aslrTracker, lohTracker, coalescedText, textCoalescer);
        adjustChainedFixups(textCoalescer);
    }
    else if ( _chainedFixupsCmd != nullptr ) {
        // need to adjust the chain fixup segment_offset fields in LINKEDIT before chains can be walked
        adjustChainedFixups(textCoalescer);
        adjustRebaseChains(aslrTracker);
        adjustCode();
    }
    else {
        adjustDataPointers(aslrTracker);
        adjustCode();
    }
    if ( _diagnostics.hasError() )
        return;
    adjustSymbolTable();
    if ( _diagnostics.hasError() )
        return;

    adjustExternalRelocations();
    if ( _diagnostics.hasError() )
        return;
    rebuildLinkEditAndLoadCommands(textCoalescer);

#if DEBUG
    Diagnostics  diag;
    _mh->validateDyldCacheDylib(diag, _dylibID);
    if ( diag.hasError() ) {
        fprintf(stderr, "%s\n", diag.errorMessage().c_str());
    }
#endif
}

template <typename P>
uint64_t Adjustor<P>::slideForOrigAddress(uint64_t addr)
{
    for (unsigned i=0; i < _segOrigStartAddresses.size(); ++i) {
        if ( (_segOrigStartAddresses[i] <= addr) && (addr < (_segOrigStartAddresses[i]+_segCmds[i]->vmsize())) )
            return _segSlides[i];
    }
    // On arm64, high nibble of pointers can have extra bits
    if ( _maskPointers && (addr & 0xF000000000000000) ) {
        return slideForOrigAddress(addr & 0x0FFFFFFFFFFFFFFF);
    }
    _diagnostics.error("slide not known for dylib address 0x%llX in %s", addr, _dylibID);
    return 0;
}

template <typename P>
void Adjustor<P>::rebuildLinkEditAndLoadCommands(const CacheBuilder::DylibTextCoalescer& textCoalescer)
{
    // Exports trie is only data structure in LINKEDIT that might grow
    std::vector<uint8_t> newTrieBytes;
    adjustExportsTrie(newTrieBytes);

    // Remove: code signature, rebase info, code-sign-dirs, split seg info
    uint32_t chainedFixupsOffset = 0;
    uint32_t chainedFixupsSize   = _chainedFixupsCmd ? _chainedFixupsCmd->datasize : 0;
    uint32_t bindOffset          = chainedFixupsOffset + chainedFixupsSize;
    uint32_t bindSize            = _dyldInfo ? _dyldInfo->bind_size : 0;
    uint32_t weakBindOffset      = bindOffset + bindSize;
    uint32_t weakBindSize        = _dyldInfo ? _dyldInfo->weak_bind_size : 0;
    uint32_t lazyBindOffset      = weakBindOffset + weakBindSize;
    uint32_t lazyBindSize        = _dyldInfo ? _dyldInfo->lazy_bind_size : 0;
    uint32_t exportOffset        = lazyBindOffset + lazyBindSize;
    uint32_t exportSize          = (uint32_t)newTrieBytes.size();
    uint32_t splitSegInfoOffset  = exportOffset + exportSize;
    uint32_t splitSegInfosSize   = (_splitSegInfoCmd ? _splitSegInfoCmd->datasize : 0);
    uint32_t funcStartsOffset    = splitSegInfoOffset + splitSegInfosSize;
    uint32_t funcStartsSize      = (_functionStartsCmd ? _functionStartsCmd->datasize : 0);
    uint32_t dataInCodeOffset    = funcStartsOffset + funcStartsSize;
    uint32_t dataInCodeSize      = (_dataInCodeCmd ? _dataInCodeCmd->datasize : 0);
    uint32_t symbolTableOffset   = dataInCodeOffset + dataInCodeSize;
    uint32_t symbolTableSize     = _symTabCmd->nsyms * sizeof(macho_nlist<P>);
    uint32_t indirectTableOffset = symbolTableOffset + symbolTableSize;
    uint32_t indirectTableSize   = _dynSymTabCmd ? (_dynSymTabCmd->nindirectsyms * sizeof(uint32_t)) : 0;
    uint32_t externalRelocOffset = indirectTableOffset + indirectTableSize;
    uint32_t externalRelocSize   = _dynSymTabCmd ? (_dynSymTabCmd->nextrel * sizeof(relocation_info)) : 0;
    uint32_t symbolStringsOffset = externalRelocOffset + externalRelocSize;
    uint32_t symbolStringsSize   = _symTabCmd->strsize;
    uint32_t newLinkEditSize     = symbolStringsOffset + symbolStringsSize;

    size_t linkeditBufferSize = align(_segCmds[_linkeditSegIndex]->vmsize(), 12);
    if ( linkeditBufferSize < newLinkEditSize ) {
        _diagnostics.error("LINKEDIT overflow in %s", _dylibID);
        return;
    }

    uint8_t* newLinkeditBufer = (uint8_t*)::calloc(linkeditBufferSize, 1);
    if ( chainedFixupsSize )
        memcpy(&newLinkeditBufer[chainedFixupsOffset], &_linkeditBias[_chainedFixupsCmd->dataoff], chainedFixupsSize);
    if ( bindSize )
        memcpy(&newLinkeditBufer[bindOffset], &_linkeditBias[_dyldInfo->bind_off], bindSize);
    if ( lazyBindSize )
        memcpy(&newLinkeditBufer[lazyBindOffset], &_linkeditBias[_dyldInfo->lazy_bind_off], lazyBindSize);
    if ( weakBindSize )
        memcpy(&newLinkeditBufer[weakBindOffset], &_linkeditBias[_dyldInfo->weak_bind_off], weakBindSize);
    if ( exportSize )
        memcpy(&newLinkeditBufer[exportOffset], &newTrieBytes[0], exportSize);
    if ( splitSegInfosSize )
        memcpy(&newLinkeditBufer[splitSegInfoOffset], &_linkeditBias[_splitSegInfoCmd->dataoff], splitSegInfosSize);
    if ( funcStartsSize )
        memcpy(&newLinkeditBufer[funcStartsOffset], &_linkeditBias[_functionStartsCmd->dataoff], funcStartsSize);
    if ( dataInCodeSize )
        memcpy(&newLinkeditBufer[dataInCodeOffset], &_linkeditBias[_dataInCodeCmd->dataoff], dataInCodeSize);
    if ( symbolTableSize )
        memcpy(&newLinkeditBufer[symbolTableOffset], &_linkeditBias[_symTabCmd->symoff], symbolTableSize);
    if ( indirectTableSize )
        memcpy(&newLinkeditBufer[indirectTableOffset], &_linkeditBias[_dynSymTabCmd->indirectsymoff], indirectTableSize);
    if ( externalRelocSize )
        memcpy(&newLinkeditBufer[externalRelocOffset], &_linkeditBias[_dynSymTabCmd->extreloff], externalRelocSize);
    if ( symbolStringsSize )
        memcpy(&newLinkeditBufer[symbolStringsOffset], &_linkeditBias[_symTabCmd->stroff], symbolStringsSize);

    memcpy(_mappingInfo[_linkeditSegIndex].dstSegment, newLinkeditBufer, newLinkEditSize);
    ::bzero(((uint8_t*)_mappingInfo[_linkeditSegIndex].dstSegment)+newLinkEditSize, linkeditBufferSize-newLinkEditSize);
    ::free(newLinkeditBufer);
    uint32_t linkeditStartOffset = (uint32_t)_mappingInfo[_linkeditSegIndex].dstCacheFileOffset;

    // updates load commands and removed ones no longer needed
    
    __block unsigned segIndex = 0;
    _mh->forEachLoadCommand(_diagnostics, ^(const load_command *cmd, bool &stop) {
        symtab_command*                    symTabCmd;
        dysymtab_command*                  dynSymTabCmd;
        dyld_info_command*                 dyldInfo;
        linkedit_data_command*             functionStartsCmd;
        linkedit_data_command*             dataInCodeCmd;
        linkedit_data_command*             chainedFixupsCmd;
        linkedit_data_command*             exportTrieCmd;
        linkedit_data_command*             splitSegInfoCmd;
        macho_segment_command<P>*          segCmd;
        macho_routines_command<P>*         routinesCmd;
        dylib_command*                     dylibIDCmd;
        int32_t segFileOffsetDelta;
        switch ( cmd->cmd ) {
            case LC_ID_DYLIB:
                dylibIDCmd = (dylib_command*)cmd;
                dylibIDCmd->dylib.timestamp = 2; // match what static linker sets in LC_LOAD_DYLIB
                break;
            case LC_SYMTAB:
                symTabCmd = (symtab_command*)cmd;
                symTabCmd->symoff = linkeditStartOffset + symbolTableOffset;
                symTabCmd->stroff = linkeditStartOffset + symbolStringsOffset;
                break;
            case LC_DYSYMTAB:
                dynSymTabCmd = (dysymtab_command*)cmd;
                dynSymTabCmd->indirectsymoff = linkeditStartOffset + indirectTableOffset;
                // Clear local relocations (ie, old style rebases) as they were tracked earlier when we applied split seg
                dynSymTabCmd->locreloff = 0;
                dynSymTabCmd->nlocrel = 0 ;
                // Update external relocations as we need these later to resolve binds from kexts
                dynSymTabCmd->extreloff = linkeditStartOffset + externalRelocOffset;
                break;
            case LC_DYLD_INFO:
            case LC_DYLD_INFO_ONLY:
                dyldInfo = (dyld_info_command*)cmd;
                dyldInfo->rebase_off = 0;
                dyldInfo->rebase_size = 0;
                dyldInfo->bind_off = bindSize ? linkeditStartOffset + bindOffset : 0;
                dyldInfo->bind_size = bindSize;
                dyldInfo->weak_bind_off = weakBindSize ? linkeditStartOffset + weakBindOffset : 0;
                dyldInfo->weak_bind_size = weakBindSize;
                dyldInfo->lazy_bind_off = lazyBindSize ? linkeditStartOffset + lazyBindOffset : 0;
                dyldInfo->lazy_bind_size = lazyBindSize;
                dyldInfo->export_off = exportSize ? linkeditStartOffset + exportOffset : 0;
                dyldInfo->export_size = exportSize;
                break;
            case LC_FUNCTION_STARTS:
                functionStartsCmd = (linkedit_data_command*)cmd;
                functionStartsCmd->dataoff = linkeditStartOffset + funcStartsOffset;
                break;
            case LC_DATA_IN_CODE:
                dataInCodeCmd = (linkedit_data_command*)cmd;
                dataInCodeCmd->dataoff = linkeditStartOffset + dataInCodeOffset;
                break;
            case LC_DYLD_CHAINED_FIXUPS:
                chainedFixupsCmd = (linkedit_data_command*)cmd;
                chainedFixupsCmd->dataoff = chainedFixupsSize ? linkeditStartOffset + chainedFixupsOffset : 0;
                chainedFixupsCmd->datasize = chainedFixupsSize;
                break;
            case LC_DYLD_EXPORTS_TRIE:
                exportTrieCmd = (linkedit_data_command*)cmd;
                exportTrieCmd->dataoff = exportSize ? linkeditStartOffset + exportOffset : 0;
                exportTrieCmd->datasize = exportSize;
                break;
            case macho_routines_command<P>::CMD:
                routinesCmd = (macho_routines_command<P>*)cmd;
                routinesCmd->set_init_address(routinesCmd->init_address()+slideForOrigAddress(routinesCmd->init_address()));
                break;
            case macho_segment_command<P>::CMD:
                segCmd = (macho_segment_command<P>*)cmd;
                segFileOffsetDelta = (int32_t)(_mappingInfo[segIndex].dstCacheFileOffset - segCmd->fileoff());
                segCmd->set_vmaddr(_mappingInfo[segIndex].dstCacheUnslidAddress);
                segCmd->set_vmsize(_mappingInfo[segIndex].dstCacheSegmentSize);
                segCmd->set_fileoff(_mappingInfo[segIndex].dstCacheFileOffset);
                segCmd->set_filesize(_mappingInfo[segIndex].dstCacheFileSize);
                if ( strcmp(segCmd->segname(), "__LINKEDIT") == 0 )
                    segCmd->set_vmsize(linkeditBufferSize);
                if ( segCmd->nsects() > 0 ) {
                    macho_section<P>* const sectionsStart = (macho_section<P>*)((uint8_t*)segCmd + sizeof(macho_segment_command<P>));
                    macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];
                    
                    for (macho_section<P>*  sect=sectionsStart; sect < sectionsEnd; ++sect) {
                        bool coalescedSection = false;
                        if ( textCoalescer.sectionWasCoalesced(sect->segname(), sect->sectname())) {
                            coalescedSection = true;
                        }
#if BUILDING_APP_CACHE_UTIL
                        if ( strcmp(segCmd->segname(), "__CTF") == 0 ) {
                            // The kernel __CTF segment data is completely removed when we link the baseKC
                            if ( _mh->isStaticExecutable() )
                                coalescedSection = true;
                        }
#endif

                        if ( coalescedSection ) {
                            // Put coalesced sections at the end of the segment
                            sect->set_addr(segCmd->vmaddr() + segCmd->filesize());
                            sect->set_offset(0);
                            sect->set_size(0);
                        } else {
                            sect->set_addr(sect->addr() + _segSlides[segIndex]);
                            if ( sect->offset() != 0 )
                                sect->set_offset(sect->offset() + segFileOffsetDelta);
                        }
                    }
                }
                ++segIndex;
                break;
            case LC_SEGMENT_SPLIT_INFO:
                splitSegInfoCmd = (linkedit_data_command*)cmd;
                splitSegInfoCmd->dataoff = linkeditStartOffset + splitSegInfoOffset;
                break;
            default:
                break;
        }
    });

    _mh->removeLoadCommand(_diagnostics, ^(const load_command *cmd, bool &remove, bool &stop) {
        switch ( cmd->cmd ) {
            case LC_RPATH:
                _diagnostics.warning("dyld shared cache does not support LC_RPATH found in %s", _dylibID);
                remove = true;
                break;
            case LC_CODE_SIGNATURE:
            case LC_DYLIB_CODE_SIGN_DRS:
                remove = true;
                break;
            default:
                break;
        }
    });
    _mh->flags |= 0x80000000;
}


template <typename P>
void Adjustor<P>::adjustSymbolTable()
{
    if ( _dynSymTabCmd == nullptr )
        return;

    macho_nlist<P>*  symbolTable = (macho_nlist<P>*)&_linkeditBias[_symTabCmd->symoff];

    // adjust global symbol table entries
    macho_nlist<P>* lastExport = &symbolTable[_dynSymTabCmd->iextdefsym + _dynSymTabCmd->nextdefsym];
    for (macho_nlist<P>* entry = &symbolTable[_dynSymTabCmd->iextdefsym]; entry < lastExport; ++entry) {
        if ( (entry->n_type() & N_TYPE) == N_SECT )
            entry->set_n_value(entry->n_value() + slideForOrigAddress(entry->n_value()));
    }

    // adjust local symbol table entries
    macho_nlist<P>*  lastLocal = &symbolTable[_dynSymTabCmd->ilocalsym+_dynSymTabCmd->nlocalsym];
    for (macho_nlist<P>* entry = &symbolTable[_dynSymTabCmd->ilocalsym]; entry < lastLocal; ++entry) {
        if ( (entry->n_sect() != NO_SECT) && ((entry->n_type() & N_STAB) == 0) )
            entry->set_n_value(entry->n_value() + slideForOrigAddress(entry->n_value()));
    }
}


template <typename P>
void Adjustor<P>::adjustChainedFixups(const CacheBuilder::DylibTextCoalescer& textCoalescer)
{
    if ( _chainedFixupsCmd == nullptr )
        return;

    // Pass a start hint in to withChainStarts which takes account of the LINKEDIT shifting but we haven't
    // yet updated that LC_SEGMENT to point to the new data
    const dyld_chained_fixups_header* header = (dyld_chained_fixups_header*)&_linkeditBias[_chainedFixupsCmd->dataoff];
    uint64_t startsOffset = ((uint64_t)header + header->starts_offset) - (uint64_t)_mh;

    // segment_offset in dyld_chained_starts_in_segment is wrong.  We need to move it to the new segment offset
    _mh->withChainStarts(_diagnostics, startsOffset, ^(const dyld_chained_starts_in_image* starts) {
        for (uint32_t segIndex=0; segIndex < starts->seg_count; ++segIndex) {
            if ( starts->seg_info_offset[segIndex] == 0 )
                continue;
            dyld_chained_starts_in_segment* segInfo = (dyld_chained_starts_in_segment*)((uint8_t*)starts + starts->seg_info_offset[segIndex]);
            segInfo->segment_offset = (uint64_t)_mappingInfo[segIndex].dstSegment - (uint64_t)_mh;

            // If the whole segment was coalesced the remove its chain starts
            if ( textCoalescer.segmentWasCoalesced(_segCmds[segIndex]->segname()) ) {
                segInfo->page_count = 0;
            }
        }
    });
}

template <typename P>
static uint64_t externalRelocBaseAddress(const dyld3::MachOAnalyzer* ma,
                                         std::vector<macho_segment_command<P>*> segCmds,
                                         std::vector<uint64_t> segOrigStartAddresses)
{
    if ( ma->isArch("x86_64") || ma->isArch("x86_64h") ) {
#if BUILDING_APP_CACHE_UTIL
        if ( ma->isKextBundle() ) {
            // for kext bundles the reloc base address starts at __TEXT segment
            return segOrigStartAddresses[0];
        }
#endif
        // for x86_64 reloc base address starts at first writable segment (usually __DATA)
        for (uint32_t i=0; i < segCmds.size(); ++i) {
            if ( segCmds[i]->initprot() & VM_PROT_WRITE )
                return segOrigStartAddresses[i];
        }
    }
    // For everyone else we start at 0
    return 0;
}


template <typename P>
void Adjustor<P>::adjustExternalRelocations()
{
    if ( _dynSymTabCmd == nullptr )
        return;

    // section index 0 refers to mach_header
    uint64_t baseAddress = _mappingInfo[0].dstCacheUnslidAddress;

    const uint64_t                  relocsStartAddress = externalRelocBaseAddress(_mh, _segCmds, _segOrigStartAddresses);
    relocation_info*                relocsStart = (relocation_info*)&_linkeditBias[_dynSymTabCmd->extreloff];
    relocation_info*                relocsEnd   = &relocsStart[_dynSymTabCmd->nextrel];
    for (relocation_info* reloc = relocsStart; reloc < relocsEnd; ++reloc) {
        // External relocations should be relative to the base address of the mach-o as otherwise they
        // probably won't fit in 32-bits.
        uint64_t newAddress = reloc->r_address + slideForOrigAddress(relocsStartAddress + reloc->r_address);
        newAddress -= baseAddress;
        reloc->r_address = (int32_t)newAddress;
        assert((uint64_t)reloc->r_address == newAddress);
    }
}

template <typename P>
void Adjustor<P>::slidePointer(int segIndex, uint64_t segOffset, uint8_t type, CacheBuilder::ASLR_Tracker& aslrTracker)
{
    pint_t*   mappedAddrP  = (pint_t*)((uint8_t*)_mappingInfo[segIndex].dstSegment + segOffset);
    uint32_t* mappedAddr32 = (uint32_t*)mappedAddrP;
    pint_t    valueP;
    uint32_t  value32;
    switch ( type ) {
        case REBASE_TYPE_POINTER:
            valueP = (pint_t)P::getP(*mappedAddrP);
            P::setP(*mappedAddrP, valueP + slideForOrigAddress(valueP));
            aslrTracker.add(mappedAddrP);
            break;
        
        case REBASE_TYPE_TEXT_ABSOLUTE32:
            value32 = P::E::get32(*mappedAddr32);
            P::E::set32(*mappedAddr32, value32 + (uint32_t)slideForOrigAddress(value32));
            break;

        case REBASE_TYPE_TEXT_PCREL32:
            // general text relocs not support
        default:
            _diagnostics.error("unknown rebase type 0x%02X in %s", type, _dylibID);
    }
}


static bool isThumbMovw(uint32_t instruction)
{
    return ( (instruction & 0x8000FBF0) == 0x0000F240 );
}

static bool isThumbMovt(uint32_t instruction)
{
    return ( (instruction & 0x8000FBF0) == 0x0000F2C0 );
}

static uint16_t getThumbWord(uint32_t instruction)
{
    uint32_t i = ((instruction & 0x00000400) >> 10);
    uint32_t imm4 = (instruction & 0x0000000F);
    uint32_t imm3 = ((instruction & 0x70000000) >> 28);
    uint32_t imm8 = ((instruction & 0x00FF0000) >> 16);
    return ((imm4 << 12) | (i << 11) | (imm3 << 8) | imm8);
}

static uint32_t setThumbWord(uint32_t instruction, uint16_t word) {
    uint32_t imm4 = (word & 0xF000) >> 12;
    uint32_t i =    (word & 0x0800) >> 11;
    uint32_t imm3 = (word & 0x0700) >> 8;
    uint32_t imm8 =  word & 0x00FF;
    return (instruction & 0x8F00FBF0) | imm4 | (i << 10) | (imm3 << 28) | (imm8 << 16);
}

static bool isArmMovw(uint32_t instruction)
{
    return (instruction & 0x0FF00000) == 0x03000000;
}

static bool isArmMovt(uint32_t instruction)
{
    return (instruction & 0x0FF00000) == 0x03400000;
}

static uint16_t getArmWord(uint32_t instruction)
{
    uint32_t imm4 = ((instruction & 0x000F0000) >> 16);
    uint32_t imm12 = (instruction & 0x00000FFF);
    return (imm4 << 12) | imm12;
}

static uint32_t setArmWord(uint32_t instruction, uint16_t word) {
    uint32_t imm4 = (word & 0xF000) >> 12;
    uint32_t imm12 = word & 0x0FFF;
    return (instruction & 0xFFF0F000) | (imm4 << 16) | imm12;
}


template <typename P>
void Adjustor<P>::convertArm64eRebaseToIntermediate(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* chainPtr, CacheBuilder::ASLR_Tracker& aslrTracker,
                                                    uint64_t targetSlide, bool convertRebaseChains)
{
    assert(chainPtr->arm64e.authRebase.bind == 0);
    assert( (_chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E)
           || (_chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_USERLAND)
           || (_chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_USERLAND24)
           || (_chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_KERNEL) );
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk orgPtr = *chainPtr;
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk tmp;
    if ( chainPtr->arm64e.authRebase.auth ) {
        uint64_t targetVMAddr = orgPtr.arm64e.authRebase.target + _segOrigStartAddresses[0] + targetSlide;

        // The merging code may have set the high bits, eg, to a tagged pointer
        // Note authRebase has no high8, so this is invalid if it occurs
        uint8_t high8 = targetVMAddr >> 56;
        if ( high8 ) {
            // The kernel uses the high bits in the vmAddr, so don't error there
            bool badPointer = true;
            if ( _chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_KERNEL ) {
                uint64_t vmOffset = targetVMAddr - _cacheBaseAddress;
                if ( (vmOffset >> 56) == 0 )
                    badPointer = false;
            }

            if ( badPointer ) {
                _diagnostics.error("Cannot set tag on pointer in '%s' as high bits are incompatible with pointer authentication", _dylibID);
                return;
            }
        }

        if ( _chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_USERLAND ) {
            // <rdar://60351693> the rebase target is a vmoffset, so we need to switch to tracking the target out of line
            aslrTracker.setAuthData(chainPtr, chainPtr->arm64e.authRebase.diversity, chainPtr->arm64e.authRebase.addrDiv, chainPtr->arm64e.authRebase.key);
            aslrTracker.setRebaseTarget64(chainPtr, targetVMAddr);
            chainPtr->arm64e.rebase.target = 0; // actual target vmAddr stored in side table
            chainPtr->arm64e.rebase.high8  = 0;
            chainPtr->arm64e.rebase.next   = orgPtr.arm64e.rebase.next;
            chainPtr->arm64e.rebase.bind   = 0;
            chainPtr->arm64e.rebase.auth   = 0;
            return;
        }

        if ( convertRebaseChains ) {
            // This chain has been broken by merging CF constants.
            // Instead of trying to maintain the chain, just set the raw value now
            aslrTracker.setAuthData(chainPtr, chainPtr->arm64e.authRebase.diversity, chainPtr->arm64e.authRebase.addrDiv, chainPtr->arm64e.authRebase.key);
            chainPtr->raw64 = targetVMAddr;
            return;
        }

        // we need to change the rebase to point to the new address in the dyld cache, but it may not fit
        tmp.arm64e.authRebase.target = targetVMAddr;
        if ( tmp.arm64e.authRebase.target == targetVMAddr ) {
            // everything fits, just update target
            chainPtr->arm64e.authRebase.target = targetVMAddr;
            return;
        }
        // see if it fits in a plain rebase
        tmp.arm64e.rebase.target = targetVMAddr;
        if ( tmp.arm64e.rebase.target == targetVMAddr ) {
            // does fit in plain rebase, so convert to that and store auth data in side table
            aslrTracker.setAuthData(chainPtr, chainPtr->arm64e.authRebase.diversity, chainPtr->arm64e.authRebase.addrDiv, chainPtr->arm64e.authRebase.key);
            chainPtr->arm64e.rebase.target = targetVMAddr;
            chainPtr->arm64e.rebase.high8  = 0;
            chainPtr->arm64e.rebase.next   = orgPtr.arm64e.rebase.next;
            chainPtr->arm64e.rebase.bind   = 0;
            chainPtr->arm64e.rebase.auth   = 0;
            return;
        }
        // target cannot fit into rebase chain, so store target in side table
        aslrTracker.setAuthData(chainPtr, chainPtr->arm64e.authRebase.diversity, chainPtr->arm64e.authRebase.addrDiv, chainPtr->arm64e.authRebase.key);
        aslrTracker.setRebaseTarget64(chainPtr, targetVMAddr);
        chainPtr->arm64e.rebase.target = 0; // actual target vmAddr stored in side table
        chainPtr->arm64e.rebase.high8  = 0;
        chainPtr->arm64e.rebase.next   = orgPtr.arm64e.rebase.next;
        chainPtr->arm64e.rebase.bind   = 0;
        chainPtr->arm64e.rebase.auth   = 0;
        return;
    }
    else {
        uint64_t targetVMAddr = 0;
        switch (_chainedFixupsFormat) {
            case DYLD_CHAINED_PTR_ARM64E:
                targetVMAddr = orgPtr.arm64e.rebase.target + targetSlide;
                break;
            case DYLD_CHAINED_PTR_ARM64E_USERLAND:
            case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
                // <rdar://60351693> the rebase target is a vmoffset, so we need to switch to tracking the target out of line
                aslrTracker.setRebaseTarget64(chainPtr, orgPtr.arm64e.rebase.target + targetSlide);
                orgPtr.arm64e.rebase.target = 0;
                targetVMAddr = 0;
                break;
            case DYLD_CHAINED_PTR_ARM64E_KERNEL:
                targetVMAddr = orgPtr.arm64e.rebase.target + _segOrigStartAddresses[0] + targetSlide;
                break;
            default:
                _diagnostics.error("Unknown chain format");
                return;
        }

        // The merging code may have set the high bits, eg, to a tagged pointer
        uint8_t high8 = targetVMAddr >> 56;
        if ( chainPtr->arm64e.rebase.high8 ) {
            if ( high8 ) {
                _diagnostics.error("Cannot set tag on pointer as high bits are in use");
                return;
            }
            aslrTracker.setHigh8(chainPtr, chainPtr->arm64e.rebase.high8);
        } else {
            if ( high8 ) {
                aslrTracker.setHigh8(chainPtr, high8);
                targetVMAddr &= 0x00FFFFFFFFFFFFFF;
            }
        }

        if ( convertRebaseChains ) {
            // This chain has been broken by merging CF constants.
            // Instead of trying to maintain the chain, just set the raw value now
            chainPtr->raw64 = targetVMAddr;
            return;
        }

        tmp.arm64e.rebase.target = targetVMAddr;
        if ( tmp.arm64e.rebase.target == targetVMAddr ) {
            // target dyld cache address fits in plain rebase, so all we need to do is adjust that
            chainPtr->arm64e.rebase.target = targetVMAddr;
            return;
        }

        // target cannot fit into rebase chain, so store target in side table
        aslrTracker.setRebaseTarget64(chainPtr, targetVMAddr);
        chainPtr->arm64e.rebase.target = 0; // actual target vmAddr stored in side table
    }
}


template <typename P>
void Adjustor<P>::convertGeneric64RebaseToIntermediate(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* chainPtr, CacheBuilder::ASLR_Tracker& aslrTracker, uint64_t targetSlide)
{
    assert( (_chainedFixupsFormat == DYLD_CHAINED_PTR_64) || (_chainedFixupsFormat == DYLD_CHAINED_PTR_64_OFFSET) );
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk orgPtr = *chainPtr;
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk tmp;

    uint64_t targetVMAddr = 0;
    switch (_chainedFixupsFormat) {
        case DYLD_CHAINED_PTR_64:
            targetVMAddr = orgPtr.generic64.rebase.target + targetSlide;
            break;
        case DYLD_CHAINED_PTR_64_OFFSET:
            // <rdar://60351693> the rebase target is a vmoffset, so we need to switch to tracking the target out of line
            targetVMAddr = orgPtr.generic64.rebase.target + _segOrigStartAddresses[0] + targetSlide;
            aslrTracker.setRebaseTarget64(chainPtr, targetVMAddr);
            chainPtr->generic64.rebase.target = 0;
            return;
            break;
        default:
            _diagnostics.error("Unknown chain format");
            return;
    }

    // we need to change the rebase to point to the new address in the dyld cache, but it may not fit
    tmp.generic64.rebase.target = targetVMAddr;
    if ( tmp.generic64.rebase.target == targetVMAddr ) {
        // everything fits, just update target
        chainPtr->generic64.rebase.target = targetVMAddr;
        return;
    }

    // target cannot fit into rebase chain, so store target in side table
     aslrTracker.setRebaseTarget64(chainPtr, targetVMAddr);
     chainPtr->generic64.rebase.target = 0; // actual target vmAddr stored in side table
}


template <typename P>
void Adjustor<P>::adjustReference(uint32_t kind, uint8_t* mappedAddr, uint64_t fromNewAddress, uint64_t toNewAddress,
                                  int64_t adjust, int64_t targetSlide, uint64_t imageStartAddress, uint64_t imageEndAddress,
                                  bool convertRebaseChains,
                                  CacheBuilder::ASLR_Tracker& aslrTracker, CacheBuilder::LOH_Tracker* lohTracker,
                                  uint32_t*& lastMappedAddr32, uint32_t& lastKind, uint64_t& lastToNewAddress)
{
    uint64_t value64;
    uint64_t* mappedAddr64 = 0;
    uint32_t value32;
    uint32_t* mappedAddr32 = 0;
    uint32_t instruction;
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk* chainPtr;
    uint32_t newPageOffset;
    int64_t delta;
    switch ( kind ) {
        case DYLD_CACHE_ADJ_V2_DELTA_32:
            mappedAddr32 = (uint32_t*)mappedAddr;
            value32 = P::E::get32(*mappedAddr32);
            delta = (int32_t)value32;
            delta += adjust;
            if ( (delta > 0x80000000) || (-delta > 0x80000000) ) {
                _diagnostics.error("DYLD_CACHE_ADJ_V2_DELTA_32 can't be adjust by 0x%016llX in %s", adjust, _dylibID);
                return;
            }
            P::E::set32(*mappedAddr32, (int32_t)delta);
            break;
        case DYLD_CACHE_ADJ_V2_POINTER_32:
            mappedAddr32 = (uint32_t*)mappedAddr;
            if ( _chainedFixupsCmd != nullptr ) {
                chainPtr = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)mappedAddr32;
                switch (_chainedFixupsFormat) {
                    case DYLD_CHAINED_PTR_32:
                        // ignore binds, fix up rebases to have new targets
                        if ( chainPtr->generic32.rebase.bind == 0 ) {
                            // there is not enough space in 32-bit pointer to store new vmaddr in cache in 26-bit target
                            // so store target in side table that will be applied when binds are resolved
                            aslrTracker.add(mappedAddr32);
                            uint32_t target = (uint32_t)(chainPtr->generic32.rebase.target + targetSlide);
                            aslrTracker.setRebaseTarget32(chainPtr, target);
                            chainPtr->generic32.rebase.target = 0; // actual target stored in side table
                        }
                        break;
                    default:
                        _diagnostics.error("unknown 32-bit chained fixup format %d in %s", _chainedFixupsFormat, _dylibID);
                        break;
                }
            }
            else if ( _mh->usesClassicRelocationsInKernelCollection() ) {
                // Classic relocs are not guaranteed to be aligned, so always store them in the side table
                if ( (uint32_t)toNewAddress != (uint32_t)(E::get32(*mappedAddr32) + targetSlide) ) {
                    _diagnostics.error("bad DYLD_CACHE_ADJ_V2_POINTER_32 value not as expected at address 0x%llX in %s", fromNewAddress, _dylibID);
                    return;
                }
                aslrTracker.setRebaseTarget32(mappedAddr32, (uint32_t)toNewAddress);
                E::set32(*mappedAddr32, 0);
                aslrTracker.add(mappedAddr32);
            }
            else {
                if ( toNewAddress != (uint64_t)(E::get32(*mappedAddr32) + targetSlide) ) {
                    _diagnostics.error("bad DYLD_CACHE_ADJ_V2_POINTER_32 value not as expected at address 0x%llX in %s", fromNewAddress, _dylibID);
                    return;
                }
                E::set32(*mappedAddr32, (uint32_t)toNewAddress);
                aslrTracker.add(mappedAddr32);
            }
            break;
        case DYLD_CACHE_ADJ_V2_POINTER_64:
            mappedAddr64 = (uint64_t*)mappedAddr;
            if ( _chainedFixupsCmd != nullptr ) {
                chainPtr = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)mappedAddr64;
                switch (_chainedFixupsFormat) {
                    case DYLD_CHAINED_PTR_ARM64E:
                    case DYLD_CHAINED_PTR_ARM64E_USERLAND:
                    case DYLD_CHAINED_PTR_ARM64E_USERLAND24:
                    case DYLD_CHAINED_PTR_ARM64E_KERNEL:
                        // ignore binds and adjust rebases to new segment locations
                        if ( chainPtr->arm64e.authRebase.bind == 0 ) {
                            convertArm64eRebaseToIntermediate(chainPtr, aslrTracker, targetSlide, convertRebaseChains);
                            // Note, the pointer remains a chain with just the target of the rebase adjusted to the new target location
                            aslrTracker.add(chainPtr);
                        }
                        break;
                    case DYLD_CHAINED_PTR_64:
                    case DYLD_CHAINED_PTR_64_OFFSET:
                        // ignore binds and adjust rebases to new segment locations
                        if ( chainPtr->generic64.rebase.bind == 0 ) {
                            convertGeneric64RebaseToIntermediate(chainPtr, aslrTracker, targetSlide);
                            // Note, the pointer remains a chain with just the target of the rebase adjusted to the new target location
                            aslrTracker.add(chainPtr);
                        }
                        break;
                    default:
                        _diagnostics.error("unknown 64-bit chained fixup format %d in %s", _chainedFixupsFormat, _dylibID);
                        break;
                }
            }
            else if ( _mh->usesClassicRelocationsInKernelCollection() ) {
                if ( toNewAddress != (E::get64(*mappedAddr64) + targetSlide) ) {
                    _diagnostics.error("bad DYLD_CACHE_ADJ_V2_POINTER_64 value not as expected at address 0x%llX in %s", fromNewAddress, _dylibID);
                    return;
                }
                aslrTracker.setRebaseTarget64(mappedAddr64, toNewAddress);
                E::set64(*mappedAddr64, 0); // actual target vmAddr stored in side table
                aslrTracker.add(mappedAddr64);
                uint8_t high8 = toNewAddress >> 56;
                if ( high8 )
                    aslrTracker.setHigh8(mappedAddr64, high8);
            }
            else {
                if ( toNewAddress != (E::get64(*mappedAddr64) + targetSlide) ) {
                    _diagnostics.error("bad DYLD_CACHE_ADJ_V2_POINTER_64 value not as expected at address 0x%llX in %s", fromNewAddress, _dylibID);
                    return;
                }
                E::set64(*mappedAddr64, toNewAddress);
                aslrTracker.add(mappedAddr64);
                uint8_t high8 = toNewAddress >> 56;
                if ( high8 )
                    aslrTracker.setHigh8(mappedAddr64, high8);
            }
            break;
        case DYLD_CACHE_ADJ_V2_THREADED_POINTER_64:
            // old style arm64e binary
            chainPtr = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)mappedAddr;
            // ignore binds, they are proccessed later
            if ( chainPtr->arm64e.authRebase.bind == 0 ) {
                convertArm64eRebaseToIntermediate(chainPtr, aslrTracker, targetSlide, convertRebaseChains);
                // Note, the pointer remains a chain with just the target of the rebase adjusted to the new target location
                aslrTracker.add(chainPtr);
            }
            break;
       case DYLD_CACHE_ADJ_V2_DELTA_64:
            mappedAddr64 = (uint64_t*)mappedAddr;
            value64 = P::E::get64(*mappedAddr64);
            E::set64(*mappedAddr64, value64 + adjust);
            break;
        case DYLD_CACHE_ADJ_V2_IMAGE_OFF_32:
            if ( adjust == 0 )
                break;
            mappedAddr32 = (uint32_t*)mappedAddr;
            value32 = P::E::get32(*mappedAddr32);
            value64 = toNewAddress - imageStartAddress;
            if ( value64 > imageEndAddress ) {
                _diagnostics.error("DYLD_CACHE_ADJ_V2_IMAGE_OFF_32 can't be adjust to 0x%016llX in %s", toNewAddress, _dylibID);
                return;
            }
            P::E::set32(*mappedAddr32, (uint32_t)value64);
            break;
        case DYLD_CACHE_ADJ_V2_ARM64_ADRP:
            mappedAddr32 = (uint32_t*)mappedAddr;
            if (lohTracker)
                (*lohTracker)[toNewAddress].insert(mappedAddr);
            instruction = P::E::get32(*mappedAddr32);
            if ( (instruction & 0x9F000000) == 0x90000000 ) {
                int64_t pageDistance = ((toNewAddress & ~0xFFF) - (fromNewAddress & ~0xFFF));
                int64_t newPage21 = pageDistance >> 12;
                if ( (newPage21 > 2097151) || (newPage21 < -2097151) ) {
                    _diagnostics.error("DYLD_CACHE_ADJ_V2_ARM64_ADRP can't be adjusted that far in %s", _dylibID);
                    return;
                }
                instruction = (instruction & 0x9F00001F) | ((newPage21 << 29) & 0x60000000) | ((newPage21 << 3) & 0x00FFFFE0);
                P::E::set32(*mappedAddr32, instruction);
            }
            else {
                // ADRP instructions are sometimes optimized to other instructions (e.g. ADR) after the split-seg-info is generated
            }
            break;
        case DYLD_CACHE_ADJ_V2_ARM64_OFF12:
            mappedAddr32 = (uint32_t*)mappedAddr;
            if (lohTracker)
                (*lohTracker)[toNewAddress].insert(mappedAddr);
            instruction = P::E::get32(*mappedAddr32);
            // This is a page offset, so if we pack both the __TEXT page with the add/ldr, and
            // the destination page with the target data, then the adjust isn't correct.  Instead
            // we always want the page offset of the target, ignoring where the source add/ldr slid
            newPageOffset = (uint32_t)(toNewAddress & 0xFFF);
            if ( (instruction & 0x3B000000) == 0x39000000 ) {
                // LDR/STR imm12
                uint32_t encodedAddend = ((instruction & 0x003FFC00) >> 10);
                uint32_t newAddend = 0;
                switch ( instruction & 0xC0000000 ) {
                    case 0x00000000:
                        if ( (instruction & 0x04800000) == 0x04800000 ) {
                            if ( newPageOffset & 0xF ) {
                                _diagnostics.error("can't adjust off12 scale=16 instruction to %d bytes at mapped address=%p in %s", newPageOffset, mappedAddr, _dylibID);
                                return;
                            }
                            if ( encodedAddend*16 >= 4096 ) {
                                _diagnostics.error("off12 scale=16 instruction points outside its page at mapped address=%p in %s", mappedAddr, _dylibID);
                            }
                            newAddend = (newPageOffset/16);
                        }
                        else {
                            // scale=1
                            newAddend = newPageOffset;
                        }
                        break;
                    case 0x40000000:
                        if ( newPageOffset & 1 ) {
                            _diagnostics.error("can't adjust off12 scale=2 instruction to %d bytes at mapped address=%p in %s", newPageOffset, mappedAddr, _dylibID);
                            return;
                        }
                        if ( encodedAddend*2 >= 4096 ) {
                            _diagnostics.error("off12 scale=2 instruction points outside its page at mapped address=%p in %s", mappedAddr, _dylibID);
                            return;
                        }
                        newAddend = (newPageOffset/2);
                        break;
                    case 0x80000000:
                        if ( newPageOffset & 3 ) {
                            _diagnostics.error("can't adjust off12 scale=4 instruction to %d bytes at mapped address=%p in %s", newPageOffset, mappedAddr, _dylibID);
                            return;
                        }
                        if ( encodedAddend*4 >= 4096 ) {
                            _diagnostics.error("off12 scale=4 instruction points outside its page at mapped address=%p in %s", mappedAddr, _dylibID);
                            return;
                        }
                        newAddend = (newPageOffset/4);
                        break;
                    case 0xC0000000:
                        if ( newPageOffset & 7 ) {
                            _diagnostics.error("can't adjust off12 scale=8 instruction to %d bytes at mapped address=%p in %s", newPageOffset, mappedAddr, _dylibID);
                            return;
                        }
                        if ( encodedAddend*8 >= 4096 ) {
                            _diagnostics.error("off12 scale=8 instruction points outside its page at mapped address=%p in %s", mappedAddr, _dylibID);
                            return;
                        }
                        newAddend = (newPageOffset/8);
                        break;
                }
                uint32_t newInstruction = (instruction & 0xFFC003FF) | (newAddend << 10);
                P::E::set32(*mappedAddr32, newInstruction);
            }
            else if ( (instruction & 0xFFC00000) == 0x91000000 ) {
                // ADD imm12
                if ( instruction & 0x00C00000 ) {
                    _diagnostics.error("ADD off12 uses shift at mapped address=%p in %s", mappedAddr, _dylibID);
                    return;
                }
                uint32_t newAddend = newPageOffset;
                uint32_t newInstruction = (instruction & 0xFFC003FF) | (newAddend << 10);
                P::E::set32(*mappedAddr32, newInstruction);
            }
            else if ( instruction != 0xD503201F ) {
                // ignore imm12 instructions optimized into a NOP, but warn about others
                _diagnostics.error("unknown off12 instruction 0x%08X at 0x%0llX in %s", instruction, fromNewAddress, _dylibID);
                return;
            }
            break;
        case DYLD_CACHE_ADJ_V2_THUMB_MOVW_MOVT:
            mappedAddr32 = (uint32_t*)mappedAddr;
            // to update a movw/movt pair we need to extract the 32-bit they will make,
            // add the adjust and write back the new movw/movt pair.
            if ( lastKind == kind ) {
                if ( lastToNewAddress == toNewAddress ) {
                    uint32_t instruction1 = P::E::get32(*lastMappedAddr32);
                    uint32_t instruction2 = P::E::get32(*mappedAddr32);
                    if ( isThumbMovw(instruction1) && isThumbMovt(instruction2) ) {
                        uint16_t high = getThumbWord(instruction2);
                        uint16_t low  = getThumbWord(instruction1);
                        uint32_t full = high << 16 | low;
                        full += adjust;
                        instruction1 = setThumbWord(instruction1, full & 0xFFFF);
                        instruction2 = setThumbWord(instruction2, full >> 16);
                    }
                    else if ( isThumbMovt(instruction1) && isThumbMovw(instruction2) ) {
                        uint16_t high = getThumbWord(instruction1);
                        uint16_t low  = getThumbWord(instruction2);
                        uint32_t full = high << 16 | low;
                        full += adjust;
                        instruction2 = setThumbWord(instruction2, full & 0xFFFF);
                        instruction1 = setThumbWord(instruction1, full >> 16);
                    }
                    else {
                        _diagnostics.error("two DYLD_CACHE_ADJ_V2_THUMB_MOVW_MOVT in a row but not paried in %s", _dylibID);
                        return;
                    }
                    P::E::set32(*lastMappedAddr32, instruction1);
                    P::E::set32(*mappedAddr32, instruction2);
                    kind = 0;
                }
                else {
                    _diagnostics.error("two DYLD_CACHE_ADJ_V2_THUMB_MOVW_MOVT in a row but target different addresses in %s", _dylibID);
                    return;
                }
            }
            break;
        case DYLD_CACHE_ADJ_V2_ARM_MOVW_MOVT:
            mappedAddr32 = (uint32_t*)mappedAddr;
            // to update a movw/movt pair we need to extract the 32-bit they will make,
            // add the adjust and write back the new movw/movt pair.
            if ( lastKind == kind ) {
                if ( lastToNewAddress == toNewAddress ) {
                    uint32_t instruction1 = P::E::get32(*lastMappedAddr32);
                    uint32_t instruction2 = P::E::get32(*mappedAddr32);
                    if ( isArmMovw(instruction1) && isArmMovt(instruction2) ) {
                        uint16_t high = getArmWord(instruction2);
                        uint16_t low  = getArmWord(instruction1);
                        uint32_t full = high << 16 | low;
                        full += adjust;
                        instruction1 = setArmWord(instruction1, full & 0xFFFF);
                        instruction2 = setArmWord(instruction2, full >> 16);
                    }
                    else if ( isArmMovt(instruction1) && isArmMovw(instruction2) ) {
                        uint16_t high = getArmWord(instruction1);
                        uint16_t low  = getArmWord(instruction2);
                        uint32_t full = high << 16 | low;
                        full += adjust;
                        instruction2 = setArmWord(instruction2, full & 0xFFFF);
                        instruction1 = setArmWord(instruction1, full >> 16);
                    }
                    else {
                        _diagnostics.error("two DYLD_CACHE_ADJ_V2_ARM_MOVW_MOVT in a row but not paired in %s", _dylibID);
                        return;
                    }
                    P::E::set32(*lastMappedAddr32, instruction1);
                    P::E::set32(*mappedAddr32, instruction2);
                    kind = 0;
                }
                else {
                    _diagnostics.error("two DYLD_CACHE_ADJ_V2_ARM_MOVW_MOVT in a row but target different addresses in %s", _dylibID);
                    return;
                }
            }
            break;
        case DYLD_CACHE_ADJ_V2_ARM64_BR26: {
            if ( adjust == 0 )
                break;
            mappedAddr32 = (uint32_t*)mappedAddr;
            instruction = P::E::get32(*mappedAddr32);

            int64_t deltaToFinalTarget = toNewAddress - fromNewAddress;
            // Make sure the target is in range
            static const int64_t b128MegLimit = 0x07FFFFFF;
            if ( (deltaToFinalTarget > -b128MegLimit) && (deltaToFinalTarget < b128MegLimit) ) {
                instruction = (instruction & 0xFC000000) | ((deltaToFinalTarget >> 2) & 0x03FFFFFF);
                P::E::set32(*mappedAddr32, instruction);
                break;
            } else {
                _diagnostics.error("br26 instruction exceeds maximum range at mapped address=%p in %s", mappedAddr, _dylibID);
                return;
            }
        }
        case DYLD_CACHE_ADJ_V2_THUMB_BR22:
        case DYLD_CACHE_ADJ_V2_ARM_BR24:
            // nothing to do with calls to stubs
            break;
        default:
            _diagnostics.error("unknown split seg kind=%d in %s", kind, _dylibID);
            return;
    }
    lastKind = kind;
    lastToNewAddress = toNewAddress;
    lastMappedAddr32 = mappedAddr32;
}

template <typename P>
void Adjustor<P>::adjustReferencesUsingInfoV2(CacheBuilder::ASLR_Tracker& aslrTracker,
                                              CacheBuilder::LOH_Tracker* lohTracker,
                                              const CacheBuilder::CacheCoalescedText* coalescedText,
                                              const CacheBuilder::DylibTextCoalescer& textCoalescer)
{
    static const bool logDefault = false;
    bool log = logDefault;

    const uint8_t* infoStart = &_linkeditBias[_splitSegInfoCmd->dataoff];
    const uint8_t* infoEnd = &infoStart[_splitSegInfoCmd->datasize];
    if ( *infoStart++ != DYLD_CACHE_ADJ_V2_FORMAT ) {
        _diagnostics.error("malformed split seg info in %s", _dylibID);
        return;
    }
    // build section arrays of slide and mapped address for each section
    std::vector<uint64_t> sectionSlides;
    std::vector<uint64_t> sectionNewAddress;
    std::vector<uint8_t*> sectionMappedAddress;

    // Also track coalesced sections, if we have any
    typedef CacheBuilder::DylibTextCoalescer::DylibSectionOffsetToCacheSectionOffset DylibSectionOffsetToCacheSectionOffset;
    std::vector<uint64_t>                                       coalescedSectionOriginalVMAddrs;
    std::vector<uint64_t>                                       coalescedSectionNewVMAddrs;
    std::vector<uint8_t*>                                       coalescedSectionBufferAddrs;
    std::vector<const DylibSectionOffsetToCacheSectionOffset*>  coalescedSectionOffsetMaps;
    std::vector<uint64_t>                                       coalescedSectionObjcTags;

    sectionSlides.reserve(16);
    sectionNewAddress.reserve(16);
    sectionMappedAddress.reserve(16);
    coalescedSectionOriginalVMAddrs.reserve(16);
    coalescedSectionNewVMAddrs.reserve(16);
    coalescedSectionBufferAddrs.reserve(16);
    coalescedSectionOffsetMaps.reserve(16);
    coalescedSectionObjcTags.reserve(16);

    // section index 0 refers to mach_header
    sectionMappedAddress.push_back((uint8_t*)_mappingInfo[0].dstSegment);
    sectionSlides.push_back(_segSlides[0]);
    sectionNewAddress.push_back(_mappingInfo[0].dstCacheUnslidAddress);
    coalescedSectionOriginalVMAddrs.push_back(0);
    coalescedSectionNewVMAddrs.push_back(0);
    coalescedSectionBufferAddrs.push_back(nullptr);
    coalescedSectionOffsetMaps.push_back(nullptr);
    coalescedSectionObjcTags.push_back(0);

    uint64_t imageStartAddress  = sectionNewAddress.front();
    uint64_t imageEndAddress    = 0;

    // section 1 and later refer to real sections
    unsigned sectionIndex = 0;
    unsigned objcSelRefsSectionIndex = ~0U;
    for (unsigned segmentIndex=0; segmentIndex < _segCmds.size(); ++segmentIndex) {
        macho_segment_command<P>* segCmd = _segCmds[segmentIndex];
        macho_section<P>* const sectionsStart = (macho_section<P>*)((char*)segCmd + sizeof(macho_segment_command<P>));
        macho_section<P>* const sectionsEnd = &sectionsStart[segCmd->nsects()];

        for(macho_section<P>* sect = sectionsStart; sect < sectionsEnd; ++sect) {
            if ( textCoalescer.sectionWasCoalesced(sect->segname(), sect->sectname())) {
                // If we coalesced the segment then the sections aren't really there to be fixed up
                const DylibSectionOffsetToCacheSectionOffset& offsetMap = textCoalescer.getSectionCoalescer(sect->segname(),
                                                                                                            sect->sectname());
                uint64_t coalescedSectionNewVMAddr = coalescedText->getSectionVMAddr(sect->segname(), sect->sectname());
                uint8_t* coalescedSectionNewBufferAddr = coalescedText->getSectionBufferAddr(sect->segname(), sect->sectname());
                uint64_t coalescedSectionObjcTag = coalescedText->getSectionObjcTag(sect->segname(), sect->sectname());
                sectionMappedAddress.push_back(nullptr);
                sectionSlides.push_back(0);
                sectionNewAddress.push_back(0);
                coalescedSectionOriginalVMAddrs.push_back(sect->addr());
                coalescedSectionNewVMAddrs.push_back(coalescedSectionNewVMAddr);
                coalescedSectionBufferAddrs.push_back(coalescedSectionNewBufferAddr);
                coalescedSectionOffsetMaps.push_back(&offsetMap);
                coalescedSectionObjcTags.push_back(coalescedSectionObjcTag);
                ++sectionIndex;
                if (log) {
                    fprintf(stderr, " %s/%s, sectIndex=%d, mapped at=%p\n",
                            sect->segname(), sect->sectname(), sectionIndex, sectionMappedAddress.back());
                }
            } else {
                sectionMappedAddress.push_back((uint8_t*)_mappingInfo[segmentIndex].dstSegment + sect->addr() - segCmd->vmaddr());
                sectionSlides.push_back(_segSlides[segmentIndex]);
                sectionNewAddress.push_back(_mappingInfo[segmentIndex].dstCacheUnslidAddress + sect->addr() - segCmd->vmaddr());
                coalescedSectionOriginalVMAddrs.push_back(0);
                coalescedSectionNewVMAddrs.push_back(0);
                coalescedSectionBufferAddrs.push_back(nullptr);
                coalescedSectionOffsetMaps.push_back(nullptr);
                coalescedSectionObjcTags.push_back(0);
                ++sectionIndex;
                if (log) {
                    fprintf(stderr, " %s/%s, sectIndex=%d, mapped at=%p\n",
                            sect->segname(), sect->sectname(), sectionIndex, sectionMappedAddress.back());
                }
                if (!strcmp(sect->segname(), "__DATA") && !strcmp(sect->sectname(), "__objc_selrefs"))
                    objcSelRefsSectionIndex = sectionIndex;

                imageEndAddress = sectionNewAddress.back();
            }
        }
    }

    // Whole         :== <count> FromToSection+
    // FromToSection :== <from-sect-index> <to-sect-index> <count> ToOffset+
    // ToOffset         :== <to-sect-offset-delta> <count> FromOffset+
    // FromOffset     :== <kind> <count> <from-sect-offset-delta>
    const uint8_t* p = infoStart;
    uint64_t sectionCount = read_uleb128(p, infoEnd);
    for (uint64_t i=0; i < sectionCount; ++i) {
        uint32_t* lastMappedAddr32 = NULL;
        uint32_t lastKind = 0;
        uint64_t lastToNewAddress = 0;
        uint64_t fromSectionIndex = read_uleb128(p, infoEnd);
        uint64_t toSectionIndex = read_uleb128(p, infoEnd);
        uint64_t toOffsetCount = read_uleb128(p, infoEnd);
        uint64_t fromSectionSlide = sectionSlides[fromSectionIndex];
        uint64_t fromSectionNewAddress = sectionNewAddress[fromSectionIndex];
        uint8_t* fromSectionMappedAddress = sectionMappedAddress[fromSectionIndex];
        uint64_t toSectionSlide = sectionSlides[toSectionIndex];
        uint64_t toSectionNewAddress = sectionNewAddress[toSectionIndex];
        CacheBuilder::LOH_Tracker* lohTrackerPtr = (toSectionIndex == objcSelRefsSectionIndex) ? lohTracker : nullptr;
        if (log) printf(" from sect=%lld (mapped=%p), to sect=%lld (new addr=0x%llX):\n", fromSectionIndex, fromSectionMappedAddress, toSectionIndex, toSectionNewAddress);
        uint64_t toSectionOffset = 0;

        for (uint64_t j=0; j < toOffsetCount; ++j) {
            uint64_t toSectionDelta = read_uleb128(p, infoEnd);
            uint64_t fromOffsetCount = read_uleb128(p, infoEnd);
            toSectionOffset += toSectionDelta;
            for (uint64_t k=0; k < fromOffsetCount; ++k) {
                uint64_t kind = read_uleb128(p, infoEnd);
                if ( kind > 13 ) {
                    _diagnostics.error("unknown split seg info v2 kind value (%llu) in %s", kind, _dylibID);
                    return;
                }
                uint64_t fromSectDeltaCount = read_uleb128(p, infoEnd);
                uint64_t fromSectionOffset = 0;
                for (uint64_t l=0; l < fromSectDeltaCount; ++l) {
                    uint64_t delta = read_uleb128(p, infoEnd);
                    fromSectionOffset += delta;
                    if (log) printf("   kind=%lld, from offset=0x%0llX, to offset=0x%0llX, adjust=0x%llX, targetSlide=0x%llX\n", kind, fromSectionOffset, toSectionOffset, delta, toSectionSlide);

                    // It's possible for all either of from/to sectiobs to be coalesced/optimized.
                    // Handle each of those combinations.
                    uint8_t* fromMappedAddr = nullptr;
                    uint64_t fromNewAddress = 0;
                    uint64_t fromAtomSlide  = 0;
                    bool convertRebaseChains = false;
                    if ( coalescedSectionOffsetMaps[fromSectionIndex] != nullptr ) {
                        // From was optimized/coalesced
                        // This is only supported on pointer kind fixups, ie, pointers in RW segments
                        assert( (kind == DYLD_CACHE_ADJ_V2_POINTER_64) || (kind == DYLD_CACHE_ADJ_V2_THREADED_POINTER_64) );
                        // Find where the atom moved to with its new section
                        // CFString's and similar may have fixups in the middle of the atom, but the map only
                        // tracks the start offset for the atom.  We use lower_bound to find the atom containing
                        // the offset we are looking for
                        const DylibSectionOffsetToCacheSectionOffset* offsetMap = coalescedSectionOffsetMaps[fromSectionIndex];
                        auto offsetIt = offsetMap->lower_bound((uint32_t)fromSectionOffset);
                        if ( offsetIt->first != fromSectionOffset ) {
                            // This points to the middle of the atom, so check the previous atom
                            assert(offsetIt != offsetMap->begin());
                            --offsetIt;
                            assert(offsetIt->first <= fromSectionOffset);
                        }
                        assert(offsetIt != offsetMap->end());
                        // FIXME: Other CF type's have different atom sizes
                        uint64_t offsetInAtom = fromSectionOffset - offsetIt->first;
                        assert(offsetInAtom < (uint64_t)DyldSharedCache::ConstantClasses::cfStringAtomSize);

                        uint8_t* baseMappedAddr = coalescedSectionBufferAddrs[fromSectionIndex];
                        fromMappedAddr = baseMappedAddr + offsetIt->second + offsetInAtom;
                        uint64_t baseVMAddr = coalescedSectionNewVMAddrs[fromSectionIndex];
                        fromNewAddress = baseVMAddr + offsetIt->second + offsetInAtom;

                        // The 'from' section is gone, but we still need the 'from' slide.  Instead of a section slide,
                        // compute the slide for this individual atom
                        uint64_t fromAtomOriginalVMAddr = coalescedSectionOriginalVMAddrs[fromSectionIndex] + fromSectionOffset;
                        fromAtomSlide = fromNewAddress - fromAtomOriginalVMAddr;
                        convertRebaseChains = true;
                    } else {
                        // From was not optimized/coalesced
                        fromMappedAddr = fromSectionMappedAddress + fromSectionOffset;
                        fromNewAddress = fromSectionNewAddress + fromSectionOffset;
                        fromAtomSlide = fromSectionSlide;
                    }

                    uint64_t toNewAddress   = 0;
                    uint64_t toAtomSlide    = 0;
                    if ( coalescedSectionOffsetMaps[toSectionIndex] != nullptr ) {
                        // To was optimized/coalesced
                        const DylibSectionOffsetToCacheSectionOffset* offsetMap = coalescedSectionOffsetMaps[toSectionIndex];
                        auto offsetIt = offsetMap->find((uint32_t)toSectionOffset);
                        assert(offsetIt != offsetMap->end());
                        uint64_t baseVMAddr = coalescedSectionNewVMAddrs[toSectionIndex];
                        toNewAddress = baseVMAddr + offsetIt->second;

                        // Add in the high bits which are the tagged pointer TBI bits
                        toNewAddress |= coalescedSectionObjcTags[toSectionIndex];

                        // The 'to' section is gone, but we still need the 'to' slide.  Instead of a section slide,
                        // compute the slide for this individual atom
                        uint64_t toAtomOriginalVMAddr = coalescedSectionOriginalVMAddrs[toSectionIndex] + toSectionOffset;
                        toAtomSlide = toNewAddress - toAtomOriginalVMAddr;
                    } else {
                        // To was not optimized/coalesced
                        toNewAddress = toSectionNewAddress + toSectionOffset;
                        toAtomSlide = toSectionSlide;
                    }

                    int64_t  deltaAdjust        = toAtomSlide - fromAtomSlide;
                    if (log) {
                        printf("   kind=%lld, from offset=0x%0llX, to offset=0x%0llX, adjust=0x%llX, targetSlide=0x%llX\n",
                               kind, fromSectionOffset, toSectionOffset, deltaAdjust, toSectionSlide);
                    }
                    adjustReference((uint32_t)kind, fromMappedAddr, fromNewAddress, toNewAddress, deltaAdjust,
                                    toAtomSlide, imageStartAddress, imageEndAddress, convertRebaseChains,
                                    aslrTracker, lohTrackerPtr,
                                    lastMappedAddr32, lastKind, lastToNewAddress);
                    if ( _diagnostics.hasError() )
                        return;
                }
            }
        }
    }

}

template <typename P>
void Adjustor<P>::adjustRebaseChains(CacheBuilder::ASLR_Tracker& aslrTracker)
{
    const dyld_chained_fixups_header* chainHeader = (dyld_chained_fixups_header*)(&_linkeditBias[_chainedFixupsCmd->dataoff]);
    const dyld_chained_starts_in_image* startsInfo = (dyld_chained_starts_in_image*)((uint8_t*)chainHeader + chainHeader->starts_offset);
    _mh->forEachFixupInAllChains(_diagnostics, startsInfo, false,
        ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stop) {
            switch ( segInfo->pointer_format ) {
                case DYLD_CHAINED_PTR_64:
                    // only look at rebases
                    if ( fixupLoc->generic64.rebase.bind == 0 ) {
                        uint64_t rebaseTargetInDylib = fixupLoc->generic64.rebase.target;
                        uint64_t rebaseTargetInDyldcache = fixupLoc->generic64.rebase.target + slideForOrigAddress(rebaseTargetInDylib);
                        convertGeneric64RebaseToIntermediate(fixupLoc, aslrTracker, rebaseTargetInDyldcache);
                        aslrTracker.add(fixupLoc);
                    }
                    break;
                case DYLD_CHAINED_PTR_64_OFFSET:
                    _diagnostics.error("unhandled 64-bit chained fixup format %d in %s", _chainedFixupsFormat, _dylibID);
                    break;
                default:
                    _diagnostics.error("unsupported chained fixup format %d", segInfo->pointer_format);
                    stop = true;
            }
    });
}

static int uint32Sorter(const void* l, const void* r) {
    if ( *((uint32_t*)l) < *((uint32_t*)r) )
        return -1;
    else
        return 1;
}

template <typename P>
static uint64_t localRelocBaseAddress(const dyld3::MachOAnalyzer* ma,
                                      std::vector<macho_segment_command<P>*> segCmds,
                                      std::vector<uint64_t> segOrigStartAddresses)
{
    if ( ma->isArch("x86_64") || ma->isArch("x86_64h") ) {
#if BUILDING_APP_CACHE_UTIL
        if ( ma->isKextBundle() ) {
            // for kext bundles the reloc base address starts at __TEXT segment
            return segOrigStartAddresses[0];
        }
#endif
        // for all other kinds, the x86_64 reloc base address starts at first writable segment (usually __DATA)
        for (uint32_t i=0; i < segCmds.size(); ++i) {
            if ( segCmds[i]->initprot() & VM_PROT_WRITE )
                return segOrigStartAddresses[i];
        }
    }
    return segOrigStartAddresses[0];
}

static bool segIndexAndOffsetForAddress(uint64_t addr, const std::vector<uint64_t>& segOrigStartAddresses,
                                        std::vector<uint64_t> segSizes, uint32_t& segIndex, uint64_t& segOffset)
{
    for (uint32_t i=0; i < segOrigStartAddresses.size(); ++i) {
        if ( (segOrigStartAddresses[i] <= addr) && (addr < (segOrigStartAddresses[i] + segSizes[i])) ) {
            segIndex  = i;
            segOffset = addr - segOrigStartAddresses[i];
            return true;
        }
    }
    return false;
}

template <typename P>
void Adjustor<P>::adjustDataPointers(CacheBuilder::ASLR_Tracker& aslrTracker)
{
    if ( (_dynSymTabCmd != nullptr) && (_dynSymTabCmd->locreloff != 0) ) {
        // kexts may have old style relocations instead of dyldinfo rebases
        assert(_dyldInfo == nullptr);

        // old binary, walk relocations
        const uint64_t                  relocsStartAddress = localRelocBaseAddress(_mh, _segCmds, _segOrigStartAddresses);
        const relocation_info* const    relocsStart = (const relocation_info* const)&_linkeditBias[_dynSymTabCmd->locreloff];
        const relocation_info* const    relocsEnd   = &relocsStart[_dynSymTabCmd->nlocrel];
        bool                            stop = false;
        const uint8_t                   relocSize = (_mh->is64() ? 3 : 2);
        STACK_ALLOC_OVERFLOW_SAFE_ARRAY(uint32_t, relocAddrs, 2048);
        for (const relocation_info* reloc=relocsStart; (reloc < relocsEnd) && !stop; ++reloc) {
            if ( reloc->r_length != relocSize ) {
                _diagnostics.error("local relocation has wrong r_length");
                break;
            }
            if ( reloc->r_type != 0 ) { // 0 == X86_64_RELOC_UNSIGNED == GENERIC_RELOC_VANILLA ==  ARM64_RELOC_UNSIGNED
                _diagnostics.error("local relocation has wrong r_type");
                break;
            }
            relocAddrs.push_back(reloc->r_address);
        }
        if ( !relocAddrs.empty() ) {
            ::qsort(&relocAddrs[0], relocAddrs.count(), sizeof(uint32_t), &uint32Sorter);
            for (uint32_t addrOff : relocAddrs) {
                uint32_t segIndex  = 0;
                uint64_t segOffset = 0;
                if ( segIndexAndOffsetForAddress(relocsStartAddress+addrOff, _segOrigStartAddresses, _segSizes, segIndex, segOffset) ) {
                    uint8_t type = REBASE_TYPE_POINTER;
                    assert(_mh->cputype != CPU_TYPE_I386);
                    slidePointer(segIndex, segOffset, type, aslrTracker);
                }
                else {
                    _diagnostics.error("local relocation has out of range r_address");
                    break;
                }
            }
        }
        // then process indirect symbols
        // FIXME: Do we need indirect symbols?  Aren't those handled as binds?

        return;
    }

    if ( _dyldInfo == NULL )
        return;

    const uint8_t* p = &_linkeditBias[_dyldInfo->rebase_off];
    const uint8_t* end = &p[_dyldInfo->rebase_size];
    
    uint8_t type = 0;
    int segIndex = 0;
    uint64_t segOffset = 0;
    uint64_t count;
    uint64_t skip;
    bool done = false;
    while ( !done && (p < end) ) {
        uint8_t immediate = *p & REBASE_IMMEDIATE_MASK;
        uint8_t opcode = *p & REBASE_OPCODE_MASK;
        ++p;
        switch (opcode) {
            case REBASE_OPCODE_DONE:
                done = true;
                break;
            case REBASE_OPCODE_SET_TYPE_IMM:
                type = immediate;
                break;
            case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
                segIndex = immediate;
                segOffset = read_uleb128(p, end);
                break;
            case REBASE_OPCODE_ADD_ADDR_ULEB:
                segOffset += read_uleb128(p, end);
                break;
            case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
                segOffset += immediate*sizeof(pint_t);
                break;
            case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
                for (int i=0; i < immediate; ++i) {
                    slidePointer(segIndex, segOffset, type, aslrTracker);
                    segOffset += sizeof(pint_t);
                }
                break;
            case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
                count = read_uleb128(p, end);
                for (uint32_t i=0; i < count; ++i) {
                    slidePointer(segIndex, segOffset, type, aslrTracker);
                    segOffset += sizeof(pint_t);
                }
                break;
            case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
                slidePointer(segIndex, segOffset, type, aslrTracker);
                segOffset += read_uleb128(p, end) + sizeof(pint_t);
                break;
            case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
                count = read_uleb128(p, end);
                skip = read_uleb128(p, end);
                for (uint32_t i=0; i < count; ++i) {
                    slidePointer(segIndex, segOffset, type, aslrTracker);
                    segOffset += skip + sizeof(pint_t);
                }
                break;
            default:
                _diagnostics.error("unknown rebase opcode 0x%02X in %s", opcode, _dylibID);
                done = true;
                break;
        }
    }
}


template <typename P>
void Adjustor<P>::adjustInstruction(uint8_t kind, uint8_t* textLoc, uint64_t codeToDataDelta)
{
    uint32_t* fixupLoc32 = (uint32_t*)textLoc;
    uint64_t* fixupLoc64 = (uint64_t*)textLoc;
    uint32_t instruction;
    uint32_t value32;
    uint64_t value64;

    switch (kind) {
    case 1:    // 32-bit pointer (including x86_64 RIP-rel)
        value32 = P::E::get32(*fixupLoc32);
        value32 += codeToDataDelta;
        P::E::set32(*fixupLoc32, value32);
        break;
    case 2: // 64-bit pointer
        value64 =  P::E::get64(*fixupLoc64);
        value64 += codeToDataDelta;
        P::E::set64(*fixupLoc64, value64);
        break;
    case 4:    // only used for i386, a reference to something in the IMPORT segment
        break;
    case 5: // used by thumb2 movw
        instruction = P::E::get32(*fixupLoc32);
        // slide is always a multiple of 4096, so only top 4 bits of lo16 will ever need adjusting
        value32 = (instruction & 0x0000000F) + ((uint32_t)codeToDataDelta >> 12);
        instruction = (instruction & 0xFFFFFFF0) | (value32 & 0x0000000F);
        P::E::set32(*fixupLoc32, instruction);
        break;
    case 6: // used by ARM movw
        instruction = P::E::get32(*fixupLoc32);
        // slide is always a multiple of 4096, so only top 4 bits of lo16 will ever need adjusting
        value32 = ((instruction & 0x000F0000) >> 16) + ((uint32_t)codeToDataDelta >> 12);
        instruction = (instruction & 0xFFF0FFFF) | ((value32 <<16) & 0x000F0000);
        P::E::set32(*fixupLoc32, instruction);
        break;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
        // used by thumb2 movt (low nibble of kind is high 4-bits of paired movw)
        {
            instruction = P::E::get32(*fixupLoc32);
            assert((instruction & 0x8000FBF0) == 0x0000F2C0);
            // extract 16-bit value from instruction
            uint32_t i     = ((instruction & 0x00000400) >> 10);
            uint32_t imm4  =  (instruction & 0x0000000F);
            uint32_t imm3  = ((instruction & 0x70000000) >> 28);
            uint32_t imm8  = ((instruction & 0x00FF0000) >> 16);
            uint32_t imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
            // combine with codeToDataDelta and kind nibble
            uint32_t targetValue = (imm16 << 16) | ((kind & 0xF) << 12);
            uint32_t newTargetValue = targetValue + (uint32_t)codeToDataDelta;
            // construct new bits slices
            uint32_t imm4_    = (newTargetValue & 0xF0000000) >> 28;
            uint32_t i_       = (newTargetValue & 0x08000000) >> 27;
            uint32_t imm3_    = (newTargetValue & 0x07000000) >> 24;
            uint32_t imm8_    = (newTargetValue & 0x00FF0000) >> 16;
            // update instruction to match codeToDataDelta 
            uint32_t newInstruction = (instruction & 0x8F00FBF0) | imm4_ | (i_ << 10) | (imm3_ << 28) | (imm8_ << 16);
            P::E::set32(*fixupLoc32, newInstruction);
        }
        break;
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
        // used by arm movt (low nibble of kind is high 4-bits of paired movw)
        {
            instruction = P::E::get32(*fixupLoc32);
            // extract 16-bit value from instruction
            uint32_t imm4 = ((instruction & 0x000F0000) >> 16);
            uint32_t imm12 = (instruction & 0x00000FFF);
            uint32_t imm16 = (imm4 << 12) | imm12;
            // combine with codeToDataDelta and kind nibble
            uint32_t targetValue = (imm16 << 16) | ((kind & 0xF) << 12);
            uint32_t newTargetValue = targetValue + (uint32_t)codeToDataDelta;
            // construct new bits slices
            uint32_t imm4_  = (newTargetValue & 0xF0000000) >> 28;
            uint32_t imm12_ = (newTargetValue & 0x0FFF0000) >> 16;
            // update instruction to match codeToDataDelta 
            uint32_t newInstruction = (instruction & 0xFFF0F000) | (imm4_ << 16) | imm12_;
            P::E::set32(*fixupLoc32, newInstruction);
        }
        break;
    case 3: // used for arm64 ADRP
        instruction = P::E::get32(*fixupLoc32);
        if ( (instruction & 0x9F000000) == 0x90000000 ) {
            // codeToDataDelta is always a multiple of 4096, so only top 4 bits of lo16 will ever need adjusting
            value64 = ((instruction & 0x60000000) >> 17) | ((instruction & 0x00FFFFE0) << 9);
            value64 += codeToDataDelta;
            instruction = (instruction & 0x9F00001F) | ((value64 << 17) & 0x60000000) | ((value64 >> 9) & 0x00FFFFE0);
            P::E::set32(*fixupLoc32, instruction);
        }
        break;
    default:
        break;
    }
}

template <typename P>
void Adjustor<P>::adjustCode()
{
    // find compressed info on how code needs to be updated
    if ( _splitSegInfoCmd == nullptr )
        return;

    const uint8_t* infoStart = &_linkeditBias[_splitSegInfoCmd->dataoff];
    const uint8_t* infoEnd = &infoStart[_splitSegInfoCmd->datasize];;

    // This encoding only works if all data segments slide by the same amount
    uint64_t codeToDataDelta = _segSlides[1] - _segSlides[0];

    // compressed data is:  [ <kind> [uleb128-delta]+ <0> ] + <0>
    for (const uint8_t* p = infoStart; (*p != 0) && (p < infoEnd);) {
        uint8_t kind = *p++;
        uint8_t* textLoc = (uint8_t*)_mappingInfo[0].dstSegment;
        while (uint64_t delta = read_uleb128(p, infoEnd)) {
            textLoc += delta;
            adjustInstruction(kind, textLoc, codeToDataDelta);
        }
    }
}


template <typename P>
void Adjustor<P>::adjustExportsTrie(std::vector<uint8_t>& newTrieBytes)
{
    // if no export info, nothing to adjust
    uint32_t exportOffset   = 0;
    uint32_t exportSize     = 0;
    if ( _dyldInfo != nullptr ) {
        exportOffset = _dyldInfo->export_off;
        exportSize = _dyldInfo->export_size;
    } else if (_exportTrieCmd != nullptr) {
        exportOffset = _exportTrieCmd->dataoff;
        exportSize = _exportTrieCmd->datasize;
    }

    if ( exportSize == 0 )
        return;

    // since export info addresses are offsets from mach_header, everything in __TEXT is fine
    // only __DATA addresses need to be updated
    const uint8_t* start = &_linkeditBias[exportOffset];
    const uint8_t* end = &start[exportSize];
    std::vector<ExportInfoTrie::Entry> originalExports;
    if ( !ExportInfoTrie::parseTrie(start, end, originalExports) ) {
        _diagnostics.error("malformed exports trie in %s", _dylibID);
        return;
    }

    std::vector<ExportInfoTrie::Entry> newExports;
    newExports.reserve(originalExports.size());
    uint64_t baseAddress = _segOrigStartAddresses[0];
    uint64_t baseAddressSlide = slideForOrigAddress(baseAddress);
    for (auto& entry:  originalExports) {
        // remove symbols used by the static linker only
        if (   (strncmp(entry.name.c_str(), "$ld$", 4) == 0)
            || (strncmp(entry.name.c_str(), ".objc_class_name",16) == 0)
            || (strncmp(entry.name.c_str(), ".objc_category_name",19) == 0) ) {
            continue;
        }
        // adjust symbols in slid segments
        if ( (entry.info.flags & EXPORT_SYMBOL_FLAGS_KIND_MASK) != EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE )
            entry.info.address += (slideForOrigAddress(entry.info.address + baseAddress) - baseAddressSlide);
        newExports.push_back(entry);
    }

    // rebuild export trie
    newTrieBytes.reserve(exportSize);
    
    ExportInfoTrie(newExports).emit(newTrieBytes);
    // align
    while ( (newTrieBytes.size() % sizeof(pint_t)) != 0 )
        newTrieBytes.push_back(0);
}


} // anonymous namespace

void CacheBuilder::adjustDylibSegments(const DylibInfo& dylib, Diagnostics& diag,
                                       uint64_t cacheBaseAddress,
                                       CacheBuilder::ASLR_Tracker& aslrTracker,
                                       CacheBuilder::LOH_Tracker* lohTracker,
                                       const CacheBuilder::CacheCoalescedText* coalescedText) const
{
    
    dyld3::MachOAnalyzer* mh = (dyld3::MachOAnalyzer*)dylib.cacheLocation[0].dstSegment;
    if ( _is64 ) {
        Adjustor<Pointer64<LittleEndian>> adjustor64(cacheBaseAddress,
                                                     mh,
                                                     dylib.dylibID.c_str(),
                                                     dylib.cacheLocation, diag);
        adjustor64.adjustImageForNewSegmentLocations(aslrTracker, lohTracker, coalescedText, dylib.textCoalescer);
    }
    else {
        Adjustor<Pointer32<LittleEndian>> adjustor32(cacheBaseAddress,
                                                     mh,
                                                     dylib.dylibID.c_str(),
                                                     dylib.cacheLocation, diag);
        adjustor32.adjustImageForNewSegmentLocations(aslrTracker, lohTracker, coalescedText, dylib.textCoalescer);
    }
}






