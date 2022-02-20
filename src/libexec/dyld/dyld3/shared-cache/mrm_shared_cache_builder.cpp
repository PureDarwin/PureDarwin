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

#include "mrm_shared_cache_builder.h"
#include "SharedCacheBuilder.h"
#include "ClosureFileSystem.h"
#include "FileUtils.h"
#include "JSONReader.h"
#include <pthread.h>
#include <memory>
#include <vector>
#include <map>
#include <sys/stat.h>


static const uint64_t kMinBuildVersion = 1; //The minimum version BuildOptions struct we can support
static const uint64_t kMaxBuildVersion = 2; //The maximum version BuildOptions struct we can support

static const uint32_t MajorVersion = 1;
static const uint32_t MinorVersion = 2;

namespace dyld3 {
namespace closure {

struct FileInfo {
    const char*     path;
    const uint8_t*  data;
    const uint64_t  length;
    FileFlags       flags;
    uint64_t        mtime;
    uint64_t        inode;
};

class FileSystemMRM : public FileSystem {
public:
    FileSystemMRM() : FileSystem() { }

    bool getRealPath(const char possiblePath[MAXPATHLEN], char realPath[MAXPATHLEN]) const override {
        Diagnostics diag;
        std::string resolvedPath = symlinkResolver.realPath(diag, possiblePath);
        if (diag.hasError()) {
            diag.verbose("MRM error: %s\n", diag.errorMessage().c_str());
            diag.clearError();
            return false;
        }

        // FIXME: Should we only return real paths of files which point to macho's?  For now that is what we are doing
        auto it = fileMap.find(resolvedPath);
        if (it == fileMap.end())
            return false;

        memcpy(realPath, resolvedPath.c_str(), std::min((size_t)MAXPATHLEN, resolvedPath.size() + 1));
        return true;
    }

    bool loadFile(const char* path, LoadedFileInfo& info, char realerPath[MAXPATHLEN], void (^error)(const char* format, ...)) const override {
        Diagnostics diag;
        std::string resolvedPath = symlinkResolver.realPath(diag, path);
        if (diag.hasError()) {
            diag.verbose("MRM error: %s\n", diag.errorMessage().c_str());
            diag.clearError();
            return false;
        }

        auto it = fileMap.find(resolvedPath);
        if (it == fileMap.end())
            return false;

        if (resolvedPath == path)
            realerPath[0] = '\0';
        else
            memcpy(realerPath, resolvedPath.c_str(), std::min((size_t)MAXPATHLEN, resolvedPath.size() + 1));

        // The file exists at this exact path.  Lets use it!
        const FileInfo& fileInfo = files[it->second];

        info.fileContent                = fileInfo.data;
        info.fileContentLen             = fileInfo.length;
        info.sliceOffset                = 0;
        info.sliceLen                   = fileInfo.length;
        info.isOSBinary                 = true;
        info.inode                      = fileInfo.inode;
        info.mtime                      = fileInfo.mtime;
        info.unload                     = nullptr;
        info.path                       = path;
        return true;
    }

    void unloadFile(const LoadedFileInfo& info) const override {
        if (info.unload)
            info.unload(info);
    }

    void unloadPartialFile(LoadedFileInfo& info, uint64_t keepStartOffset, uint64_t keepLength) const override {
        // Note we don't actually unload the data here, but we do want to update the offsets for other data structures to track where we are
        info.fileContent = (const void*)((char*)info.fileContent + keepStartOffset);
        info.fileContentLen = keepLength;
    }

    bool fileExists(const char* path, uint64_t* inode=nullptr, uint64_t* mtime=nullptr,
                    bool* issetuid=nullptr, bool* inodesMatchRuntime = nullptr) const override {
        Diagnostics diag;
        std::string resolvedPath = symlinkResolver.realPath(diag, path);
        if (diag.hasError()) {
            diag.verbose("MRM error: %s\n", diag.errorMessage().c_str());
            diag.clearError();
            return false;
        }

        auto it = fileMap.find(resolvedPath);
        if (it == fileMap.end())
            return false;

        // The file exists at this exact path.  Lets use it!
        const FileInfo& fileInfo = files[it->second];
        if (inode)
            *inode = fileInfo.inode;
        if (mtime)
            *mtime = fileInfo.mtime;
        if (issetuid)
            *issetuid = false;
        if (inodesMatchRuntime)
            *inodesMatchRuntime = false;
        return true;
    }

