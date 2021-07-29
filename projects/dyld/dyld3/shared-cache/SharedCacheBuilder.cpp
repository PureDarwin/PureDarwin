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

#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_time.h>
#include <mach/shared_region.h>
#include <apfs/apfs_fsctl.h>
#include <iostream>

#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include "mach-o/dyld_priv.h"
#include "ClosureBuilder.h"
#include "Closure.h"
#include "ClosureFileSystemNull.h"
#include "CodeSigningTypes.h"
#include "MachOFileAbstraction.hpp"
#include "SharedCacheBuilder.h"
#include "RootsChecker.h"
#include "IMPCachesBuilder.hpp"

#include "FileUtils.h"
#include "StringUtils.h"
#include "Trie.hpp"

#if __has_include("dyld_cache_config.h")
    #include "dyld_cache_config.h"
#else
    #define ARM_SHARED_REGION_START      0x1A000000ULL
    #define ARM_SHARED_REGION_SIZE       0x26000000ULL
    #define ARM64_SHARED_REGION_START   0x180000000ULL
    #define ARM64_SHARED_REGION_SIZE     0x100000000ULL
#endif

#if ARM64_SHARED_REGION_START == 0x7FFF00000000
    #define ARM64_DELTA_MASK 0x00FF000000000000
#else
    #define ARM64_DELTA_MASK 0x00FFFF0000000000
#endif

#ifndef ARM64_32_SHARED_REGION_START
    #define ARM64_32_SHARED_REGION_START 0x1A000000ULL
    #define ARM64_32_SHARED_REGION_SIZE  0x26000000ULL
#endif

#define  ARMV7K_CHAIN_BITS    0xC0000000

#if BUILDING_UPDATE_DYLD_CACHE_BUILDER
    #define DISCONTIGUOUS_RX   0x7FFF20000000ULL
#else
    #define DISCONTIGUOUS_RX   0x7FFF20000000ULL    // size for MRM builder
#endif
#define DISCONTIGUOUS_RW   0x7FFF80000000ULL
#define DISCONTIGUOUS_RO   0x7FFFC0000000ULL
#define DISCONTIGUOUS_RX_SIZE (DISCONTIGUOUS_RW - DISCONTIGUOUS_RX)
#define DISCONTIGUOUS_RW_SIZE 0x40000000
#define DISCONTIGUOUS_RO_SIZE 0x3FE00000

const SharedCacheBuilder::ArchLayout SharedCacheBuilder::_s_archLayout[] = {
    { DISCONTIGUOUS_RX,             0xEFE00000ULL,               0x40000000, 0x00FFFF0000000000, "x86_64",   CS_PAGE_SIZE_4K,  14, 2, true,  true,  true  },
    { DISCONTIGUOUS_RX,             0xEFE00000ULL,               0x40000000, 0x00FFFF0000000000, "x86_64h",  CS_PAGE_SIZE_4K,  14, 2, true,  true,  true  },
    { SHARED_REGION_BASE_I386,      SHARED_REGION_SIZE_I386,     0x00200000,                0x0, "i386",     CS_PAGE_SIZE_4K,  12, 0, false, false, true  },
    { ARM64_SHARED_REGION_START,    ARM64_SHARED_REGION_SIZE,    0x02000000,   ARM64_DELTA_MASK, "arm64",    CS_PAGE_SIZE_4K,  14, 2, false, true,  false },
#if SUPPORT_ARCH_arm64e
    { ARM64_SHARED_REGION_START,    ARM64_SHARED_REGION_SIZE,    0x02000000,   ARM64_DELTA_MASK, "arm64e",   CS_PAGE_SIZE_16K, 14, 2, false, true,  false },
#endif
#if SUPPORT_ARCH_arm64_32
    { ARM64_32_SHARED_REGION_START, ARM64_32_SHARED_REGION_SIZE, 0x02000000,         0xC0000000, "arm64_32", CS_PAGE_SIZE_16K, 14, 6, false, false, true  },
#endif
    { ARM_SHARED_REGION_START,      ARM_SHARED_REGION_SIZE,      0x02000000,         0xE0000000, "armv7s",   CS_PAGE_SIZE_4K,  14, 4, false, false, true  },
    { ARM_SHARED_REGION_START,      ARM_SHARED_REGION_SIZE,      0x00400000,  ARMV7K_CHAIN_BITS, "armv7k",   CS_PAGE_SIZE_4K,  14, 4, false, false, true  },
    { 0x40000000,                   0x40000000,                  0x02000000,                0x0, "sim-x86",  CS_PAGE_SIZE_4K,  14, 0, false, false, true  }
};

// These are functions that are interposed by Instruments.app or ASan
const char* const SharedCacheBuilder::_s_neverStubEliminateSymbols[] = {
    "___bzero",
    "___cxa_atexit",
    "___cxa_throw",
    "__longjmp",
    "__objc_autoreleasePoolPop",
    "_accept",
    "_access",
    "_asctime",
    "_asctime_r",
    "_asprintf",
    "_atoi",
    "_atol",
    "_atoll",
    "_calloc",
    "_chmod",
    "_chown",
    "_close",
    "_confstr",
    "_ctime",
    "_ctime_r",
    "_dispatch_after",
    "_dispatch_after_f",
    "_dispatch_async",
    "_dispatch_async_f",
    "_dispatch_barrier_async_f",
    "_dispatch_group_async",
    "_dispatch_group_async_f",
    "_dispatch_source_set_cancel_handler",
    "_dispatch_source_set_event_handler",
    "_dispatch_sync_f",
    "_dlclose",
    "_dlopen",
    "_dup",
    "_dup2",
    "_endgrent",
    "_endpwent",
    "_ether_aton",
    "_ether_hostton",
    "_ether_line",
    "_ether_ntoa",
    "_ether_ntohost",
    "_fchmod",
    "_fchown",
    "_fclose",
    "_fdopen",
    "_fflush",
    "_fopen",
    "_fork",
    "_fprintf",
    "_free",
    "_freopen",
    "_frexp",
    "_frexpf",
    "_frexpl",
    "_fscanf",
    "_fstat",
    "_fstatfs",
    "_fstatfs64",
    "_fsync",
    "_ftime",
    "_getaddrinfo",
    "_getattrlist",
    "_getcwd",
    "_getgrent",
    "_getgrgid",
    "_getgrgid_r",
    "_getgrnam",
    "_getgrnam_r",
    "_getgroups",
    "_gethostbyaddr",
    "_gethostbyname",
    "_gethostbyname2",
    "_gethostent",
    "_getifaddrs",
    "_getitimer",
    "_getnameinfo",
    "_getpass",
    "_getpeername",
    "_getpwent",
    "_getpwnam",
    "_getpwnam_r",
    "_getpwuid",
    "_getpwuid_r",
    "_getsockname",
    "_getsockopt",
    "_gmtime",
    "_gmtime_r",
    "_if_indextoname",
    "_if_nametoindex",
    "_index",
    "_inet_aton",
    "_inet_ntop",
    "_inet_pton",
    "_initgroups",
    "_ioctl",
    "_lchown",
    "_lgamma",
    "_lgammaf",
    "_lgammal",
    "_link",
    "_listxattr",
    "_localtime",
    "_localtime_r",
    "_longjmp",
    "_lseek",
    "_lstat",
    "_malloc",
    "_malloc_create_zone",
    "_malloc_default_purgeable_zone",
    "_malloc_default_zone",
    "_malloc_destroy_zone",
    "_malloc_good_size",
    "_malloc_make_nonpurgeable",
    "_malloc_make_purgeable",
    "_malloc_set_zone_name",
    "_malloc_zone_from_ptr",
    "_mbsnrtowcs",
    "_mbsrtowcs",
    "_mbstowcs",
    "_memchr",
    "_memcmp",
    "_memcpy",
    "_memmove",
    "_memset",
    "_mktime",
    "_mlock",
    "_mlockall",
    "_modf",
    "_modff",
    "_modfl",
    "_munlock",
    "_munlockall",
    "_objc_autoreleasePoolPop",
    "_objc_setProperty",
    "_objc_setProperty_atomic",
    "_objc_setProperty_atomic_copy",
    "_objc_setProperty_nonatomic",
    "_objc_setProperty_nonatomic_copy",
    "_objc_storeStrong",
    "_open",
    "_opendir",
    "_poll",
    "_posix_memalign",
    "_pread",
    "_printf",
    "_pthread_attr_getdetachstate",
    "_pthread_attr_getguardsize",
    "_pthread_attr_getinheritsched",
    "_pthread_attr_getschedparam",
    "_pthread_attr_getschedpolicy",
    "_pthread_attr_getscope",
    "_pthread_attr_getstack",
    "_pthread_attr_getstacksize",
    "_pthread_condattr_getpshared",
    "_pthread_create",
    "_pthread_getschedparam",
    "_pthread_join",
    "_pthread_mutex_lock",
    "_pthread_mutex_unlock",
    "_pthread_mutexattr_getprioceiling",
    "_pthread_mutexattr_getprotocol",
    "_pthread_mutexattr_getpshared",
    "_pthread_mutexattr_gettype",
    "_pthread_rwlockattr_getpshared",
    "_pwrite",
    "_rand_r",
    "_read",
    "_readdir",
    "_readdir_r",
    "_readv",
    "_readv$UNIX2003",
    "_realloc",
    "_realpath",
    "_recv",
    "_recvfrom",
    "_recvmsg",
    "_remquo",
    "_remquof",
    "_remquol",
    "_scanf",
    "_send",
    "_sendmsg",
    "_sendto",
    "_setattrlist",
    "_setgrent",
    "_setitimer",
    "_setlocale",
    "_setpwent",
    "_shm_open",
    "_shm_unlink",
    "_sigaction",
    "_sigemptyset",
    "_sigfillset",
    "_siglongjmp",
    "_signal",
    "_sigpending",
    "_sigprocmask",
    "_sigwait",
    "_snprintf",
    "_sprintf",
    "_sscanf",
    "_stat",
    "_statfs",
    "_statfs64",
    "_strcasecmp",
    "_strcat",
    "_strchr",
    "_strcmp",
    "_strcpy",
    "_strdup",
    "_strerror",
    "_strerror_r",
    "_strlen",
    "_strncasecmp",
    "_strncat",
    "_strncmp",
    "_strncpy",
    "_strptime",
    "_strtoimax",
    "_strtol",
    "_strtoll",
    "_strtoumax",
    "_tempnam",
    "_time",
    "_times",
    "_tmpnam",
    "_tsearch",
    "_unlink",
    "_valloc",
    "_vasprintf",
    "_vfprintf",
    "_vfscanf",
    "_vprintf",
    "_vscanf",
    "_vsnprintf",
    "_vsprintf",
    "_vsscanf",
    "_wait",
    "_wait$UNIX2003",
    "_wait3",
    "_wait4",
    "_waitid",
    "_waitid$UNIX2003",
    "_waitpid",
    "_waitpid$UNIX2003",
    "_wcslen",
    "_wcsnrtombs",
    "_wcsrtombs",
    "_wcstombs",
    "_wordexp",
    "_write",
    "_writev",
    "_writev$UNIX2003",
    // <rdar://problem/22050956> always use stubs for C++ symbols that can be overridden
    "__ZdaPv",
    "__ZdlPv",
    "__Znam",
    "__Znwm",

    nullptr
};


inline uint32_t absolutetime_to_milliseconds(uint64_t abstime)
{
    return (uint32_t)(abstime/1000/1000);
}

// Handles building a list of input files to the SharedCacheBuilder itself.
class CacheInputBuilder {
public:
    CacheInputBuilder(const dyld3::closure::FileSystem& fileSystem,
                      const dyld3::GradedArchs& archs, dyld3::Platform reqPlatform)
    : fileSystem(fileSystem), reqArchs(archs), reqPlatform(reqPlatform) { }

    // Loads and maps any MachOs in the given list of files.
    void loadMachOs(std::vector<CacheBuilder::InputFile>& inputFiles,
                    std::vector<CacheBuilder::LoadedMachO>& dylibsToCache,
                    std::vector<CacheBuilder::LoadedMachO>& otherDylibs,
                    std::vector<CacheBuilder::LoadedMachO>& executables,
                    std::vector<CacheBuilder::LoadedMachO>& couldNotLoadFiles) {

        std::map<std::string, uint64_t> dylibInstallNameMap;
        for (CacheBuilder::InputFile& inputFile : inputFiles) {
            char realerPath[MAXPATHLEN];
            dyld3::closure::LoadedFileInfo loadedFileInfo = dyld3::MachOAnalyzer::load(inputFile.diag, fileSystem, inputFile.path, reqArchs, reqPlatform, realerPath);
            if ( (reqPlatform == dyld3::Platform::macOS) && inputFile.diag.hasError() ) {
                // Try again with iOSMac
                inputFile.diag.clearError();
                loadedFileInfo = dyld3::MachOAnalyzer::load(inputFile.diag, fileSystem, inputFile.path, reqArchs, dyld3::Platform::iOSMac, realerPath);
            }
            const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)loadedFileInfo.fileContent;
            if (ma == nullptr) {
                couldNotLoadFiles.emplace_back((CacheBuilder::LoadedMachO){ DyldSharedCache::MappedMachO(), loadedFileInfo, &inputFile });
                continue;
            }

            DyldSharedCache::MappedMachO mappedFile(inputFile.path, ma, loadedFileInfo.sliceLen, false, false,
                                                    loadedFileInfo.sliceOffset, loadedFileInfo.mtime, loadedFileInfo.inode);

            // The file can be loaded with the given slice, but we may still want to exlude it from the cache.
            if (ma->isDylib()) {
                std::string installName = ma->installName();

                const char* dylibPath = inputFile.path;
                if ( (installName != inputFile.path) && (reqPlatform == dyld3::Platform::macOS) ) {
                    // We now typically require that install names and paths match.  However symlinks may allow us to bring in a path which
                    // doesn't match its install name.
                    // For example:
                    //   /usr/lib/libstdc++.6.0.9.dylib is a real file with install name /usr/lib/libstdc++.6.dylib
                    //   /usr/lib/libstdc++.6.dylib is a symlink to /usr/lib/libstdc++.6.0.9.dylib
                    // So long as we add both paths (with one as an alias) then this will work, even if dylibs are removed from disk
                    // but the symlink remains.
                    char resolvedSymlinkPath[PATH_MAX];
                    if ( fileSystem.getRealPath(installName.c_str(), resolvedSymlinkPath) ) {
                        if (!strcmp(resolvedSymlinkPath, inputFile.path)) {
                            // Symlink is the install name and points to the on-disk dylib
                            //fprintf(stderr, "Symlink works: %s == %s\n", inputFile.path, installName.c_str());
                            dylibPath = installName.c_str();
                        }
                    }
                }

                if (!ma->canBePlacedInDyldCache(dylibPath, ^(const char* msg) {
                    inputFile.diag.warning("Dylib located at '%s' cannot be placed in cache because: %s", inputFile.path, msg);
                })) {

                    if (!ma->canHavePrecomputedDlopenClosure(inputFile.path, ^(const char* msg) {
                        inputFile.diag.verbose("Dylib located at '%s' cannot prebuild dlopen closure in cache because: %s", inputFile.path, msg);
                    }) ) {
                        fileSystem.unloadFile(loadedFileInfo);
                        continue;
                    }
                    otherDylibs.emplace_back((CacheBuilder::LoadedMachO){ mappedFile, loadedFileInfo, &inputFile });
                    continue;
                }

                // Otherwise see if we have another file with this install name
                auto iteratorAndInserted = dylibInstallNameMap.insert(std::make_pair(installName, dylibsToCache.size()));
                if (iteratorAndInserted.second) {
                    // We inserted the dylib so we haven't seen another with this name.
                    if (installName[0] != '@' && installName != inputFile.path) {
                        inputFile.diag.warning("Dylib located at '%s' has installname '%s'", inputFile.path, installName.c_str());
                    }

                    dylibsToCache.emplace_back((CacheBuilder::LoadedMachO){ mappedFile, loadedFileInfo, &inputFile });
                } else {
                    // We didn't insert this one so we've seen it before.
                    CacheBuilder::LoadedMachO& previousLoadedMachO = dylibsToCache[iteratorAndInserted.first->second];
                    inputFile.diag.warning("Multiple dylibs claim installname '%s' ('%s' and '%s')", installName.c_str(), inputFile.path, previousLoadedMachO.mappedFile.runtimePath.c_str());

                    // This is the "Good" one, overwrite
                    if (inputFile.path == installName) {
                        // Unload the old one
                        fileSystem.unloadFile(previousLoadedMachO.loadedFileInfo);

                        // And replace with this one.
                        previousLoadedMachO.mappedFile = mappedFile;
                        previousLoadedMachO.loadedFileInfo = loadedFileInfo;
                    }
                }
            } else if (ma->isBundle()) {

                if (!ma->canHavePrecomputedDlopenClosure(inputFile.path, ^(const char* msg) {
                    inputFile.diag.verbose("Dylib located at '%s' cannot prebuild dlopen closure in cache because: %s", inputFile.path, msg);
                }) ) {
                    fileSystem.unloadFile(loadedFileInfo);
                    continue;
                }
                otherDylibs.emplace_back((CacheBuilder::LoadedMachO){ mappedFile, loadedFileInfo, &inputFile });
            } else if (ma->isDynamicExecutable()) {

                // Let the platform exclude the file before we do anything else.
                if (platformExcludesExecutablePath(inputFile.path)) {
                    inputFile.diag.verbose("Platform excluded file\n");
                    fileSystem.unloadFile(loadedFileInfo);
                    continue;
                }
                executables.emplace_back((CacheBuilder::LoadedMachO){ mappedFile, loadedFileInfo, &inputFile });
            } else {
                inputFile.diag.verbose("Unsupported mach file type\n");
                fileSystem.unloadFile(loadedFileInfo);
            }
        }
    }

private:

    static bool platformExcludesExecutablePath_macOS(const std::string& path) {
        // We no longer support ROSP, so skip all paths which start with the special prefix
        if ( startsWith(path, "/System/Library/Templates/Data/") )
            return true;

        static const char* sAllowedPrefixes[] = {
            "/bin/",
            "/sbin/",
            "/usr/",
            "/System/",
            "/Library/Apple/System/",
            "/Library/Apple/usr/",
            "/System/Applications/Safari.app/",
            "/Library/CoreMediaIO/Plug-Ins/DAL/"                // temp until plugins moved or closured working
        };

        bool inSearchDir = false;
        for (const char* searchDir : sAllowedPrefixes ) {
            if ( strncmp(searchDir, path.c_str(), strlen(searchDir)) == 0 )  {
                inSearchDir = true;
                break;
            }
        }

        return !inSearchDir;
    }

    // Returns true if the current platform requires that this path be excluded from the shared cache
    // Note that this overrides any exclusion from anywhere else.
    bool platformExcludesExecutablePath(const std::string& path) {
        switch (reqPlatform) {
            case dyld3::Platform::unknown:
                return false;
            case dyld3::Platform::macOS:
                return platformExcludesExecutablePath_macOS(path);
            case dyld3::Platform::iOS:
                return false;
            case dyld3::Platform::tvOS:
                return false;
            case dyld3::Platform::watchOS:
                return false;
            case dyld3::Platform::bridgeOS:
                return false;
            case dyld3::Platform::iOSMac:
                return platformExcludesExecutablePath_macOS(path);
            case dyld3::Platform::iOS_simulator:
                return false;
            case dyld3::Platform::tvOS_simulator:
                return false;
            case dyld3::Platform::watchOS_simulator:
                return false;
            case dyld3::Platform::driverKit:
                return false;
        }
    }

    const dyld3::closure::FileSystem&                   fileSystem;
    const dyld3::GradedArchs&                           reqArchs;
    dyld3::Platform                                     reqPlatform;
};

SharedCacheBuilder::SharedCacheBuilder(const DyldSharedCache::CreateOptions& options,
                                       const dyld3::closure::FileSystem& fileSystem)
    : CacheBuilder(options, fileSystem) {

    std::string targetArch = options.archs->name();
    if ( options.forSimulator && (options.archs == &dyld3::GradedArchs::i386) )
        targetArch = "sim-x86";

    for (const ArchLayout& layout : _s_archLayout) {
        if ( layout.archName == targetArch ) {
            _archLayout = &layout;
            _is64 = _archLayout->is64;
            break;
        }
    }

    if (!_archLayout) {
        _diagnostics.error("Tool was built without support for: '%s'", targetArch.c_str());
    }
}

static void verifySelfContained(const dyld3::closure::FileSystem& fileSystem,
                                std::vector<CacheBuilder::LoadedMachO>& dylibsToCache,
                                std::vector<CacheBuilder::LoadedMachO>& otherDylibs,
                                std::vector<CacheBuilder::LoadedMachO>& couldNotLoadFiles)
{
    // build map of dylibs
    __block std::map<std::string, const CacheBuilder::LoadedMachO*> knownDylibs;
    __block std::map<std::string, const CacheBuilder::LoadedMachO*> allDylibs;
    for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
        knownDylibs.insert({ dylib.mappedFile.runtimePath, &dylib });
        allDylibs.insert({ dylib.mappedFile.runtimePath, &dylib });
        if (const char* installName = dylib.mappedFile.mh->installName()) {
            knownDylibs.insert({ installName, &dylib });
            allDylibs.insert({ installName, &dylib });
        }
    }

    for (const CacheBuilder::LoadedMachO& dylib : otherDylibs) {
        allDylibs.insert({ dylib.mappedFile.runtimePath, &dylib });
        if (const char* installName = dylib.mappedFile.mh->installName())
            allDylibs.insert({ installName, &dylib });
    }

    for (const CacheBuilder::LoadedMachO& dylib : couldNotLoadFiles) {
        allDylibs.insert({ dylib.inputFile->path, &dylib });
    }

    // Exclude bad unzippered twins.  These are where a zippered binary links
    // an unzippered twin
    std::unordered_map<std::string, std::string> macOSPathToTwinPath;
    for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
        macOSPathToTwinPath[dylib.mappedFile.runtimePath] = "";
    }
    for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
        if ( startsWith(dylib.mappedFile.runtimePath, "/System/iOSSupport/") ) {
            std::string tail = dylib.mappedFile.runtimePath.substr(18);
            if ( macOSPathToTwinPath.find(tail) != macOSPathToTwinPath.end() )
                macOSPathToTwinPath[tail] = dylib.mappedFile.runtimePath;
        }
    }

    __block std::map<std::string, std::set<std::string>> badDylibs;
    for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
        if ( badDylibs.count(dylib.mappedFile.runtimePath) != 0 )
            continue;
        if ( dylib.mappedFile.mh->isZippered() ) {
            dylib.mappedFile.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
                auto macOSAndTwinPath = macOSPathToTwinPath.find(loadPath);
                if ( macOSAndTwinPath != macOSPathToTwinPath.end() ) {
                    const std::string& twinPath = macOSAndTwinPath->second;
                    if ( badDylibs.count(twinPath) != 0 )
                        return;
                    knownDylibs.erase(twinPath);
                    badDylibs[twinPath].insert(std::string("evicting UIKitForMac binary as it is linked by zippered binary '") + dylib.mappedFile.runtimePath + "'");
                }
            });
        }
    }

    // HACK: Exclude some dylibs and transitive deps for now until we have project fixes
    __block std::set<std::string> badProjects;
    badProjects.insert("/System/Library/PrivateFrameworks/TuriCore.framework/Versions/A/TuriCore");
    badProjects.insert("/System/Library/PrivateFrameworks/UHASHelloExtensionPoint-macOS.framework/Versions/A/UHASHelloExtensionPoint-macOS");

    // check all dependencies to assure every dylib in cache only depends on other dylibs in cache
    __block bool doAgain = true;
    while ( doAgain ) {
        doAgain = false;
        // scan dylib list making sure all dependents are in dylib list
        for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
            if ( badDylibs.count(dylib.mappedFile.runtimePath) != 0 )
                continue;
            if ( badProjects.count(dylib.mappedFile.runtimePath) != 0 )
                continue;
            dylib.mappedFile.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
                if (isWeak)
                    return;
                if ( badProjects.count(loadPath) != 0 ) {
                    // We depend on a bad dylib, so add this one to the list too
                    badProjects.insert(dylib.mappedFile.runtimePath);
                    badProjects.insert(dylib.mappedFile.mh->installName());
                    knownDylibs.erase(dylib.mappedFile.runtimePath);
                    knownDylibs.erase(dylib.mappedFile.mh->installName());
                    badDylibs[dylib.mappedFile.runtimePath].insert(std::string("Depends on bad project '") + loadPath + "'");
                    doAgain = true;
                    return;
                }
                char resolvedSymlinkPath[PATH_MAX];
                if ( knownDylibs.count(loadPath) == 0 ) {
                    // The loadPath was embedded when the dylib was built, but we may be in the process of moving
                    // a dylib with symlinks from old to new paths
                    // In this case, the realpath will tell us the new location
                    if ( fileSystem.getRealPath(loadPath, resolvedSymlinkPath) ) {
                        if ( strcmp(resolvedSymlinkPath, loadPath) != 0 ) {
                            loadPath = resolvedSymlinkPath;
                        }
                    }
                }
                if ( knownDylibs.count(loadPath) == 0 ) {
                    badDylibs[dylib.mappedFile.runtimePath].insert(std::string("Could not find dependency '") + loadPath + "'");
                    knownDylibs.erase(dylib.mappedFile.runtimePath);
                    knownDylibs.erase(dylib.mappedFile.mh->installName());
                    doAgain = true;
                }
            });
        }
    }

    // Now walk the dylibs which depend on missing dylibs and see if any of them are required binaries.
    for (auto badDylibsIterator : badDylibs) {
        const std::string& dylibRuntimePath = badDylibsIterator.first;
        auto requiredDylibIterator = allDylibs.find(dylibRuntimePath);
        if (requiredDylibIterator == allDylibs.end())
            continue;
        if (!requiredDylibIterator->second->inputFile->mustBeIncluded())
            continue;
        // This dylib is required so mark all dependencies as requried too
        __block std::vector<const CacheBuilder::LoadedMachO*> worklist;
        worklist.push_back(requiredDylibIterator->second);
        while (!worklist.empty()) {
            const CacheBuilder::LoadedMachO* dylib = worklist.back();
            worklist.pop_back();
            if (!dylib->mappedFile.mh)
                continue;
            dylib->mappedFile.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
                if (isWeak)
                    return;
                auto dylibIterator = allDylibs.find(loadPath);
                if (dylibIterator != allDylibs.end()) {
                    if (dylibIterator->second->inputFile->state == CacheBuilder::InputFile::Unset) {
                        dylibIterator->second->inputFile->state = CacheBuilder::InputFile::MustBeIncludedForDependent;
                        worklist.push_back(dylibIterator->second);
                    }
                }
            });
        }
    }

    // FIXME: Make this an option we can pass in
    const bool evictLeafDylibs = true;
    if (evictLeafDylibs) {
        doAgain = true;
        while ( doAgain ) {
            doAgain = false;

            // build count of how many references there are to each dylib
            __block std::set<std::string> referencedDylibs;
            for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
                if ( badDylibs.count(dylib.mappedFile.runtimePath) != 0 )
                    continue;
                dylib.mappedFile.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
                    referencedDylibs.insert(loadPath);
                });
            }

            // find all dylibs not referenced
            for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
                if ( badDylibs.count(dylib.mappedFile.runtimePath) != 0 )
                    continue;
                const char* installName = dylib.mappedFile.mh->installName();
                if ( (referencedDylibs.count(installName) == 0) && (dylib.inputFile->state == CacheBuilder::InputFile::MustBeExcludedIfUnused) ) {
                    badDylibs[dylib.mappedFile.runtimePath].insert(std::string("It has been explicitly excluded as it is unused"));
                    doAgain = true;
                }
            }
        }
    }

    // Move bad dylibs from dylibs to cache to other dylibs.
    for (const CacheBuilder::LoadedMachO& dylib : dylibsToCache) {
        auto i = badDylibs.find(dylib.mappedFile.runtimePath);
        if ( i != badDylibs.end()) {
            otherDylibs.push_back(dylib);
            for (const std::string& reason : i->second )
                otherDylibs.back().inputFile->diag.warning("Dylib located at '%s' not placed in shared cache because: %s", dylib.mappedFile.runtimePath.c_str(), reason.c_str());
        }
    }

    const auto& badDylibsLambdaRef = badDylibs;
    dylibsToCache.erase(std::remove_if(dylibsToCache.begin(), dylibsToCache.end(), [&](const CacheBuilder::LoadedMachO& dylib) {
        if (badDylibsLambdaRef.find(dylib.mappedFile.runtimePath) != badDylibsLambdaRef.end())
            return true;
        return false;
    }), dylibsToCache.end());
}

