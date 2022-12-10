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


#ifndef ClosureBuilder_h
#define ClosureBuilder_h


#include "Closure.h"
#include "ClosureFileSystem.h"
#include "ClosureWriter.h"
#include "PathOverrides.h"
#include "DyldSharedCache.h"
#include "MachOAnalyzer.h"
#include "MachOAnalyzerSet.h"
#include "Map.h"
#include "Loading.h"

#include <optional>

namespace objc_opt {
struct objc_clsopt_t;
struct objc_selopt_t;
struct objc_protocolopt2_t;
}

typedef dyld3::MachOAnalyzerSet::WrappedMachO   WrappedMachO;

namespace dyld3 {

class RootsChecker;

namespace closure {

class ClosureAnalyzerSet;

class VIS_HIDDEN ClosureBuilder : public dyld3::MachOAnalyzerSet
{
public:

    struct ResolvedTargetInfo
    {
        const MachOLoaded* foundInDylib;
        const char*        requestedSymbolName;
        const char*        foundSymbolName;
        uint64_t           addend;
        bool               weakBindCoalese;
        bool               weakBindSameImage;
        bool               isWeakDef;
        bool               skippableWeakDef;
        int                libOrdinal;
    };

    typedef void (^DylibFixupHandler)(const MachOLoaded* fixupIn, uint64_t fixupLocRuntimeOffset, PointerMetaData pmd, const MachOAnalyzerSet::FixupTarget& target);

    enum class AtPath { none, all, onlyInRPaths };

                                ClosureBuilder(uint32_t startImageNum, const FileSystem& fileSystem, const RootsChecker& rootsChecker,
                                               const DyldSharedCache* dyldCache, bool dyldCacheIsLive,
                                               const GradedArchs& archs, const PathOverrides& pathOverrides,
                                               AtPath atPathHandling=AtPath::all, bool allowRelativePaths=true, LaunchErrorInfo* errorInfo=nullptr,
                                               Platform platform=MachOFile::currentPlatform(), DylibFixupHandler dylibFixupHandler=nullptr);
                                ~ClosureBuilder();
    Diagnostics&                diagnostics() { return _diag; }

    const LaunchClosure*        makeLaunchClosure(const LoadedFileInfo& fileInfo, bool allowInsertFailures);

    const LaunchClosure*        makeLaunchClosure(const char* mainPath,bool allowInsertFailures);
    void                        makeMinimalClosures() { _makeMinimalClosure = true; }
    void                        setCanSkipEncodingRebases() { _leaveRebasesAsOpcodes = true; }

    static const DlopenClosure* sRetryDlopenClosure;
    const DlopenClosure*        makeDlopenClosure(const char* dylibPath, const LaunchClosure* mainClosure, const Array<LoadedImage>& loadedList,
                                                  closure::ImageNum callerImageNum, bool noLoad, bool forceBindLazies, bool canUseSharedCacheClosure,
                                                  closure::ImageNum* topImageNum);

    ImageNum                    nextFreeImageNum() const { return _startImageNum + _nextIndex; }
    Platform                    platform() const { return _platform; }
    
    void                        setDyldCacheInvalidFormatVersion();
    void                        disableInterposing() { _interposingDisabled = true; }


    struct PatchableExport
    {
        uint32_t                                cacheOffsetOfImpl;
        uint32_t                                cacheOffsetOfName;
        uint32_t                                patchLocationsCount;
        const dyld_cache_patchable_location*    patchLocations;
    };

    struct CachedDylibInfo
    {
        LoadedFileInfo          fileInfo;
    };

    struct CachedDylibAlias
    {
        const char*             realPath;
        const char*             aliasPath;
    };

    const ImageArray*           makeDyldCacheImageArray(const Array<CachedDylibInfo>& dylibs, const Array<CachedDylibAlias>& aliases);

    const ImageArray*           makeOtherDylibsImageArray(const Array<LoadedFileInfo>& otherDylibs, uint32_t cachedDylibsCount);

    static void                 buildLoadOrder(Array<LoadedImage>& loadedList, const Array<const ImageArray*>& imagesArrays, const Closure* toAdd);

private:
    friend ClosureAnalyzerSet;

    struct InitInfo
    {
        uint32_t   initOrder       : 30,
                   danglingUpward  :  1,
                   visited         :  1;
    };

