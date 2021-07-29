/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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



#ifndef __DYLD_ROOTS_CHECKER_H__
#define __DYLD_ROOTS_CHECKER_H__

#include <TargetConditionals.h>

#if TARGET_OS_OSX && (BUILDING_DYLD || BUILDING_CLOSURE_UTIL) && !TARGET_OS_SIMULATOR
#define DYLD_SIMULATOR_ROOTS_SUPPORT 1
#else
#define DYLD_SIMULATOR_ROOTS_SUPPORT 0
#endif

class DyldSharedCache;

namespace dyld3 {

namespace closure {
class FileSystem;
struct Image;
}

#define VIS_HIDDEN __attribute__((visibility("hidden")))

class VIS_HIDDEN RootsChecker
{
public:
    RootsChecker() = default;

    // Does a on-disk binary represent a root.
    // If the cache expects dylibs on disk, then so long as the dylib matches
    // the mod time and inode in the cache, it is not a root.
    // For the macOS simulator support dylibs, which are on disk, whether they
    // are roots depends on the state we are tracking in the comm page.
    // For all other dylibs and all other platforms, on-disk dylibs are always roots
    bool onDiskFileIsRoot(const char* path, const DyldSharedCache* cache, const closure::Image* image,
                          const closure::FileSystem* fileSystem,
                          uint64_t inode, uint64_t modtime) const;

#if DYLD_SIMULATOR_ROOTS_SUPPORT
    static bool uuidMatchesSharedCache(const char* path, const closure::FileSystem* fileSystem,
                                       const DyldSharedCache* cache);

    void setLibsystemKernelIsRoot(bool v) {
        libsystemKernelIsRoot = v;
    }
    void setLibsystemPlatformIsRoot(bool v) {
        libsystemPlatformIsRoot = v;
    }
    void setLibsystemPThreadIsRoot(bool v) {
        libsystemPThreadIsRoot = v;
    }
    void setFileSystemCanBeModified(bool v) {
        fileSystemCanBeModified = v;
    }
#endif

private:
    // By default, assume nothing is a root, but that the file system is writable.
    // This should be conservatively correct.
#if DYLD_SIMULATOR_ROOTS_SUPPORT
    bool libsystemKernelIsRoot      = false;
    bool libsystemPlatformIsRoot    = false;
    bool libsystemPThreadIsRoot     = false;
    bool fileSystemCanBeModified    = true;
#endif
};

} // namespace dyld3

#endif // __DYLD_ROOTS_CHECKER_H__