// This is the new build API which takes the raw files (which could be FAT) and tries to build a cache from them.
// We should remove the other build() method, or make it private so that this can wrap it.
void SharedCacheBuilder::build(std::vector<CacheBuilder::InputFile>& inputFiles,
                               std::vector<DyldSharedCache::FileAlias>& aliases) {
    // First filter down to files which are actually MachO's
    CacheInputBuilder cacheInputBuilder(_fileSystem, *_options.archs, _options.platform);

    std::vector<LoadedMachO> dylibsToCache;
    std::vector<LoadedMachO> otherDylibs;
    std::vector<LoadedMachO> executables;
    std::vector<LoadedMachO> couldNotLoadFiles;
    cacheInputBuilder.loadMachOs(inputFiles, dylibsToCache, otherDylibs, executables, couldNotLoadFiles);

    verifySelfContained(_fileSystem, dylibsToCache, otherDylibs, couldNotLoadFiles);

    // Check for required binaries before we try to build the cache
    if (!_diagnostics.hasError()) {
        // If we succeeded in building, then now see if there was a missing required file, and if so why its missing.
        std::string errorString;
        for (const LoadedMachO& dylib : otherDylibs) {
            if (dylib.inputFile->mustBeIncluded()) {
                // An error loading a required file must be propagated up to the top level diagnostic handler.
                bool gotWarning = false;
                for (const std::string& warning : dylib.inputFile->diag.warnings()) {
                    gotWarning = true;
                    std::string message = warning;
                    if (message.back() == '\n')
                        message.pop_back();
                    if (!errorString.empty())
                        errorString += "ERROR: ";
                    errorString += "Required binary was not included in the shared cache '" + std::string(dylib.inputFile->path) + "' because: " + message + "\n";
                }
                if (!gotWarning) {
                    if (!errorString.empty())
                        errorString += "ERROR: ";
                    errorString += "Required binary was not included in the shared cache '" + std::string(dylib.inputFile->path) + "' because: 'unknown error.  Please report to dyld'\n";
                }
            }
        }
        for (const LoadedMachO& dylib : couldNotLoadFiles) {
            if (dylib.inputFile->mustBeIncluded()) {
                if (dylib.inputFile->diag.hasError()) {
                    if (!errorString.empty())
                        errorString += "ERROR: ";
                    errorString += "Required binary was not included in the shared cache '" + std::string(dylib.inputFile->path) + "' because: " + dylib.inputFile->diag.errorMessage() + "\n";
                } else {
                    if (!errorString.empty())
                        errorString += "ERROR: ";
                    errorString += "Required binary was not included in the shared cache '" + std::string(dylib.inputFile->path) + "' because: 'unknown error.  Please report to dyld'\n";

                }
            }
        }
        if (!errorString.empty()) {
            _diagnostics.error("%s", errorString.c_str());
        }
    }

    if (!_diagnostics.hasError())
        build(dylibsToCache, otherDylibs, executables, aliases);

    if (!_diagnostics.hasError()) {
        // If we succeeded in building, then now see if there was a missing required file, and if so why its missing.
        std::string errorString;
        for (CacheBuilder::InputFile& inputFile : inputFiles) {
            if (inputFile.mustBeIncluded() && inputFile.diag.hasError()) {
                // An error loading a required file must be propagated up to the top level diagnostic handler.
                std::string message = inputFile.diag.errorMessage();
                if (message.back() == '\n')
                    message.pop_back();
                errorString += "Required binary was not included in the shared cache '" + std::string(inputFile.path) + "' because: " + message + "\n";
            }
        }
        if (!errorString.empty()) {
            _diagnostics.error("%s", errorString.c_str());
        }
    }

    // Add all the warnings from the input files to the top level warnings on the main diagnostics object.
    for (CacheBuilder::InputFile& inputFile : inputFiles) {
        for (const std::string& warning : inputFile.diag.warnings())
            _diagnostics.warning("%s", warning.c_str());
    }

    // Clean up the loaded files
    for (LoadedMachO& loadedMachO : dylibsToCache)
        _fileSystem.unloadFile(loadedMachO.loadedFileInfo);
    for (LoadedMachO& loadedMachO : otherDylibs)
        _fileSystem.unloadFile(loadedMachO.loadedFileInfo);
    for (LoadedMachO& loadedMachO : executables)
        _fileSystem.unloadFile(loadedMachO.loadedFileInfo);
}

void SharedCacheBuilder::build(const std::vector<DyldSharedCache::MappedMachO>& dylibs,
                               const std::vector<DyldSharedCache::MappedMachO>& otherOsDylibsInput,
                               const std::vector<DyldSharedCache::MappedMachO>& osExecutables,
                               std::vector<DyldSharedCache::FileAlias>& aliases) {

    std::vector<LoadedMachO> dylibsToCache;
    std::vector<LoadedMachO> otherDylibs;
    std::vector<LoadedMachO> executables;

    for (const DyldSharedCache::MappedMachO& mappedMachO : dylibs) {
        dyld3::closure::LoadedFileInfo loadedFileInfo;
        loadedFileInfo.fileContent      = mappedMachO.mh;
        loadedFileInfo.fileContentLen   = mappedMachO.length;
        loadedFileInfo.sliceOffset      = mappedMachO.sliceFileOffset;
        loadedFileInfo.sliceLen         = mappedMachO.length;
        loadedFileInfo.inode            = mappedMachO.inode;
        loadedFileInfo.mtime            = mappedMachO.modTime;
        loadedFileInfo.path             = mappedMachO.runtimePath.c_str();
        dylibsToCache.emplace_back((LoadedMachO){ mappedMachO, loadedFileInfo, nullptr });
    }

    for (const DyldSharedCache::MappedMachO& mappedMachO : otherOsDylibsInput) {
        dyld3::closure::LoadedFileInfo loadedFileInfo;
        loadedFileInfo.fileContent      = mappedMachO.mh;
        loadedFileInfo.fileContentLen   = mappedMachO.length;
        loadedFileInfo.sliceOffset      = mappedMachO.sliceFileOffset;
        loadedFileInfo.sliceLen         = mappedMachO.length;
        loadedFileInfo.inode            = mappedMachO.inode;
        loadedFileInfo.mtime            = mappedMachO.modTime;
        loadedFileInfo.path             = mappedMachO.runtimePath.c_str();
        otherDylibs.emplace_back((LoadedMachO){ mappedMachO, loadedFileInfo, nullptr });
    }

    for (const DyldSharedCache::MappedMachO& mappedMachO : osExecutables) {
        dyld3::closure::LoadedFileInfo loadedFileInfo;
        loadedFileInfo.fileContent      = mappedMachO.mh;
        loadedFileInfo.fileContentLen   = mappedMachO.length;
        loadedFileInfo.sliceOffset      = mappedMachO.sliceFileOffset;
        loadedFileInfo.sliceLen         = mappedMachO.length;
        loadedFileInfo.inode            = mappedMachO.inode;
        loadedFileInfo.mtime            = mappedMachO.modTime;
        loadedFileInfo.path             = mappedMachO.runtimePath.c_str();
        executables.emplace_back((LoadedMachO){ mappedMachO, loadedFileInfo, nullptr });
    }

    build(dylibsToCache, otherDylibs, executables, aliases);
}

void SharedCacheBuilder::build(const std::vector<LoadedMachO>& dylibs,
                               const std::vector<LoadedMachO>& otherOsDylibsInput,
                               const std::vector<LoadedMachO>& osExecutables,
                               std::vector<DyldSharedCache::FileAlias>& aliases)
{
    // <rdar://problem/21317611> error out instead of crash if cache has no dylibs
    // FIXME: plist should specify required vs optional dylibs
    if ( dylibs.size() < 30 ) {
        _diagnostics.error("missing required minimum set of dylibs");
        return;
    }

    _timeRecorder.pushTimedSection();

    // make copy of dylib list and sort
    makeSortedDylibs(dylibs, _options.dylibOrdering);

    // allocate space used by largest possible cache plus room for LINKEDITS before optimization
    _allocatedBufferSize = _archLayout->sharedMemorySize * 1.50;
    if ( vm_allocate(mach_task_self(), &_fullAllocatedBuffer, _allocatedBufferSize, VM_FLAGS_ANYWHERE) != 0 ) {
        _diagnostics.error("could not allocate buffer");
        return;
    }

    _timeRecorder.recordTime("sort dylibs");

    bool impCachesSuccess = false;
    IMPCaches::HoleMap selectorAddressIntervals;
    _impCachesBuilder = new IMPCaches::IMPCachesBuilder(_sortedDylibs, _options.objcOptimizations, _diagnostics, _timeRecorder, _fileSystem);

    // Note, macOS allows install names and paths to mismatch.  This is currently not supported by
    // IMP caches as we use install names to look up the set of dylibs.
    if (    _archLayout->is64
        && (_options.platform !=  dyld3::Platform::macOS)
        && ((_impCachesBuilder->neededClasses.size() > 0) || (_impCachesBuilder->neededMetaclasses.size() > 0))) {
        // Build the class map across all dylibs (including cross-image superclass references)
        _impCachesBuilder->buildClassesMap(_diagnostics);

        // Determine which methods will end up in each class's IMP cache
        impCachesSuccess = _impCachesBuilder->parseDylibs(_diagnostics);

        // Compute perfect hash functions for IMP caches
        if (impCachesSuccess) _impCachesBuilder->buildPerfectHashes(selectorAddressIntervals, _diagnostics);
    }

    constexpr bool log = false;
    if (log) {
        for (const auto& p : _impCachesBuilder->selectors.map) {
            printf("0x%06x %s\n", p.second->offset, p.second->name);
        }
    }

    _timeRecorder.recordTime("compute IMP caches");

    IMPCaches::SelectorMap emptyMap;
    IMPCaches::SelectorMap& selectorMap = impCachesSuccess ? _impCachesBuilder->selectors : emptyMap;
    // assign addresses for each segment of each dylib in new cache
    parseCoalescableSegments(selectorMap, selectorAddressIntervals);
    processSelectorStrings(osExecutables, selectorAddressIntervals);

    assignSegmentAddresses();
    std::vector<const LoadedMachO*> overflowDylibs;
    while ( cacheOverflowAmount() != 0 ) {
        // IMP caches: we may need to recompute the selector addresses here to be slightly more compact
        // if we remove dylibs? This is probably overkill.

        if ( !_options.evictLeafDylibsOnOverflow ) {
            _diagnostics.error("cache overflow by %lluMB", cacheOverflowAmount() / 1024 / 1024);
            return;
        }
        size_t evictionCount = evictLeafDylibs(cacheOverflowAmount(), overflowDylibs);
        // re-layout cache
        for (DylibInfo& dylib : _sortedDylibs)
            dylib.cacheLocation.clear();
        _dataRegions.clear();
        _coalescedText.clear();
        
        // Re-generate the hole map to remove any cruft that was added when parsing the coalescable text the first time.
        // Always clear the hole map, even if IMP caches are off, as it is used by the text coalescer
        selectorAddressIntervals.clear();
        if (impCachesSuccess) _impCachesBuilder->computeLowBits(selectorAddressIntervals);
        
        parseCoalescableSegments(selectorMap, selectorAddressIntervals);
        processSelectorStrings(osExecutables, selectorAddressIntervals);
        assignSegmentAddresses();

        _diagnostics.verbose("cache overflow, evicted %lu leaf dylibs\n", evictionCount);
    }
    markPaddingInaccessible();

     // copy all segments into cache

    unsigned long wastedSelectorsSpace = selectorAddressIntervals.totalHoleSize();
    if (wastedSelectorsSpace > 0) {
        _diagnostics.verbose("Selector placement for IMP caches wasted %lu bytes\n", wastedSelectorsSpace);
        if (log) {
            std::cerr << selectorAddressIntervals << std::endl;
        }
    }

    _timeRecorder.recordTime("layout cache");

    writeCacheHeader();
    copyRawSegments();
    _timeRecorder.recordTime("copy cached dylibs into buffer");

    // rebase all dylibs for new location in cache

    _aslrTracker.setDataRegion(firstDataRegion()->buffer, dataRegionsTotalSize());
    if ( !_options.cacheSupportsASLR )
        _aslrTracker.disable();
    adjustAllImagesForNewSegmentLocations(_archLayout->sharedMemoryStart, _aslrTracker,
                                          &_lohTracker, &_coalescedText);
    if ( _diagnostics.hasError() )
        return;

    _timeRecorder.recordTime("adjust segments for new split locations");

    // build ImageArray for dyld3, which has side effect of binding all cached dylibs
    buildImageArray(aliases);
    if ( _diagnostics.hasError() )
        return;

    _timeRecorder.recordTime("bind all images");

    // optimize ObjC
    DyldSharedCache* dyldCache = (DyldSharedCache*)_readExecuteRegion.buffer;
    optimizeObjC(impCachesSuccess, _impCachesBuilder->inlinedSelectors);

    delete _impCachesBuilder;
    _impCachesBuilder = nullptr;

    if ( _diagnostics.hasError() )
        return;

    _timeRecorder.recordTime("optimize Objective-C");

    if ( _options.optimizeStubs ) {
        __block std::vector<std::pair<const mach_header*, const char*>> images;
        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            images.push_back({ mh, installName });
        });

        int64_t cacheSlide = (long)dyldCache - dyldCache->unslidLoadAddress();
        uint64_t cacheUnslideAddr = dyldCache->unslidLoadAddress();
        optimizeAwayStubs(images, cacheSlide, cacheUnslideAddr,
                          dyldCache, _s_neverStubEliminateSymbols);
    }


    // FIPS seal corecrypto, This must be done after stub elimination (so that __TEXT,__text is not changed after sealing)
    fipsSign();

    _timeRecorder.recordTime("do stub elimination");

    // merge and compact LINKEDIT segments
    {
        // If we want to remove, not just unmap locals, then set the dylibs themselves to be stripped
        DylibStripMode dylibStripMode = DylibStripMode::stripNone;
        if ( _options.localSymbolMode == DyldSharedCache::LocalSymbolsMode::strip )
            dylibStripMode = CacheBuilder::DylibStripMode::stripLocals;

        __block std::vector<std::tuple<const mach_header*, const char*, DylibStripMode>> images;
        dyldCache->forEachImage(^(const mach_header *mh, const char *installName) {
            images.push_back({ mh, installName, dylibStripMode });
        });
        optimizeLinkedit(&_localSymbolsRegion, images);
    }

    // copy ImageArray to end of read-only region
    addImageArray();
    if ( _diagnostics.hasError() )
        return;

    _timeRecorder.recordTime("optimize LINKEDITs");

    // don't add dyld3 closures to simulator cache or the base system where size is more of an issue
    if ( _options.optimizeDyldDlopens ) {
        // compute and add dlopen closures for all other dylibs
        addOtherImageArray(otherOsDylibsInput, overflowDylibs);
        if ( _diagnostics.hasError() )
            return;
    }
    if ( _options.optimizeDyldLaunches ) {
        // compute and add launch closures to end of read-only region
        addClosures(osExecutables);
        if ( _diagnostics.hasError() )
            return;
    }

    // update final readOnly region size
    dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)(_readExecuteRegion.buffer + dyldCache->header.mappingOffset);
    mappings[dyldCache->header.mappingCount - 1].size = _readOnlyRegion.sizeInUse;
    dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + dyldCache->header.mappingWithSlideOffset);
    slidableMappings[dyldCache->header.mappingCount - 1].size = _readOnlyRegion.sizeInUse;
    if ( _localSymbolsRegion.sizeInUse != 0 ) {
        dyldCache->header.localSymbolsOffset = _readOnlyRegion.cacheFileOffset + _readOnlyRegion.sizeInUse;
        dyldCache->header.localSymbolsSize   = _localSymbolsRegion.sizeInUse;
    }

    // record max slide now that final size is established
    if ( _archLayout->sharedRegionsAreDiscontiguous ) {
        // special case x86_64 which has three non-contiguous chunks each in their own 1GB regions
        uint64_t maxSlide0 = DISCONTIGUOUS_RX_SIZE - _readExecuteRegion.sizeInUse; // TEXT region has 1.5GB region
        uint64_t maxSlide1 = DISCONTIGUOUS_RW_SIZE - dataRegionsTotalSize();
        uint64_t maxSlide2 = DISCONTIGUOUS_RO_SIZE - _readOnlyRegion.sizeInUse;
        dyldCache->header.maxSlide = std::min(std::min(maxSlide0, maxSlide1), maxSlide2);
    }
    else {
        // <rdar://problem/49852839> branch predictor on arm64 currently only looks at low 32-bits, so don't slide cache more than 2GB
        if ( (_archLayout->sharedMemorySize == 0x100000000) && (_readExecuteRegion.sizeInUse < 0x80000000) )
            dyldCache->header.maxSlide = 0x80000000 - _readExecuteRegion.sizeInUse;
        else
            dyldCache->header.maxSlide = (_archLayout->sharedMemoryStart + _archLayout->sharedMemorySize) - (_readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse);
    }

    // mark if any input dylibs were built with chained fixups
    dyldCache->header.builtFromChainedFixups = _someDylibsUsedChainedFixups;

    _timeRecorder.recordTime("build %lu closures", osExecutables.size());
    // Emit the CF strings without their ISAs being signed
    // This must be after addImageArray() as it depends on hasImageIndex().
    // It also has to be before emitting slide info as it adds ASLR entries.
    emitContantObjects();

    _timeRecorder.recordTime("emit constant objects");

    // fill in slide info at start of region[2]
    // do this last because it modifies pointers in DATA segments
    if ( _options.cacheSupportsASLR ) {
#if SUPPORT_ARCH_arm64e
        if ( strcmp(_archLayout->archName, "arm64e") == 0 )
            writeSlideInfoV3(_aslrTracker.bitmap(), _aslrTracker.dataPageCount());
        else
#endif
        if ( _archLayout->is64 )
            writeSlideInfoV2<Pointer64<LittleEndian>>(_aslrTracker.bitmap(), _aslrTracker.dataPageCount());
#if SUPPORT_ARCH_arm64_32 || SUPPORT_ARCH_armv7k
        else if ( _archLayout->pointerDeltaMask == 0xC0000000 )
            writeSlideInfoV4<Pointer32<LittleEndian>>(_aslrTracker.bitmap(), _aslrTracker.dataPageCount());
#endif
        else
            writeSlideInfoV2<Pointer32<LittleEndian>>(_aslrTracker.bitmap(), _aslrTracker.dataPageCount());
    }

    _timeRecorder.recordTime("compute slide info");

    // last sanity check on size
    if ( cacheOverflowAmount() != 0 ) {
        _diagnostics.error("cache overflow after optimizations 0x%llX -> 0x%llX", _readExecuteRegion.unslidLoadAddress, _readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse);
        return;
    }

    // codesignature is part of file, but is not mapped
    codeSign();
    if ( _diagnostics.hasError() )
        return;

    _timeRecorder.recordTime("compute UUID and codesign cache file");

    if (_options.verbose) {
        _timeRecorder.logTimings();
    }

    return;
}

const std::set<std::string> SharedCacheBuilder::warnings()
{
    return _diagnostics.warnings();
}

const std::set<const dyld3::MachOAnalyzer*> SharedCacheBuilder::evictions()
{
    return _evictions;
}

void SharedCacheBuilder::deleteBuffer()
{
    // Cache buffer
    if ( _allocatedBufferSize != 0 ) {
        vm_deallocate(mach_task_self(), _fullAllocatedBuffer, _allocatedBufferSize);
        _fullAllocatedBuffer = 0;
        _allocatedBufferSize = 0;
    }
    // Local symbols buffer
    if ( _localSymbolsRegion.bufferSize != 0 ) {
        vm_deallocate(mach_task_self(), (vm_address_t)_localSymbolsRegion.buffer, _localSymbolsRegion.bufferSize);
        _localSymbolsRegion.buffer = 0;
        _localSymbolsRegion.bufferSize = 0;
    }
    // Code signatures
    if ( _codeSignatureRegion.bufferSize != 0 ) {
        vm_deallocate(mach_task_self(), (vm_address_t)_codeSignatureRegion.buffer, _codeSignatureRegion.bufferSize);
        _codeSignatureRegion.buffer = 0;
        _codeSignatureRegion.bufferSize = 0;
    }
}


void SharedCacheBuilder::makeSortedDylibs(const std::vector<LoadedMachO>& dylibs, const std::unordered_map<std::string, unsigned> sortOrder)
{
    for (const LoadedMachO& dylib : dylibs) {
        _sortedDylibs.push_back({ &dylib, dylib.mappedFile.runtimePath, {} });
    }

    std::sort(_sortedDylibs.begin(), _sortedDylibs.end(), [&](const DylibInfo& a, const DylibInfo& b) {
        const auto& orderA = sortOrder.find(a.input->mappedFile.runtimePath);
        const auto& orderB = sortOrder.find(b.input->mappedFile.runtimePath);
        bool foundA = (orderA != sortOrder.end());
        bool foundB = (orderB != sortOrder.end());

        // Order all __DATA_DIRTY segments specified in the order file first, in
        // the order specified in the file, followed by any other __DATA_DIRTY
        // segments in lexicographic order.
        if ( foundA && foundB )
            return orderA->second < orderB->second;
        else if ( foundA )
            return true;
        else if ( foundB )
             return false;

        // Sort mac before iOSMac
        bool isIOSMacA = strncmp(a.input->mappedFile.runtimePath.c_str(), "/System/iOSSupport/", 19) == 0;
        bool isIOSMacB = strncmp(b.input->mappedFile.runtimePath.c_str(), "/System/iOSSupport/", 19) == 0;
        if (isIOSMacA != isIOSMacB)
            return !isIOSMacA;

        // Finally sort by path
        return a.input->mappedFile.runtimePath < b.input->mappedFile.runtimePath;
    });
}

