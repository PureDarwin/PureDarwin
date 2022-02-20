/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
 *
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

#include "kernel_collection_builder.h"

#include "AppCacheBuilder.h"
#include "Diagnostics.h"
#include "ClosureFileSystemNull.h"
#include "MachOAppCache.h"

#include <span>
#include <string>
#include <vector>

static const uint64_t kMinBuildVersion = 1; //The minimum version BuildOptions struct we can support
static const uint64_t kMaxBuildVersion = 1; //The maximum version BuildOptions struct we can support

static const uint32_t MajorVersion = 1;
static const uint32_t MinorVersion = 0;

struct KernelCollectionBuilder {

    KernelCollectionBuilder(const BuildOptions_v1* options);

    struct CollectionFile {
        const uint8_t*  data = nullptr;
        uint64_t        size = 0;
        const char*     path = nullptr;
    };

    struct CacheBuffer {
        uint8_t*    buffer      = nullptr;
        uint64_t    bufferSize  = 0;
    };

    const char*                                 arch = "";
    BuildOptions_v1                             options;
    std::list<Diagnostics>                      inputFileDiags;
    std::vector<AppCacheBuilder::InputDylib>    inputFiles;
    dyld3::closure::LoadedFileInfo              kernelCollectionFileInfo;
    dyld3::closure::LoadedFileInfo              pageableCollectionFileInfo;
    std::vector<AppCacheBuilder::CustomSegment> customSections;
    CFDictionaryRef                             prelinkInfoExtraData = nullptr;

    std::list<CollectionFileResult_v1>          fileResultsStorage;
    std::vector<const CollectionFileResult_v1*> fileResults;
    CFDictionaryRef                             errorsDict           = nullptr;

    std::vector<const char*>    errors;
    std::vector<std::string>    errorStorage;

    std::list<std::string>      duplicatedStrings;
    std::vector<CFTypeRef>      typeRefs;

    __attribute__((format(printf, 2, 3)))
    void error(const char* format, ...) {
        va_list list;
        va_start(list, format);
        Diagnostics diag;
        diag.error(format, list);
        va_end(list);

        errorStorage.push_back(diag.errorMessage());
        errors.push_back(errorStorage.back().data());
    }

    void retain(CFTypeRef v) {
        CFRetain(v);
        typeRefs.push_back(v);
    }

    const char* strdup(CFStringRef str) {
        size_t length = CFStringGetLength(str);
        char buffer[length + 1];
        memset(&buffer[0], 0, length + 1);
        if ( !CFStringGetCString(str, buffer, length + 1, kCFStringEncodingASCII) ) {
            error("Could not convert to ASCII");
            return nullptr;
        }
        duplicatedStrings.push_back(buffer);
        return duplicatedStrings.back().c_str();
    }

};

// What is the version of this builder dylib.  Can be used to determine what API is available.
void getVersion(uint32_t *major, uint32_t *minor) {
    *major = MajorVersion;
    *minor = MinorVersion;
}

KernelCollectionBuilder::KernelCollectionBuilder(const BuildOptions_v1* options)
    : options(*options) {
    retain(this->options.arch);
}

// Returns a valid object on success, or NULL on failure.
__API_AVAILABLE(macos(10.14))
struct KernelCollectionBuilder* createKernelCollectionBuilder(const struct BuildOptions_v1* options) {
    KernelCollectionBuilder* builder = new KernelCollectionBuilder(options);

    if (options->version < kMinBuildVersion) {
        builder->error("Builder version %llu is less than minimum supported version of %llu", options->version, kMinBuildVersion);
        return builder;
    }
    if (options->version > kMaxBuildVersion) {
        builder->error("Builder version %llu is greater than maximum supported version of %llu", options->version, kMaxBuildVersion);
        return builder;
    }
    if ( options->arch == nullptr ) {
        builder->error("arch must not be null");
        return builder;
    }
    const char* archName = builder->strdup(options->arch);
    if ( archName == nullptr ) {
        // Already generated an error in strdup.
        return builder;
    }
    builder->arch = archName;

