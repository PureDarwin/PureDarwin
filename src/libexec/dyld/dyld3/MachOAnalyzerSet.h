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

#ifndef MachOAnalyzerSet_h
#define MachOAnalyzerSet_h


#include "MachOAnalyzer.h"
#include "Array.h"


namespace dyld3 {

//
// MachOAnalyzerSet is an abstraction to deal with sets of mach-o files. For instance,
// if a mach-o file A binds to a symbol in mach-o file B, the MachOAnalyzerSet lets you
// evaulate the bind such that you know where in B, the bind pointer in A needs to point.
//
// The goal of MachOAnalyzerSet is to be the one place for code that handles mach-o
// file interactions, such as two-level namespace binding, weak-def coalescing, and
// dyld cache patching.
//
// Conceptually, MachOAnalyzerSet is an ordered list of mach-o files. Each file is modeled
// as an WrappedMachO object.  This is a lightweight POD struct of three pointers that
// can be copied around.  A WrappedMachO consists of the MachOAnalyzer* that is represents,
// a pointer to the MachOAnalyzerSet it is in, and an abstract "other" pointer that the
// concrete implementation of MachOAnalyzerSet defines.  All uses of mach-o files in
// the MachOAnalyzerSet method uses WrappedMachO types.
//
//      // This is the key method on WrappedMachO. It is called during closure building to
//      // compile down the fixups, as well as at runtime in dyld3 minimal closure mode
//      // to parse LINKEDIT and rebase/bind pointers.
//      void   forEachFixup(Diagnostics& diag, FixUpHandler, CachePatchHandler) const;
//
//
// It would have been nice to have virtual methods on WrappedMachO, but C++ won't allow
// objects with vtables to be copied.  So instead the methods on WrappedMachO simply
// forward to virtual methods in the owning MachOAnalyzerSet object.  Therefore, there
// are two kinds of methods on MachOAnalyzerSet: methods for a WrappedMachO start with wmo_,
// whereas methods for the whole set start with mas_.
//
//      // Walk all images in the set in order.  "hidden" means the image was loaded with RTLD_LOCAL
//      void    mas_forEachImage(void (^handler)(const WrappedMachO& wmo, bool hidden, bool& stop)) const = 0;
//
//      // fills in mainWmo with the WrappedMachO for the main executable in the set
//      void    mas_mainExecutable(WrappedMachO& mainWmo) const = 0;
//
//      // returns a pointer to the start of the dyld cache used the mach-o files in the set
//      void*   mas_dyldCache() const = 0;
//
//      // For weak-def coalescing. The file fromWmo needs to bind to symbolName.  All files with weak-defs should be searched.
//       // As a side effect of doing this binding, it may find that the symbol is bound overrides something in the dyld cache.
//      // In that case, the CachePatchHandler function is called with info about how to patch the dyld cache.
//      // This function has a default implementation.  Only the dyld cache builder overrides this, because the set is all the
//      // dylibs in the dyld cache, and coalescing should only look at files actually linked.
//      bool    mas_fromImageWeakDefLookup(const WrappedMachO& fromWmo, const char* symbolName, uint64_t addend,
//                                         CachePatchHandler patcher, FixupTarget& target) const;
//
//
//      // For a given WrappedMachO (fromWmo), find the nth dependent dylib.  If depIndex is out of range, return false.
//      // If child is weak-linked dylib that could not be loaded, set missingWeakDylib to true and return true.
//      // Otherwise fill in childWmo and return true
//      bool        wmo_dependent(const WrappedMachO* fromWmo, uint32_t depIndex, WrappedMachO& childWmo, bool& missingWeakDylib) const = 0;
//
//      // Returns the path to the specified WrappedMachO
//      const char* wmo_path(const WrappedMachO* wmo) const = 0;
//
//      // Called if a symbol cannot be found.  If false is returned, then the symbol binding code will return an error.
//      // If true is returned, then "target" must be set.  It may be set to "NULL" as absolute/0.
//      bool        wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName,
//                                            const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const = 0;
//
//      // Returns the exports trie for the given binary. There is a default implementation which walks the load commands.
//      // This should only be overriden if the MachOAnalyzerSet caches the export trie location.
//      ExportsTrie wmo_getExportsTrie(const WrappedMachO* wmo) const;
//
//      // This handles special symbols like C++ operator new which can exist in the main executable as non-weak but
//      // coalesce with other weak implementations. It does not need to be overridden.
//      void        wmo_findExtraSymbolFrom(const WrappedMachO* fromWmo, CachePatchHandler ph) const;
//
//      // This is core logic for two-level namespace symbol look ups.  It does not need to be overridden.
//      bool        wmo_findSymbolFrom(const WrappedMachO* fromWmo, Diagnostics& diag, int libOrdinal, const char* symbolName,
//                                     bool weakImport, bool lazyBind, uint64_t addend, CachePatchHandler ph, FixupTarget& target) const;
//
//

struct VIS_HIDDEN MachOAnalyzerSet
{
public:
    struct FixupTarget;