struct DylibAndSize
{
    const CacheBuilder::LoadedMachO*    input;
    const char*                         installName;
    uint64_t                            size;
};

uint64_t SharedCacheBuilder::cacheOverflowAmount()
{
    if ( _archLayout->sharedRegionsAreDiscontiguous ) {
        // for macOS x86_64 cache, need to check each region for overflow
        if ( _readExecuteRegion.sizeInUse > DISCONTIGUOUS_RX_SIZE )
            return (_readExecuteRegion.sizeInUse - DISCONTIGUOUS_RX_SIZE);

        uint64_t dataSize = dataRegionsTotalSize();
        if ( dataSize > DISCONTIGUOUS_RW_SIZE )
            return (dataSize - DISCONTIGUOUS_RW_SIZE);

        if ( _readOnlyRegion.sizeInUse > DISCONTIGUOUS_RO_SIZE )
            return (_readOnlyRegion.sizeInUse - DISCONTIGUOUS_RO_SIZE);
    }
    else {
        bool alreadyOptimized = (_readOnlyRegion.sizeInUse != _readOnlyRegion.bufferSize);
        uint64_t vmSize = _readOnlyRegion.unslidLoadAddress - _readExecuteRegion.unslidLoadAddress;
        if ( alreadyOptimized )
            vmSize += _readOnlyRegion.sizeInUse;
        else if ( _options.localSymbolMode == DyldSharedCache::LocalSymbolsMode::unmap )
            vmSize += (_readOnlyRegion.sizeInUse * 37/100); // assume locals removal and LINKEDIT optimzation reduces LINKEDITs %37 of original size
        else
            vmSize += (_readOnlyRegion.sizeInUse * 80/100); // assume LINKEDIT optimzation reduces LINKEDITs to %80 of original size
        if ( vmSize > _archLayout->sharedMemorySize )
            return vmSize - _archLayout->sharedMemorySize;
    }
    // fits in shared region
    return 0;
}

size_t SharedCacheBuilder::evictLeafDylibs(uint64_t reductionTarget, std::vector<const LoadedMachO*>& overflowDylibs)
{
    // build a reverse map of all dylib dependencies
    __block std::map<std::string, std::set<std::string>> references;
    std::map<std::string, std::set<std::string>>* referencesPtr = &references;
    for (const DylibInfo& dylib : _sortedDylibs) {
        // Esnure we have an entry (even if it is empty)
        if (references.count(dylib.input->mappedFile.mh->installName()) == 0) {
            references[dylib.input->mappedFile.mh->installName()] = std::set<std::string>();
        };
        dylib.input->mappedFile.mh->forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool &stop) {
            references[loadPath].insert(dylib.input->mappedFile.mh->installName());
        });
    }

    // Find the sizes of all the dylibs
    std::vector<DylibAndSize> dylibsToSort;
    std::vector<DylibAndSize> sortedDylibs;
    for (const DylibInfo& dylib : _sortedDylibs) {
        const char* installName = dylib.input->mappedFile.mh->installName();
        __block uint64_t segsSize = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& info, bool& stop) {
            if ( strcmp(info.segName, "__LINKEDIT") != 0 )
                segsSize += info.vmSize;
        });
        dylibsToSort.push_back({ dylib.input, installName, segsSize });
    }

    // Build an ordered list of what to remove. At each step we do following
    // 1) Find all dylibs that nothing else depends on
    // 2a) If any of those dylibs are not in the order select the largest one of them
    // 2b) If all the leaf dylibs are in the order file select the last dylib that appears last in the order file
    // 3) Remove all entries to the removed file from the reverse dependency map
    // 4) Go back to one and repeat until there are no more evictable dylibs
    // This results in us always choosing the locally optimal selection, and then taking into account how that impacts
    // the dependency graph for subsequent selections

    bool candidateFound = true;
    while (candidateFound) {
        candidateFound = false;
        DylibAndSize candidate;
        uint64_t candidateOrder = 0;
        for(const auto& dylib : dylibsToSort) {
            const auto& i = referencesPtr->find(dylib.installName);
            assert(i != referencesPtr->end());
            if (!i->second.empty()) {
                continue;
            }
            const auto& j = _options.dylibOrdering.find(dylib.input->mappedFile.runtimePath);
            uint64_t order = 0;
            if (j != _options.dylibOrdering.end()) {
                order = j->second;
            } else {
                // Not in the order file, set order sot it goes to the front of the list
                order = UINT64_MAX;
            }
            if (order > candidateOrder ||
                (order == UINT64_MAX && candidate.size < dylib.size)) {
                    // The new file is either a lower priority in the order file
                    // or the same priority as the candidate but larger
                    candidate = dylib;
                    candidateOrder = order;
                    candidateFound = true;
            }
        }
        if (candidateFound) {
            sortedDylibs.push_back(candidate);
            referencesPtr->erase(candidate.installName);
            for (auto& dependent : references) {
                (void)dependent.second.erase(candidate.installName);
            }
            auto j = std::find_if(dylibsToSort.begin(), dylibsToSort.end(), [&candidate](const DylibAndSize& dylib) {
                return (strcmp(candidate.installName, dylib.installName) == 0);
            });
            if (j != dylibsToSort.end()) {
                dylibsToSort.erase(j);
            }
        }
    }

     // build set of dylibs that if removed will allow cache to build
    for (DylibAndSize& dylib : sortedDylibs) {
        if ( _options.verbose )
            _diagnostics.warning("to prevent cache overflow, not caching %s", dylib.installName);
        _evictions.insert(dylib.input->mappedFile.mh);
        // Track the evicted dylibs so we can try build "other" dlopen closures for them.
        overflowDylibs.push_back(dylib.input);
        if ( dylib.size > reductionTarget )
            break;
        reductionTarget -= dylib.size;
    }

    // prune _sortedDylibs
    _sortedDylibs.erase(std::remove_if(_sortedDylibs.begin(), _sortedDylibs.end(), [&](const DylibInfo& dylib) {
        return (_evictions.count(dylib.input->mappedFile.mh) != 0);
    }),_sortedDylibs.end());

    return _evictions.size();
}


void SharedCacheBuilder::writeCacheHeader()
{
    // "dyld_v1" + spaces + archName(), with enough spaces to pad to 15 bytes
    std::string magic = "dyld_v1";
    magic.append(15 - magic.length() - strlen(_options.archs->name()), ' ');
    magic.append(_options.archs->name());
    assert(magic.length() == 15);

    // 1 __TEXT segment, n __DATA segments, and 1 __LINKEDIT segment
    const uint32_t mappingCount = 2 + (uint32_t)_dataRegions.size();
    assert(mappingCount <= DyldSharedCache::MaxMappings);

    // fill in header
    dyld_cache_header* dyldCacheHeader = (dyld_cache_header*)_readExecuteRegion.buffer;
    memcpy(dyldCacheHeader->magic, magic.c_str(), 16);
    dyldCacheHeader->mappingOffset        = sizeof(dyld_cache_header);
    dyldCacheHeader->mappingCount         = mappingCount;
    dyldCacheHeader->mappingWithSlideOffset = (uint32_t)(dyldCacheHeader->mappingOffset + mappingCount*sizeof(dyld_cache_mapping_and_slide_info));
    dyldCacheHeader->mappingWithSlideCount  = mappingCount;
    dyldCacheHeader->imagesOffset         = (uint32_t)(dyldCacheHeader->mappingWithSlideOffset + mappingCount*sizeof(dyld_cache_mapping_and_slide_info));
    dyldCacheHeader->imagesCount          = (uint32_t)_sortedDylibs.size() + _aliasCount;
    dyldCacheHeader->dyldBaseAddress      = 0;
    dyldCacheHeader->codeSignatureOffset  = 0;
    dyldCacheHeader->codeSignatureSize    = 0;
    dyldCacheHeader->slideInfoOffsetUnused     = 0;
    dyldCacheHeader->slideInfoSizeUnused       = 0;
    dyldCacheHeader->localSymbolsOffset   = 0;
    dyldCacheHeader->localSymbolsSize     = 0;
    dyldCacheHeader->cacheType            = _options.optimizeStubs ? kDyldSharedCacheTypeProduction : kDyldSharedCacheTypeDevelopment;
    dyldCacheHeader->accelerateInfoAddr   = 0;
    dyldCacheHeader->accelerateInfoSize   = 0;
    bzero(dyldCacheHeader->uuid, 16);// overwritten later by recomputeCacheUUID()
    dyldCacheHeader->branchPoolsOffset    = 0;
    dyldCacheHeader->branchPoolsCount     = 0;
    dyldCacheHeader->imagesTextOffset     = dyldCacheHeader->imagesOffset + sizeof(dyld_cache_image_info)*dyldCacheHeader->imagesCount;
    dyldCacheHeader->imagesTextCount      = _sortedDylibs.size();
    dyldCacheHeader->patchInfoAddr        = 0;
    dyldCacheHeader->patchInfoSize        = 0;
    dyldCacheHeader->otherImageGroupAddrUnused  = 0;
    dyldCacheHeader->otherImageGroupSizeUnused  = 0;
    dyldCacheHeader->progClosuresAddr     = 0;
    dyldCacheHeader->progClosuresSize     = 0;
    dyldCacheHeader->progClosuresTrieAddr = 0;
    dyldCacheHeader->progClosuresTrieSize = 0;
    dyldCacheHeader->platform             = (uint8_t)_options.platform;
    dyldCacheHeader->formatVersion        = dyld3::closure::kFormatVersion;
    dyldCacheHeader->dylibsExpectedOnDisk = !_options.dylibsRemovedDuringMastering;
    dyldCacheHeader->simulator            = _options.forSimulator;
    dyldCacheHeader->locallyBuiltCache    = _options.isLocallyBuiltCache;
    dyldCacheHeader->builtFromChainedFixups= false;
    dyldCacheHeader->formatVersion        = dyld3::closure::kFormatVersion;
    dyldCacheHeader->sharedRegionStart    = _archLayout->sharedMemoryStart;
    dyldCacheHeader->sharedRegionSize     = _archLayout->sharedMemorySize;

   // fill in mappings
    dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)(_readExecuteRegion.buffer + dyldCacheHeader->mappingOffset);
    assert(_readExecuteRegion.cacheFileOffset == 0);
    mappings[0].address    = _readExecuteRegion.unslidLoadAddress;
    mappings[0].fileOffset = _readExecuteRegion.cacheFileOffset;
    mappings[0].size       = _readExecuteRegion.sizeInUse;
    mappings[0].maxProt    = VM_PROT_READ | VM_PROT_EXECUTE;
    mappings[0].initProt   = VM_PROT_READ | VM_PROT_EXECUTE;
    for (uint32_t i = 0; i != _dataRegions.size(); ++i) {
        if ( i == 0 ) {
            assert(_dataRegions[i].cacheFileOffset == _readExecuteRegion.sizeInUse);
        }

        assert(_dataRegions[i].initProt != 0);
        assert(_dataRegions[i].maxProt != 0);

        mappings[i + 1].address    = _dataRegions[i].unslidLoadAddress;
        mappings[i + 1].fileOffset = _dataRegions[i].cacheFileOffset;
        mappings[i + 1].size       = _dataRegions[i].sizeInUse;
        mappings[i + 1].maxProt    = _dataRegions[i].maxProt;
        mappings[i + 1].initProt   = _dataRegions[i].initProt;
    }
    assert(_readOnlyRegion.cacheFileOffset == (_dataRegions.back().cacheFileOffset + _dataRegions.back().sizeInUse));
    mappings[mappingCount - 1].address    = _readOnlyRegion.unslidLoadAddress;
    mappings[mappingCount - 1].fileOffset = _readOnlyRegion.cacheFileOffset;
    mappings[mappingCount - 1].size       = _readOnlyRegion.sizeInUse;
    mappings[mappingCount - 1].maxProt    = VM_PROT_READ;
    mappings[mappingCount - 1].initProt   = VM_PROT_READ;

    // Add in the new mappings with also have slide info
    dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + dyldCacheHeader->mappingWithSlideOffset);
    slidableMappings[0].address             = _readExecuteRegion.unslidLoadAddress;
    slidableMappings[0].fileOffset          = _readExecuteRegion.cacheFileOffset;
    slidableMappings[0].size                = _readExecuteRegion.sizeInUse;
    slidableMappings[0].maxProt             = VM_PROT_READ | VM_PROT_EXECUTE;
    slidableMappings[0].initProt            = VM_PROT_READ | VM_PROT_EXECUTE;
    slidableMappings[0].slideInfoFileOffset = 0;
    slidableMappings[0].slideInfoFileSize   = 0;
    slidableMappings[0].flags               = 0;
    for (uint32_t i = 0; i != _dataRegions.size(); ++i) {
        // Work out which flags this mapping has
        uint64_t flags = 0;
        if ( startsWith(_dataRegions[i].name, "__AUTH") )
            flags |= DYLD_CACHE_MAPPING_AUTH_DATA;
        if ( (_dataRegions[i].name == "__AUTH_DIRTY") || (_dataRegions[i].name == "__DATA_DIRTY") ) {
            flags |= DYLD_CACHE_MAPPING_DIRTY_DATA;
        } else if ( (_dataRegions[i].name == "__AUTH_CONST") || (_dataRegions[i].name == "__DATA_CONST") ) {
            flags |= DYLD_CACHE_MAPPING_CONST_DATA;
        }

        assert(_dataRegions[i].initProt != 0);
        assert(_dataRegions[i].maxProt != 0);

        slidableMappings[i + 1].address             = _dataRegions[i].unslidLoadAddress;
        slidableMappings[i + 1].fileOffset          = _dataRegions[i].cacheFileOffset;
        slidableMappings[i + 1].size                = _dataRegions[i].sizeInUse;
        slidableMappings[i + 1].maxProt             = _dataRegions[i].maxProt;
        slidableMappings[i + 1].initProt            = _dataRegions[i].initProt;
        slidableMappings[i + 1].slideInfoFileOffset = _dataRegions[i].slideInfoFileOffset;
        slidableMappings[i + 1].slideInfoFileSize   = _dataRegions[i].slideInfoFileSize;
        slidableMappings[i + 1].flags               = flags;
    }
    slidableMappings[mappingCount - 1].address             = _readOnlyRegion.unslidLoadAddress;
    slidableMappings[mappingCount - 1].fileOffset          = _readOnlyRegion.cacheFileOffset;
    slidableMappings[mappingCount - 1].size                = _readOnlyRegion.sizeInUse;
    slidableMappings[mappingCount - 1].maxProt             = VM_PROT_READ;
    slidableMappings[mappingCount - 1].initProt            = VM_PROT_READ;
    slidableMappings[mappingCount - 1].slideInfoFileOffset = 0;
    slidableMappings[mappingCount - 1].slideInfoFileSize   = 0;
    slidableMappings[mappingCount - 1].flags               = 0;

    // fill in image table
    dyld_cache_image_info* images = (dyld_cache_image_info*)(_readExecuteRegion.buffer + dyldCacheHeader->imagesOffset);
    for (const DylibInfo& dylib : _sortedDylibs) {
        const char* installName = dylib.input->mappedFile.mh->installName();
        images->address = dylib.cacheLocation[0].dstCacheUnslidAddress;
        if ( _options.dylibsRemovedDuringMastering ) {
            images->modTime = 0;
            images->inode   = pathHash(installName);
        }
        else {
            images->modTime = dylib.input->mappedFile.modTime;
            images->inode   = dylib.input->mappedFile.inode;
        }
        uint32_t installNameOffsetInTEXT =  (uint32_t)(installName - (char*)dylib.input->mappedFile.mh);
        images->pathFileOffset = (uint32_t)dylib.cacheLocation[0].dstCacheFileOffset + installNameOffsetInTEXT;
        ++images;
    }
    // append aliases image records and strings
/*
    for (auto &dylib : _dylibs) {
        if (!dylib->installNameAliases.empty()) {
            for (const std::string& alias : dylib->installNameAliases) {
                images->set_address(_segmentMap[dylib][0].address);
                if (_manifest.platform() == "osx") {
                    images->modTime = dylib->lastModTime;
                    images->inode = dylib->inode;
                }
                else {
                    images->modTime = 0;
                    images->inode = pathHash(alias.c_str());
                }
                images->pathFileOffset = offset;
                //fprintf(stderr, "adding alias %s for %s\n", alias.c_str(), dylib->installName.c_str());
                ::strcpy((char*)&_buffer[offset], alias.c_str());
                offset += alias.size() + 1;
                ++images;
            }
        }
    }
*/
    // calculate start of text image array and trailing string pool
    dyld_cache_image_text_info* textImages = (dyld_cache_image_text_info*)(_readExecuteRegion.buffer + dyldCacheHeader->imagesTextOffset);
    uint32_t stringOffset = (uint32_t)(dyldCacheHeader->imagesTextOffset + sizeof(dyld_cache_image_text_info) * _sortedDylibs.size());

    // write text image array and image names pool at same time
    for (const DylibInfo& dylib : _sortedDylibs) {
        dylib.input->mappedFile.mh->getUuid(textImages->uuid);
        textImages->loadAddress     = dylib.cacheLocation[0].dstCacheUnslidAddress;
        textImages->textSegmentSize = (uint32_t)dylib.cacheLocation[0].dstCacheSegmentSize;
        textImages->pathOffset      = stringOffset;
        const char* installName = dylib.input->mappedFile.mh->installName();
        ::strcpy((char*)_readExecuteRegion.buffer + stringOffset, installName);
        stringOffset += (uint32_t)strlen(installName)+1;
        ++textImages;
    }

    // make sure header did not overflow into first mapped image
    const dyld_cache_image_info* firstImage = (dyld_cache_image_info*)(_readExecuteRegion.buffer + dyldCacheHeader->imagesOffset);
    assert(stringOffset <= (firstImage->address - mappings[0].address));
}

void SharedCacheBuilder::processSelectorStrings(const std::vector<LoadedMachO>& executables, IMPCaches::HoleMap& selectorsHoleMap) {
    const bool log = false;

    // We only do this optimisation to reduce the size of the shared cache executable closures
    // Skip this is those closures are not being built
    if ( !_options.optimizeDyldDlopens || !_options.optimizeDyldLaunches )
        return;

    _selectorStringsFromExecutables = 0;
    uint64_t totalBytesPulledIn = 0;

    // Don't do this optimisation on watchOS where the shared cache is too small
    if (_options.platform == dyld3::Platform::watchOS)
        return;

    // Get the method name coalesced section as that is where we need to put these strings
    CacheBuilder::CacheCoalescedText::StringSection& cacheStringSection = _coalescedText.getSectionData("__objc_methname");
    for (const LoadedMachO& executable : executables) {
        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)executable.loadedFileInfo.fileContent;

        uint64_t sizeBeforeProcessing = cacheStringSection.bufferSize;

        ma->forEachObjCMethodName(^(const char* methodName) {
            std::string_view str = methodName;
            if (cacheStringSection.stringsToOffsets.find(str) == cacheStringSection.stringsToOffsets.end()) {
                int offset = selectorsHoleMap.addStringOfSize((unsigned)str.size() + 1);
                cacheStringSection.stringsToOffsets[str] = offset;

                // If we inserted the string past the end then we need to include it in the total
                int possibleNewEnd = offset + (int)str.size() + 1;
                if (cacheStringSection.bufferSize < (uint32_t)possibleNewEnd) {
                    cacheStringSection.bufferSize = (uint32_t)possibleNewEnd;
                }
                // if (log) printf("Selector: %s -> %s\n", ma->installName(), methodName);
                ++_selectorStringsFromExecutables;
            }
        });

        uint64_t sizeAfterProcessing = cacheStringSection.bufferSize;
        totalBytesPulledIn += (sizeAfterProcessing - sizeBeforeProcessing);
        if ( log && (sizeBeforeProcessing != sizeAfterProcessing) ) {
            printf("Pulled in % 6lld bytes of selectors from %s\n",
                   sizeAfterProcessing - sizeBeforeProcessing, executable.loadedFileInfo.path);
        }
    }

    _diagnostics.verbose("Pulled in %lld selector strings (%lld bytes) from executables\n",
                         _selectorStringsFromExecutables, totalBytesPulledIn);
}

void SharedCacheBuilder::parseCoalescableSegments(IMPCaches::SelectorMap& selectors, IMPCaches::HoleMap& selectorsHoleMap) {
    const bool log = false;

    for (DylibInfo& dylib : _sortedDylibs)
        _coalescedText.parseCoalescableText(dylib.input->mappedFile.mh, dylib.textCoalescer, selectors, selectorsHoleMap);

    if (log) {
        for (const char* section : CacheCoalescedText::SupportedSections) {
            CacheCoalescedText::StringSection& sectionData = _coalescedText.getSectionData(section);
            printf("Coalesced %s from % 10lld -> % 10d, saving % 10lld bytes\n", section,
                   sectionData.bufferSize + sectionData.savedSpace, sectionData.bufferSize, sectionData.savedSpace);
        }
    }

    // arm64e needs to convert CF constants to tagged pointers
    if ( !strcmp(_archLayout->archName, "arm64e") ) {
        // Find the dylib which exports the CFString ISA.  It's likely CoreFoundation but it could move
        CacheCoalescedText::CFSection& cfStrings = _coalescedText.cfStrings;
        for (DylibInfo& dylib : _sortedDylibs) {
            const dyld3::MachOAnalyzer* ma = dylib.input->mappedFile.mh;
            dyld3::MachOAnalyzer::FoundSymbol foundInfo;
            bool foundISASymbol = ma->findExportedSymbol(_diagnostics, cfStrings.isaClassName, false, foundInfo, nullptr);
            if ( foundISASymbol ) {
                // This dylib exports the ISA, so everyone else should look here for the ISA too.
                if ( cfStrings.isaInstallName != nullptr ) {
                    // Found a duplicate.  We can't do anything here
                    _diagnostics.verbose("Could not optimize CFString's due to duplicate ISA symbols");
                    cfStrings.isaInstallName = nullptr;
                    break;
                } else {
                    cfStrings.isaInstallName = ma->installName();
                    cfStrings.isaVMOffset    = foundInfo.value;
                }
            }
        }
        if ( cfStrings.isaInstallName != nullptr ) {
            for (DylibInfo& dylib : _sortedDylibs) {
                _coalescedText.parseCFConstants(dylib.input->mappedFile.mh, dylib.textCoalescer);
            }
        }
    }
}