    struct BuilderLoadedImage
    {
        Array<Image::LinkedImage>   dependents;
        ImageNum                    imageNum;
        uint32_t                    unmapWhenDone           : 1,
                                    contentRebased          : 1,
                                    hasInits                : 1,
                                    markNeverUnload         : 1,
                                    rtldLocal               : 1,
                                    isBadImage              : 1,
                                    mustBuildClosure        : 1,
                                    hasMissingWeakImports   : 1,
                                    hasInterposingTuples    : 1,
                                    padding                 : 11,
                                    overrideImageNum        : 12;
        uint32_t                    exportsTrieOffset;
        uint32_t                    exportsTrieSize;
        LoadedFileInfo              loadedFileInfo;

        // Convenience method to get the information from the loadedFileInfo
        const MachOAnalyzer*        loadAddress() const { return (const MachOAnalyzer*)loadedFileInfo.fileContent; }
        const char*                 path() const { return loadedFileInfo.path; }
    };

    struct LoadedImageChain
    {
        LoadedImageChain*    previous;
        BuilderLoadedImage&  image;
    };

    // Represents how the current image is linked
    enum class LinkageType {
        kStatic,    // Linked via LC_LOAD_* by main executable or other dylib
        kDynamic,   // Only used for the image we dlopen'ed, not anything it links
        kInserted   // This is an inserted library
    };

    typedef LaunchClosure::SkippedFile SkippedFile;


    void                    recursiveLoadDependents(LoadedImageChain& forImageChain, bool canUseSharedCacheClosure = true);
    void                    loadDanglingUpwardLinks(bool canUseSharedCacheClosure = true);
    void                    forEachResolvedPathVar(const char* loadPath, const LoadedImageChain& forImageChain, bool implictRPath, LinkageType linkageType,
                                                   void (^handler)(const char* possiblePath, bool& stop));
    bool                    findImage(const char* loadPath, const LoadedImageChain& forImageChain, BuilderLoadedImage*& foundImage, LinkageType linkageType,
                                      uint32_t compatVersion, bool canUseSharedCacheClosure);
    void                    buildImage(ImageWriter& writer, BuilderLoadedImage& forImage);
    void                    addSegments(ImageWriter& writer, const MachOAnalyzer* mh);
    void                    addFixupInfo(ImageWriter& writer, BuilderLoadedImage& forImage);
    void                    addChainedFixupInfo(ImageWriter& writer, BuilderLoadedImage& forImage);
    void                    addInterposingTuples(LaunchClosureWriter& writer, const Image* image, const MachOAnalyzer* mh);
    void                    computeInitOrder(ImageWriter& writer, uint32_t loadIndex);
    void                    addClosureInfo(LaunchClosureWriter& closureWriter);
    void                    depthFirstRecurseSetInitInfo(uint32_t loadIndex, InitInfo initInfos[], uint32_t& initOrder, bool& hasError);
    const MachOAnalyzer*    machOForImageNum(ImageNum imageNum);
    ImageNum                imageNumForMachO(const MachOAnalyzer* mh);
    const MachOAnalyzer*    findDependent(const MachOLoaded* mh, uint32_t depIndex);
    BuilderLoadedImage&     findLoadedImage(ImageNum imageNum);
    const BuilderLoadedImage& findLoadedImage(ImageNum imageNum) const;
    BuilderLoadedImage&     findLoadedImage(const MachOAnalyzer* mh);
    uint32_t                index(const BuilderLoadedImage&);
    bool                    expandAtLoaderPath(const char* loadPath, bool fromLCRPATH, const BuilderLoadedImage& loadedImage, char fixedPath[]);
    bool                    expandAtExecutablePath(const char* loadPath, bool fromLCRPATH, bool fromLCRPATHinMain, char fixedPath[]);
    void                    addMustBeMissingPath(const char* path);
    void                    addSkippedFile(const char* path, uint64_t inode, uint64_t mtime);
    const char*             strdup_temp(const char* path) const;
    bool                    overridableDylib(const BuilderLoadedImage& forImage);
    void                    addOperatorCachePatches(BuilderLoadedImage& forImage);
    void                    addWeakDefCachePatch(uint32_t cachedDylibIndex, uint32_t exportCacheOffset, const FixupTarget& patchTarget);

    struct HashCString {
        static size_t hash(const char* v);
    };

    struct EqualCString {
        static bool equal(const char* s1, const char* s2);
    };

    struct HashPointer {
        template<typename T>
        static size_t hash(const T* v) {
            return std::hash<const T*>{}(v);
        }
    };

    struct EqualPointer {
        template<typename T>
        static bool equal(const T* s1, const T* s2) {
            return s1 == s2;
        }
    };

    struct ObjCOptimizerImage {

        ObjCOptimizerImage() {
        }

        ~ObjCOptimizerImage() {
        }

