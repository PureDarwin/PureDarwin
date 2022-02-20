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

#include <TargetConditionals.h>

#include "ClosureFileSystemPhysical.h"
#include "DyldSharedCache.h"
#include "MachOAnalyzer.h"
#include "RootsChecker.h"

#if DYLD_SIMULATOR_ROOTS_SUPPORT
#include "SharedCacheRuntime.h"
#endif

namespace dyld3 {

#if DYLD_SIMULATOR_ROOTS_SUPPORT
static bool imageUUIDMatchesSharedCache(const char* path, const closure::FileSystem* fileSystem,
                                       const DyldSharedCache* cache, const closure::Image* image) {
    // We can only use the cache if the file UUID matches the shared cache
    bool uuidMatchesCache = false;

    // The simulator can't pass in a file system as its not hooked up to the vector.
    // They can just pass in nullptr where needed and we'll assume its the physical file system
    closure::FileSystemPhysical fileSystemPhysical;
    if ( fileSystem == nullptr )
        fileSystem = &fileSystemPhysical;

    Diagnostics diag;
    Platform platform = cache->platform();
    const GradedArchs& archs = GradedArchs::forName(cache->archName(), true);
    char realerPath[MAXPATHLEN];
    dyld3::closure::LoadedFileInfo loadedFileInfo = dyld3::MachOAnalyzer::load(diag, *fileSystem, path, archs, platform, realerPath);
    const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)loadedFileInfo.fileContent;
    if ( ma != nullptr ) {
        uuid_t uuid;
        uuid_t image_uuid;
        if ( !ma->getUuid(uuid) )
            memset(&uuid, 0, sizeof(uuid_t));
        if ( !image->getUuid(image_uuid) )
            memset(&image_uuid, 0, sizeof(uuid_t));
        uuidMatchesCache = ( memcmp(uuid, image_uuid, sizeof(uuid_t)) == 0 );
        fileSystem->unloadFile(loadedFileInfo);
    }
    return uuidMatchesCache;
}

bool RootsChecker::uuidMatchesSharedCache(const char* path, const closure::FileSystem* fileSystem,
                                          const DyldSharedCache* cache) {
    const dyld3::closure::ImageArray* images = cache->cachedDylibsImageArray();
    const closure::Image* image = nullptr;
    uint32_t imageIndex;
    if ( cache->hasImagePath(path, imageIndex) ) {
        image = images->imageForNum(imageIndex+1);
    }
    return imageUUIDMatchesSharedCache(path, fileSystem, cache, image);
}
#endif

bool RootsChecker::onDiskFileIsRoot(const char* path, const DyldSharedCache* cache,
                                    const closure::Image* image,
                                    const closure::FileSystem* fileSystem,
                                    uint64_t inode, uint64_t modtime) const {

    // Upon entry, we know the dylib exists and has a mod time and inode.  Now
    // we need to see if its a root or not

    // Note we don't check if dylibs are expected on disk here.  We assume that an
    // image only has a mod time and inode if its expected on disk
    uint64_t expectedINode;
    uint64_t expectedMtime;
    if ( image->hasFileModTimeAndInode(expectedINode, expectedMtime) ) {
        if ( (expectedMtime == modtime) && (expectedINode == inode) )
            return false;
        // mod time or inode don't match, so this is a root
        return true;
    }

#if DYLD_SIMULATOR_ROOTS_SUPPORT
    if ( strcmp(path, "/usr/lib/system/libsystem_kernel.dylib") == 0 ) {
        // If this was a root when launchd checked, then assume we are a root now
        if ( libsystemKernelIsRoot )
            return true;

        // If the file system is read-only, then this cannot be a root now
        if ( !fileSystemCanBeModified )
            return false;

        // Possibly a root.  Open the file and check
        return !imageUUIDMatchesSharedCache(path, fileSystem, cache, image);
    } else if ( strcmp(path, "/usr/lib/system/libsystem_platform.dylib") == 0 ) {
        // If this was a root when launchd checked, then assume we are a root now
        if ( libsystemPlatformIsRoot )
            return true;

        // If the file system is read-only, then this cannot be a root now
        if ( !fileSystemCanBeModified )
            return false;

        // Possibly a root.  Open the file and check
        return !imageUUIDMatchesSharedCache(path, fileSystem, cache, image);
    } else if ( strcmp(path, "/usr/lib/system/libsystem_pthread.dylib") == 0 ) {
        // If this was a root when launchd checked, then assume we are a root now
        if ( libsystemPThreadIsRoot )
            return true;

        // If the file system is read-only, then this cannot be a root now
        if ( !fileSystemCanBeModified )
            return false;

        // Possibly a root.  Open the file and check
        return !imageUUIDMatchesSharedCache(path, fileSystem, cache, image);
    }
#endif

    // If we aren't a special simulator dylib, then we must be a root
    return true;
}

} // namespace dyld3