    return builder;
}

static bool loadFileFromData(struct KernelCollectionBuilder* builder,
                             const CFStringRef path, const CFDataRef data,
                             dyld3::closure::LoadedFileInfo& fileInfo) {
    fileInfo.fileContent        = CFDataGetBytePtr(data);
    fileInfo.fileContentLen     = CFDataGetLength(data);
    fileInfo.sliceOffset        = 0;
    fileInfo.sliceLen           = CFDataGetLength(data);
    fileInfo.isOSBinary         = false;
    fileInfo.inode              = 0;
    fileInfo.mtime              = 0;
    fileInfo.unload             = nullptr;
    fileInfo.path               = builder->strdup(path);

    Diagnostics diag;
    dyld3::closure::FileSystemNull fileSystem;
    const dyld3::GradedArchs& arch = dyld3::GradedArchs::forName(builder->arch);
    auto loaded = dyld3::MachOAnalyzer::loadFromBuffer(diag, fileSystem, fileInfo.path, arch,
                                                       dyld3::Platform::unknown, fileInfo);
    if ( !loaded ) {
        builder->error("%s", diag.errorMessage().c_str());
        return false;
    }

    return true;
}

// Add a kernel static executable file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addKernelFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data) {
    builder->retain(path);
    builder->retain(data);

    dyld3::closure::LoadedFileInfo info;
    bool loaded = loadFileFromData(builder, path, data, info);
    if ( !loaded )
        return false;

    DyldSharedCache::MappedMachO mappedFile(info.path, (const dyld3::MachOAnalyzer *)info.fileContent,
                                            info.fileContentLen, false, false,
                                            info.sliceOffset, info.mtime, info.inode);

    AppCacheBuilder::InputDylib input;
    input.dylib.mappedFile      = mappedFile;
    input.dylib.loadedFileInfo  = info;
    input.dylib.inputFile       = nullptr;
    input.dylibID               = "com.apple.kernel";
    input.errors                = &builder->inputFileDiags.emplace_back();

    builder->inputFiles.push_back(input);
    return true;
}

// Add kext mach-o and plist files.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addKextDataFile(struct KernelCollectionBuilder* builder, const KextFileData_v1* fileData) {
    if (fileData->version != 1) {
        builder->error("addKextDataFile version %llu is less than minimum supported version of %d", fileData->version, 1);
        return false;
    }

    builder->retain(fileData->dependencies);
    builder->retain(fileData->bundleID);
    builder->retain(fileData->bundlePath);
    builder->retain(fileData->plist);

    dyld3::closure::LoadedFileInfo info;

    // Code-less kext's don't have a mach-o to load
    const char* kextpath = nullptr;
    if ( fileData->kextdata != nullptr ) {
        builder->retain(fileData->kextpath);
        builder->retain(fileData->kextdata);
        bool loaded = loadFileFromData(builder, fileData->kextpath, fileData->kextdata, info);
        if ( !loaded )
            return false;
        kextpath = info.path;
    } else {
        kextpath = "codeless";
    }

    DyldSharedCache::MappedMachO mappedFile(kextpath, (const dyld3::MachOAnalyzer *)info.fileContent,
                                            info.fileContentLen, false, false,
                                            info.sliceOffset, info.mtime, info.inode);

    uint64_t numDependencies = CFArrayGetCount(fileData->dependencies);

    AppCacheBuilder::InputDylib input;
    input.dylib.mappedFile      = mappedFile;
    input.dylib.loadedFileInfo  = info;
    input.dylib.inputFile       = nullptr;
    input.dylibID               = builder->strdup(fileData->bundleID);
    for (uint64_t i = 0; i != numDependencies; ++i) {
        CFTypeRef elementRef = CFArrayGetValueAtIndex(fileData->dependencies, i);
        if ( CFGetTypeID(elementRef) != CFStringGetTypeID() ) {
            builder->error("Dependency %llu of %s is not a string", i, info.path);
            return false;
        }
        CFStringRef stringRef = (CFStringRef)elementRef;
        input.dylibDeps.push_back(builder->strdup(stringRef));
    }
    input.infoPlist             = fileData->plist;
    input.errors                = &builder->inputFileDiags.emplace_back();
    input.bundlePath            = builder->strdup(fileData->bundlePath);
    switch ( fileData->stripMode ) {
        case binaryUnknownStripMode:
            input.stripMode     = CacheBuilder::DylibStripMode::stripNone;
            break;
        case binaryStripNone:
            input.stripMode     = CacheBuilder::DylibStripMode::stripNone;
            break;
        case binaryStripExports:
            input.stripMode     = CacheBuilder::DylibStripMode::stripExports;
            break;
        case binaryStripLocals:
            input.stripMode     = CacheBuilder::DylibStripMode::stripLocals;
            break;
        case binaryStripAll:
            input.stripMode     = CacheBuilder::DylibStripMode::stripAll;
            break;
    }

    builder->inputFiles.push_back(input);
    return true;
}