// This is the new method which will put all __DATA* mappings in to a their own mappings
void SharedCacheBuilder::assignMultipleDataSegmentAddresses(uint64_t& addr, uint32_t totalProtocolDefCount) {
    uint64_t nextRegionFileOffset = _readExecuteRegion.sizeInUse;

    const size_t dylibCount = _sortedDylibs.size();
    BLOCK_ACCCESSIBLE_ARRAY(uint32_t, dirtyDataSortIndexes, dylibCount);
    for (size_t i=0; i < dylibCount; ++i)
        dirtyDataSortIndexes[i] = (uint32_t)i;
    std::sort(&dirtyDataSortIndexes[0], &dirtyDataSortIndexes[dylibCount], [&](const uint32_t& a, const uint32_t& b) {
        const auto& orderA = _options.dirtyDataSegmentOrdering.find(_sortedDylibs[a].input->mappedFile.runtimePath);
        const auto& orderB = _options.dirtyDataSegmentOrdering.find(_sortedDylibs[b].input->mappedFile.runtimePath);
        bool foundA = (orderA != _options.dirtyDataSegmentOrdering.end());
        bool foundB = (orderB != _options.dirtyDataSegmentOrdering.end());

        // Order all __DATA_DIRTY segments specified in the order file first, in the order specified in the file,
        // followed by any other __DATA_DIRTY segments in lexicographic order.
        if ( foundA && foundB )
            return orderA->second < orderB->second;
        else if ( foundA )
            return true;
        else if ( foundB )
             return false;
        else
             return _sortedDylibs[a].input->mappedFile.runtimePath < _sortedDylibs[b].input->mappedFile.runtimePath;
    });

    bool supportsAuthFixups = false;

    // This tracks which segments contain authenticated data, even if their name isn't __AUTH*
    std::set<uint32_t> authenticatedSegments[dylibCount];
    if ( strcmp(_archLayout->archName, "arm64e") == 0 ) {
        supportsAuthFixups = true;

        for (DylibInfo& dylib : _sortedDylibs) {
            uint64_t dylibIndex = &dylib - _sortedDylibs.data();
            __block std::set<uint32_t>& authSegmentIndices = authenticatedSegments[dylibIndex];

            // Put all __DATA_DIRTY segments in the __AUTH region first, then we don't need to walk their chains
            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( strcmp(segInfo.segName, "__DATA_DIRTY") == 0 ) {
                    authSegmentIndices.insert(segInfo.segIndex);
                    stop = true;
                }
            });
            dylib.input->mappedFile.mh->withChainStarts(_diagnostics, 0,
                                                        ^(const dyld_chained_starts_in_image *starts) {
                dylib.input->mappedFile.mh->forEachFixupChainSegment(_diagnostics, starts,
                                                                     ^(const dyld_chained_starts_in_segment* segmentInfo, uint32_t segIndex, bool& stopSegment) {
                    // Skip walking segments we already know are __AUTH, ie, __DATA_DIRTY
                    if ( authSegmentIndices.count(segIndex) )
                        return;

                    dylib.input->mappedFile.mh->forEachFixupInSegmentChains(_diagnostics, segmentInfo, false,
                                                                            ^(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* fixupLoc, const dyld_chained_starts_in_segment* segInfo, bool& stopChain) {
                        uint16_t chainedFixupsFormat = segInfo->pointer_format;
                        assert( (chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E) || (chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_USERLAND) || (chainedFixupsFormat == DYLD_CHAINED_PTR_ARM64E_USERLAND24) );

                        if ( fixupLoc->arm64e.authRebase.auth ) {
                            authSegmentIndices.insert(segIndex);
                            stopChain = true;
                            return;
                        }
                    });
                });
            });
        }
    }

    // Categorize each segment in each binary
    enum class SegmentType : uint8_t {
        skip,       // used for non-data segments we should ignore here
        data,
        dataDirty,
        dataConst,
        auth,
        authDirty,
        authConst,
    };

    BLOCK_ACCCESSIBLE_ARRAY(uint64_t, textSegVmAddrs, dylibCount);
    BLOCK_ACCCESSIBLE_ARRAY(std::vector<SegmentType>, segmentTypes, dylibCount);

    // Just in case __AUTH is used in a non-arm64e binary, we can force it to use data enums
    SegmentType authSegment      = supportsAuthFixups ? SegmentType::auth      : SegmentType::data;
    SegmentType authConstSegment = supportsAuthFixups ? SegmentType::authConst : SegmentType::dataConst;

    for (const DylibInfo& dylib : _sortedDylibs) {
        uint64_t dylibIndex = &dylib - _sortedDylibs.data();
        __block std::set<uint32_t>& authSegmentIndices = authenticatedSegments[dylibIndex];
        __block std::vector<SegmentType>& dylibSegmentTypes = segmentTypes[dylibIndex];
        uint64_t &textSegVmAddr = textSegVmAddrs[dylibIndex];
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 ) {
                textSegVmAddr = segInfo.vmAddr;
            }

            // Skip non-DATA segments
            if ( segInfo.protections != (VM_PROT_READ | VM_PROT_WRITE) ) {
                dylibSegmentTypes.push_back(SegmentType::skip);
                return;
            }

            // If we don't have split seg v2, then all remaining segments must look like __DATA so that they
            // stay contiguous
            if (!dylib.input->mappedFile.mh->isSplitSegV2()) {
                dylibSegmentTypes.push_back(SegmentType::data);
                return;
            }

            __block bool supportsDataConst = true;
            if ( dylib.input->mappedFile.mh->isSwiftLibrary() ) {
                uint64_t objcConstSize = 0;
                bool containsObjCSection = dylib.input->mappedFile.mh->findSectionContent(segInfo.segName, "__objc_const", objcConstSize);

                // <rdar://problem/66284631> Don't put __objc_const read-only memory as Swift has method lists we can't see
                if ( containsObjCSection )
                    supportsDataConst = false;
            } else if ( !strcmp(dylib.input->mappedFile.mh->installName(), "/System/Library/Frameworks/Foundation.framework/Foundation") ||
                        !strcmp(dylib.input->mappedFile.mh->installName(), "/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation") ) {
                // <rdar://problem/69813664> _NSTheOneTruePredicate is incompatible with __DATA_CONST
                supportsDataConst = false;
            } else if ( !strcmp(dylib.input->mappedFile.mh->installName(), "/usr/lib/system/libdispatch.dylib") ) {
               // rdar://72361509 (Speechrecognitiond crashing on AzulE18E123)
               supportsDataConst = false;
            } else if ( !strcmp(dylib.input->mappedFile.mh->installName(), "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation") ||
                        !strcmp(dylib.input->mappedFile.mh->installName(), "/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation") ) {
                // rdar://74112547 CF writes to kCFNull constant object
                supportsDataConst = false;
            }

            // Don't use data const for dylibs containing resolver functions.  This will be fixed in ld64 by moving their pointer atoms to __DATA
            if ( supportsDataConst && endsWith(segInfo.segName, "_CONST") ) {
                dylib.input->mappedFile.mh->forEachExportedSymbol(_diagnostics,
                                                                  ^(const char *symbolName, uint64_t imageOffset, uint64_t flags, uint64_t other, const char *importName, bool &stop) {
                    if ( (flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER ) != 0 ) {
                        _diagnostics.verbose("%s: preventing use of __DATA_CONST due to resolvers\n", dylib.dylibID.c_str());
                        supportsDataConst = false;
                        stop = true;
                    }
                });
            }

            // If we are still allowed to use __DATA_CONST, then make sure that we are not using pointer based method lists.  These may not be written in libobjc due
            // to uniquing or sorting (as those are done in the builder), but clients can still call setIMP to mutate them.
            if ( supportsDataConst && endsWith(segInfo.segName, "_CONST") ) {
                uint64_t segStartVMAddr = segInfo.vmAddr;
                uint64_t segEndVMAddr = segInfo.vmAddr + segInfo.vmSize;

                auto vmAddrConverter = dylib.input->mappedFile.mh->makeVMAddrConverter(false);
                const uint32_t pointerSize = dylib.input->mappedFile.mh->pointerSize();

                __block bool foundPointerBasedMethodList = false;
                auto visitMethodList = ^(uint64_t methodListVMAddr) {
                    if ( foundPointerBasedMethodList )
                        return;
                    if ( methodListVMAddr == 0 )
                        return;
                    // Ignore method lists in other segments
                    if ( (methodListVMAddr < segStartVMAddr) || (methodListVMAddr >= segEndVMAddr) )
                        return;
                    auto visitMethod = ^(uint64_t methodVMAddr, const dyld3::MachOAnalyzer::ObjCMethod& method) { };
                    bool isRelativeMethodList = false;
                    dylib.input->mappedFile.mh->forEachObjCMethod(methodListVMAddr, vmAddrConverter, visitMethod, &isRelativeMethodList);
                    if ( !isRelativeMethodList )
                        foundPointerBasedMethodList = true;
                };

                auto visitClass = ^(Diagnostics& diag, uint64_t classVMAddr,
                                    uint64_t classSuperclassVMAddr, uint64_t classDataVMAddr,
                                    const dyld3::MachOAnalyzer::ObjCClassInfo& objcClass, bool isMetaClass) {
                    visitMethodList(objcClass.baseMethodsVMAddr(pointerSize));
                };

                auto visitCategory = ^(Diagnostics& diag, uint64_t categoryVMAddr,
                                       const dyld3::MachOAnalyzer::ObjCCategory& objcCategory) {
                    visitMethodList(objcCategory.instanceMethodsVMAddr);
                    visitMethodList(objcCategory.classMethodsVMAddr);
                };

                // Walk the class list
                Diagnostics classDiag;
                dylib.input->mappedFile.mh->forEachObjCClass(classDiag, vmAddrConverter, visitClass);

                // Walk the category list
                Diagnostics categoryDiag;
                dylib.input->mappedFile.mh->forEachObjCCategory(categoryDiag, vmAddrConverter, visitCategory);

                // Note we don't walk protocols as they don't have an IMP to set

                if ( foundPointerBasedMethodList ) {
                    _diagnostics.verbose("%s: preventing use of read-only %s due to pointer based method list\n", dylib.dylibID.c_str(), segInfo.segName);
                    supportsDataConst = false;
                }
            }

            // __AUTH_CONST
            if ( strcmp(segInfo.segName, "__AUTH_CONST") == 0 ) {
                dylibSegmentTypes.push_back(supportsDataConst ? authConstSegment : authSegment);
                return;
            }

            // __DATA_CONST
            if ( (strcmp(segInfo.segName, "__DATA_CONST") == 0) || (strcmp(segInfo.segName, "__OBJC_CONST") == 0) ) {
                if ( authSegmentIndices.count(segInfo.segIndex) ) {
                    // _diagnostics.verbose("%s: treating authenticated %s as __AUTH_CONST\n", dylib.dylibID.c_str(), segInfo.segName);
                    dylibSegmentTypes.push_back(supportsDataConst ? SegmentType::authConst : SegmentType::auth);
                } else {
                    dylibSegmentTypes.push_back(supportsDataConst ? SegmentType::dataConst : SegmentType::data);
                }
                return;
            }

            // __DATA_DIRTY
            if ( strcmp(segInfo.segName, "__DATA_DIRTY") == 0 ) {
                if ( authSegmentIndices.count(segInfo.segIndex) ) {
                    dylibSegmentTypes.push_back(SegmentType::authDirty);
                } else {
                    dylibSegmentTypes.push_back(SegmentType::dataDirty);
                }
                return;
            }

            // __AUTH
            if ( strcmp(segInfo.segName, "__AUTH") == 0 ) {
                dylibSegmentTypes.push_back(authSegment);
                return;
            }

            // DATA
            if ( authSegmentIndices.count(segInfo.segIndex) ) {
                // _diagnostics.verbose("%s: treating authenticated %s as __AUTH\n", dylib.dylibID.c_str(), segInfo.segName);
                dylibSegmentTypes.push_back(SegmentType::auth);
            } else {
                dylibSegmentTypes.push_back(SegmentType::data);
            }
        });
    }

    auto processDylibSegments = ^(SegmentType onlyType, Region& region) {
        for (size_t unsortedDylibIndex = 0; unsortedDylibIndex != dylibCount; ++unsortedDylibIndex) {
            size_t dylibIndex = unsortedDylibIndex;
            if ( (onlyType == SegmentType::dataDirty) || (onlyType == SegmentType::authDirty) )
                dylibIndex = dirtyDataSortIndexes[dylibIndex];

            DylibInfo& dylib = _sortedDylibs[dylibIndex];
            const std::vector<SegmentType>& dylibSegmentTypes = segmentTypes[dylibIndex];
            const uint64_t textSegVmAddr = textSegVmAddrs[dylibIndex];

            bool forcePageAlignedData = false;
            if ( (_options.platform == dyld3::Platform::macOS) && (onlyType == SegmentType::data) ) {
                forcePageAlignedData = dylib.input->mappedFile.mh->hasUnalignedPointerFixups();
                //if ( forcePageAlignedData )
                //    warning("unaligned pointer in %s\n", dylib.input->mappedFile.runtimePath.c_str());
            }

            dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
                if ( dylibSegmentTypes[segInfo.segIndex] != onlyType )
                    return;

                // We may have coalesced the sections at the end of this segment.  In that case, shrink the segment to remove them.
                __block size_t sizeOfSections = 0;
                __block bool foundCoalescedSection = false;
                dylib.input->mappedFile.mh->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stopSection) {
                    if (strcmp(sectInfo.segInfo.segName, segInfo.segName) != 0)
                        return;
                    if ( dylib.textCoalescer.sectionWasCoalesced(segInfo.segName, sectInfo.sectName)) {
                        foundCoalescedSection = true;
                    } else {
                        sizeOfSections = sectInfo.sectAddr + sectInfo.sectSize - segInfo.vmAddr;
                    }
                });
                if (!foundCoalescedSection)
                    sizeOfSections = segInfo.sizeOfSections;

                if ( !forcePageAlignedData ) {
                    // Pack __DATA segments
                    addr = align(addr, segInfo.p2align);
                }
                else {
                    // Keep __DATA segments 4K or more aligned
                    addr = align(addr, std::max((int)segInfo.p2align, (int)12));
                }

                size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)sizeOfSections);
                uint64_t offsetInRegion = addr - region.unslidLoadAddress;
                SegmentMappingInfo loc;
                loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
                loc.segName                = segInfo.segName;
                loc.dstSegment             = region.buffer + offsetInRegion;
                loc.dstCacheUnslidAddress  = addr;
                loc.dstCacheFileOffset     = (uint32_t)(region.cacheFileOffset + offsetInRegion);
                loc.dstCacheSegmentSize    = (uint32_t)sizeOfSections;
                loc.dstCacheFileSize       = (uint32_t)copySize;
                loc.copySegmentSize        = (uint32_t)copySize;
                loc.srcSegmentIndex        = segInfo.segIndex;
                dylib.cacheLocation.push_back(loc);
                addr += loc.dstCacheSegmentSize;
            });
        }

        // align region end
        addr = align(addr, _archLayout->sharedRegionAlignP2);
    };

    struct DataRegion {
        const char*                 regionName;
        SegmentType                 dataSegment;
        std::optional<SegmentType>  dirtySegment;
        // Note this is temporary as once all platforms/archs support __DATA_CONST, we can move to a DataRegion just for CONST
        std::optional<SegmentType>  dataConstSegment;
        bool                        addCFStrings;
        bool                        addObjCRW;
    };
    std::vector<DataRegion> dataRegions;

    // We only support __DATA_CONST on arm64(e) for now.
    bool supportDataConst = false;
    //supportDataConst |= strcmp(_archLayout->archName, "arm64") == 0;
    supportDataConst |= strcmp(_archLayout->archName, "arm64e") == 0;
    if ( supportDataConst ) {
        bool addObjCRWToData = !supportsAuthFixups;
        DataRegion dataWriteRegion  = { "__DATA",       SegmentType::data,      SegmentType::dataDirty, {}, false,  addObjCRWToData  };
        DataRegion dataConstRegion  = { "__DATA_CONST", SegmentType::dataConst, {},                     {}, true,   false            };
        DataRegion authWriteRegion  = { "__AUTH",       SegmentType::auth,      SegmentType::authDirty, {}, false,  !addObjCRWToData };
        DataRegion authConstRegion  = { "__AUTH_CONST", SegmentType::authConst, {},                     {}, false,  false            };
        dataRegions.push_back(dataWriteRegion);
        dataRegions.push_back(dataConstRegion);
        if ( supportsAuthFixups ) {
            dataRegions.push_back(authWriteRegion);
            dataRegions.push_back(authConstRegion);
        }
    } else {
        DataRegion dataWriteRegion  = { "__DATA",       SegmentType::data,      SegmentType::dataDirty, SegmentType::dataConst, false,  true  };
        dataRegions.push_back(dataWriteRegion);
    }

    for (DataRegion& dataRegion : dataRegions)
    {
        Region region;
        region.buffer               = (uint8_t*)_fullAllocatedBuffer + addr - _archLayout->sharedMemoryStart;
        region.bufferSize           = 0;
        region.sizeInUse            = 0;
        region.unslidLoadAddress    = addr;
        region.cacheFileOffset      = nextRegionFileOffset;
        region.name                 = dataRegion.regionName;
        region.initProt             = endsWith(dataRegion.regionName, "_CONST") ? VM_PROT_READ : (VM_PROT_READ | VM_PROT_WRITE);
        region.maxProt              = VM_PROT_READ | VM_PROT_WRITE;

        // layout all __DATA_DIRTY segments, sorted (FIXME)
        if (dataRegion.dirtySegment.has_value())
            processDylibSegments(*dataRegion.dirtySegment, region);

        // layout all __DATA segments (and other r/w non-dirty, non-const, non-auth) segments
        processDylibSegments(dataRegion.dataSegment, region);

        // When __DATA_CONST is not its own DataRegion, we fold it in to the __DATA DataRegion
        if (dataRegion.dataConstSegment.has_value())
            processDylibSegments(*dataRegion.dataConstSegment, region);

        // Make space for the cfstrings
        if ( (dataRegion.addCFStrings) && (_coalescedText.cfStrings.bufferSize != 0) ) {
            // Keep __DATA segments 4K or more aligned
            addr = align(addr, 12);
            uint64_t offsetInRegion = addr - region.unslidLoadAddress;

            CacheCoalescedText::CFSection& cacheSection = _coalescedText.cfStrings;
            cacheSection.bufferAddr         = region.buffer + offsetInRegion;
            cacheSection.bufferVMAddr       = addr;
            cacheSection.cacheFileOffset    = region.cacheFileOffset + offsetInRegion;
            addr += cacheSection.bufferSize;
        }

        if ( dataRegion.addObjCRW ) {
            // reserve space for objc r/w optimization tables
            _objcReadWriteBufferSizeAllocated = align(computeReadWriteObjC((uint32_t)_sortedDylibs.size(), totalProtocolDefCount), 14);
            addr = align(addr, 4); // objc r/w section contains pointer and must be at least pointer align
            _objcReadWriteBuffer = region.buffer + (addr - region.unslidLoadAddress);
            _objcReadWriteFileOffset = (uint32_t)((_objcReadWriteBuffer - region.buffer) + region.cacheFileOffset);
            addr += _objcReadWriteBufferSizeAllocated;


            // align region end
            addr = align(addr, _archLayout->sharedRegionAlignP2);
        }

        // align DATA region end
        uint64_t endDataAddress = addr;
        region.bufferSize   = endDataAddress - region.unslidLoadAddress;
        region.sizeInUse    = region.bufferSize;

        _dataRegions.push_back(region);
        nextRegionFileOffset = region.cacheFileOffset + region.sizeInUse;

        // Only arm64 and arm64e shared caches have enough space to pad between __DATA and __DATA_CONST
        // All other caches are overflowing.
        if ( !strcmp(_archLayout->archName, "arm64") || !strcmp(_archLayout->archName, "arm64e") )
            addr = align((addr + _archLayout->sharedRegionPadding), _archLayout->sharedRegionAlignP2);
    }

    // Sanity check that we didn't put the same segment in 2 different ranges
    for (DylibInfo& dylib : _sortedDylibs) {
        std::unordered_set<uint64_t> seenSegmentIndices;
        for (SegmentMappingInfo& segmentInfo : dylib.cacheLocation) {
            if ( seenSegmentIndices.count(segmentInfo.srcSegmentIndex) != 0 ) {
                _diagnostics.error("%s segment %s was duplicated in layout",
                                   dylib.input->mappedFile.mh->installName(), segmentInfo.segName);
                return;
            }
            seenSegmentIndices.insert(segmentInfo.srcSegmentIndex);
        }
    }
}

void SharedCacheBuilder::assignSegmentAddresses()
{
    // calculate size of header info and where first dylib's mach_header should start
    size_t startOffset = sizeof(dyld_cache_header) + DyldSharedCache::MaxMappings * sizeof(dyld_cache_mapping_info);
    startOffset += DyldSharedCache::MaxMappings * sizeof(dyld_cache_mapping_and_slide_info);
    startOffset += sizeof(dyld_cache_image_info) * _sortedDylibs.size();
    startOffset += sizeof(dyld_cache_image_text_info) * _sortedDylibs.size();
    for (const DylibInfo& dylib : _sortedDylibs) {
        startOffset += (strlen(dylib.input->mappedFile.mh->installName()) + 1);
    }
    //fprintf(stderr, "%s total header size = 0x%08lX\n", _options.archName.c_str(), startOffset);
    startOffset = align(startOffset, 12);

    // HACK!: Rebase v4 assumes that values below 0x8000 are not pointers (encoding as offsets from the cache header).
    // If using a minimal cache, we need to pad out the cache header to make sure a pointer doesn't fall within that range
#if SUPPORT_ARCH_arm64_32 || SUPPORT_ARCH_armv7k
    if ( _options.cacheSupportsASLR && !_archLayout->is64 ) {
        if ( _archLayout->pointerDeltaMask == 0xC0000000 )
            startOffset = std::max(startOffset, (size_t)0x8000);
    }
#endif

    // assign TEXT segment addresses
    _readExecuteRegion.buffer               = (uint8_t*)_fullAllocatedBuffer;
    _readExecuteRegion.bufferSize           = 0;
    _readExecuteRegion.sizeInUse            = 0;
    _readExecuteRegion.unslidLoadAddress    = _archLayout->sharedMemoryStart;
    _readExecuteRegion.cacheFileOffset      = 0;
    __block uint64_t addr = _readExecuteRegion.unslidLoadAddress + startOffset; // header
    for (DylibInfo& dylib : _sortedDylibs) {
        __block uint64_t textSegVmAddr = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                textSegVmAddr = segInfo.vmAddr;
            if ( segInfo.protections != (VM_PROT_READ | VM_PROT_EXECUTE) )
                return;
            // We may have coalesced the sections at the end of this segment.  In that case, shrink the segment to remove them.
            __block size_t sizeOfSections = 0;
            __block bool foundCoalescedSection = false;
            dylib.input->mappedFile.mh->forEachSection(^(const dyld3::MachOAnalyzer::SectionInfo &sectInfo, bool malformedSectionRange, bool &stopSection) {
                if (strcmp(sectInfo.segInfo.segName, segInfo.segName) != 0)
                    return;
                if ( dylib.textCoalescer.sectionWasCoalesced(segInfo.segName, sectInfo.sectName)) {
                    foundCoalescedSection = true;
                } else {
                    sizeOfSections = sectInfo.sectAddr + sectInfo.sectSize - segInfo.vmAddr;
                }
            });
            if (!foundCoalescedSection)
                sizeOfSections = segInfo.sizeOfSections;

            // Keep __TEXT segments 4K or more aligned
            addr = align(addr, std::max((int)segInfo.p2align, (int)12));
            uint64_t offsetInRegion = addr - _readExecuteRegion.unslidLoadAddress;
            SegmentMappingInfo loc;
            loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
            loc.segName                = segInfo.segName;
            loc.dstSegment             = _readExecuteRegion.buffer + offsetInRegion;
            loc.dstCacheUnslidAddress  = addr;
            loc.dstCacheFileOffset     = (uint32_t)offsetInRegion;
            loc.dstCacheSegmentSize    = (uint32_t)align(sizeOfSections, 12);
            loc.dstCacheFileSize       = (uint32_t)align(sizeOfSections, 12);
            loc.copySegmentSize        = (uint32_t)sizeOfSections;
            loc.srcSegmentIndex        = segInfo.segIndex;
            dylib.cacheLocation.push_back(loc);
            addr += loc.dstCacheSegmentSize;
        });
    }

    // reserve space for objc optimization tables and deduped strings
    uint64_t objcReadOnlyBufferVMAddr = addr;
    _objcReadOnlyBuffer = _readExecuteRegion.buffer + (addr - _readExecuteRegion.unslidLoadAddress);

    // First the strings as we'll fill in the objc tables later in the optimizer
    for (const char* section: CacheCoalescedText::SupportedSections) {
        CacheCoalescedText::StringSection& cacheStringSection = _coalescedText.getSectionData(section);
        cacheStringSection.bufferAddr = _readExecuteRegion.buffer + (addr - _readExecuteRegion.unslidLoadAddress);
        cacheStringSection.bufferVMAddr = addr;
        addr += cacheStringSection.bufferSize;
    }

    addr = align(addr, 14);
    _objcReadOnlyBufferSizeUsed = addr - objcReadOnlyBufferVMAddr;

    uint32_t totalSelectorRefCount = (uint32_t)_selectorStringsFromExecutables;
    uint32_t totalClassDefCount    = 0;
    uint32_t totalProtocolDefCount = 0;
    for (DylibInfo& dylib : _sortedDylibs) {
        dyld3::MachOAnalyzer::ObjCInfo info = dylib.input->mappedFile.mh->getObjCInfo();
        totalSelectorRefCount   += info.selRefCount;
        totalClassDefCount      += info.classDefCount;
        totalProtocolDefCount   += info.protocolDefCount;
    }

    // now that shared cache coalesces all selector strings, use that better count
    uint32_t coalescedSelectorCount = (uint32_t)_coalescedText.objcMethNames.stringsToOffsets.size();
    if ( coalescedSelectorCount > totalSelectorRefCount )
        totalSelectorRefCount = coalescedSelectorCount;
    addr += align(computeReadOnlyObjC(totalSelectorRefCount, totalClassDefCount, totalProtocolDefCount), 14);

    size_t impCachesSize = _impCachesBuilder->totalIMPCachesSize();
    size_t alignedImpCachesSize = align(impCachesSize, 14);
    _diagnostics.verbose("Reserving %zd bytes for IMP caches (aligned to %zd)\n", impCachesSize, alignedImpCachesSize);
    addr += alignedImpCachesSize;

    _objcReadOnlyBufferSizeAllocated = addr - objcReadOnlyBufferVMAddr;

    // align TEXT region end
    uint64_t endTextAddress = align(addr, _archLayout->sharedRegionAlignP2);
    _readExecuteRegion.bufferSize = endTextAddress - _readExecuteRegion.unslidLoadAddress;
    _readExecuteRegion.sizeInUse  = _readExecuteRegion.bufferSize;


    // assign __DATA* addresses
    if ( _archLayout->sharedRegionsAreDiscontiguous )
        addr = DISCONTIGUOUS_RW;
    else
        addr = align((addr + _archLayout->sharedRegionPadding), _archLayout->sharedRegionAlignP2);

    // __DATA*
    assignMultipleDataSegmentAddresses(addr, totalProtocolDefCount);

    // start read-only region
    if ( _archLayout->sharedRegionsAreDiscontiguous )
        addr = DISCONTIGUOUS_RO;
    else
        addr = align((addr + _archLayout->sharedRegionPadding), _archLayout->sharedRegionAlignP2);
    _readOnlyRegion.buffer               = (uint8_t*)_fullAllocatedBuffer + addr - _archLayout->sharedMemoryStart;
    _readOnlyRegion.bufferSize           = 0;
    _readOnlyRegion.sizeInUse            = 0;
    _readOnlyRegion.unslidLoadAddress    = addr;
    _readOnlyRegion.cacheFileOffset      = lastDataRegion()->cacheFileOffset + lastDataRegion()->sizeInUse;


    // reserve space for kernel ASLR slide info at start of r/o region
    if ( _options.cacheSupportsASLR ) {
        size_t slideInfoSize = sizeof(dyld_cache_slide_info);
        slideInfoSize = std::max(slideInfoSize, sizeof(dyld_cache_slide_info2));
        slideInfoSize = std::max(slideInfoSize, sizeof(dyld_cache_slide_info3));
        slideInfoSize = std::max(slideInfoSize, sizeof(dyld_cache_slide_info4));
        // We need one slide info header per data region, plus enough space for that regions pages
        // Each region will also be padded to a page-size so that the kernel can wire it.
        for (Region& region : _dataRegions) {
            uint64_t offsetInRegion = addr - _readOnlyRegion.unslidLoadAddress;
            region.slideInfoBuffer = _readOnlyRegion.buffer + offsetInRegion;
            region.slideInfoBufferSizeAllocated = align(slideInfoSize + (region.sizeInUse/4096) * _archLayout->slideInfoBytesPerPage + 0x4000, _archLayout->sharedRegionAlignP2);
            region.slideInfoFileOffset = _readOnlyRegion.cacheFileOffset + offsetInRegion;
            addr += region.slideInfoBufferSizeAllocated;
        }
    }

    // layout all read-only (but not LINKEDIT) segments
    for (DylibInfo& dylib : _sortedDylibs) {
        __block uint64_t textSegVmAddr = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                textSegVmAddr = segInfo.vmAddr;
            if ( segInfo.protections != VM_PROT_READ )
                return;
            if ( strcmp(segInfo.segName, "__LINKEDIT") == 0 )
                return;

            // Keep segments segments 4K or more aligned
            addr = align(addr, std::max((int)segInfo.p2align, (int)12));
            uint64_t offsetInRegion = addr - _readOnlyRegion.unslidLoadAddress;
            SegmentMappingInfo loc;
            loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
            loc.segName                = segInfo.segName;
            loc.dstSegment             = _readOnlyRegion.buffer + offsetInRegion;
            loc.dstCacheUnslidAddress  = addr;
            loc.dstCacheFileOffset     = (uint32_t)(_readOnlyRegion.cacheFileOffset + offsetInRegion);
            loc.dstCacheSegmentSize    = (uint32_t)align(segInfo.sizeOfSections, 12);
            loc.dstCacheFileSize       = (uint32_t)segInfo.sizeOfSections;
            loc.copySegmentSize        = (uint32_t)segInfo.sizeOfSections;
            loc.srcSegmentIndex        = segInfo.segIndex;
            dylib.cacheLocation.push_back(loc);
            addr += loc.dstCacheSegmentSize;
        });
    }

    // layout all LINKEDIT segments (after other read-only segments), aligned to 16KB
    addr = align(addr, 14);
    _nonLinkEditReadOnlySize =  addr - _readOnlyRegion.unslidLoadAddress;
    for (DylibInfo& dylib : _sortedDylibs) {
        __block uint64_t textSegVmAddr = 0;
        dylib.input->mappedFile.mh->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& segInfo, bool& stop) {
            if ( strcmp(segInfo.segName, "__TEXT") == 0 )
                textSegVmAddr = segInfo.vmAddr;
            if ( segInfo.protections != VM_PROT_READ )
                return;
            if ( strcmp(segInfo.segName, "__LINKEDIT") != 0 )
                return;
            // Keep segments segments 4K or more aligned
            addr = align(addr, std::max((int)segInfo.p2align, (int)12));
            size_t copySize = std::min((size_t)segInfo.fileSize, (size_t)segInfo.sizeOfSections);
            uint64_t offsetInRegion = addr - _readOnlyRegion.unslidLoadAddress;
            SegmentMappingInfo loc;
            loc.srcSegment             = (uint8_t*)dylib.input->mappedFile.mh + segInfo.vmAddr - textSegVmAddr;
            loc.segName                = segInfo.segName;
            loc.dstSegment             = _readOnlyRegion.buffer + offsetInRegion;
            loc.dstCacheUnslidAddress  = addr;
            loc.dstCacheFileOffset     = (uint32_t)(_readOnlyRegion.cacheFileOffset + offsetInRegion);
            loc.dstCacheSegmentSize    = (uint32_t)align(segInfo.sizeOfSections, 12);
            loc.dstCacheFileSize       = (uint32_t)copySize;
            loc.copySegmentSize        = (uint32_t)copySize;
            loc.srcSegmentIndex        = segInfo.segIndex;
            dylib.cacheLocation.push_back(loc);
            addr += loc.dstCacheSegmentSize;
        });
    }

    // align r/o region end
    addr = align(addr, _archLayout->sharedRegionAlignP2);
    uint64_t endReadOnlyAddress = addr;
    _readOnlyRegion.bufferSize  = endReadOnlyAddress - _readOnlyRegion.unslidLoadAddress + 0x100000;
    _readOnlyRegion.sizeInUse   = _readOnlyRegion.bufferSize;

    //fprintf(stderr, "RX region=%p -> %p, logical addr=0x%llX\n", _readExecuteRegion.buffer, _readExecuteRegion.buffer+_readExecuteRegion.bufferSize, _readExecuteRegion.unslidLoadAddress);
    //fprintf(stderr, "RW region=%p -> %p, logical addr=0x%llX\n", readWriteRegion.buffer,   readWriteRegion.buffer+readWriteRegion.bufferSize, readWriteRegion.unslidLoadAddress);
    //fprintf(stderr, "RO region=%p -> %p, logical addr=0x%llX\n", _readOnlyRegion.buffer,    _readOnlyRegion.buffer+_readOnlyRegion.bufferSize, _readOnlyRegion.unslidLoadAddress);

    // sort SegmentMappingInfo for each image to be in the same order as original segments
    for (DylibInfo& dylib : _sortedDylibs) {
        std::sort(dylib.cacheLocation.begin(), dylib.cacheLocation.end(), [&](const SegmentMappingInfo& a, const SegmentMappingInfo& b) {
            return a.srcSegmentIndex < b.srcSegmentIndex;
        });
    }
}