    // MRM file APIs
    bool addFile(const char* path, uint8_t* data, uint64_t size, Diagnostics& diag, FileFlags fileFlags) {
        auto iteratorAndInserted = fileMap.insert(std::make_pair(path, files.size()));
        if (!iteratorAndInserted.second) {
            diag.error("Already have content for path: '%s'", path);
            return false;
        }

        symlinkResolver.addFile(diag, path);
        if (diag.hasError())
            return false;

        // on iOS, inode is just a placeholder
        // Note its safe to just use the index here as we only compare it during closure building
        // and never record it in the closures
        uint64_t inode = files.size() + 1;
        uint64_t mtime = 0;

        files.push_back((FileInfo){ path, data, size, fileFlags, mtime, inode });
        return true;
    }

    bool addSymlink(const char* fromPath, const char* toPath, Diagnostics& diag) {
        symlinkResolver.addSymlink(diag, fromPath, toPath);
        return !diag.hasError();
    }

    void forEachFileInfo(std::function<void(const char* path, FileFlags fileFlags)> lambda) {
        for (const FileInfo& fileInfo : files)
            lambda(fileInfo.path, fileInfo.flags);
    }

    size_t fileCount() const {
        return files.size();
    }

    std::vector<DyldSharedCache::FileAlias> getResolvedSymlinks(Diagnostics& diag) {
        return symlinkResolver.getResolvedSymlinks(diag);
    }

private:
    std::vector<FileInfo> files;
    std::map<std::string, uint64_t> fileMap;
    SymlinkResolver symlinkResolver;
};

} // namespace closure
} // namespace dyld3

struct BuildInstance {
    std::unique_ptr<DyldSharedCache::CreateOptions> options;
    std::unique_ptr<SharedCacheBuilder>             builder;
    std::vector<CacheBuilder::InputFile>            inputFiles;
    std::vector<const char*>                        errors;
    std::vector<const char*>                        warnings;
    std::vector<std::string>                        errorStrings;   // Owns the data for the errors
    std::vector<std::string>                        warningStrings; // Owns the data for the warnings
    uint8_t*                                        cacheData       = nullptr;
    uint64_t                                        cacheSize       = 0;
    std::string                                     jsonMap;
    std::string                                     macOSMap;       // For compatibility with update_dyld_shared_cache's .map file
    std::string                                     macOSMapPath;   // Owns the string for the path
    std::string                                     cdHash;         // Owns the data for the cdHash
    std::string                                     cdHashType;     // Owns the data for the cdHashType
    std::string                                     uuid;           // Owns the data for the uuid
};

struct BuildFileResult {
    std::string                                 path;
    const uint8_t*                              data;
    uint64_t                                    size;
};

struct TranslationResult {
    const uint8_t*   data;
    size_t           size;
    std::string      cdHash;
    std::string      path;
    bool             bufferWasMalloced;
};

struct MRMSharedCacheBuilder {
    MRMSharedCacheBuilder(const BuildOptions_v1* options);
    const BuildOptions_v1*          options;
    dyld3::closure::FileSystemMRM   fileSystem;

    std::string dylibOrderFileData;
    std::string dirtyDataOrderFileData;
    void* objcOptimizationsFileData;
    size_t objcOptimizationsFileLength;

    // An array of builders and their options as we may have more than one builder for a given device variant.
    std::vector<BuildInstance> builders;

    // The paths in all of the caches
    // We keep this here to own the std::string path data
    std::map<std::string, std::unordered_set<const BuildInstance*>> dylibsInCaches;

    // The results from all of the builders
    // We keep this in a vector to own the data.
    std::vector<FileResult*>                     fileResults;
    std::vector<FileResult>                      fileResultStorage;
    std::vector<std::pair<uint64_t, bool>>       fileResultBuffers;