// Add interface plist file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addInterfaceFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data) {
    builder->retain(path);
    builder->retain(data);

    assert(0);
}

// Add collection file.  Returns true on success.
__API_AVAILABLE(macos(10.14))
bool addCollectionFile(struct KernelCollectionBuilder* builder, const CFStringRef path, const CFDataRef data,
                       CollectionKind kind) {
    dyld3::closure::LoadedFileInfo* fileInfo = nullptr;
    if ( kind == baseKC ) {
        if ( (builder->options.collectionKind != auxKC) && (builder->options.collectionKind != pageableKC) ) {
            builder->error("Invalid collection file for build");
            return false;
        }
        fileInfo = &builder->kernelCollectionFileInfo;
    } else if ( kind == pageableKC ) {
        if ( builder->options.collectionKind != auxKC ) {
            builder->error("Invalid collection file for build");
            return false;
        }
        if ( builder->pageableCollectionFileInfo.fileContent != nullptr ) {
            builder->error("Already have collection file");
            return false;
        }
        fileInfo = &builder->pageableCollectionFileInfo;
    } else {
        builder->error("Unsupported collection kind");
        return false;
    }
    if ( fileInfo->fileContent != nullptr ) {
        builder->error("Already have collection file");
        return false;
    }

    builder->retain(path);
    builder->retain(data);

    bool loaded = loadFileFromData(builder, path, data, *fileInfo);
    if ( !loaded )
        return false;

    const dyld3::MachOFile* mf = (const dyld3::MachOFile*)fileInfo->fileContent;
    if ( !mf->isFileSet() ) {
        builder->error("kernel collection is not a cache file: %s\n", builder->strdup(path));
        return false;
    }

    return true;
}

// Add data to the given segment of the final file.  Note the section can be null (or "") if desired
__API_AVAILABLE(macos(10.14))
bool addSegmentData(struct KernelCollectionBuilder* builder, const CFStringRef segmentName,
                    const CFStringRef sectionName, const CFDataRef data) {
    // Check the segment name
    if ( segmentName == nullptr ) {
        builder->error("Segment data name must be non-null");
        return false;
    }
    CFIndex segmentNameLength = CFStringGetLength(segmentName);
    if ( (segmentNameLength == 0) || (segmentNameLength > 16) ) {
        builder->error("Segment data name must not be empty or > 16 characters");
        return false;
    }

    AppCacheBuilder::CustomSegment::CustomSection section;

    // Check the section name
    if ( sectionName != nullptr ) {
        CFIndex sectionNameLength = CFStringGetLength(sectionName);
        if ( sectionNameLength != 0 ) {
            if ( sectionNameLength > 16 ) {
                builder->error("Section data name must not be empty or > 16 characters");
                return false;
            }
            section.sectionName = builder->strdup(sectionName);
        }
    }

    // Check the data
    if ( data == nullptr ) {
        builder->error("Segment data payload must be non-null");
        return false;
    }
    const uint8_t* dataStart = CFDataGetBytePtr(data);
    const uint64_t dataLength = CFDataGetLength(data);
    if ( dataLength == 0 ) {
        builder->error("Segment data payload must not be empty");
        return false;
    }

    builder->retain(data);
    section.data.insert(section.data.end(), dataStart, dataStart + dataLength);

    AppCacheBuilder::CustomSegment segment;
    segment.segmentName = builder->strdup(segmentName);
    segment.sections.push_back(section);
    builder->customSections.push_back(segment);
    return true;
}