// Return the total size of the data regions, including padding between them.
// Note this assumes they are contiguous, or that we don't care about including
// additional space between them.
uint64_t SharedCacheBuilder::dataRegionsTotalSize() const {
    const Region* firstRegion = nullptr;
    const Region* lastRegion = nullptr;
    for (const Region& region : _dataRegions) {
        if ( (firstRegion == nullptr) || (region.buffer < firstRegion->buffer) )
            firstRegion = &region;
        if ( (lastRegion == nullptr) || (region.buffer > lastRegion->buffer) )
            lastRegion = &region;
    }
    return (lastRegion->buffer - firstRegion->buffer) + lastRegion->sizeInUse;
}


// Return the total size of the data regions, excluding padding between them
uint64_t SharedCacheBuilder::dataRegionsSizeInUse() const {
    size_t size = 0;
    for (const Region& dataRegion : _dataRegions)
        size += dataRegion.sizeInUse;
    return size;
}

// Return the earliest data region by address
const CacheBuilder::Region* SharedCacheBuilder::firstDataRegion() const {
    const Region* firstRegion = nullptr;
    for (const Region& region : _dataRegions) {
        if ( (firstRegion == nullptr) || (region.buffer < firstRegion->buffer) )
            firstRegion = &region;
    }
    return firstRegion;
}

// Return the lateset data region by address
const CacheBuilder::Region* SharedCacheBuilder::lastDataRegion() const {
    const Region* lastRegion = nullptr;
    for (const Region& region : _dataRegions) {
        if ( (lastRegion == nullptr) || (region.buffer > lastRegion->buffer) )
            lastRegion = &region;
    }
    return lastRegion;
}
static dyld_cache_patchable_location makePatchLocation(size_t cacheOff, dyld3::MachOAnalyzerSet::PointerMetaData pmd, uint64_t addend) {
     dyld_cache_patchable_location patch;
    patch.cacheOffset           = cacheOff;
    patch.high7                 = pmd.high8 >> 1;
    patch.addend                = addend;
    patch.authenticated         = pmd.authenticated;
    patch.usesAddressDiversity  = pmd.usesAddrDiversity;
    patch.key                   = pmd.key;
    patch.discriminator         = pmd.diversity;
    // check for truncations
    assert(patch.cacheOffset == cacheOff);
    assert(patch.addend == addend);
    assert((patch.high7 << 1) == pmd.high8);
    return patch;
}

void SharedCacheBuilder::buildImageArray(std::vector<DyldSharedCache::FileAlias>& aliases)
{
    typedef dyld3::closure::ClosureBuilder::CachedDylibInfo         CachedDylibInfo;

    // convert STL data structures to simple arrays to passe to makeDyldCacheImageArray()
    __block std::vector<CachedDylibInfo> dylibInfos;
    __block std::unordered_map<dyld3::closure::ImageNum, const dyld3::MachOLoaded*> imageNumToML;
    DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    cache->forEachImage(^(const mach_header* mh, const char* installName) {
        const dyld3::MachOLoaded* ml = (dyld3::MachOLoaded*)mh;
        if ( !_someDylibsUsedChainedFixups && ml->hasChainedFixups() )
            _someDylibsUsedChainedFixups = true;
        uint64_t mtime;
        uint64_t inode;
        cache->getIndexedImageEntry((uint32_t)dylibInfos.size(), mtime, inode);
        CachedDylibInfo entry;
        entry.fileInfo.fileContent  = mh;
        entry.fileInfo.path         = installName;
        entry.fileInfo.sliceOffset  = 0;
        entry.fileInfo.inode        = inode;
        entry.fileInfo.mtime        = mtime;
        dylibInfos.push_back(entry);
        imageNumToML[(dyld3::closure::ImageNum)(dylibInfos.size())] = ml;
    });

    // Convert symlinks from STL to simple char pointers.
    std::vector<dyld3::closure::ClosureBuilder::CachedDylibAlias> dylibAliases;
    dylibAliases.reserve(aliases.size());
    for (const auto& alias : aliases)
        dylibAliases.push_back({ alias.realPath.c_str(), alias.aliasPath.c_str() });

    typedef dyld3::MachOAnalyzerSet::FixupTarget        FixupTarget;
    typedef dyld3::MachOAnalyzerSet::PointerMetaData    PointerMetaData;

    dyld3::closure::ClosureBuilder::DylibFixupHandler handler = ^(const dyld3::MachOLoaded* fixupIn, uint64_t fixupLocRuntimeOffset,
                                                                  PointerMetaData pmd, const FixupTarget& target) {
        uint8_t*  fixupLoc = (uint8_t*)fixupIn + fixupLocRuntimeOffset;
        uint32_t* fixupLoc32 = (uint32_t*)fixupLoc;
        uint64_t* fixupLoc64 = (uint64_t*)fixupLoc;
        uint64_t  targetSymbolOffsetInCache;
        switch ( target.kind ) {
            case FixupTarget::Kind::rebase:
                // rebasing already done in AdjustDylibSegments, but if input dylib uses chained fixups, target might not fit
                if ( _archLayout->is64 ) {
                    if ( pmd.authenticated )
                        _aslrTracker.setAuthData(fixupLoc, pmd.diversity, pmd.usesAddrDiversity, pmd.key);
                    if ( pmd.high8 )
                        _aslrTracker.setHigh8(fixupLoc, pmd.high8);
                    uint64_t targetVmAddr;
                    if ( _aslrTracker.hasRebaseTarget64(fixupLoc, &targetVmAddr) )
                        *fixupLoc64 = targetVmAddr;
                    else
                        *fixupLoc64 = (uint8_t*)target.foundInImage._mh - _readExecuteRegion.buffer + target.offsetInImage + _readExecuteRegion.unslidLoadAddress;
                }
                else {
                    uint32_t targetVmAddr;
                    assert(_aslrTracker.hasRebaseTarget32(fixupLoc, &targetVmAddr) && "32-bit archs always store target in side table");
                    *fixupLoc32 = targetVmAddr;
                }
                break;
            case FixupTarget::Kind::bindAbsolute:
                if ( _archLayout->is64 )
                    *fixupLoc64 = target.offsetInImage;
                else
                    *fixupLoc32 = (uint32_t)(target.offsetInImage);
                // don't record absolute targets for ASLR
                _aslrTracker.remove(fixupLoc);
                break;
            case FixupTarget::Kind::bindToImage:
                targetSymbolOffsetInCache = (uint8_t*)target.foundInImage._mh - _readExecuteRegion.buffer + target.offsetInImage - target.addend;
                if ( !target.weakCoalesced || !_aslrTracker.has(fixupLoc) ) {
                    // this handler is called a second time for weak_bind info, which we ignore when building cache
                    _aslrTracker.add(fixupLoc);
                    if ( _archLayout->is64 ) {
                        if ( pmd.high8 )
                            _aslrTracker.setHigh8(fixupLoc, pmd.high8);
                        if ( pmd.authenticated )
                            _aslrTracker.setAuthData(fixupLoc, pmd.diversity, pmd.usesAddrDiversity, pmd.key);
                        *fixupLoc64 = _archLayout->sharedMemoryStart + targetSymbolOffsetInCache + target.addend;
                    }
                    else {
                        assert(targetSymbolOffsetInCache < (_readOnlyRegion.buffer - _readExecuteRegion.buffer) && "offset not into TEXT or DATA of cache file");
                        uint32_t targetVmAddr;
                        if ( _aslrTracker.hasRebaseTarget32(fixupLoc, &targetVmAddr) )
                            *fixupLoc32 = targetVmAddr;
                        else
                            *fixupLoc32 = (uint32_t)(_archLayout->sharedMemoryStart + targetSymbolOffsetInCache + target.addend);
                    }
                }
                _dylibToItsExports[target.foundInImage._mh].insert(targetSymbolOffsetInCache);
                if ( target.isWeakDef )
                    _dylibWeakExports.insert({ target.foundInImage._mh, targetSymbolOffsetInCache });
                _exportsToUses[targetSymbolOffsetInCache].push_back(makePatchLocation(fixupLoc - _readExecuteRegion.buffer, pmd, target.addend));
                _exportsToName[targetSymbolOffsetInCache] = target.foundSymbolName;
                break;
            case FixupTarget::Kind::bindMissingSymbol:
                // if there are missing symbols, makeDyldCacheImageArray() will error 
                break;
        }
    };


    // build ImageArray for all dylibs in dyld cache
    dyld3::closure::PathOverrides pathOverrides;
    dyld3::RootsChecker rootsChecker;
    dyld3::closure::ClosureBuilder cb(dyld3::closure::kFirstDyldCacheImageNum, _fileSystem, rootsChecker, cache, false, *_options.archs, pathOverrides,
                                      dyld3::closure::ClosureBuilder::AtPath::none, false, nullptr, _options.platform, handler);
    dyld3::Array<CachedDylibInfo> dylibs(&dylibInfos[0], dylibInfos.size(), dylibInfos.size());
    const dyld3::Array<dyld3::closure::ClosureBuilder::CachedDylibAlias> aliasesArray(dylibAliases.data(), dylibAliases.size(), dylibAliases.size());
    _imageArray = cb.makeDyldCacheImageArray(dylibs, aliasesArray);
    if ( cb.diagnostics().hasError() ) {
        _diagnostics.error("%s", cb.diagnostics().errorMessage().c_str());
        return;
    }
}

static bool operator==(const dyld_cache_patchable_location& a, const dyld_cache_patchable_location& b) {
    return a.cacheOffset == b.cacheOffset;
}

void SharedCacheBuilder::addImageArray()
{
    // build trie of dylib paths
    __block std::vector<DylibIndexTrie::Entry> dylibEntrys;
    _imageArray->forEachImage(^(const dyld3::closure::Image* image, bool& stop) {
        dylibEntrys.push_back(DylibIndexTrie::Entry(image->path(), DylibIndex(image->imageNum()-1)));
        image->forEachAlias(^(const char *aliasPath, bool &innerStop) {
            dylibEntrys.push_back(DylibIndexTrie::Entry(aliasPath, DylibIndex(image->imageNum()-1)));
        });
    });
    DylibIndexTrie dylibsTrie(dylibEntrys);
    std::vector<uint8_t> trieBytes;
    dylibsTrie.emit(trieBytes);
    while ( (trieBytes.size() % 4) != 0 )
        trieBytes.push_back(0);

    // build set of functions to never stub-eliminate because tools may need to override them
    std::unordered_set<std::string> alwaysGeneratePatch;
    for (const char* const* p=_s_neverStubEliminateSymbols; *p != nullptr; ++p)
        alwaysGeneratePatch.insert(*p);

    // Add the patches for the image array.
    __block uint64_t numPatchImages             = _imageArray->size();
    __block uint64_t numPatchExports            = 0;
    __block uint64_t numPatchLocations          = 0;
    __block uint64_t numPatchExportNameBytes    = 0;

    auto needsPatch = [&](bool dylibNeedsPatching, const dyld3::MachOLoaded* mh,
                          CacheOffset offset) -> bool {
        if (dylibNeedsPatching)
            return true;
        if (_dylibWeakExports.find({ mh, offset }) != _dylibWeakExports.end())
            return true;
        const std::string& exportName = _exportsToName[offset];
        return alwaysGeneratePatch.find(exportName) != alwaysGeneratePatch.end();
    };

    // First calculate how much space we need
    const DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    cache->forEachImage(^(const mach_header* mh, const char* installName) {
        const dyld3::MachOLoaded* ml = (const dyld3::MachOLoaded*)mh;
        const std::set<CacheOffset>& dylibExports = _dylibToItsExports[ml];

        // On a customer cache, only store patch locations for interposable dylibs and weak binding
        bool dylibNeedsPatching = cache->isOverridablePath(installName);

        uint64_t numDylibExports = 0;
        for (CacheOffset exportCacheOffset : dylibExports) {
            if (!needsPatch(dylibNeedsPatching, ml, exportCacheOffset))
                continue;
            std::vector<dyld_cache_patchable_location>& uses = _exportsToUses[exportCacheOffset];
            uses.erase(std::unique(uses.begin(), uses.end()), uses.end());
            numPatchLocations += uses.size();

            std::string exportName = _exportsToName[exportCacheOffset];
            numPatchExportNameBytes += exportName.size() + 1;
        }
        numPatchExports += numDylibExports;
    });

    // Now reserve the space
    __block std::vector<dyld_cache_image_patches>       patchImages;
    __block std::vector<dyld_cache_patchable_export>    patchExports;
    __block std::vector<dyld_cache_patchable_location>  patchLocations;
    __block std::vector<char>                           patchExportNames;

    patchImages.reserve(numPatchImages);
    patchExports.reserve(numPatchExports);
    patchLocations.reserve(numPatchLocations);
    patchExportNames.reserve(numPatchExportNameBytes);

    // And now fill it with the patch data
    cache->forEachImage(^(const mach_header* mh, const char* installName) {
        const dyld3::MachOLoaded* ml = (const dyld3::MachOLoaded*)mh;
        const std::set<CacheOffset>& dylibExports = _dylibToItsExports[ml];

        // On a customer cache, only store patch locations for interposable dylibs and weak binding
        bool dylibNeedsPatching = cache->isOverridablePath(installName);

        // Add the patch image which points in to the exports
        dyld_cache_image_patches patchImage;
        patchImage.patchExportsStartIndex   = (uint32_t)patchExports.size();
        patchImage.patchExportsCount        = 0;

        // Then add each export which points to a list of locations and a name
        for (CacheOffset exportCacheOffset : dylibExports) {
            if (!needsPatch(dylibNeedsPatching, ml, exportCacheOffset))
                continue;
            ++patchImage.patchExportsCount;
            std::vector<dyld_cache_patchable_location>& uses = _exportsToUses[exportCacheOffset];

            dyld_cache_patchable_export cacheExport;
            cacheExport.cacheOffsetOfImpl           = (uint32_t)exportCacheOffset;
            cacheExport.patchLocationsStartIndex    = (uint32_t)patchLocations.size();
            cacheExport.patchLocationsCount         = (uint32_t)uses.size();
            cacheExport.exportNameOffset            = (uint32_t)patchExportNames.size();
            patchExports.push_back(cacheExport);

            // Now add the list of locations.
            patchLocations.insert(patchLocations.end(), uses.begin(), uses.end());

            // And add the export name
            const std::string& exportName = _exportsToName[exportCacheOffset];
            patchExportNames.insert(patchExportNames.end(), &exportName[0], &exportName[0] + exportName.size() + 1);
        }
        patchImages.push_back(patchImage);
    });

    while ( (patchExportNames.size() % 4) != 0 )
        patchExportNames.push_back('\0');

    uint64_t patchInfoSize = sizeof(dyld_cache_patch_info);
    patchInfoSize += sizeof(dyld_cache_image_patches) * patchImages.size();
    patchInfoSize += sizeof(dyld_cache_patchable_export) * patchExports.size();
    patchInfoSize += sizeof(dyld_cache_patchable_location) * patchLocations.size();
    patchInfoSize += patchExportNames.size();

    // check for fit
    uint64_t imageArraySize = _imageArray->size();
    size_t freeSpace = _readOnlyRegion.bufferSize - _readOnlyRegion.sizeInUse;
    if ( (imageArraySize+trieBytes.size()+patchInfoSize) > freeSpace ) {
        _diagnostics.error("cache buffer too small to hold ImageArray and Trie (buffer size=%lldMB, imageArray size=%lldMB, trie size=%luKB, patch size=%lluKB, free space=%ldMB)",
                            _allocatedBufferSize/1024/1024, imageArraySize/1024/1024, trieBytes.size()/1024, patchInfoSize/1024, freeSpace/1024/1024);
        return;
    }

    // copy into cache and update header
    DyldSharedCache* dyldCache = (DyldSharedCache*)_readExecuteRegion.buffer;
    dyldCache->header.dylibsImageArrayAddr = _readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse;
    dyldCache->header.dylibsImageArraySize = imageArraySize;
    dyldCache->header.dylibsTrieAddr       = dyldCache->header.dylibsImageArrayAddr + imageArraySize;
    dyldCache->header.dylibsTrieSize       = trieBytes.size();
    ::memcpy(_readOnlyRegion.buffer + _readOnlyRegion.sizeInUse, _imageArray, imageArraySize);
    ::memcpy(_readOnlyRegion.buffer + _readOnlyRegion.sizeInUse + imageArraySize, &trieBytes[0], trieBytes.size());

    // Also write out the patch info
    dyldCache->header.patchInfoAddr = dyldCache->header.dylibsTrieAddr + dyldCache->header.dylibsTrieSize;
    dyldCache->header.patchInfoSize = patchInfoSize;
    dyld_cache_patch_info patchInfo;
    patchInfo.patchTableArrayAddr = dyldCache->header.patchInfoAddr + sizeof(dyld_cache_patch_info);
    patchInfo.patchTableArrayCount = patchImages.size();
    patchInfo.patchExportArrayAddr = patchInfo.patchTableArrayAddr + (patchInfo.patchTableArrayCount * sizeof(dyld_cache_image_patches));
    patchInfo.patchExportArrayCount = patchExports.size();
    patchInfo.patchLocationArrayAddr = patchInfo.patchExportArrayAddr + (patchInfo.patchExportArrayCount * sizeof(dyld_cache_patchable_export));
    patchInfo.patchLocationArrayCount = patchLocations.size();
    patchInfo.patchExportNamesAddr = patchInfo.patchLocationArrayAddr + (patchInfo.patchLocationArrayCount * sizeof(dyld_cache_patchable_location));
    patchInfo.patchExportNamesSize = patchExportNames.size();
    ::memcpy(_readOnlyRegion.buffer + dyldCache->header.patchInfoAddr - _readOnlyRegion.unslidLoadAddress,
             &patchInfo, sizeof(dyld_cache_patch_info));
    ::memcpy(_readOnlyRegion.buffer + patchInfo.patchTableArrayAddr - _readOnlyRegion.unslidLoadAddress,
             &patchImages[0], sizeof(patchImages[0]) * patchImages.size());
    ::memcpy(_readOnlyRegion.buffer + patchInfo.patchExportArrayAddr - _readOnlyRegion.unslidLoadAddress,
             &patchExports[0], sizeof(patchExports[0]) * patchExports.size());
    ::memcpy(_readOnlyRegion.buffer + patchInfo.patchLocationArrayAddr - _readOnlyRegion.unslidLoadAddress,
             &patchLocations[0], sizeof(patchLocations[0]) * patchLocations.size());
    ::memcpy(_readOnlyRegion.buffer + patchInfo.patchExportNamesAddr - _readOnlyRegion.unslidLoadAddress,
             &patchExportNames[0], patchExportNames.size());

    _readOnlyRegion.sizeInUse += align(imageArraySize+trieBytes.size()+patchInfoSize,14);

    // Free the underlying image array buffer
    _imageArray->deallocate();
    _imageArray = nullptr;
}