        typedef std::pair<closure::Image::ObjCClassNameImageOffset, closure::Image::ObjCClassImageOffset>   SeenClass;

        struct SelectorFixup {
            uint32_t    fixupVMOffset;
            bool        isSharedCache;
            union {
                struct {
                    uint32_t selectorTableIndex;
                } sharedCache;
                struct {
                    const char* selectorString;
                } image;
            };
        };

        BuilderLoadedImage*             loadedImage                 = nullptr;
        ImageWriter*                    writer                      = nullptr;
        uint64_t                        fairplayFileOffsetStart     = 0;
        uint64_t                        fairplayFileOffsetEnd       = 0;
        Diagnostics                     diag;

        // Image info optimisation
        uint64_t                                            objcImageInfoVMOffset;

        // Class and protocol optimisation data structures
        OverflowSafeArray<std::pair<uint64_t, uint64_t>>                                        classesNameAndDataVMOffsets;
        OverflowSafeArray<SeenClass>                                                            seenClasses;
        OverflowSafeArray<uint64_t>                                                             classStableSwiftFixups;
        Map<const char*, dyld3::closure::Image::ObjCDuplicateClass, HashCString, EqualCString>  classSharedCacheDuplicates;
        OverflowSafeArray<SeenClass>                                                            seenProtocols;
        OverflowSafeArray<uint64_t>                                                             protocolISAFixups;

        // Selector optimsation data structures
        OverflowSafeArray<SelectorFixup>                                                    selectorFixups;
        Map<const char*, dyld3::closure::Image::ObjCImageOffset, HashCString, EqualCString> selectorMap;
        std::optional<uint64_t>                                                             methodNameVMOffset;
        OverflowSafeArray<uint64_t>                                                         methodListFixups;
    };

    bool                    optimizeObjC(Array<ImageWriter>& writers);
    void                    optimizeObjCSelectors(const objc_opt::objc_selopt_t* objcSelOpt,
                                                  const Map<const char*, dyld3::closure::Image::ObjCImageOffset, HashCString, EqualCString>& closureSelectorMap,
                                                  ObjCOptimizerImage& image);
    void                    optimizeObjCClasses(const objc_opt::objc_clsopt_t* objcClassOpt,
                                                const Map<const dyld3::MachOAnalyzer*, bool, HashPointer, EqualPointer>& sharedCacheImagesMap,
                                                const Map<const char*, dyld3::closure::Image::ObjCDuplicateClass, HashCString, EqualCString>& duplicateSharedCacheClasses,
                                                const Map<const char*, bool, HashCString, EqualCString>& duplicateClassesToIgnore,
                                                ObjCOptimizerImage& image);
    void                    optimizeObjCProtocols(const objc_opt::objc_protocolopt2_t* objcProtocolOpt,
                                                  const Map<const dyld3::MachOAnalyzer*, bool, HashPointer, EqualPointer>& sharedCacheImagesMap,
                                                  ObjCOptimizerImage& image);
    void                    writeClassOrProtocolHashTable(bool classes, Array<ObjCOptimizerImage>& objcImages);

    void                    addDuplicateObjCClassWarning(const char* className,
                                                         const char* duplicateDefinitionPath,
                                                         const char* canonicalDefinitionPath);

    void                    parseObjCClassDuplicates(Map<const char*, bool, HashCString, EqualCString>& duplicateClassesToIgnore);

    static bool             inLoadedImageArray(const Array<LoadedImage>& loadedList, ImageNum imageNum);
    static void             buildLoadOrderRecurse(Array<LoadedImage>& loadedList, const Array<const ImageArray*>& imagesArrays, const Image* toAdd);
    void                    invalidateInitializerRoots();
    Image::ResolvedSymbolTarget makeResolvedTarget(const FixupTarget& target) const;

    // MachOAnalyzerSet implementations
    void                mas_forEachImage(void (^handler)(const WrappedMachO& anImage, bool hidden, bool& stop)) const override;
    bool                mas_fromImageWeakDefLookup(const WrappedMachO& fromWmo, const char* symbolName, uint64_t addend, CachePatchHandler patcher, FixupTarget& target) const override;
    void                mas_mainExecutable(WrappedMachO& anImage) const override;
    void*               mas_dyldCache() const override;
    bool                wmo_dependent(const WrappedMachO* image, uint32_t depIndex, WrappedMachO& childObj, bool& missingWeakDylib) const override;
    const char*         wmo_path(const WrappedMachO* image) const override;
    ExportsTrie         wmo_getExportsTrie(const WrappedMachO* image) const override;
    bool                wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const override;