    // The results from all of the builders
    // We keep this in a vector to own the data.
    std::vector<CacheResult*>    cacheResults;
    std::vector<CacheResult>     cacheResultStorage;


    // The files to remove.  These are in every copy of the caches we built
    std::vector<const char*> filesToRemove;

    std::vector<const char*> errors;
    std::vector<std::string> errorStorage;
    pthread_mutex_t lock;

    enum State {
        AcceptingFiles,
        Building,
        FinishedBuilding
    };

    State state = AcceptingFiles;

    void runSync(void (^block)()) {
        pthread_mutex_lock(&lock);
        block();
        pthread_mutex_unlock(&lock);
    }

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
};

MRMSharedCacheBuilder::MRMSharedCacheBuilder(const BuildOptions_v1* options)
: options(options)
, lock(PTHREAD_MUTEX_INITIALIZER)
, objcOptimizationsFileData(nullptr)
, objcOptimizationsFileLength(0)
{

}

void validiateBuildOptions(const BuildOptions_v1* options, MRMSharedCacheBuilder& builder) {
    if (options->version < kMinBuildVersion) {
        builder.error("Builder version %llu is less than minimum supported version of %llu", options->version, kMinBuildVersion);
    }
    if (options->version > kMaxBuildVersion) {
        builder.error("Builder version %llu is greater than maximum supported version of %llu", options->version, kMaxBuildVersion);
    }
    if (!options->updateName) {
        builder.error("updateName must not be null");
    }
    if (!options->deviceName) {
        builder.error("deviceName must not be null");
    }
    switch (options->disposition) {
        case Disposition::Unknown:
        case Disposition::InternalDevelopment:
        case Disposition::Customer:
        case Disposition::InternalMinDevelopment:
            break;
        default:
            builder.error("unknown disposition value");
            break;
    }
    switch (options->platform) {
        case Platform::unknown:
            builder.error("platform must not be unknown");
            break;
        case Platform::macOS:
        case Platform::iOS:
        case Platform::tvOS:
        case Platform::watchOS:
        case Platform::bridgeOS:
        case Platform::iOSMac:
        case Platform::iOS_simulator:
        case Platform::tvOS_simulator:
        case Platform::watchOS_simulator:
            break;
        default:
            builder.error("unknown platform value");
            break;
    }
    if (!options->archs) {
        builder.error("archs must not be null");
    }
    if (!options->numArchs) {
        builder.error("numArchs must not be 0");
    }
}

void getVersion(uint32_t *major, uint32_t *minor) {
    *major = MajorVersion;
    *minor = MinorVersion;
}

struct MRMSharedCacheBuilder* createSharedCacheBuilder(const BuildOptions_v1* options) {
    MRMSharedCacheBuilder* builder = new MRMSharedCacheBuilder(options);

    // Check the option struct values are valid
    validiateBuildOptions(options, *builder);

    return builder;
}

bool addFile(struct MRMSharedCacheBuilder* builder, const char* path, uint8_t* data, uint64_t size, FileFlags fileFlags) {
    __block bool success = false;
    builder->runSync(^() {
        if (builder->state != MRMSharedCacheBuilder::AcceptingFiles) {
            builder->error("Cannot add file: '%s' as we have already started building", path);
            return;
        }
        size_t pathLength = strlen(path);
        if (pathLength == 0) {
            builder->error("Empty path");
            return;
        }
        if (pathLength >= MAXPATHLEN) {
            builder->error("Path is too long: '%s'", path);
            return;
        }
        if (data == nullptr) {
            builder->error("Data cannot be null for file: '%s'", path);
            return;
        }
        switch (fileFlags) {
            case NoFlags:
            case MustBeInCache:
            case ShouldBeExcludedFromCacheIfUnusedLeaf:
            case RequiredClosure:
                break;
            case DylibOrderFile:
                builder->dylibOrderFileData = std::string((char*)data, size);
                success = true;
                return;
            case DirtyDataOrderFile:
                builder->dirtyDataOrderFileData = std::string((char*)data, size);
                success = true;
                return;
            case ObjCOptimizationsFile:
                builder->objcOptimizationsFileData = data;
                builder->objcOptimizationsFileLength = size;
                success = true;
                return;
            default:
                builder->error("unknown file flags value");
                break;
        }
        Diagnostics diag;
        if (!builder->fileSystem.addFile(path, data, size, diag, fileFlags)) {
            builder->errorStorage.push_back(diag.errorMessage());
            builder->errors.push_back(builder->errorStorage.back().data());
            return;
        }
        success = true;
    });
    return success;
}