void SharedCacheBuilder::addOtherImageArray(const std::vector<LoadedMachO>& otherDylibsAndBundles, std::vector<const LoadedMachO*>& overflowDylibs)
{
    DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    dyld3::closure::PathOverrides pathOverrides;
    dyld3::closure::FileSystemNull nullFileSystem;
    dyld3::RootsChecker rootsChecker;
    dyld3::closure::ClosureBuilder cb(dyld3::closure::kFirstOtherOSImageNum, nullFileSystem, rootsChecker, cache, false, *_options.archs, pathOverrides,
                                      dyld3::closure::ClosureBuilder::AtPath::none, false, nullptr, _options.platform);

    // make ImageArray for other dylibs and bundles
    STACK_ALLOC_ARRAY(dyld3::closure::LoadedFileInfo, others, otherDylibsAndBundles.size() + overflowDylibs.size());
    for (const LoadedMachO& other : otherDylibsAndBundles) {
        if ( !contains(other.loadedFileInfo.path, "staged_system_apps/") )
            others.push_back(other.loadedFileInfo);
    }

    for (const LoadedMachO* dylib : overflowDylibs) {
        if (dylib->mappedFile.mh->canHavePrecomputedDlopenClosure(dylib->mappedFile.runtimePath.c_str(), ^(const char*) {}) )
            others.push_back(dylib->loadedFileInfo);
    }

    // Sort the others array by name so that it is deterministic
    std::sort(others.begin(), others.end(),
              [](const dyld3::closure::LoadedFileInfo& a, const dyld3::closure::LoadedFileInfo& b) {
                  // Sort mac before iOSMac
                  bool isIOSMacA = strncmp(a.path, "/System/iOSSupport/", 19) == 0;
                  bool isIOSMacB = strncmp(b.path, "/System/iOSSupport/", 19) == 0;
                  if (isIOSMacA != isIOSMacB)
                      return !isIOSMacA;
                  return strcmp(a.path, b.path) < 0;
              });

    const dyld3::closure::ImageArray* otherImageArray = cb.makeOtherDylibsImageArray(others, (uint32_t)_sortedDylibs.size());

    // build trie of paths
    __block std::vector<DylibIndexTrie::Entry> otherEntrys;
    otherImageArray->forEachImage(^(const dyld3::closure::Image* image, bool& stop) {
        if ( !image->isInvalid() )
            otherEntrys.push_back(DylibIndexTrie::Entry(image->path(), DylibIndex(image->imageNum())));
    });
    DylibIndexTrie dylibsTrie(otherEntrys);
    std::vector<uint8_t> trieBytes;
    dylibsTrie.emit(trieBytes);
    while ( (trieBytes.size() % 4) != 0 )
        trieBytes.push_back(0);

    // check for fit
    uint64_t imageArraySize = otherImageArray->size();
    size_t freeSpace = _readOnlyRegion.bufferSize - _readOnlyRegion.sizeInUse;
    if ( imageArraySize+trieBytes.size() > freeSpace ) {
        _diagnostics.error("cache buffer too small to hold ImageArray and Trie (buffer size=%lldMB, imageArray size=%lldMB, trie size=%luKB, free space=%ldMB)",
                           _allocatedBufferSize/1024/1024, imageArraySize/1024/1024, trieBytes.size()/1024, freeSpace/1024/1024);
        return;
    }

    // copy into cache and update header
    DyldSharedCache* dyldCache = (DyldSharedCache*)_readExecuteRegion.buffer;
    dyldCache->header.otherImageArrayAddr = _readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse;
    dyldCache->header.otherImageArraySize = imageArraySize;
    dyldCache->header.otherTrieAddr       = dyldCache->header.otherImageArrayAddr + imageArraySize;
    dyldCache->header.otherTrieSize       = trieBytes.size();
    ::memcpy(_readOnlyRegion.buffer + _readOnlyRegion.sizeInUse, otherImageArray, imageArraySize);
    ::memcpy(_readOnlyRegion.buffer + _readOnlyRegion.sizeInUse + imageArraySize, &trieBytes[0], trieBytes.size());
    _readOnlyRegion.sizeInUse += align(imageArraySize+trieBytes.size(),14);

    // Free the underlying buffer
    otherImageArray->deallocate();
}


void SharedCacheBuilder::addClosures(const std::vector<LoadedMachO>& osExecutables)
{
    const DyldSharedCache* dyldCache = (DyldSharedCache*)_readExecuteRegion.buffer;

    __block std::vector<Diagnostics> osExecutablesDiags;
    __block std::vector<const dyld3::closure::LaunchClosure*> osExecutablesClosures;
    osExecutablesDiags.resize(osExecutables.size());
    osExecutablesClosures.resize(osExecutables.size());

    dispatch_apply(osExecutables.size(), DISPATCH_APPLY_AUTO, ^(size_t index) {
        const LoadedMachO& loadedMachO = osExecutables[index];
        // don't pre-build closures for staged apps into dyld cache, since they won't run from that location
        if ( startsWith(loadedMachO.mappedFile.runtimePath, "/private/var/staged_system_apps/") ) {
            return;
        }

        // prebuilt closures use the cdhash of the dylib to verify that the dylib is still the same
        // at runtime as when the shared cache processed it.  We must have a code signature to record this information
        uint32_t codeSigFileOffset;
        uint32_t codeSigSize;
        if ( !loadedMachO.mappedFile.mh->hasCodeSignature(codeSigFileOffset, codeSigSize) ) {
            return;
        }

        dyld3::closure::PathOverrides pathOverrides;
        dyld3::RootsChecker rootsChecker;
        dyld3::closure::ClosureBuilder builder(dyld3::closure::kFirstLaunchClosureImageNum, _fileSystem, rootsChecker, dyldCache, false, *_options.archs, pathOverrides,
                                               dyld3::closure::ClosureBuilder::AtPath::all, false, nullptr, _options.platform, nullptr);
        bool issetuid = false;
        if ( this->_options.platform == dyld3::Platform::macOS || dyld3::MachOFile::isSimulatorPlatform(this->_options.platform) )
            _fileSystem.fileExists(loadedMachO.loadedFileInfo.path, nullptr, nullptr, &issetuid, nullptr);
        const dyld3::closure::LaunchClosure* mainClosure = builder.makeLaunchClosure(loadedMachO.loadedFileInfo, issetuid);
        if ( builder.diagnostics().hasError() ) {
           osExecutablesDiags[index].error("%s", builder.diagnostics().errorMessage().c_str());
        }
        else {
            assert(mainClosure != nullptr);
            osExecutablesClosures[index] = mainClosure;
        }
    });

    std::map<std::string, const dyld3::closure::LaunchClosure*> closures;
    for (uint64_t i = 0, e = osExecutables.size(); i != e; ++i) {
        const LoadedMachO& loadedMachO = osExecutables[i];
        const Diagnostics& diag = osExecutablesDiags[i];
        if (diag.hasError()) {
            if ( _options.verbose ) {
                _diagnostics.warning("building closure for '%s': %s", loadedMachO.mappedFile.runtimePath.c_str(), diag.errorMessage().c_str());
                for (const std::string& warn : diag.warnings() )
                    _diagnostics.warning("%s", warn.c_str());
            }
            if ( loadedMachO.inputFile && (loadedMachO.inputFile->mustBeIncluded()) ) {
                loadedMachO.inputFile->diag.error("%s", diag.errorMessage().c_str());
            }
        } else {
            // Note, a closure could be null here if it has a path we skip.
            if (osExecutablesClosures[i] != nullptr)
                closures[loadedMachO.mappedFile.runtimePath] = osExecutablesClosures[i];
        }
    }

    osExecutablesDiags.clear();
    osExecutablesClosures.clear();

    // preflight space needed
    size_t closuresSpace = 0;
    for (const auto& entry : closures) {
        closuresSpace += entry.second->size();
    }
    size_t freeSpace = _readOnlyRegion.bufferSize - _readOnlyRegion.sizeInUse;
    if ( closuresSpace > freeSpace ) {
        _diagnostics.error("cache buffer too small to hold all closures (buffer size=%lldMB, closures size=%ldMB, free space=%ldMB)",
                            _allocatedBufferSize/1024/1024, closuresSpace/1024/1024, freeSpace/1024/1024);
        return;
    }
    DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    cache->header.progClosuresAddr = _readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse;
    uint8_t* closuresBase = _readOnlyRegion.buffer + _readOnlyRegion.sizeInUse;
    std::vector<DylibIndexTrie::Entry> closureEntrys;
    uint32_t currentClosureOffset = 0;
    for (const auto& entry : closures) {
        const dyld3::closure::LaunchClosure* closure = entry.second;
        closureEntrys.push_back(DylibIndexTrie::Entry(entry.first, DylibIndex(currentClosureOffset)));
        size_t size = closure->size();
        assert((size % 4) == 0);
        memcpy(closuresBase+currentClosureOffset, closure, size);
        currentClosureOffset += size;
        freeSpace -= size;
        closure->deallocate();
    }
    cache->header.progClosuresSize = currentClosureOffset;
    _readOnlyRegion.sizeInUse += currentClosureOffset;
    freeSpace = _readOnlyRegion.bufferSize - _readOnlyRegion.sizeInUse;
    // build trie of indexes into closures list
    DylibIndexTrie closureTrie(closureEntrys);
    std::vector<uint8_t> trieBytes;
    closureTrie.emit(trieBytes);
    while ( (trieBytes.size() % 8) != 0 )
        trieBytes.push_back(0);
    if ( trieBytes.size() > freeSpace ) {
        _diagnostics.error("cache buffer too small to hold all closures trie (buffer size=%lldMB, trie size=%ldMB, free space=%ldMB)",
                            _allocatedBufferSize/1024/1024, trieBytes.size()/1024/1024, freeSpace/1024/1024);
        return;
    }
    memcpy(_readOnlyRegion.buffer + _readOnlyRegion.sizeInUse, &trieBytes[0], trieBytes.size());
    cache->header.progClosuresTrieAddr = _readOnlyRegion.unslidLoadAddress + _readOnlyRegion.sizeInUse;
    cache->header.progClosuresTrieSize = trieBytes.size();
    _readOnlyRegion.sizeInUse += trieBytes.size();
    _readOnlyRegion.sizeInUse = align(_readOnlyRegion.sizeInUse, 14);
}

void SharedCacheBuilder::emitContantObjects() {
    if ( _coalescedText.cfStrings.bufferSize == 0 )
        return;

    assert(_coalescedText.cfStrings.isaInstallName != nullptr);
    DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    __block uint64_t targetSymbolOffsetInCache = 0;
    __block const dyld3::MachOAnalyzer* targetSymbolMA = nullptr;
    __block const dyld3::MachOAnalyzer* libdyldMA = nullptr;
    cache->forEachImage(^(const mach_header* mh, const char* installName) {
        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)mh;

        if ( strcmp(installName, "/usr/lib/system/libdyld.dylib") == 0 ) {
            libdyldMA = ma;
        }

        if ( targetSymbolOffsetInCache != 0 )
            return;
        if ( strcmp(installName, _coalescedText.cfStrings.isaInstallName) != 0 )
            return;
        dyld3::MachOAnalyzer::FoundSymbol foundInfo;
        bool foundSymbol = ma->findExportedSymbol(_diagnostics, _coalescedText.cfStrings.isaClassName,
                                                  false, foundInfo, nullptr);
        if ( foundSymbol ) {
            targetSymbolOffsetInCache = (uint8_t*)ma - _readExecuteRegion.buffer + foundInfo.value;
            targetSymbolMA = ma;
        }
    });
    if ( targetSymbolOffsetInCache == 0 ) {
        _diagnostics.error("Could not find export of '%s' in '%s'", _coalescedText.cfStrings.isaClassName,
                           _coalescedText.cfStrings.isaInstallName);
        return;
    }
    if ( libdyldMA == nullptr ) {
        _diagnostics.error("Could not libdyld.dylib in shared cache");
        return;
    }

    // If all binds to this symbol were via CF constants, then we'll never have seen the ISA patch export
    // os add it now just in case
    _dylibToItsExports[targetSymbolMA].insert(targetSymbolOffsetInCache);
    _exportsToName[targetSymbolOffsetInCache] = _coalescedText.cfStrings.isaClassName;

    // CFString's have so far just been memcpy'ed from the source dylib to the shared cache.
    // We now need to rewrite their ISAs to be rebases to the ___CFConstantStringClassReference class
    const uint64_t cfStringAtomSize = (uint64_t)DyldSharedCache::ConstantClasses::cfStringAtomSize;
    assert( (_coalescedText.cfStrings.bufferSize % cfStringAtomSize) == 0);
    for (uint64_t bufferOffset = 0; bufferOffset != _coalescedText.cfStrings.bufferSize; bufferOffset += cfStringAtomSize) {
        uint8_t* atomBuffer = _coalescedText.cfStrings.bufferAddr + bufferOffset;
        // The ISA fixup is at an offset of 0 in to the atom
        uint8_t* fixupLoc = atomBuffer;
        // We purposefully want to remove the pointer authentication from the ISA so
        // just use an empty pointer metadata
        dyld3::Loader::PointerMetaData pmd;
        uint64_t addend = 0;
        _exportsToUses[targetSymbolOffsetInCache].push_back(makePatchLocation(fixupLoc - _readExecuteRegion.buffer, pmd, addend));
        *(uint64_t*)fixupLoc = _archLayout->sharedMemoryStart + targetSymbolOffsetInCache;
        _aslrTracker.add(fixupLoc);
    }

    // Set the ranges in the libdyld in the shared cache.  At runtime we can use these to quickly check if a given address
    // is a valid constant
    typedef std::pair<const uint8_t*, const uint8_t*> ObjCConstantRange;
    std::pair<const void*, uint64_t> sharedCacheRanges = cache->getObjCConstantRange();
    uint64_t numRanges = sharedCacheRanges.second / sizeof(ObjCConstantRange);
    dyld3::Array<ObjCConstantRange> rangeArray((ObjCConstantRange*)sharedCacheRanges.first, numRanges, numRanges);

    if ( numRanges > dyld_objc_string_kind ) {
        rangeArray[dyld_objc_string_kind].first = (const uint8_t*)_coalescedText.cfStrings.bufferVMAddr;
        rangeArray[dyld_objc_string_kind].second = rangeArray[dyld_objc_string_kind].first + _coalescedText.cfStrings.bufferSize;
        _aslrTracker.add(&rangeArray[dyld_objc_string_kind].first);
        _aslrTracker.add(&rangeArray[dyld_objc_string_kind].second);
    }

    // Update the __SHARED_CACHE range in libdyld to contain the cf/objc constants
    libdyldMA->forEachLoadCommand(_diagnostics, ^(const load_command* cmd, bool& stop) {
        // We don't handle 32-bit as this is only needed for pointer authentication
        assert(cmd->cmd != LC_SEGMENT);
        if ( cmd->cmd == LC_SEGMENT_64 ) {
            segment_command_64* seg = (segment_command_64*)cmd;
            if ( strcmp(seg->segname, "__SHARED_CACHE") == 0 ) {
                // Update the range of this segment, and any sections inside
                seg->vmaddr     = _coalescedText.cfStrings.bufferVMAddr;
                seg->vmsize     = _coalescedText.cfStrings.bufferSize;
                seg->fileoff    = _coalescedText.cfStrings.cacheFileOffset;
                seg->fileoff    = _coalescedText.cfStrings.bufferSize;
                section_64* const sectionsStart = (section_64*)((char*)seg + sizeof(struct segment_command_64));
                section_64* const sectionsEnd   = &sectionsStart[seg->nsects];
                for (section_64* sect=sectionsStart; sect < sectionsEnd; ++sect) {
                    if ( !strcmp(sect->sectname, "__cfstring") ) {
                        sect->addr      = _coalescedText.cfStrings.bufferVMAddr;
                        sect->size      = _coalescedText.cfStrings.bufferSize;
                        sect->offset    = (uint32_t)_coalescedText.cfStrings.cacheFileOffset;
                    }
                }
                stop = true;
            }
        }
    });
}


bool SharedCacheBuilder::writeCache(void (^cacheSizeCallback)(uint64_t size), bool (^copyCallback)(const uint8_t* src, uint64_t size, uint64_t dstOffset))
{
    const dyld_cache_header*       cacheHeader = (dyld_cache_header*)_readExecuteRegion.buffer;
    const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)(_readExecuteRegion.buffer + cacheHeader->mappingOffset);
    const uint32_t mappingsCount = cacheHeader->mappingCount;
    // Check the sizes of all the regions are correct
    assert(_readExecuteRegion.sizeInUse       == mappings[0].size);
    for (uint32_t i = 0; i != _dataRegions.size(); ++i) {
        assert(_dataRegions[i].sizeInUse == mappings[i + 1].size);
    }
    assert(_readOnlyRegion.sizeInUse          == mappings[mappingsCount - 1].size);

    // Check the file offsets of all the regions are correct
    assert(_readExecuteRegion.cacheFileOffset == mappings[0].fileOffset);
    for (uint32_t i = 0; i != _dataRegions.size(); ++i) {
        assert(_dataRegions[i].cacheFileOffset   == mappings[i + 1].fileOffset);
    }
    assert(_readOnlyRegion.cacheFileOffset    == mappings[mappingsCount - 1].fileOffset);
    assert(_codeSignatureRegion.sizeInUse     == cacheHeader->codeSignatureSize);
    assert(cacheHeader->codeSignatureOffset   == _readOnlyRegion.cacheFileOffset+_readOnlyRegion.sizeInUse+_localSymbolsRegion.sizeInUse);

    // Make sure the slidable mappings have the same ranges as the original mappings
    const dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + cacheHeader->mappingWithSlideOffset);
    assert(cacheHeader->mappingCount == cacheHeader->mappingWithSlideCount);
    for (uint32_t i = 0; i != cacheHeader->mappingCount; ++i) {
        assert(mappings[i].address      == slidableMappings[i].address);
        assert(mappings[i].size         == slidableMappings[i].size);
        assert(mappings[i].fileOffset   == slidableMappings[i].fileOffset);
        assert(mappings[i].maxProt      == slidableMappings[i].maxProt);
        assert(mappings[i].initProt     == slidableMappings[i].initProt);
    }

    // Now that we know everything is correct, actually copy the data
    cacheSizeCallback(_readExecuteRegion.sizeInUse+dataRegionsSizeInUse()+_readOnlyRegion.sizeInUse+_localSymbolsRegion.sizeInUse+_codeSignatureRegion.sizeInUse);
    bool fullyWritten = copyCallback(_readExecuteRegion.buffer, _readExecuteRegion.sizeInUse, mappings[0].fileOffset);
    for (uint32_t i = 0; i != _dataRegions.size(); ++i) {
        fullyWritten &= copyCallback(_dataRegions[i].buffer, _dataRegions[i].sizeInUse, mappings[i + 1].fileOffset);
    }
    fullyWritten &= copyCallback(_readOnlyRegion.buffer, _readOnlyRegion.sizeInUse, mappings[cacheHeader->mappingCount - 1].fileOffset);
    if ( _localSymbolsRegion.sizeInUse != 0 ) {
        assert(cacheHeader->localSymbolsOffset == mappings[cacheHeader->mappingCount - 1].fileOffset+_readOnlyRegion.sizeInUse);
        fullyWritten &= copyCallback(_localSymbolsRegion.buffer, _localSymbolsRegion.sizeInUse, cacheHeader->localSymbolsOffset);
    }
    fullyWritten &= copyCallback(_codeSignatureRegion.buffer, _codeSignatureRegion.sizeInUse, cacheHeader->codeSignatureOffset);
    return fullyWritten;
}


void SharedCacheBuilder::writeFile(const std::string& path)
{
    std::string pathTemplate = path + "-XXXXXX";
    size_t templateLen = strlen(pathTemplate.c_str())+2;
    BLOCK_ACCCESSIBLE_ARRAY(char, pathTemplateSpace, templateLen);
    strlcpy(pathTemplateSpace, pathTemplate.c_str(), templateLen);
    int fd = mkstemp(pathTemplateSpace);
    if ( fd != -1 ) {
        auto cacheSizeCallback = ^(uint64_t size) {
            // set final cache file size (may help defragment file)
            ::ftruncate(fd, size);
        };
        auto copyCallback = ^(const uint8_t* src, uint64_t size, uint64_t dstOffset) {
            uint64_t writtenSize = pwrite(fd, src, size, dstOffset);
            return writtenSize == size;
        };
        // <rdar://problem/55370916> TOCTOU: verify path is still a realpath (not changed)
        char tempPath[MAXPATHLEN];
        if ( ::fcntl(fd, F_GETPATH, tempPath) == 0 ) {
            size_t tempPathLen = strlen(tempPath);
            if ( tempPathLen > 7 )
                tempPath[tempPathLen-7] = '\0'; // remove trailing -xxxxxx
            if ( path != tempPath ) {
                _diagnostics.error("output file path changed from: '%s' to: '%s'", path.c_str(), tempPath);
                ::close(fd);
                return;
            }
        }
        else {
            _diagnostics.error("unable to fcntl(fd, F_GETPATH) on output file");
            ::close(fd);
            return;
        }
        bool fullyWritten = writeCache(cacheSizeCallback, copyCallback);
        if ( fullyWritten ) {
            ::fchmod(fd, S_IRUSR|S_IRGRP|S_IROTH); // mkstemp() makes file "rw-------", switch it to "r--r--r--"
            // <rdar://problem/55370916> TOCTOU: verify path is still a realpath (not changed)
            // For MRM bringup, dyld installs symlinks from:
            //   dyld_shared_cache_x86_64 -> ../../../../System/Library/dyld/dyld_shared_cache_x86_64
            //   dyld_shared_cache_x86_64h -> ../../../../System/Library/dyld/dyld_shared_cache_x86_64h
            // We don't want to follow that symlink when we install the cache, but instead write over it
            auto lastSlash = path.find_last_of("/");
            if ( lastSlash != std::string::npos ) {
                std::string directoryPath = path.substr(0, lastSlash);

                char resolvedPath[PATH_MAX];
                ::realpath(directoryPath.c_str(), resolvedPath);
                // Note: if the target cache file does not already exist, realpath() will return NULL, but still fill in the path buffer
                if ( directoryPath != resolvedPath ) {
                    _diagnostics.error("output directory file path changed from: '%s' to: '%s'", directoryPath.c_str(), resolvedPath);
                    return;
                }
            }
            if ( ::rename(pathTemplateSpace, path.c_str()) == 0) {
                ::close(fd);
                return; // success
            } else {
                _diagnostics.error("could not rename file '%s' to: '%s'", pathTemplateSpace, path.c_str());
            }
        }
        else {
            _diagnostics.error("could not write file %s", pathTemplateSpace);
        }
        ::close(fd);
        ::unlink(pathTemplateSpace);
    }
    else {
        _diagnostics.error("could not open file %s", pathTemplateSpace);
    }
}

void SharedCacheBuilder::writeBuffer(uint8_t*& buffer, uint64_t& bufferSize) {
    auto cacheSizeCallback = ^(uint64_t size) {
        buffer = (uint8_t*)malloc(size);
        bufferSize = size;
    };
    auto copyCallback = ^(const uint8_t* src, uint64_t size, uint64_t dstOffset) {
        memcpy(buffer + dstOffset, src, size);
        return true;
    };
    bool fullyWritten = writeCache(cacheSizeCallback, copyCallback);
    assert(fullyWritten);
}

void SharedCacheBuilder::writeMapFile(const std::string& path)
{
    std::string mapContent = getMapFileBuffer();
    safeSave(mapContent.c_str(), mapContent.size(), path);
}

std::string SharedCacheBuilder::getMapFileBuffer() const
{
    const DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    return cache->mapFile();
}

std::string SharedCacheBuilder::getMapFileJSONBuffer(const std::string& cacheDisposition) const
{
    const DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    return cache->generateJSONMap(cacheDisposition.c_str());
}

void SharedCacheBuilder::markPaddingInaccessible()
{
    // region between RX and RW
    uint8_t* startPad1 = _readExecuteRegion.buffer+_readExecuteRegion.sizeInUse;
    uint8_t* endPad1   = firstDataRegion()->buffer;
    ::vm_protect(mach_task_self(), (vm_address_t)startPad1, endPad1-startPad1, false, 0);

    // region between RW and RO
    const Region* lastRegion = lastDataRegion();
    uint8_t* startPad2 = lastRegion->buffer+lastRegion->sizeInUse;
    uint8_t* endPad2   = _readOnlyRegion.buffer;
    ::vm_protect(mach_task_self(), (vm_address_t)startPad2, endPad2-startPad2, false, 0);
}


