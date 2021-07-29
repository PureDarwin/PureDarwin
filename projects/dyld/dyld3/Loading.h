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



#ifndef __DYLD_LOADING_H__
#define __DYLD_LOADING_H__

#include <string.h>
#include <stdint.h>
#include <mach/mach.h>
#include <_simple.h>

#include "Closure.h"
#include "MachOLoaded.h"
#include "MachOAnalyzerSet.h"

namespace objc_opt {
struct objc_clsopt_t;
struct objc_selopt_t;
}

namespace dyld3 {

class RootsChecker;

struct LaunchErrorInfo
{
    uintptr_t       kind;
    const char*     clientOfDylibPath;
    const char*     targetDylibPath;
    const char*     symbol;
};

//
// Tuple of info about a loaded image. Contains the loaded address, Image*, and state.
//
class VIS_HIDDEN LoadedImage {
public:
    enum class State { unmapped=0, mapped=1, fixedUp=2, beingInited=3, inited=4 };

    static LoadedImage      make(const closure::Image* img)         { LoadedImage result; result._image = img; return result; }
    static LoadedImage      make(const closure::Image* img, const MachOLoaded* mh)
                                                                    { LoadedImage result; result._image = img; result.setLoadedAddress(mh); return result; }

    const closure::Image*   image() const                           { return _image; }
    const MachOLoaded*      loadedAddress() const                   { return (MachOLoaded*)(_loadAddr & (-4096)); }
    void                    setLoadedAddress(const MachOLoaded* a)  { _loadAddr |= ((uintptr_t)a & (-4096)); }
    State                   state() const                           { return (State)(asBits().state); }
    void                    setState(State s)                       { asBits().state = (int)s; }
    bool                    hideFromFlatSearch() const              { return asBits().hide; }
    void                    setHideFromFlatSearch(bool h)           { asBits().hide = h; }
    bool                    leaveMapped() const                     { return asBits().leaveMapped; }
    void                    markLeaveMapped()                       { asBits().leaveMapped = true; }

private:
    // since loaded MachO files are always page aligned, that means at least low 12-bits are always zero
    // so we don't need to record the low 12 bits, instead those bits hold various flags in the _loadeAddr field
    struct AddrBits {
        uintptr_t    state       :  3,
                     hide        :  1,
                     leaveMapped :  1,
                     extra       :  7,
          #if __LP64__
                     addr        : 52;
          #else
                     addr        : 20;
          #endif
    };
    AddrBits&               asBits()       { return *((AddrBits*)&_loadAddr); }
    const AddrBits&         asBits() const { return *((AddrBits*)&_loadAddr); }

    // Note: this must be statically initializable so as to not cause static initializers
    const closure::Image*   _image      = nullptr;
    uintptr_t               _loadAddr   = 0;        // really AddrBits
};


//
// Utility class to recursively load dependents
//
class VIS_HIDDEN Loader : public MachOAnalyzerSet {
public:
        typedef bool (*LogFunc)(const char*, ...) __attribute__((format(printf, 1, 2)));

                        Loader(const Array<LoadedImage>& existingImages, Array<LoadedImage>& newImagesStorage,
                               const void* cacheAddress, const Array<const dyld3::closure::ImageArray*>& imagesArrays,
                               const closure::ObjCSelectorOpt* selOpt, const Array<closure::Image::ObjCSelectorImage>& selImages,
                               const RootsChecker& rootsChecker, dyld3::Platform platform,
                               LogFunc log_loads, LogFunc log_segments, LogFunc log_fixups, LogFunc log_dofs,
                               bool allowMissingLazies=false, dyld3::LaunchErrorInfo* launchErrorInfo=nullptr);

    void                addImage(const LoadedImage&);
    void                completeAllDependents(Diagnostics& diag, bool& someCacheImageOverridden);
    void                mapAndFixupAllImages(Diagnostics& diag, bool processDOFs, bool fromOFI, bool* closureOutOfDate, bool* recoverable);
    uintptr_t           resolveTarget(closure::Image::ResolvedSymbolTarget target);
    LoadedImage*        findImage(closure::ImageNum targetImageNum) const;
    void                forEachImage(void (^handler)(const LoadedImage& li, bool& stop)) const;