// Add data to the given segment of the final file.  Note the section can be null (or "") if desired
__API_AVAILABLE(macos(10.14))
bool addPrelinkInfo(struct KernelCollectionBuilder* builder, const CFDictionaryRef extraData) {
    if ( builder->prelinkInfoExtraData != nullptr ) {
        builder->error("Prelink info data has already been set by an earlier call");
        return false;
    }
    // Check the data
    if ( extraData == nullptr ) {
        builder->error("Prelink info data payload must be non-null");
        return false;
    }
    if ( CFDictionaryGetCount(extraData) == 0 ) {
        builder->error("Prelink info data payload must not be empty");
        return false;
    }
    builder->retain(extraData);
    builder->prelinkInfoExtraData = extraData;
    return true;
}

// Set a handler to be called at various points during the build to notify the user of progress.
__API_AVAILABLE(macos(10.14))
void setProgressCallback(const ProgressCallback callback) {
    assert(0);
}

static AppCacheBuilder::Options::AppCacheKind cacheKind(CollectionKind kind) {
    switch (kind) {
        case unknownKC:
            return AppCacheBuilder::Options::AppCacheKind::none;
        case baseKC:
            return AppCacheBuilder::Options::AppCacheKind::kernel;
        case auxKC:
            return AppCacheBuilder::Options::AppCacheKind::auxKC;
        case pageableKC:
            return AppCacheBuilder::Options::AppCacheKind::pageableKC;
    }
}

static AppCacheBuilder::Options::StripMode stripMode(StripMode mode) {
    switch (mode) {
        case unknownStripMode:
            return AppCacheBuilder::Options::StripMode::none;
        case stripNone:
            return AppCacheBuilder::Options::StripMode::none;
        case stripAll:
            return AppCacheBuilder::Options::StripMode::all;
        case stripAllKexts:
            return AppCacheBuilder::Options::StripMode::allExceptKernel;
    }
}

static void generatePerKextErrors(struct KernelCollectionBuilder* builder) {
    CFMutableDictionaryRef errorsDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr);
    builder->typeRefs.push_back(errorsDict);

    for (const AppCacheBuilder::InputDylib& file : builder->inputFiles) {
        if ( !file.errors->hasError() )
            continue;

        CFStringRef bundleID = CFStringCreateWithCString(kCFAllocatorDefault, file.dylibID.c_str(),
                                                            kCFStringEncodingASCII);
        builder->typeRefs.push_back(bundleID);

        CFMutableArrayRef errorsArray = CFArrayCreateMutable(kCFAllocatorDefault, 1, nullptr);
        builder->typeRefs.push_back(errorsArray);

        CFStringRef errorString = CFStringCreateWithCString(kCFAllocatorDefault, file.errors->errorMessage().c_str(),
                                                            kCFStringEncodingASCII);
        builder->typeRefs.push_back(errorString);

        CFArrayAppendValue(errorsArray, errorString);

        // Add this bundle to the dictionary
        CFDictionarySetValue(errorsDict, bundleID, errorsArray);

        // FIXME: Remove this once the kernel linker has adopted the new API to get the per-kext errors
        // For now also put the per-kext errors on the main error list
        builder->error("Could not use '%s' because: %s", file.dylibID.c_str(), file.errors->errorMessage().c_str());
    }

    if ( CFDictionaryGetCount(errorsDict) != 0 ) {
        builder->errorsDict = errorsDict;
    }
}