bool addSymlink(struct MRMSharedCacheBuilder* builder, const char* fromPath, const char* toPath) {
    __block bool success = false;
    builder->runSync(^() {
        if (builder->state != MRMSharedCacheBuilder::AcceptingFiles) {
            builder->error("Cannot add file: '%s' as we have already started building", fromPath);
            return;
        }
        size_t pathLength = strlen(fromPath);
        if (pathLength == 0) {
            builder->error("Empty path");
            return;
        }
        if (pathLength >= MAXPATHLEN) {
            builder->error("Path is too long: '%s'", fromPath);
            return;
        }
        Diagnostics diag;
        if (!builder->fileSystem.addSymlink(fromPath, toPath, diag)) {
            builder->errorStorage.push_back(diag.errorMessage());
            builder->errors.push_back(builder->errorStorage.back().data());
            return;
        }
        success = true;
    });
    return success;
}

static DyldSharedCache::LocalSymbolsMode platformExcludeLocalSymbols(Platform platform) {
    switch (platform) {
        case Platform::unknown:
        case Platform::macOS:
            return DyldSharedCache::LocalSymbolsMode::keep;
        case Platform::iOS:
        case Platform::tvOS:
        case Platform::watchOS:
        case Platform::bridgeOS:
            return DyldSharedCache::LocalSymbolsMode::unmap;
        case Platform::iOSMac:
        case Platform::iOS_simulator:
        case Platform::tvOS_simulator:
        case Platform::watchOS_simulator:
            return DyldSharedCache::LocalSymbolsMode::keep;
    }
}

static DyldSharedCache::LocalSymbolsMode excludeLocalSymbols(const BuildOptions_v1* options) {
    if ( options->version >= 2 ) {
        const BuildOptions_v2* v2 = (const BuildOptions_v2*)options;
        if ( v2->optimizeForSize )
            return DyldSharedCache::LocalSymbolsMode::strip;
    }

    // Old build options always use the platform default
    return platformExcludeLocalSymbols(options->platform);
}

static bool optimizeDyldDlopens(const BuildOptions_v1* options) {
    // Old builds always default to dyld3 optimisations
    if ( options->version < 2 ) {
        return true;
    }

    // If we want to optimize for size instead of speed, then disable dyld3 dlopen closures
    const BuildOptions_v2* v2 = (const BuildOptions_v2*)options;
    return !v2->optimizeForSize;
}

static DyldSharedCache::CodeSigningDigestMode platformCodeSigningDigestMode(Platform platform) {
    switch (platform) {
        case Platform::unknown:
        case Platform::macOS:
        case Platform::iOS:
        case Platform::tvOS:
            return DyldSharedCache::SHA256only;
        case Platform::watchOS:
            return DyldSharedCache::Agile;
        case Platform::bridgeOS:
        case Platform::iOSMac:
        case Platform::iOS_simulator:
        case Platform::tvOS_simulator:
        case Platform::watchOS_simulator:
            return DyldSharedCache::SHA256only;
    }
}

static bool platformIsForSimulator(Platform platform) {
    switch (platform) {
        case Platform::unknown:
        case Platform::macOS:
        case Platform::iOS:
        case Platform::tvOS:
        case Platform::watchOS:
        case Platform::bridgeOS:
        case Platform::iOSMac:
            return false;
        case Platform::iOS_simulator:
        case Platform::tvOS_simulator:
        case Platform::watchOS_simulator:
            return true;
    }
}