    struct ExportsTrie { const uint8_t* start; const uint8_t* end; } ;

    // Extra info needed when setting an actual pointer value at runtime
    struct PointerMetaData
    {
                    PointerMetaData();
                    PointerMetaData(const MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, uint16_t pointer_format);

        uint32_t    diversity         : 16,
                    high8             :  8,
                    authenticated     :  1,
                    key               :  2,
                    usesAddrDiversity :  1;
    };

    typedef void (^FixUpHandler)(uint64_t fixupLocRuntimeOffset, PointerMetaData pmd, const FixupTarget& target, bool& stop);
    typedef void (^CachePatchHandler)(uint32_t cachedDylibIndex, uint32_t exportCacheOffset, const FixupTarget& target);

    struct WrappedMachO
    {
        const MachOAnalyzer*           _mh;
        const MachOAnalyzerSet*        _set;
        void*                          _other;

                        WrappedMachO() : _mh(nullptr), _set((nullptr)), _other((nullptr)) { }
                        WrappedMachO(const MachOAnalyzer* ma, const MachOAnalyzerSet* mas, void* o) : _mh(ma), _set(mas), _other(o) { }
                        ~WrappedMachO() {} 

        // Used by: dyld cache building, dyld3s fixup applying, app closure building traditional format, dyldinfo tool
        void            forEachFixup(Diagnostics& diag, FixUpHandler, CachePatchHandler) const;

        // convenience functions
        bool            dependent(uint32_t depIndex, WrappedMachO& childObj, bool& missingWeakDylib) const { return _set->wmo_dependent(this, depIndex, childObj, missingWeakDylib); }
        const char*     path() const { return (_set ? _set->wmo_path(this) : nullptr); }
        ExportsTrie     getExportsTrie() const { return _set->wmo_getExportsTrie(this); }
        bool            findSymbolFrom(Diagnostics& diag, int libOrdinal, const char* symbolName, bool weakImport, bool lazyBind, uint64_t addend, CachePatchHandler ph, FixupTarget& target) const
                                        { return _set->wmo_findSymbolFrom(this, diag, libOrdinal, symbolName, weakImport, lazyBind, addend, ph, target); }
        bool            missingSymbolResolver(bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const
                                        { return _set->wmo_missingSymbolResolver(this, weakImport, lazyBind, symbolName, expectedInDylibPath, clientPath, target); }

        bool            findSymbolIn(Diagnostics& diag, const char* symbolName, uint64_t addend, FixupTarget& target) const;

    private:
        void            forEachBind(Diagnostics& diag, FixUpHandler, CachePatchHandler) const;
    };


    struct FixupTarget
    {
        enum class Kind { rebase, bindToImage, bindAbsolute, bindMissingSymbol };
        WrappedMachO            foundInImage;
        uint64_t                offsetInImage           = 0;     // includes addend
        const char*             requestedSymbolName     = nullptr;
        const char*             foundSymbolName         = nullptr;
        uint64_t                addend                  = 0;     // already added into offsetInImage
        int                     libOrdinal              = 0;
        Kind                    kind                    = Kind::rebase;
        bool                    isLazyBindRebase        = false; // target is stub helper in same image
        bool                    isWeakDef               = false; // target symbol is a weak-def
        bool                    weakCoalesced           = false; // target found searching all images
        bool                    weakBoundSameImage      = false; // first weak-def was in same image as use
        bool                    skippableWeakDef        = false; // old binary that stripped symbol, so def will never be found
    };

    virtual void    mas_forEachImage(void (^handler)(const WrappedMachO& wmo, bool hidden, bool& stop)) const = 0;
    virtual bool    mas_fromImageWeakDefLookup(const WrappedMachO& fromWmo, const char* symbolName, uint64_t addend, CachePatchHandler patcher, FixupTarget& target) const;
    virtual void    mas_mainExecutable(WrappedMachO& mainWmo) const = 0;
    virtual void*   mas_dyldCache() const = 0;


protected:
    friend WrappedMachO;

    virtual bool            wmo_dependent(const WrappedMachO* fromWmo, uint32_t depIndex, WrappedMachO& childWmo, bool& missingWeakDylib) const = 0;
    virtual const char*     wmo_path(const WrappedMachO* wmo) const = 0;
    virtual ExportsTrie     wmo_getExportsTrie(const WrappedMachO* wmo) const;
    virtual bool            wmo_findSymbolFrom(const WrappedMachO* fromWmo, Diagnostics& diag, int libOrdinal, const char* symbolName, bool weakImport,
                                               bool lazyBind, uint64_t addend, CachePatchHandler ph, FixupTarget& target) const;
    virtual bool            wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName,
                                                      const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const = 0;
    virtual void            wmo_findExtraSymbolFrom(const WrappedMachO* fromWmo, CachePatchHandler ph) const;
};



} // namespace dyld3

#endif /* MachOAnalyzerSet_h */