// Returns true on success.
__API_AVAILABLE(macos(10.14))
bool runKernelCollectionBuilder(struct KernelCollectionBuilder* builder) {

    // Make sure specificed bundle-id's are not already in the caches we
    // are linking to
    __block std::set<std::string_view> existingBundles;
    if ( (builder->options.collectionKind == auxKC) || (builder->options.collectionKind == pageableKC) ) {
        if ( builder->kernelCollectionFileInfo.fileContent == nullptr ) {
            builder->error("Cannot build pageableKC/auxKC without baseKC");
            return false;
        }
        Diagnostics diag;

        // Check the base KC
        const dyld3::MachOAppCache* kernelCacheMA = (const dyld3::MachOAppCache*)builder->kernelCollectionFileInfo.fileContent;
        kernelCacheMA->forEachDylib(diag, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
            existingBundles.insert(name);
        });

        // Check the pageableKC if we have one
        const dyld3::MachOAppCache* pageableCacheMA = (const dyld3::MachOAppCache*)builder->kernelCollectionFileInfo.fileContent;
        if ( pageableCacheMA != nullptr ) {
            pageableCacheMA->forEachDylib(diag, ^(const dyld3::MachOAnalyzer *ma, const char *name, bool &stop) {
                existingBundles.insert(name);
            });
        }

        bool foundBadBundle = false;
        for (const AppCacheBuilder::InputDylib& input : builder->inputFiles) {
            if ( existingBundles.find(input.dylibID) != existingBundles.end() ) {
                builder->error("kernel collection already contains bundle-id: %s\n", input.dylibID.c_str());
                foundBadBundle = true;
            }
        }
        if ( foundBadBundle )
            return false;
    }

    dispatch_apply(builder->inputFiles.size(), DISPATCH_APPLY_AUTO, ^(size_t index) {
        const AppCacheBuilder::InputDylib& input = builder->inputFiles[index];
        auto errorHandler = ^(const char* msg) {
            input.errors->error("cannot be placed in kernel collection because: %s", msg);
        };
        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)input.dylib.loadedFileInfo.fileContent;
        // Skip codeless kexts
        if ( ma == nullptr )
            return;
        if (!ma->canBePlacedInKernelCollection(input.dylib.loadedFileInfo.path, errorHandler)) {
            assert(input.errors->hasError());
        }
    });
    for (const AppCacheBuilder::InputDylib& input : builder->inputFiles) {
        if ( input.errors->hasError() ) {
            builder->error("One or more binaries has an error which prevented linking.  See other errors.");
            generatePerKextErrors(builder);
            return false;
        }
    }

    DyldSharedCache::CreateOptions builderOptions = {};
    std::string runtimePath = "";
    builderOptions.outputFilePath = runtimePath;
    builderOptions.outputMapFilePath = builderOptions.outputFilePath + ".json";
    builderOptions.archs = &dyld3::GradedArchs::forName(builder->arch);
    builderOptions.platform = dyld3::Platform::unknown;
    builderOptions.localSymbolMode = DyldSharedCache::LocalSymbolsMode::keep;
    builderOptions.optimizeStubs = true;
    builderOptions.optimizeDyldDlopens = false;
    builderOptions.optimizeDyldLaunches = false;
    builderOptions.codeSigningDigestMode = DyldSharedCache::CodeSigningDigestMode::SHA256only;
    builderOptions.dylibsRemovedDuringMastering = true;
    builderOptions.inodesAreSameAsRuntime = false;
    builderOptions.cacheSupportsASLR = true;
    builderOptions.forSimulator = false;
    builderOptions.isLocallyBuiltCache = true;
    builderOptions.verbose = builder->options.verboseDiagnostics;
    builderOptions.evictLeafDylibsOnOverflow = true;
    builderOptions.loggingPrefix = "";
    builderOptions.dylibOrdering = {};
    builderOptions.dirtyDataSegmentOrdering = {};
    builderOptions.objcOptimizations = {};

    AppCacheBuilder::Options appCacheOptions;
    appCacheOptions.cacheKind = cacheKind(builder->options.collectionKind);
    appCacheOptions.stripMode = stripMode(builder->options.stripMode);

    const dyld3::closure::FileSystemNull builderFileSystem;

    AppCacheBuilder cacheBuilder(builderOptions, appCacheOptions, builderFileSystem);
    if ( builder->kernelCollectionFileInfo.fileContent != nullptr ) {
        const dyld3::MachOAppCache* appCacheMA = (const dyld3::MachOAppCache*)builder->kernelCollectionFileInfo.fileContent;
        cacheBuilder.setExistingKernelCollection(appCacheMA);
    }
    if ( builder->pageableCollectionFileInfo.fileContent != nullptr ) {
        const dyld3::MachOAppCache* appCacheMA = (const dyld3::MachOAppCache*)builder->pageableCollectionFileInfo.fileContent;
        cacheBuilder.setExistingPageableKernelCollection(appCacheMA);
    }

    // Add custom sections
    for (const AppCacheBuilder::CustomSegment& segment : builder->customSections) {
        if ( !cacheBuilder.addCustomSection(segment.segmentName, segment.sections.front()) ) {
            builder->error("%s", cacheBuilder.errorMessage().c_str());
            return false;
        }
    }

    // Add prelink info data
    if ( builder->prelinkInfoExtraData != nullptr ) {
        cacheBuilder.setExtraPrelinkInfo(builder->prelinkInfoExtraData);
    }

    cacheBuilder.buildAppCache(builder->inputFiles);
    if ( !cacheBuilder.errorMessage().empty() ) {
        builder->error("%s", cacheBuilder.errorMessage().c_str());
        generatePerKextErrors(builder);
        return false;
    }

    uint8_t* cacheBuffer    = nullptr;
    uint64_t cacheSize      = 0;
    cacheBuilder.writeBuffer(cacheBuffer, cacheSize);

    CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, cacheBuffer, cacheSize, kCFAllocatorDefault);
    builder->retain(dataRef);
    CFRelease(dataRef);

    CFArrayRef warningsArrayRef = CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr);
    builder->retain(warningsArrayRef);
    CFRelease(warningsArrayRef);

    CollectionFileResult_v1 fileResult = { 1, kernelCollection, dataRef, warningsArrayRef };

    builder->fileResultsStorage.push_back(fileResult);
    builder->fileResults.push_back(&builder->fileResultsStorage.back());

    return true;
}