    const FileSystem&                       _fileSystem;
    const RootsChecker&                     _rootsChecker;
    const DyldSharedCache* const            _dyldCache;
    const PathOverrides&                    _pathOverrides;
    const GradedArchs&                      _archs;
    Platform const                          _platform;
    uint32_t const                          _startImageNum;
    const ImageArray*                       _dyldImageArray                 = nullptr;
    DylibFixupHandler                       _dylibFixupHandler              = nullptr;
    const Array<CachedDylibAlias>*          _aliases                        = nullptr;
    const AtPath                            _atPathHandling                 = AtPath::none;
    uint32_t                                _mainProgLoadIndex              = 0;
    const char*                             _mainProgLoadPath               = nullptr;
    Diagnostics                             _diag;
    LaunchErrorInfo*                        _launchErrorInfo                = nullptr;
    mutable PathPool*                       _tempPaths                      = nullptr;
    PathPool*                               _mustBeMissingPaths             = nullptr;
    OverflowSafeArray<SkippedFile>          _skippedFiles;
    uint32_t                                _nextIndex                      = 0;
    OverflowSafeArray<BuilderLoadedImage,2048>  _loadedImages;
    OverflowSafeArray<Image::LinkedImage,65536> _dependencies;                  // all dylibs in cache need ~20,000 edges
    OverflowSafeArray<InterposingTuple>     _interposingTuples;
    OverflowSafeArray<Closure::PatchEntry>  _weakDefCacheOverrides;
    OverflowSafeArray<uint8_t>              _objcSelectorsHashTable;
    OverflowSafeArray<Image::ObjCSelectorImage> _objcSelectorsHashTableImages;
    OverflowSafeArray<uint8_t>              _objcClassesHashTable;
    OverflowSafeArray<uint8_t>              _objcProtocolsHashTable;
    OverflowSafeArray<Image::ObjCClassImage> _objcClassesHashTableImages;
    OverflowSafeArray<uint8_t>              _objcClassesDuplicatesHashTable;
    PathPool*                               _objcDuplicateClassWarnings     = nullptr;
    uint32_t                                _alreadyInitedIndex             = 0;
    bool                                    _isLaunchClosure                = false;
    bool                                    _makingDyldCacheImages          = false;
    bool                                    _dyldCacheIsLive                = false;    // means kernel is rebasing dyld cache content being viewed
    bool                                    _makingClosuresInCache          = false;
    bool                                    _allowRelativePaths             = false;
    bool                                    _atPathUsed                     = false;
    bool                                    _interposingTuplesUsed          = false;
    bool                                    _fallbackPathUsed               = false;
    bool                                    _allowMissingLazies             = false;
    bool                                    _dyldCacheInvalidFormatVersion  = false;
    bool                                    _foundNonCachedImage            = false;    // true means we have one or more images from disk we need to build closure(s) for
    bool                                    _foundDyldCacheRoots            = false;    // true if one or more images are roots of the shared cache
    bool                                    _foundMissingLazyBinds          = false;    // true if one or more images having missing lazy binds
    bool                                    _makeMinimalClosure             = false;
    bool                                    _interposingDisabled            = false;
    bool                                    _leaveRebasesAsOpcodes          = false;
    ImageNum                                _libDyldImageNum                = 0;
    ImageNum                                _libSystemImageNum              = 0;
};

class VIS_HIDDEN RebasePatternBuilder
{
public:
            RebasePatternBuilder(OverflowSafeArray<closure::Image::RebasePattern>& entriesStorage, uint64_t ptrSize);
    void    add(uint64_t runtimeOffset);
private:
    OverflowSafeArray<closure::Image::RebasePattern>&   _rebaseEntries;
    uint64_t                                            _lastLocation;
    const uint64_t                                      _ptrSize;

    static const Image::RebasePattern                   _s_maxLeapPattern;
    static const uint64_t                               _s_maxLeapCount;
};


class VIS_HIDDEN BindPatternBuilder
{
public:
            BindPatternBuilder(OverflowSafeArray<closure::Image::BindPattern>& entriesStorage, uint64_t ptrSize);
    void    add(uint64_t runtimeOffset, Image::ResolvedSymbolTarget target, bool weakBindCoalese);
private:
    OverflowSafeArray<closure::Image::BindPattern>&   _bindEntries;
    const uint64_t                                    _ptrSize;
    uint64_t                                          _lastOffset;
    Image::ResolvedSymbolTarget                       _lastTarget;
};


} //  namespace closure
} //  namespace dyld3


#endif /* ClosureBuilder_h */