static const char* dispositionName(Disposition disposition) {
    switch (disposition) {
        case Disposition::Unknown:
            return "";
        case Disposition::InternalDevelopment:
            return "Internal";
        case Disposition::Customer:
            return "Customer";
        case Disposition::InternalMinDevelopment:
            return "InternalMinDevelopment";
    }
}

// This is a JSON file containing the list of classes for which
// we should try to build IMP caches.
dyld3::json::Node parseObjcOptimizationsFile(Diagnostics& diags, const void* data, size_t length) {
    return dyld3::json::readJSON(diags, data, length);
}

bool runSharedCacheBuilder(struct MRMSharedCacheBuilder* builder) {
    __block bool success = false;
    builder->runSync(^() {
        if (builder->state != MRMSharedCacheBuilder::AcceptingFiles) {
            builder->error("Builder has already been run");
            return;
        }
        builder->state = MRMSharedCacheBuilder::Building;
        if (builder->fileSystem.fileCount() == 0) {
            builder->error("Cannot run builder with no files");
        }

        __block Diagnostics diag;
        std::vector<DyldSharedCache::FileAlias> aliases = builder->fileSystem.getResolvedSymlinks(diag);
        if (diag.hasError()) {
            diag.verbose("Symlink resolver error: %s\n", diag.errorMessage().c_str());
        }

        if (!builder->errors.empty()) {
            builder->error("Skipping running shared cache builder due to previous errors");
            return;
        }

        __block std::vector<SharedCacheBuilder::InputFile> inputFiles;
        builder->fileSystem.forEachFileInfo(^(const char* path, FileFlags fileFlags) {
            SharedCacheBuilder::InputFile::State state = SharedCacheBuilder::InputFile::Unset;
            switch (fileFlags) {
                case FileFlags::NoFlags:
                    state = SharedCacheBuilder::InputFile::Unset;
                    break;
                case FileFlags::MustBeInCache:
                    state = SharedCacheBuilder::InputFile::MustBeIncluded;
                    break;
                case FileFlags::ShouldBeExcludedFromCacheIfUnusedLeaf:
                    state = SharedCacheBuilder::InputFile::MustBeExcludedIfUnused;
                    break;
                case FileFlags::RequiredClosure:
                    state = SharedCacheBuilder::InputFile::MustBeIncluded;
                    break;
                case FileFlags::DylibOrderFile:
                case FileFlags::DirtyDataOrderFile:
                case FileFlags::ObjCOptimizationsFile:
                    builder->error("Order files should not be in the file system");
                    return;
            }
            inputFiles.emplace_back((SharedCacheBuilder::InputFile){ path, state });
        });

        auto addCacheConfiguration = ^(bool isOptimized) {
            for (uint64_t i = 0; i != builder->options->numArchs; ++i) {
                // HACK: Skip i386 for macOS
                if ( (builder->options->platform == Platform::macOS) && (strcmp(builder->options->archs[i], "i386") == 0 ) )
                    continue;
                auto options = std::make_unique<DyldSharedCache::CreateOptions>((DyldSharedCache::CreateOptions){});
                const char *cacheSuffix = (isOptimized ? "" : ".development");
                if ( builder->options->platform == Platform::macOS )
                    cacheSuffix = "";
                std::string runtimePath = (builder->options->platform == Platform::macOS) ? MACOSX_MRM_DYLD_SHARED_CACHE_DIR : IPHONE_DYLD_SHARED_CACHE_DIR;
                options->outputFilePath = runtimePath + "dyld_shared_cache_" + builder->options->archs[i] + cacheSuffix;
                options->outputMapFilePath = options->outputFilePath + ".json";
                options->archs = &dyld3::GradedArchs::forName(builder->options->archs[i]);
                options->platform = (dyld3::Platform)builder->options->platform;
                options->localSymbolMode = excludeLocalSymbols(builder->options);
                options->optimizeStubs = isOptimized;
                options->optimizeDyldDlopens = optimizeDyldDlopens(builder->options);
                options->optimizeDyldLaunches = true;
                options->codeSigningDigestMode = platformCodeSigningDigestMode(builder->options->platform);
                options->dylibsRemovedDuringMastering = true;
                options->inodesAreSameAsRuntime = false;
                options->cacheSupportsASLR = true;
                options->forSimulator = platformIsForSimulator(builder->options->platform);
                options->isLocallyBuiltCache = builder->options->isLocallyBuiltCache;
                options->verbose = builder->options->verboseDiagnostics;
                options->evictLeafDylibsOnOverflow = true;
                options->loggingPrefix = std::string(builder->options->deviceName) + dispositionName(builder->options->disposition) + "." + builder->options->archs[i] + cacheSuffix;
                options->dylibOrdering = parseOrderFile(builder->dylibOrderFileData);
                options->dirtyDataSegmentOrdering = parseOrderFile(builder->dirtyDataOrderFileData);
                options->objcOptimizations = parseObjcOptimizationsFile(diag, builder->objcOptimizationsFileData, builder->objcOptimizationsFileLength);

                auto cacheBuilder = std::make_unique<SharedCacheBuilder>(*options.get(), builder->fileSystem);
                builder->builders.emplace_back((BuildInstance) { std::move(options), std::move(cacheBuilder), inputFiles });
            }
        };

        // Enqueue a cache for each configuration
        switch (builder->options->disposition) {
            case Disposition::Unknown:
            case Disposition::InternalDevelopment:
                // HACK: MRM for the mac should only get development, even if it requested both
                if (builder->options->platform == Platform::macOS) {
                    addCacheConfiguration(false);
                } else {
                    addCacheConfiguration(false);
                    addCacheConfiguration(true);
                }
                break;
            case Disposition::Customer:
                addCacheConfiguration(true);
                break;
            case Disposition::InternalMinDevelopment:
                addCacheConfiguration(false);
                break;
        }

        // FIXME: This step can run in parallel.
        for (auto& buildInstance : builder->builders) {
            SharedCacheBuilder* cacheBuilder = buildInstance.builder.get();
            cacheBuilder->build(buildInstance.inputFiles, aliases);

            // First put the warnings in to a vector to own them.
            buildInstance.warningStrings.reserve(cacheBuilder->warnings().size());
            for (const std::string& warning : cacheBuilder->warnings())
                buildInstance.warningStrings.push_back(warning);

            // Then copy to a vector to reference the owner
            buildInstance.warnings.reserve(buildInstance.warningStrings.size());
            for (const std::string& warning : buildInstance.warningStrings)
                buildInstance.warnings.push_back(warning.c_str());

            if (!cacheBuilder->errorMessage().empty()) {
                // First put the errors in to a vector to own them.
                buildInstance.errorStrings.push_back(cacheBuilder->errorMessage());

                // Then copy to a vector to reference the owner
                buildInstance.errors.reserve(buildInstance.errorStrings.size());
                for (const std::string& error : buildInstance.errorStrings)
                    buildInstance.errors.push_back(error.c_str());
            }

            if (cacheBuilder->errorMessage().empty()) {
                cacheBuilder->writeBuffer(buildInstance.cacheData, buildInstance.cacheSize);
                buildInstance.jsonMap = cacheBuilder->getMapFileJSONBuffer(builder->options->deviceName);
                if ( buildInstance.options->platform == dyld3::Platform::macOS ) {
                    // For compatibility with update_dyld_shared_cache, put a .map file next to the shared cache
                    buildInstance.macOSMap = cacheBuilder->getMapFileBuffer();
                    buildInstance.macOSMapPath = buildInstance.options->outputFilePath + ".map";
                }
                buildInstance.cdHash = cacheBuilder->cdHashFirst();
                buildInstance.uuid = cacheBuilder->uuid();
                switch (buildInstance.options->codeSigningDigestMode) {
                    case DyldSharedCache::SHA256only:
                        buildInstance.cdHashType = "sha256";
                        break;
                    case DyldSharedCache::SHA1only:
                        buildInstance.cdHashType = "sha1";
                        break;
                    case DyldSharedCache::Agile:
                        buildInstance.cdHashType = "sha1";
                        break;
                }

                // Track the dylibs which were included in this cache
                cacheBuilder->forEachCacheDylib(^(const std::string &path) {
                    builder->dylibsInCaches[path.c_str()].insert(&buildInstance);
                });
                cacheBuilder->forEachCacheSymlink(^(const std::string &path) {
                    builder->dylibsInCaches[path.c_str()].insert(&buildInstance);
                });
            }
            // Free the cache builder now so that we don't keep too much memory resident
            cacheBuilder->deleteBuffer();
            buildInstance.builder.reset();
        }


        // Now that we have run all of the builds, collect the results
        // First push file results for each of the shared caches we built
        for (auto& buildInstance : builder->builders) {
            CacheResult cacheBuildResult;
            cacheBuildResult.version                = 1;
            cacheBuildResult.loggingPrefix          = buildInstance.options->loggingPrefix.c_str();
            cacheBuildResult.deviceConfiguration    = buildInstance.options->loggingPrefix.c_str();
            cacheBuildResult.warnings               = buildInstance.warnings.empty() ? nullptr : buildInstance.warnings.data();
            cacheBuildResult.numWarnings            = buildInstance.warnings.size();
            cacheBuildResult.errors                 = buildInstance.errors.empty() ? nullptr : buildInstance.errors.data();
            cacheBuildResult.numErrors              = buildInstance.errors.size();
            cacheBuildResult.uuidString             = buildInstance.uuid.c_str();
            cacheBuildResult.mapJSON                = buildInstance.jsonMap.c_str();

            builder->cacheResultStorage.emplace_back(cacheBuildResult);

            if (!buildInstance.errors.empty())
                continue;

            FileResult cacheFileResult;
            cacheFileResult.version          = 1;
            cacheFileResult.path             = buildInstance.options->outputFilePath.c_str();
            cacheFileResult.behavior         = AddFile;
            cacheFileResult.data             = buildInstance.cacheData;
            cacheFileResult.size             = buildInstance.cacheSize;
            cacheFileResult.hashArch         = buildInstance.options->archs->name();
            cacheFileResult.hashType         = buildInstance.cdHashType.c_str();
            cacheFileResult.hash             = buildInstance.cdHash.c_str();

            builder->fileResultBuffers.push_back({ builder->fileResultStorage.size(), true });
            builder->fileResultStorage.emplace_back(cacheFileResult);

            // Add a file result for the .map file
            if ( !buildInstance.macOSMap.empty() ) {
                FileResult cacheFileResult;
                cacheFileResult.version          = 1;
                cacheFileResult.path             = buildInstance.macOSMapPath.c_str();
                cacheFileResult.behavior         = AddFile;
                cacheFileResult.data             = (const uint8_t*)buildInstance.macOSMap.data();
                cacheFileResult.size             = buildInstance.macOSMap.size();
                cacheFileResult.hashArch         = buildInstance.options->archs->name();
                cacheFileResult.hashType         = buildInstance.cdHashType.c_str();
                cacheFileResult.hash             = buildInstance.cdHash.c_str();

                builder->fileResultStorage.emplace_back(cacheFileResult);
            }
        }

        // Copy from the storage to the vector we can return to the API.
        for (auto &fileResult : builder->fileResultStorage)
            builder->fileResults.push_back(&fileResult);
        for (auto &cacheResult : builder->cacheResultStorage)
            builder->cacheResults.push_back(&cacheResult);


        // Add entries to tell us to remove all of the dylibs from disk which are in every cache.
        const size_t numCaches = builder->builders.size();
        for (const auto& dylibAndCount : builder->dylibsInCaches) {
            const char* pathToRemove = dylibAndCount.first.c_str();

            if ( builder->options->platform == Platform::macOS ) {
                // macOS has to leave the simulator support binaries on disk
                if ( strcmp(pathToRemove, "/usr/lib/system/libsystem_kernel.dylib") == 0 )
                    continue;
                if ( strcmp(pathToRemove, "/usr/lib/system/libsystem_platform.dylib") == 0 )
                    continue;
                if ( strcmp(pathToRemove, "/usr/lib/system/libsystem_pthread.dylib") == 0 )
                    continue;
            }

            if (dylibAndCount.second.size() == numCaches) {
                builder->filesToRemove.push_back(pathToRemove);
            } else {
                // File is not in every cache, so likely has perhaps only x86_64h slice
                // but we built both x86_64 and x86_64h caches.
                // We may still delete it if its in all caches it's eligible for, ie, we
                // assume the cache builder knows about all possible arch's on the system and
                // can delete anything it knows can't run
                bool canDeletePath = true;
                for (auto& buildInstance : builder->builders) {
                    if ( dylibAndCount.second.count(&buildInstance) != 0 )
                        continue;
                    // This builder didn't get this image.  See if the image was ineligible
                    // based on slide, ie, that dyld at runtime couldn't load this anyway, so
                    // so removing it from disk won't hurt
                    Diagnostics loaderDiag;
                    const dyld3::GradedArchs* archs = buildInstance.options->archs;
                    dyld3::Platform platform = buildInstance.options->platform;
                    char realerPath[MAXPATHLEN];
                    dyld3::closure::LoadedFileInfo fileInfo = dyld3::MachOAnalyzer::load(loaderDiag, builder->fileSystem,
                                                                                         pathToRemove, *archs, platform, realerPath);
                    if ( (platform == dyld3::Platform::macOS) && loaderDiag.hasError() ) {
                        // Try again with iOSMac
                        loaderDiag.clearError();
                        fileInfo = dyld3::MachOAnalyzer::load(loaderDiag, builder->fileSystem,
                                                              pathToRemove, *archs, dyld3::Platform::iOSMac, realerPath);
                    }

                    // We don't need the file content now, as we only needed to know if this file could be loaded
                    builder->fileSystem.unloadFile(fileInfo);

                    if ( loaderDiag.hasError() || (fileInfo.fileContent == nullptr) ) {
                        // This arch/platform combination couldn't load this path, so we can remove it
                        continue;
                    }

                    // This arch was compatible, so the dylib was rejected from this cache for some other reason, eg,
                    // cache overflow.  We need to keep it on-disk
                    canDeletePath = false;
                    break;
                }
                if ( canDeletePath )
                    builder->filesToRemove.push_back(pathToRemove);
            }
        }

        // Quit if we had any errors.
        for (auto& buildInstance : builder->builders) {
            if (!buildInstance.errors.empty())
                return;
        }

        builder->state = MRMSharedCacheBuilder::FinishedBuilding;
        success = true;
    });
    return success;
}