    static void         unmapImage(LoadedImage& info);
    static bool         dtraceUserProbesEnabled();
    static void         vmAccountingSetSuspended(bool suspend, LogFunc);

private:

    struct ImageOverride
    {
        closure::ImageNum  inCache;
        closure::ImageNum  replacement;
    };

    struct DOFInfo {
        const void*            dof;
        const mach_header*     imageHeader;
        const char*            imageShortName;
    };

#if BUILDING_DYLD
    struct LaunchImagesCache {
        LoadedImage*                    findImage(closure::ImageNum targetImageNum,
                                                  Array<LoadedImage>& images) const;
        void                            tryAddImage(closure::ImageNum targetImageNum, uint64_t allImagesIndex) const;

        static const uint64_t           _cacheSize = 128;
        static const closure::ImageNum  _firstImageNum = closure::kFirstLaunchClosureImageNum;
        static const closure::ImageNum  _lastImageNum = closure::kFirstLaunchClosureImageNum + _cacheSize;

        // Note, the cache stores "indices + 1" into the _allImages array.
        // 0 means we haven't cached an entry yet
        uint32_t                        _cacheStorage[_cacheSize] = { 0 };
        mutable Array<uint32_t>         _imageIndices = { &_cacheStorage[0], _cacheSize, _cacheSize };
    };
#endif

    void                mapImage(Diagnostics& diag, LoadedImage& info, bool fromOFI, bool* closureOutOfDate);
    void                applyFixupsToImage(Diagnostics& diag, LoadedImage& info);
    void                registerDOFs(const Array<DOFInfo>& dofs);
    void                setSegmentProtects(const LoadedImage& info, bool write);
	bool                sandboxBlockedMmap(const char* path);
    bool                sandboxBlockedOpen(const char* path);
    bool                sandboxBlockedStat(const char* path);
    bool                sandboxBlocked(const char* path, const char* kind);
    void                unmapAllImages();

    // MachOAnalyzerSet support
    void            mas_forEachImage(void (^handler)(const WrappedMachO& anImage, bool hidden, bool& stop)) const override;
    void            mas_mainExecutable(WrappedMachO& anImage) const override;
    void*           mas_dyldCache() const override;
    bool            wmo_dependent(const WrappedMachO* image, uint32_t depIndex, WrappedMachO& childObj, bool& missingWeakDylib) const override;
    const char*     wmo_path(const WrappedMachO* image) const override;
    bool            wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const override;


    const Array<LoadedImage>&                       _existingImages;
    Array<LoadedImage>&                             _newImages;
    const Array<const closure::ImageArray*>&        _imagesArrays;
    const void*                                     _dyldCacheAddress;
    const objc_opt::objc_selopt_t*                  _dyldCacheSelectorOpt;
    const closure::ObjCSelectorOpt*                 _closureSelectorOpt;
    const Array<closure::Image::ObjCSelectorImage>& _closureSelectorImages;
    const RootsChecker&                             _rootsChecker;
#if BUILDING_DYLD
    LaunchImagesCache                               _launchImagesCache;
#endif
    bool                                            _allowMissingLazies;
    dyld3::Platform                                 _platform;
    LogFunc                                         _logLoads;
    LogFunc                                         _logSegments;
    LogFunc                                         _logFixups;
    LogFunc                                         _logDofs;
    dyld3::LaunchErrorInfo*                         _launchErrorInfo;
};



#if (BUILDING_LIBDYLD || BUILDING_DYLD)
bool internalInstall();
#endif
#if BUILDING_DYLD
void forEachLineInFile(const char* path, void (^lineHandler)(const char* line, bool& stop));
void forEachLineInFile(const char* buffer, size_t bufferLen, void (^lineHandler)(const char* line, bool& stop));
#endif

} // namespace dyld3

#endif // __DYLD_LOADING_H__