void SharedCacheBuilder::forEachCacheDylib(void (^callback)(const std::string& path)) {
    for (const DylibInfo& dylibInfo : _sortedDylibs)
        callback(dylibInfo.dylibID);
}


void SharedCacheBuilder::forEachCacheSymlink(void (^callback)(const std::string& path))
{
    const DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    const dyld3::closure::ImageArray* images = cache->cachedDylibsImageArray();
    if ( images == nullptr )
        return;

    // Aliases we folded in to the cache are in the cache dylib closures
    images->forEachImage(^(const dyld3::closure::Image *image, bool &stop) {
        image->forEachAlias(^(const char *aliasPath, bool &stop) {
            callback(aliasPath);
        });
    });
}


uint64_t SharedCacheBuilder::pathHash(const char* path)
{
    uint64_t sum = 0;
    for (const char* s=path; *s != '\0'; ++s)
        sum += sum*4 + *s;
    return sum;
}


void SharedCacheBuilder::findDylibAndSegment(const void* contentPtr, std::string& foundDylibName, std::string& foundSegName)
{
    foundDylibName = "???";
    foundSegName   = "???";
    uint64_t unslidVmAddr = ((uint8_t*)contentPtr - _readExecuteRegion.buffer) + _readExecuteRegion.unslidLoadAddress;
    const DyldSharedCache* cache = (DyldSharedCache*)_readExecuteRegion.buffer;
    cache->forEachImage(^(const mach_header* mh, const char* installName) {
        ((dyld3::MachOLoaded*)mh)->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& info, bool &stop) {
            if ( (unslidVmAddr >= info.vmAddr) && (unslidVmAddr < (info.vmAddr+info.vmSize)) ) {
                foundDylibName = installName;
                foundSegName   = info.segName;
                stop           = true;
            }
        });
    });
}


void SharedCacheBuilder::fipsSign()
{
    // find libcorecrypto.dylib in cache being built
    DyldSharedCache* dyldCache = (DyldSharedCache*)_readExecuteRegion.buffer;
    __block const dyld3::MachOLoaded* ml = nullptr;
    dyldCache->forEachImage(^(const mach_header* mh, const char* installName) {
        if ( strcmp(installName, "/usr/lib/system/libcorecrypto.dylib") == 0 )
            ml = (dyld3::MachOLoaded*)mh;
    });
    if ( ml == nullptr ) {
        _diagnostics.warning("Could not find libcorecrypto.dylib, skipping FIPS sealing");
        return;
    }

    // find location in libcorecrypto.dylib to store hash of __text section
    uint64_t hashStoreSize;
    const void* hashStoreLocation = ml->findSectionContent("__TEXT", "__fips_hmacs", hashStoreSize);
    if ( hashStoreLocation == nullptr ) {
        _diagnostics.warning("Could not find __TEXT/__fips_hmacs section in libcorecrypto.dylib, skipping FIPS sealing");
        return;
    }
    if ( hashStoreSize != 32 ) {
        _diagnostics.warning("__TEXT/__fips_hmacs section in libcorecrypto.dylib is not 32 bytes in size, skipping FIPS sealing");
        return;
    }

    // compute hmac hash of __text section
    uint64_t textSize;
    const void* textLocation = ml->findSectionContent("__TEXT", "__text", textSize);
    if ( textLocation == nullptr ) {
        _diagnostics.warning("Could not find __TEXT/__text section in libcorecrypto.dylib, skipping FIPS sealing");
        return;
    }
    unsigned char hmac_key = 0;
    CCHmac(kCCHmacAlgSHA256, &hmac_key, 1, textLocation, textSize, (void*)hashStoreLocation); // store hash directly into hashStoreLocation
}

void SharedCacheBuilder::codeSign()
{
    uint8_t  dscHashType;
    uint8_t  dscHashSize;
    uint32_t dscDigestFormat;
    bool agile = false;

    // select which codesigning hash
    switch (_options.codeSigningDigestMode) {
        case DyldSharedCache::Agile:
            agile = true;
            // Fall through to SHA1, because the main code directory remains SHA1 for compatibility.
            [[clang::fallthrough]];
        case DyldSharedCache::SHA1only:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            dscHashType     = CS_HASHTYPE_SHA1;
            dscHashSize     = CS_HASH_SIZE_SHA1;
            dscDigestFormat = kCCDigestSHA1;
#pragma clang diagnostic pop
            break;
        case DyldSharedCache::SHA256only:
            dscHashType     = CS_HASHTYPE_SHA256;
            dscHashSize     = CS_HASH_SIZE_SHA256;
            dscDigestFormat = kCCDigestSHA256;
            break;
        default:
            _diagnostics.error("codeSigningDigestMode has unknown, unexpected value %d, bailing out.",
                               _options.codeSigningDigestMode);
            return;
    }

    std::string cacheIdentifier = "com.apple.dyld.cache.";
    cacheIdentifier +=  _options.archs->name();
    if ( _options.dylibsRemovedDuringMastering ) {
        if ( _options.optimizeStubs  )
            cacheIdentifier +=  ".release";
        else
            cacheIdentifier += ".development";
    }
    // get pointers into shared cache buffer
    size_t          inBbufferSize = _readExecuteRegion.sizeInUse+dataRegionsSizeInUse()+_readOnlyRegion.sizeInUse+_localSymbolsRegion.sizeInUse;

    const uint16_t pageSize = _archLayout->csPageSize;

    // layout code signature contents
    uint32_t blobCount     = agile ? 4 : 3;
    size_t   idSize        = cacheIdentifier.size()+1; // +1 for terminating 0
    uint32_t slotCount     = (uint32_t)((inBbufferSize + pageSize - 1) / pageSize);
    uint32_t xSlotCount    = CSSLOT_REQUIREMENTS;
    size_t   idOffset      = offsetof(CS_CodeDirectory, end_withExecSeg);
    size_t   hashOffset    = idOffset+idSize + dscHashSize*xSlotCount;
    size_t   hash256Offset = idOffset+idSize + CS_HASH_SIZE_SHA256*xSlotCount;
    size_t   cdSize        = hashOffset + (slotCount * dscHashSize);
    size_t   cd256Size     = agile ? hash256Offset + (slotCount * CS_HASH_SIZE_SHA256) : 0;
    size_t   reqsSize      = 12;
    size_t   cmsSize       = sizeof(CS_Blob);
    size_t   cdOffset      = sizeof(CS_SuperBlob) + blobCount*sizeof(CS_BlobIndex);
    size_t   cd256Offset   = cdOffset + cdSize;
    size_t   reqsOffset    = cd256Offset + cd256Size; // equals cdOffset + cdSize if not agile
    size_t   cmsOffset     = reqsOffset + reqsSize;
    size_t   sbSize        = cmsOffset + cmsSize;
    size_t   sigSize       = align(sbSize, 14);       // keep whole cache 16KB aligned

    // allocate space for blob
    vm_address_t codeSigAlloc;
    if ( vm_allocate(mach_task_self(), &codeSigAlloc, sigSize, VM_FLAGS_ANYWHERE) != 0 ) {
        _diagnostics.error("could not allocate code signature buffer");
        return;
    }
    _codeSignatureRegion.buffer     = (uint8_t*)codeSigAlloc;
    _codeSignatureRegion.bufferSize = sigSize;
    _codeSignatureRegion.sizeInUse  = sigSize;

    // create overall code signature which is a superblob
    CS_SuperBlob* sb = reinterpret_cast<CS_SuperBlob*>(_codeSignatureRegion.buffer);
    sb->magic           = htonl(CSMAGIC_EMBEDDED_SIGNATURE);
    sb->length          = htonl(sbSize);
    sb->count           = htonl(blobCount);
    sb->index[0].type   = htonl(CSSLOT_CODEDIRECTORY);
    sb->index[0].offset = htonl(cdOffset);
    sb->index[1].type   = htonl(CSSLOT_REQUIREMENTS);
    sb->index[1].offset = htonl(reqsOffset);
    sb->index[2].type   = htonl(CSSLOT_CMS_SIGNATURE);
    sb->index[2].offset = htonl(cmsOffset);
    if ( agile ) {
        sb->index[3].type = htonl(CSSLOT_ALTERNATE_CODEDIRECTORIES + 0);
        sb->index[3].offset = htonl(cd256Offset);
    }

    // fill in empty requirements
    CS_RequirementsBlob* reqs = (CS_RequirementsBlob*)(((char*)sb)+reqsOffset);
    reqs->magic  = htonl(CSMAGIC_REQUIREMENTS);
    reqs->length = htonl(sizeof(CS_RequirementsBlob));
    reqs->data   = 0;

    // initialize fixed fields of Code Directory
    CS_CodeDirectory* cd = (CS_CodeDirectory*)(((char*)sb)+cdOffset);
    cd->magic           = htonl(CSMAGIC_CODEDIRECTORY);
    cd->length          = htonl(cdSize);
    cd->version         = htonl(0x20400);               // supports exec segment
    cd->flags           = htonl(kSecCodeSignatureAdhoc);
    cd->hashOffset      = htonl(hashOffset);
    cd->identOffset     = htonl(idOffset);
    cd->nSpecialSlots   = htonl(xSlotCount);
    cd->nCodeSlots      = htonl(slotCount);
    cd->codeLimit       = htonl(inBbufferSize);
    cd->hashSize        = dscHashSize;
    cd->hashType        = dscHashType;
    cd->platform        = 0;                            // not platform binary
    cd->pageSize        = __builtin_ctz(pageSize);      // log2(CS_PAGE_SIZE);
    cd->spare2          = 0;                            // unused (must be zero)
    cd->scatterOffset   = 0;                            // not supported anymore
    cd->teamOffset      = 0;                            // no team ID
    cd->spare3          = 0;                            // unused (must be zero)
    cd->codeLimit64     = 0;                            // falls back to codeLimit

    // executable segment info
    cd->execSegBase     = htonll(_readExecuteRegion.cacheFileOffset); // base of TEXT segment
    cd->execSegLimit    = htonll(_readExecuteRegion.sizeInUse);       // size of TEXT segment
    cd->execSegFlags    = 0;                                          // not a main binary

    // initialize dynamic fields of Code Directory
    strcpy((char*)cd + idOffset, cacheIdentifier.c_str());

    // add special slot hashes
    uint8_t* hashSlot = (uint8_t*)cd + hashOffset;
    uint8_t* reqsHashSlot = &hashSlot[-CSSLOT_REQUIREMENTS*dscHashSize];
    CCDigest(dscDigestFormat, (uint8_t*)reqs, sizeof(CS_RequirementsBlob), reqsHashSlot);

    CS_CodeDirectory* cd256;
    uint8_t* hash256Slot;
    uint8_t* reqsHash256Slot;
    if ( agile ) {
        // Note that the assumption here is that the size up to the hashes is the same as for
        // sha1 code directory, and that they come last, after everything else.

        cd256 = (CS_CodeDirectory*)(((char*)sb)+cd256Offset);
        cd256->magic           = htonl(CSMAGIC_CODEDIRECTORY);
        cd256->length          = htonl(cd256Size);
        cd256->version         = htonl(0x20400);               // supports exec segment
        cd256->flags           = htonl(kSecCodeSignatureAdhoc);
        cd256->hashOffset      = htonl(hash256Offset);
        cd256->identOffset     = htonl(idOffset);
        cd256->nSpecialSlots   = htonl(xSlotCount);
        cd256->nCodeSlots      = htonl(slotCount);
        cd256->codeLimit       = htonl(inBbufferSize);
        cd256->hashSize        = CS_HASH_SIZE_SHA256;
        cd256->hashType        = CS_HASHTYPE_SHA256;
        cd256->platform        = 0;                            // not platform binary
        cd256->pageSize        = __builtin_ctz(pageSize);      // log2(CS_PAGE_SIZE);
        cd256->spare2          = 0;                            // unused (must be zero)
        cd256->scatterOffset   = 0;                            // not supported anymore
        cd256->teamOffset      = 0;                            // no team ID
        cd256->spare3          = 0;                            // unused (must be zero)
        cd256->codeLimit64     = 0;                            // falls back to codeLimit

        // executable segment info
        cd256->execSegBase     = cd->execSegBase;
        cd256->execSegLimit    = cd->execSegLimit;
        cd256->execSegFlags    = cd->execSegFlags;

        // initialize dynamic fields of Code Directory
        strcpy((char*)cd256 + idOffset, cacheIdentifier.c_str());

        // add special slot hashes
        hash256Slot = (uint8_t*)cd256 + hash256Offset;
        reqsHash256Slot = &hash256Slot[-CSSLOT_REQUIREMENTS*CS_HASH_SIZE_SHA256];
        CCDigest(kCCDigestSHA256, (uint8_t*)reqs, sizeof(CS_RequirementsBlob), reqsHash256Slot);
    }
    else {
        cd256 = NULL;
        hash256Slot = NULL;
        reqsHash256Slot = NULL;
    }

    // fill in empty CMS blob for ad-hoc signing
    CS_Blob* cms = (CS_Blob*)(((char*)sb)+cmsOffset);
    cms->magic  = htonl(CSMAGIC_BLOBWRAPPER);
    cms->length = htonl(sizeof(CS_Blob));


    // alter header of cache to record size and location of code signature
    // do this *before* hashing each page
    dyld_cache_header* cache = (dyld_cache_header*)_readExecuteRegion.buffer;
    cache->codeSignatureOffset  = inBbufferSize;
    cache->codeSignatureSize    = sigSize;

    struct SlotRange {
        uint64_t        start   = 0;
        uint64_t        end     = 0;
        const uint8_t*  buffer  = nullptr;
    };
    std::vector<SlotRange> regionSlots;
    // __TEXT
    regionSlots.push_back({ 0, (_readExecuteRegion.sizeInUse / pageSize), _readExecuteRegion.buffer });
    // __DATA
    for (const Region& dataRegion : _dataRegions) {
        // The first data region starts at the end of __TEXT, and subsequent regions are
        // after the previous __DATA region.
        uint64_t previousEnd = regionSlots.back().end;
        uint64_t numSlots = dataRegion.sizeInUse / pageSize;
        regionSlots.push_back({ previousEnd, previousEnd + numSlots, dataRegion.buffer });
    }
    // __LINKEDIT
    {
        uint64_t previousEnd = regionSlots.back().end;
        uint64_t numSlots = _readOnlyRegion.sizeInUse / pageSize;
        regionSlots.push_back({ previousEnd, previousEnd + numSlots, _readOnlyRegion.buffer });
    }
    // local symbols
    if ( _localSymbolsRegion.sizeInUse != 0 ) {
        uint64_t previousEnd = regionSlots.back().end;
        uint64_t numSlots = _localSymbolsRegion.sizeInUse / pageSize;
        regionSlots.push_back({ previousEnd, previousEnd + numSlots, _localSymbolsRegion.buffer });
    }

    auto codeSignPage = ^(size_t i) {
        // move to correct region
        for (const SlotRange& slotRange : regionSlots) {
            if ( (i >= slotRange.start) && (i < slotRange.end) ) {
                const uint8_t* code = slotRange.buffer + ((i - slotRange.start) * pageSize);

                CCDigest(dscDigestFormat, code, pageSize, hashSlot + (i * dscHashSize));

                if ( agile ) {
                    CCDigest(kCCDigestSHA256, code, pageSize, hash256Slot + (i * CS_HASH_SIZE_SHA256));
                }
                return;
            }
        }
        assert(0 && "Out of range slot");
    };

    // compute hashes
    dispatch_apply(slotCount, DISPATCH_APPLY_AUTO, ^(size_t i) {
        codeSignPage(i);
    });

    // Now that we have a code signature, compute a cache UUID by hashing the code signature blob
    {
        uint8_t* uuidLoc = cache->uuid;
        assert(uuid_is_null(uuidLoc));
        static_assert(offsetof(dyld_cache_header, uuid) / CS_PAGE_SIZE_4K == 0, "uuid is expected in the first page of the cache");
        uint8_t fullDigest[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256((const void*)cd, (unsigned)cdSize, fullDigest);
        memcpy(uuidLoc, fullDigest, 16);
        // <rdar://problem/6723729> uuids should conform to RFC 4122 UUID version 4 & UUID version 5 formats
        uuidLoc[6] = ( uuidLoc[6] & 0x0F ) | ( 3 << 4 );
        uuidLoc[8] = ( uuidLoc[8] & 0x3F ) | 0x80;

        // Now codesign page 0 again, because we modified it by setting uuid in header
        codeSignPage(0);
    }

    // hash of entire code directory (cdHash) uses same hash as each page
    uint8_t fullCdHash[dscHashSize];
    CCDigest(dscDigestFormat, (const uint8_t*)cd, cdSize, fullCdHash);
    // Note: cdHash is defined as first 20 bytes of hash
    memcpy(_cdHashFirst, fullCdHash, 20);
    if ( agile ) {
        uint8_t fullCdHash256[CS_HASH_SIZE_SHA256];
        CCDigest(kCCDigestSHA256, (const uint8_t*)cd256, cd256Size, fullCdHash256);
        // Note: cdHash is defined as first 20 bytes of hash, even for sha256
        memcpy(_cdHashSecond, fullCdHash256, 20);
    }
    else {
        memset(_cdHashSecond, 0, 20);
    }
}

const bool SharedCacheBuilder::agileSignature()
{
    return _options.codeSigningDigestMode == DyldSharedCache::Agile;
}

static const std::string cdHash(uint8_t hash[20])
{
    char buff[48];
    for (int i = 0; i < 20; ++i)
        sprintf(&buff[2*i], "%2.2x", hash[i]);
    return buff;
}

const std::string SharedCacheBuilder::cdHashFirst()
{
    return cdHash(_cdHashFirst);
}

const std::string SharedCacheBuilder::cdHashSecond()
{
    return cdHash(_cdHashSecond);
}

const std::string SharedCacheBuilder::uuid() const
{
    dyld_cache_header* cache = (dyld_cache_header*)_readExecuteRegion.buffer;
    uuid_string_t uuidStr;
    uuid_unparse(cache->uuid, uuidStr);
    return uuidStr;
}

void SharedCacheBuilder::forEachDylibInfo(void (^callback)(const DylibInfo& dylib, Diagnostics& dylibDiag)) {
    for (const DylibInfo& dylibInfo : _sortedDylibs) {
        // The shared cache builder doesn't use per-dylib errors right now
        // so just share the global diagnostics
        callback(dylibInfo, _diagnostics);
    }
}



template <typename P>
bool SharedCacheBuilder::makeRebaseChainV2(uint8_t* pageContent, uint16_t lastLocationOffset, uint16_t offset, const dyld_cache_slide_info2* info)
{
    typedef typename P::uint_t     pint_t;

    const pint_t   deltaMask    = (pint_t)(info->delta_mask);
    const pint_t   valueMask    = ~deltaMask;
    const pint_t   valueAdd     = (pint_t)(info->value_add);
    const unsigned deltaShift   = __builtin_ctzll(deltaMask) - 2;
    const uint32_t maxDelta     = (uint32_t)(deltaMask >> deltaShift);

    pint_t* lastLoc = (pint_t*)&pageContent[lastLocationOffset+0];
    pint_t lastValue = (pint_t)P::getP(*lastLoc);
    if ( (lastValue - valueAdd) & deltaMask ) {
        std::string dylibName;
        std::string segName;
        findDylibAndSegment((void*)pageContent, dylibName, segName);
        _diagnostics.error("rebase pointer (0x%0lX) does not point within cache. lastOffset=0x%04X, seg=%s, dylib=%s\n",
                            (long)lastValue, lastLocationOffset, segName.c_str(), dylibName.c_str());
        return false;
    }
    if ( offset <= (lastLocationOffset+maxDelta) ) {
        // previous location in range, make link from it
        // encode this location into last value
        pint_t delta = offset - lastLocationOffset;
        pint_t newLastValue = ((lastValue - valueAdd) & valueMask) | (delta << deltaShift);
        //warning("  add chain: delta = %d, lastOffset=0x%03X, offset=0x%03X, org value=0x%08lX, new value=0x%08lX",
        //                    offset - lastLocationOffset, lastLocationOffset, offset, (long)lastValue, (long)newLastValue);
        uint8_t highByte;
        if ( _aslrTracker.hasHigh8(lastLoc, &highByte) ) {
            uint64_t tbi = (uint64_t)highByte << 56;
            newLastValue |= tbi;
        }
        P::setP(*lastLoc, newLastValue);
        return true;
    }
    //fprintf(stderr, "  too big delta = %d, lastOffset=0x%03X, offset=0x%03X\n", offset - lastLocationOffset, lastLocationOffset, offset);

    // distance between rebase locations is too far
    // see if we can make a chain from non-rebase locations
    uint16_t nonRebaseLocationOffsets[1024];
    unsigned nrIndex = 0;
    for (uint16_t i = lastLocationOffset; i < offset-maxDelta; ) {
        nonRebaseLocationOffsets[nrIndex] = 0;
        for (int j=maxDelta; j > 0; j -= 4) {
            pint_t value = (pint_t)P::getP(*(pint_t*)&pageContent[i+j]);
            if ( value == 0 ) {
                // Steal values of 0 to be used in the rebase chain
                nonRebaseLocationOffsets[nrIndex] = i+j;
                break;
            }
        }
        if ( nonRebaseLocationOffsets[nrIndex] == 0 ) {
            lastValue = (pint_t)P::getP(*lastLoc);
            pint_t newValue = ((lastValue - valueAdd) & valueMask);
            //warning("   no way to make non-rebase delta chain, terminate off=0x%03X, old value=0x%08lX, new value=0x%08lX", lastLocationOffset, (long)value, (long)newValue);
            P::setP(*lastLoc, newValue);
            return false;
        }
        i = nonRebaseLocationOffsets[nrIndex];
        ++nrIndex;
    }

    // we can make chain. go back and add each non-rebase location to chain
    uint16_t prevOffset = lastLocationOffset;
    pint_t* prevLoc = (pint_t*)&pageContent[prevOffset];
    for (unsigned n=0; n < nrIndex; ++n) {
        uint16_t nOffset = nonRebaseLocationOffsets[n];
        assert(nOffset != 0);
        pint_t* nLoc = (pint_t*)&pageContent[nOffset];
        pint_t delta2 = nOffset - prevOffset;
        pint_t value = (pint_t)P::getP(*prevLoc);
        pint_t newValue;
        if ( value == 0 )
            newValue = (delta2 << deltaShift);
        else
            newValue = ((value - valueAdd) & valueMask) | (delta2 << deltaShift);
        //warning("    non-rebase delta = %d, to off=0x%03X, old value=0x%08lX, new value=0x%08lX", delta2, nOffset, (long)value, (long)newValue);
        P::setP(*prevLoc, newValue);
        prevOffset = nOffset;
        prevLoc = nLoc;
    }
    pint_t delta3 = offset - prevOffset;
    pint_t value = (pint_t)P::getP(*prevLoc);
    pint_t newValue;
    if ( value == 0 )
        newValue = (delta3 << deltaShift);
    else
        newValue = ((value - valueAdd) & valueMask) | (delta3 << deltaShift);
    //warning("    non-rebase delta = %d, to off=0x%03X, old value=0x%08lX, new value=0x%08lX", delta3, offset, (long)value, (long)newValue);
    P::setP(*prevLoc, newValue);

    return true;
}


template <typename P>
void SharedCacheBuilder::addPageStartsV2(uint8_t* pageContent, const bool bitmap[], const dyld_cache_slide_info2* info,
                                         std::vector<uint16_t>& pageStarts, std::vector<uint16_t>& pageExtras)
{
    typedef typename P::uint_t     pint_t;

    const pint_t   deltaMask    = (pint_t)(info->delta_mask);
    const pint_t   valueMask    = ~deltaMask;
    const uint32_t pageSize     = info->page_size;
    const pint_t   valueAdd     = (pint_t)(info->value_add);

    uint16_t startValue = DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE;
    uint16_t lastLocationOffset = 0xFFFF;
    for(uint32_t i=0; i < pageSize/4; ++i) {
        unsigned offset = i*4;
        if ( bitmap[i] ) {
            if ( startValue == DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE ) {
                // found first rebase location in page
                startValue = i;
            }
            else if ( !makeRebaseChainV2<P>(pageContent, lastLocationOffset, offset, info) ) {
                // can't record all rebasings in one chain
                if ( (startValue & DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA) == 0 ) {
                    // switch page_start to "extras" which is a list of chain starts
                    unsigned indexInExtras = (unsigned)pageExtras.size();
                    if ( indexInExtras > 0x3FFF ) {
                        _diagnostics.error("rebase overflow in v2 page extras");
                        return;
                    }
                    pageExtras.push_back(startValue);
                    startValue = indexInExtras | DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA;
                }
                pageExtras.push_back(i);
            }
            lastLocationOffset = offset;
        }
    }
    if ( lastLocationOffset != 0xFFFF ) {
        // mark end of chain
        pint_t* lastLoc = (pint_t*)&pageContent[lastLocationOffset];
        pint_t lastValue = (pint_t)P::getP(*lastLoc);
        pint_t newValue = ((lastValue - valueAdd) & valueMask);
        P::setP(*lastLoc, newValue);
    }
    if ( startValue & DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA ) {
        // add end bit to extras
        pageExtras.back() |= DYLD_CACHE_SLIDE_PAGE_ATTR_END;
    }
    pageStarts.push_back(startValue);
}

template <typename P>
void SharedCacheBuilder::writeSlideInfoV2(const bool bitmapForAllDataRegions[], unsigned dataPageCountForAllDataRegions)
{
    typedef typename P::uint_t    pint_t;
    typedef typename P::E         E;

    const uint32_t  pageSize = _aslrTracker.pageSize();
    const uint8_t*  firstDataRegionBuffer = firstDataRegion()->buffer;
    for (uint32_t dataRegionIndex = 0; dataRegionIndex != _dataRegions.size(); ++dataRegionIndex) {
        Region& dataRegion = _dataRegions[dataRegionIndex];

        // fill in fixed info
        assert(dataRegion.slideInfoFileOffset != 0);
        assert((dataRegion.sizeInUse % pageSize) == 0);
        unsigned dataPageCount = (uint32_t)dataRegion.sizeInUse / pageSize;
        dyld_cache_slide_info2* info = (dyld_cache_slide_info2*)dataRegion.slideInfoBuffer;
        info->version    = 2;
        info->page_size  = pageSize;
        info->delta_mask = _archLayout->pointerDeltaMask;
        info->value_add  = _archLayout->useValueAdd ? _archLayout->sharedMemoryStart : 0;

        // set page starts and extras for each page
        std::vector<uint16_t> pageStarts;
        std::vector<uint16_t> pageExtras;
        pageStarts.reserve(dataPageCount);

        const size_t bitmapEntriesPerPage = (sizeof(bool)*(pageSize/4));
        uint8_t* pageContent = dataRegion.buffer;
        unsigned numPagesFromFirstDataRegion = (uint32_t)(dataRegion.buffer - firstDataRegionBuffer) / pageSize;
        assert((numPagesFromFirstDataRegion + dataPageCount) <= dataPageCountForAllDataRegions);
        const bool* bitmapForRegion = (const bool*)bitmapForAllDataRegions + (bitmapEntriesPerPage * numPagesFromFirstDataRegion);
        const bool* bitmapForPage = bitmapForRegion;
        for (unsigned i=0; i < dataPageCount; ++i) {
            //warning("page[%d]", i);
            addPageStartsV2<P>(pageContent, bitmapForPage, info, pageStarts, pageExtras);
            if ( _diagnostics.hasError() ) {
                return;
            }
            pageContent += pageSize;
            bitmapForPage += (sizeof(bool)*(pageSize/4));
        }

        // fill in computed info
        info->page_starts_offset = sizeof(dyld_cache_slide_info2);
        info->page_starts_count  = (unsigned)pageStarts.size();
        info->page_extras_offset = (unsigned)(sizeof(dyld_cache_slide_info2)+pageStarts.size()*sizeof(uint16_t));
        info->page_extras_count  = (unsigned)pageExtras.size();
        uint16_t* pageStartsBuffer = (uint16_t*)((char*)info + info->page_starts_offset);
        uint16_t* pageExtrasBuffer = (uint16_t*)((char*)info + info->page_extras_offset);
        for (unsigned i=0; i < pageStarts.size(); ++i)
            pageStartsBuffer[i] = pageStarts[i];
        for (unsigned i=0; i < pageExtras.size(); ++i)
            pageExtrasBuffer[i] = pageExtras[i];
        // update header with final size
        uint64_t slideInfoSize = align(info->page_extras_offset + pageExtras.size()*sizeof(uint16_t), _archLayout->sharedRegionAlignP2);
        dataRegion.slideInfoFileSize = slideInfoSize;
        if ( dataRegion.slideInfoFileSize > dataRegion.slideInfoBufferSizeAllocated ) {
            _diagnostics.error("kernel slide info overflow buffer");
        }
        // Update the mapping entry on the cache header
        const dyld_cache_header*       cacheHeader = (dyld_cache_header*)_readExecuteRegion.buffer;
        dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + cacheHeader->mappingWithSlideOffset);
        slidableMappings[1 + dataRegionIndex].slideInfoFileSize = dataRegion.slideInfoFileSize;
        //fprintf(stderr, "pageCount=%u, page_starts_count=%lu, page_extras_count=%lu\n", dataPageCount, pageStarts.size(), pageExtras.size());
    }
}