// Gets the list of errors we have produced.  These may be from incorrect input values, or failures in the build itself
__API_AVAILABLE(macos(10.14))
const char* const* getErrors(const struct KernelCollectionBuilder* builder, uint64_t* errorCount) {
    if (builder->errors.empty())
        return nullptr;
    *errorCount = builder->errors.size();
    return builder->errors.data();
}

__API_AVAILABLE(macos(10.14))
CFDictionaryRef getKextErrors(const struct KernelCollectionBuilder* builder) {
    return builder->errorsDict;
}

// Returns an array of the resulting files.  These may be new collections, or other files required later
__API_AVAILABLE(macos(10.14))
const struct CollectionFileResult_v1* const* getCollectionFileResults(struct KernelCollectionBuilder* builder, uint64_t* resultCount) {
    if ( builder->fileResults.empty() ) {
        *resultCount = 0;
        return nullptr;
    }

    *resultCount = builder->fileResults.size();
    return builder->fileResults.data();
}

__API_AVAILABLE(macos(10.14))
void destroyKernelCollectionBuilder(struct KernelCollectionBuilder* builder) {
    for (CFTypeRef ref : builder->typeRefs)
        CFRelease(ref);
    dyld3::closure::FileSystemNull fileSystem;
    for (const AppCacheBuilder::InputDylib& inputFile : builder->inputFiles) {
        fileSystem.unloadFile(inputFile.dylib.loadedFileInfo);
    }
    if ( builder->kernelCollectionFileInfo.fileContent != nullptr) {
        fileSystem.unloadFile(builder->kernelCollectionFileInfo);
    }
    if ( builder->pageableCollectionFileInfo.fileContent != nullptr) {
        fileSystem.unloadFile(builder->pageableCollectionFileInfo);
    }
    delete builder;
}