const char* const* getErrors(const struct MRMSharedCacheBuilder* builder, uint64_t* errorCount) {
    if (builder->errors.empty())
        return nullptr;
    *errorCount = builder->errors.size();
    return builder->errors.data();
}

const struct FileResult* const* getFileResults(struct MRMSharedCacheBuilder* builder, uint64_t* resultCount) {
    if (builder->fileResults.empty())
        return nullptr;
    *resultCount = builder->fileResults.size();
    return builder->fileResults.data();
}

const struct CacheResult* const* getCacheResults(struct MRMSharedCacheBuilder* builder, uint64_t* resultCount) {
    if (builder->cacheResults.empty())
        return nullptr;
    *resultCount = builder->cacheResults.size();
    return builder->cacheResults.data();
}

const char* const* getFilesToRemove(const struct MRMSharedCacheBuilder* builder, uint64_t* fileCount) {
    if (builder->filesToRemove.empty())
        return nullptr;
    *fileCount = builder->filesToRemove.size();
    return builder->filesToRemove.data();
}

void destroySharedCacheBuilder(struct MRMSharedCacheBuilder* builder) {
    for (auto &indexAndIsDataMalloced : builder->fileResultBuffers) {
        FileResult& fileResult = builder->fileResultStorage[indexAndIsDataMalloced.first];
        if (indexAndIsDataMalloced.second) {
            free((void*)fileResult.data);
        } else {
            vm_deallocate(mach_task_self(), (vm_address_t)fileResult.data, fileResult.size);
        }
        fileResult.data = nullptr;
    }
    delete builder;
}