#if SUPPORT_ARCH_arm64_32 || SUPPORT_ARCH_armv7k
// fits in to int16_t
static bool smallValue(uint64_t value)
{
    uint32_t high = (value & 0xFFFF8000);
    return (high == 0) || (high == 0xFFFF8000);
}

template <typename P>
bool SharedCacheBuilder::makeRebaseChainV4(uint8_t* pageContent, uint16_t lastLocationOffset, uint16_t offset, const dyld_cache_slide_info4* info)
{
    typedef typename P::uint_t     pint_t;

    const pint_t   deltaMask    = (pint_t)(info->delta_mask);
    const pint_t   valueMask    = ~deltaMask;
    const pint_t   valueAdd     = (pint_t)(info->value_add);
    const unsigned deltaShift   = __builtin_ctzll(deltaMask) - 2;
    const uint32_t maxDelta     = (uint32_t)(deltaMask >> deltaShift);

    pint_t* lastLoc = (pint_t*)&pageContent[lastLocationOffset+0];
    pint_t lastValue = (pint_t)P::getP(*lastLoc);
    if ( (lastValue - valueAdd) & deltaMask ) {
        std::string dylibName;
        std::string segName;
        findDylibAndSegment((void*)pageContent, dylibName, segName);
        _diagnostics.error("rebase pointer does not point within cache. lastOffset=0x%04X, seg=%s, dylib=%s\n",
                            lastLocationOffset, segName.c_str(), dylibName.c_str());
        return false;
    }
    if ( offset <= (lastLocationOffset+maxDelta) ) {
        // previous location in range, make link from it
        // encode this location into last value
        pint_t delta = offset - lastLocationOffset;
        pint_t newLastValue = ((lastValue - valueAdd) & valueMask) | (delta << deltaShift);
        //warning("  add chain: delta = %d, lastOffset=0x%03X, offset=0x%03X, org value=0x%08lX, new value=0x%08lX",
        //                    offset - lastLocationOffset, lastLocationOffset, offset, (long)lastValue, (long)newLastValue);
        P::setP(*lastLoc, newLastValue);
        return true;
    }
    //fprintf(stderr, "  too big delta = %d, lastOffset=0x%03X, offset=0x%03X\n", offset - lastLocationOffset, lastLocationOffset, offset);

    // distance between rebase locations is too far
    // see if we can make a chain from non-rebase locations
    uint16_t nonRebaseLocationOffsets[1024];
    unsigned nrIndex = 0;
    for (uint16_t i = lastLocationOffset; i < offset-maxDelta; ) {
        nonRebaseLocationOffsets[nrIndex] = 0;
        for (int j=maxDelta; j > 0; j -= 4) {
            pint_t value = (pint_t)P::getP(*(pint_t*)&pageContent[i+j]);
            if ( smallValue(value) ) {
                // Steal values of 0 to be used in the rebase chain
                nonRebaseLocationOffsets[nrIndex] = i+j;
                break;
            }
        }
        if ( nonRebaseLocationOffsets[nrIndex] == 0 ) {
            lastValue = (pint_t)P::getP(*lastLoc);
            pint_t newValue = ((lastValue - valueAdd) & valueMask);
            //fprintf(stderr, "   no way to make non-rebase delta chain, terminate off=0x%03X, old value=0x%08lX, new value=0x%08lX\n",
            //                lastLocationOffset, (long)lastValue, (long)newValue);
            P::setP(*lastLoc, newValue);
            return false;
        }
        i = nonRebaseLocationOffsets[nrIndex];
        ++nrIndex;
    }

    // we can make chain. go back and add each non-rebase location to chain
    uint16_t prevOffset = lastLocationOffset;
    pint_t* prevLoc = (pint_t*)&pageContent[prevOffset];
    for (unsigned n=0; n < nrIndex; ++n) {
        uint16_t nOffset = nonRebaseLocationOffsets[n];
        assert(nOffset != 0);
        pint_t* nLoc = (pint_t*)&pageContent[nOffset];
        uint32_t delta2 = nOffset - prevOffset;
        pint_t value = (pint_t)P::getP(*prevLoc);
        pint_t newValue;
        if ( smallValue(value) )
            newValue = (value & valueMask) | (delta2 << deltaShift);
        else
            newValue = ((value - valueAdd) & valueMask) | (delta2 << deltaShift);
        //warning("    non-rebase delta = %d, to off=0x%03X, old value=0x%08lX, new value=0x%08lX", delta2, nOffset, (long)value, (long)newValue);
        P::setP(*prevLoc, newValue);
        prevOffset = nOffset;
        prevLoc = nLoc;
    }
    uint32_t delta3 = offset - prevOffset;
    pint_t value = (pint_t)P::getP(*prevLoc);
    pint_t newValue;
    if ( smallValue(value) )
        newValue = (value & valueMask) | (delta3 << deltaShift);
    else
        newValue = ((value - valueAdd) & valueMask) | (delta3 << deltaShift);
    //warning("    non-rebase delta = %d, to off=0x%03X, old value=0x%08lX, new value=0x%08lX", delta3, offset, (long)value, (long)newValue);
    P::setP(*prevLoc, newValue);

    return true;
}


template <typename P>
void SharedCacheBuilder::addPageStartsV4(uint8_t* pageContent, const bool bitmap[], const dyld_cache_slide_info4* info,
                                         std::vector<uint16_t>& pageStarts, std::vector<uint16_t>& pageExtras)
{
    typedef typename P::uint_t     pint_t;

    const pint_t   deltaMask    = (pint_t)(info->delta_mask);
    const pint_t   valueMask    = ~deltaMask;
    const uint32_t pageSize     = info->page_size;
    const pint_t   valueAdd     = (pint_t)(info->value_add);

    uint16_t startValue = DYLD_CACHE_SLIDE4_PAGE_NO_REBASE;
    uint16_t lastLocationOffset = 0xFFFF;
    for(uint32_t i=0; i < pageSize/4; ++i) {
        unsigned offset = i*4;
        if ( bitmap[i] ) {
            if ( startValue == DYLD_CACHE_SLIDE4_PAGE_NO_REBASE ) {
                // found first rebase location in page
                startValue = i;
            }
            else if ( !makeRebaseChainV4<P>(pageContent, lastLocationOffset, offset, info) ) {
                // can't record all rebasings in one chain
                if ( (startValue & DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA) == 0 ) {
                    // switch page_start to "extras" which is a list of chain starts
                    unsigned indexInExtras = (unsigned)pageExtras.size();
                    if ( indexInExtras >= DYLD_CACHE_SLIDE4_PAGE_INDEX ) {
                        _diagnostics.error("rebase overflow in v4 page extras");
                        return;
                    }
                    pageExtras.push_back(startValue);
                    startValue = indexInExtras | DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA;
                }
                pageExtras.push_back(i);
            }
            lastLocationOffset = offset;
        }
    }
    if ( lastLocationOffset != 0xFFFF ) {
        // mark end of chain
        pint_t* lastLoc = (pint_t*)&pageContent[lastLocationOffset];
        pint_t lastValue = (pint_t)P::getP(*lastLoc);
        pint_t newValue = ((lastValue - valueAdd) & valueMask);
        P::setP(*lastLoc, newValue);
        if ( startValue & DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA ) {
            // add end bit to extras
            pageExtras.back() |= DYLD_CACHE_SLIDE4_PAGE_EXTRA_END;
        }
    }
    pageStarts.push_back(startValue);
}



template <typename P>
void SharedCacheBuilder::writeSlideInfoV4(const bool bitmapForAllDataRegions[], unsigned dataPageCountForAllDataRegions)
{
    typedef typename P::uint_t    pint_t;
    typedef typename P::E         E;

    const uint32_t  pageSize = _aslrTracker.pageSize();
    const uint8_t*  firstDataRegionBuffer = firstDataRegion()->buffer;
    for (uint32_t dataRegionIndex = 0; dataRegionIndex != _dataRegions.size(); ++dataRegionIndex) {
        Region& dataRegion = _dataRegions[dataRegionIndex];

        // fill in fixed info
        assert(dataRegion.slideInfoFileOffset != 0);
        assert((dataRegion.sizeInUse % pageSize) == 0);
        unsigned dataPageCount = (uint32_t)dataRegion.sizeInUse / pageSize;
        dyld_cache_slide_info4* info = (dyld_cache_slide_info4*)dataRegion.slideInfoBuffer;
        info->version    = 4;
        info->page_size  = pageSize;
        info->delta_mask = _archLayout->pointerDeltaMask;
        info->value_add  = info->value_add  = _archLayout->useValueAdd ? _archLayout->sharedMemoryStart : 0;

        // set page starts and extras for each page
        std::vector<uint16_t> pageStarts;
        std::vector<uint16_t> pageExtras;
        pageStarts.reserve(dataPageCount);
        const size_t bitmapEntriesPerPage = (sizeof(bool)*(pageSize/4));
        uint8_t* pageContent = dataRegion.buffer;
        unsigned numPagesFromFirstDataRegion = (uint32_t)(dataRegion.buffer - firstDataRegionBuffer) / pageSize;
        assert((numPagesFromFirstDataRegion + dataPageCount) <= dataPageCountForAllDataRegions);
        const bool* bitmapForRegion = (const bool*)bitmapForAllDataRegions + (bitmapEntriesPerPage * numPagesFromFirstDataRegion);
        const bool* bitmapForPage = bitmapForRegion;
        for (unsigned i=0; i < dataPageCount; ++i) {
            addPageStartsV4<P>(pageContent, bitmapForPage, info, pageStarts, pageExtras);
            if ( _diagnostics.hasError() ) {
                return;
            }
            pageContent += pageSize;
            bitmapForPage += (sizeof(bool)*(pageSize/4));
        }
        // fill in computed info
        info->page_starts_offset = sizeof(dyld_cache_slide_info4);
        info->page_starts_count  = (unsigned)pageStarts.size();
        info->page_extras_offset = (unsigned)(sizeof(dyld_cache_slide_info4)+pageStarts.size()*sizeof(uint16_t));
        info->page_extras_count  = (unsigned)pageExtras.size();
        uint16_t* pageStartsBuffer = (uint16_t*)((char*)info + info->page_starts_offset);
        uint16_t* pageExtrasBuffer = (uint16_t*)((char*)info + info->page_extras_offset);
        for (unsigned i=0; i < pageStarts.size(); ++i)
            pageStartsBuffer[i] = pageStarts[i];
        for (unsigned i=0; i < pageExtras.size(); ++i)
            pageExtrasBuffer[i] = pageExtras[i];
        // update header with final size
        uint64_t slideInfoSize = align(info->page_extras_offset + pageExtras.size()*sizeof(uint16_t), _archLayout->sharedRegionAlignP2);
        dataRegion.slideInfoFileSize = slideInfoSize;
        if ( dataRegion.slideInfoFileSize > dataRegion.slideInfoBufferSizeAllocated ) {
            _diagnostics.error("kernel slide info overflow buffer");
        }
        // Update the mapping entry on the cache header
        const dyld_cache_header*       cacheHeader = (dyld_cache_header*)_readExecuteRegion.buffer;
        dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + cacheHeader->mappingWithSlideOffset);
        slidableMappings[1 + dataRegionIndex].slideInfoFileSize = dataRegion.slideInfoFileSize;
        //fprintf(stderr, "pageCount=%u, page_starts_count=%lu, page_extras_count=%lu\n", dataPageCount, pageStarts.size(), pageExtras.size());
    }
}
#endif

/*
void CacheBuilder::writeSlideInfoV1()
{
    // build one 128-byte bitmap per page (4096) of DATA
    uint8_t* const dataStart = (uint8_t*)_buffer.get() + regions[1].fileOffset;
    uint8_t* const dataEnd   = dataStart + regions[1].size;
    const long bitmapSize = (dataEnd - dataStart)/(4*8);
    uint8_t* bitmap = (uint8_t*)calloc(bitmapSize, 1);
    for (void* p : _pointersForASLR) {
        if ( (p < dataStart) || ( p > dataEnd) )
            terminate("DATA pointer for sliding, out of range\n");
        long offset = (long)((uint8_t*)p - dataStart);
        if ( (offset % 4) != 0 )
            terminate("pointer not 4-byte aligned in DATA offset 0x%08lX\n", offset);
        long byteIndex = offset / (4*8);
        long bitInByte =  (offset % 32) >> 2;
        bitmap[byteIndex] |= (1 << bitInByte);
    }

    // allocate worst case size block of all slide info
    const unsigned entry_size = 4096/(8*4); // 8 bits per byte, possible pointer every 4 bytes.
    const unsigned toc_count = (unsigned)bitmapSize/entry_size;
    dyld_cache_slide_info* slideInfo = (dyld_cache_slide_info*)((uint8_t*)_buffer + _slideInfoFileOffset);
    slideInfo->version          = 1;
    slideInfo->toc_offset       = sizeof(dyld_cache_slide_info);
    slideInfo->toc_count        = toc_count;
    slideInfo->entries_offset   = (slideInfo->toc_offset+2*toc_count+127)&(-128);
    slideInfo->entries_count    = 0;
    slideInfo->entries_size     = entry_size;
    // append each unique entry
    const dyldCacheSlideInfoEntry* bitmapAsEntries = (dyldCacheSlideInfoEntry*)bitmap;
    dyldCacheSlideInfoEntry* const entriesInSlidInfo = (dyldCacheSlideInfoEntry*)((char*)slideInfo+slideInfo->entries_offset());
    int entry_count = 0;
    for (int i=0; i < toc_count; ++i) {
        const dyldCacheSlideInfoEntry* thisEntry = &bitmapAsEntries[i];
        // see if it is same as one already added
        bool found = false;
        for (int j=0; j < entry_count; ++j) {
            if ( memcmp(thisEntry, &entriesInSlidInfo[j], entry_size) == 0 ) {
                slideInfo->set_toc(i, j);
                found = true;
                break;
            }
        }
        if ( !found ) {
            // append to end
            memcpy(&entriesInSlidInfo[entry_count], thisEntry, entry_size);
            slideInfo->set_toc(i, entry_count++);
        }
    }
    slideInfo->entries_count  = entry_count;
    ::free((void*)bitmap);

    _buffer.header->slideInfoSize = align(slideInfo->entries_offset + entry_count*entry_size, _archLayout->sharedRegionAlignP2);
}

*/


void SharedCacheBuilder::setPointerContentV3(dyld3::MachOLoaded::ChainedFixupPointerOnDisk* loc, uint64_t targetVMAddr, size_t next)
{
    assert(targetVMAddr > _readExecuteRegion.unslidLoadAddress);
    assert(targetVMAddr < _readOnlyRegion.unslidLoadAddress+_readOnlyRegion.sizeInUse);
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk tmp;
    uint16_t diversity;
    bool     hasAddrDiv;
    uint8_t  key;
    if ( _aslrTracker.hasAuthData(loc, &diversity, &hasAddrDiv, &key) ) {
        // if base cache address cannot fit into target, then use offset
        tmp.arm64e.authRebase.target = _readExecuteRegion.unslidLoadAddress;
        if (  tmp.arm64e.authRebase.target != _readExecuteRegion.unslidLoadAddress )
            targetVMAddr -= _readExecuteRegion.unslidLoadAddress;
        loc->arm64e.authRebase.target    = targetVMAddr;
        loc->arm64e.authRebase.diversity = diversity;
        loc->arm64e.authRebase.addrDiv   = hasAddrDiv;
        loc->arm64e.authRebase.key       = key;
        loc->arm64e.authRebase.next      = next;
        loc->arm64e.authRebase.bind      = 0;
        loc->arm64e.authRebase.auth      = 1;
        assert(loc->arm64e.authRebase.target == targetVMAddr && "target truncated");
        assert(loc->arm64e.authRebase.next == next && "next location truncated");
    }
    else {
        uint8_t highByte = 0;
        _aslrTracker.hasHigh8(loc, &highByte);
        // if base cache address cannot fit into target, then use offset
        tmp.arm64e.rebase.target = _readExecuteRegion.unslidLoadAddress;
        if ( tmp.arm64e.rebase.target != _readExecuteRegion.unslidLoadAddress )
            targetVMAddr -= _readExecuteRegion.unslidLoadAddress;
        loc->arm64e.rebase.target   = targetVMAddr;
        loc->arm64e.rebase.high8    = highByte;
        loc->arm64e.rebase.next     = next;
        loc->arm64e.rebase.bind     = 0;
        loc->arm64e.rebase.auth     = 0;
        assert(loc->arm64e.rebase.target == targetVMAddr && "target truncated");
        assert(loc->arm64e.rebase.next == next && "next location truncated");
    }
}

uint16_t SharedCacheBuilder::pageStartV3(uint8_t* pageContent, uint32_t pageSize, const bool bitmap[])
{
    const int maxPerPage = pageSize / 4;
    uint16_t result = DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE;
    dyld3::MachOLoaded::ChainedFixupPointerOnDisk* lastLoc = nullptr;
    for (int i=0; i < maxPerPage; ++i) {
        if ( bitmap[i] ) {
            if ( result == DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE ) {
                // found first rebase location in page
                result = i * 4;
            }
            dyld3::MachOLoaded::ChainedFixupPointerOnDisk* loc = (dyld3::MachOLoaded::ChainedFixupPointerOnDisk*)(pageContent + i*4);;
            if ( lastLoc != nullptr ) {
                // convert vmaddr based pointers to arm64e dyld cache chains
                setPointerContentV3(lastLoc, lastLoc->raw64, loc - lastLoc);
            }
            lastLoc = loc;
        }
    }
    if ( lastLoc != nullptr ) {
        // convert vmaddr based pointers to arm64e dyld cache chain, and mark end of chain
        setPointerContentV3(lastLoc, lastLoc->raw64, 0);
    }
    return result;
}


void SharedCacheBuilder::writeSlideInfoV3(const bool bitmapForAllDataRegions[], unsigned dataPageCountForAllDataRegions)
{
    const uint32_t  pageSize = _aslrTracker.pageSize();
    const uint8_t*  firstDataRegionBuffer = firstDataRegion()->buffer;
    for (uint32_t dataRegionIndex = 0; dataRegionIndex != _dataRegions.size(); ++dataRegionIndex) {
        Region& dataRegion = _dataRegions[dataRegionIndex];
        // fprintf(stderr, "writeSlideInfoV3: %s 0x%llx->0x%llx\n", dataRegion.name.c_str(), dataRegion.cacheFileOffset, dataRegion.cacheFileOffset + dataRegion.sizeInUse);
        // fill in fixed info
        assert(dataRegion.slideInfoFileOffset != 0);
        assert((dataRegion.sizeInUse % pageSize) == 0);
        unsigned dataPageCount = (uint32_t)dataRegion.sizeInUse / pageSize;
        dyld_cache_slide_info3* info = (dyld_cache_slide_info3*)dataRegion.slideInfoBuffer;
        info->version           = 3;
        info->page_size         = pageSize;
        info->page_starts_count = dataPageCount;
        info->auth_value_add    = _archLayout->sharedMemoryStart;

        // fill in per-page starts
        const size_t bitmapEntriesPerPage = (sizeof(bool)*(pageSize/4));
        uint8_t* pageContent = dataRegion.buffer;
        unsigned numPagesFromFirstDataRegion = (uint32_t)(dataRegion.buffer - firstDataRegionBuffer) / pageSize;
        assert((numPagesFromFirstDataRegion + dataPageCount) <= dataPageCountForAllDataRegions);
        const bool* bitmapForRegion = (const bool*)bitmapForAllDataRegions + (bitmapEntriesPerPage * numPagesFromFirstDataRegion);
        const bool* bitmapForPage = bitmapForRegion;
        //for (unsigned i=0; i < dataPageCount; ++i) {
        dispatch_apply(dataPageCount, DISPATCH_APPLY_AUTO, ^(size_t i) {
            info->page_starts[i] = pageStartV3(pageContent + (i * pageSize), pageSize, bitmapForPage + (i * bitmapEntriesPerPage));
        });

        // update region with final size
        dataRegion.slideInfoFileSize = align(__offsetof(dyld_cache_slide_info3, page_starts[dataPageCount]), _archLayout->sharedRegionAlignP2);
        if ( dataRegion.slideInfoFileSize > dataRegion.slideInfoBufferSizeAllocated ) {
            _diagnostics.error("kernel slide info overflow buffer");
        }
        // Update the mapping entry on the cache header
        const dyld_cache_header*       cacheHeader = (dyld_cache_header*)_readExecuteRegion.buffer;
        dyld_cache_mapping_and_slide_info* slidableMappings = (dyld_cache_mapping_and_slide_info*)(_readExecuteRegion.buffer + cacheHeader->mappingWithSlideOffset);
        slidableMappings[1 + dataRegionIndex].slideInfoFileSize = dataRegion.slideInfoFileSize;
    }
}
