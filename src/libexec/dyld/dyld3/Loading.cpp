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


#include <bitset>

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <uuid/uuid.h>
#include <mach/mach.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <sys/dtrace.h>
#include <sys/errno.h>
#include <unistd.h>
#include <System/sys/mman.h>
#include <System/sys/csr.h>
#include <System/machine/cpu_capabilities.h>
#if !TARGET_OS_SIMULATOR && !TARGET_OS_DRIVERKIT
#include <sandbox.h>
#include <sandbox/private.h>
#endif
//#include <dispatch/dispatch.h>
#include <mach/vm_page_size.h>

#include "ClosureFileSystemPhysical.h"
#include "MachOFile.h"
#include "MachOLoaded.h"
#include "MachOAnalyzer.h"
#include "Logging.h"
#include "Loading.h"
#include "RootsChecker.h"
#include "Tracing.h"
#include "dyld2.h"
#include "dyld_cache_format.h"
#include "libdyldEntryVector.h"

#include "objc-shared-cache.h"

namespace dyld {
    void log(const char* m, ...);
}


namespace {

// utility to track a set of ImageNum's in use
class VIS_HIDDEN ImageNumSet
{
public:
    void    add(dyld3::closure::ImageNum num);
    bool    contains(dyld3::closure::ImageNum num) const;

private:
    std::bitset<5120>                                   _bitmap;
    dyld3::OverflowSafeArray<dyld3::closure::ImageNum>  _overflowArray;
};

void ImageNumSet::add(dyld3::closure::ImageNum num)
{
    if ( num < 5120 )
        _bitmap.set(num);
    else
        _overflowArray.push_back(num);
}

bool ImageNumSet::contains(dyld3::closure::ImageNum num) const
{
    if ( num < 5120 )
        return _bitmap.test(num);

    for (dyld3::closure::ImageNum existingNum : _overflowArray) {
        if ( existingNum == num )
            return true;
    }
    return false;
}
} // namespace anonymous


namespace dyld3 {

Loader::Loader(const Array<LoadedImage>& existingImages, Array<LoadedImage>& newImagesStorage,
               const void* cacheAddress, const Array<const dyld3::closure::ImageArray*>& imagesArrays,
               const closure::ObjCSelectorOpt* selOpt, const Array<closure::Image::ObjCSelectorImage>& selImages,
               const RootsChecker& rootsChecker, dyld3::Platform platform,
               LogFunc logLoads, LogFunc logSegments, LogFunc logFixups, LogFunc logDof,
               bool allowMissingLazies, dyld3::LaunchErrorInfo* launchErrorInfo)
       : _existingImages(existingImages), _newImages(newImagesStorage),
         _imagesArrays(imagesArrays), _dyldCacheAddress(cacheAddress), _dyldCacheSelectorOpt(nullptr),
         _closureSelectorOpt(selOpt), _closureSelectorImages(selImages),
         _rootsChecker(rootsChecker), _allowMissingLazies(allowMissingLazies), _platform(platform),
         _logLoads(logLoads), _logSegments(logSegments), _logFixups(logFixups), _logDofs(logDof), _launchErrorInfo(launchErrorInfo)

{
#if BUILDING_DYLD
    // This is only needed for dyld and the launch closure, not the dlopen closures
    if ( _dyldCacheAddress != nullptr ) {
        _dyldCacheSelectorOpt = ((const DyldSharedCache*)_dyldCacheAddress)->objcOpt()->selopt();
    }
#endif
}

void Loader::addImage(const LoadedImage& li)
{
    _newImages.push_back(li);
}

LoadedImage* Loader::findImage(closure::ImageNum targetImageNum) const
{
#if BUILDING_DYLD
    // The launch images are different in dyld vs libdyld.  In dyld, the new images are
    // the launch images, while in libdyld, the existing images are the launch images
    if (LoadedImage* info = _launchImagesCache.findImage(targetImageNum, _newImages)) {
        return info;
    }

    for (uintptr_t index = 0; index != _newImages.count(); ++index) {
        LoadedImage& info = _newImages[index];
        if ( info.image()->representsImageNum(targetImageNum) ) {
            // Try cache this entry for next time
            _launchImagesCache.tryAddImage(targetImageNum, index);
            return &info;
        }
    }
#elif BUILDING_LIBDYLD
    for (const LoadedImage& info : _existingImages) {
        if ( info.image()->representsImageNum(targetImageNum) )
            return (LoadedImage*)&info;
    }
    for (LoadedImage& info : _newImages) {
        if ( info.image()->representsImageNum(targetImageNum) )
            return &info;
    }
#else
#error Must be building dyld or libdyld
#endif
    return nullptr;
}

uintptr_t Loader::resolveTarget(closure::Image::ResolvedSymbolTarget target)
{
    const LoadedImage* info;
    switch ( target.sharedCache.kind ) {
        case closure::Image::ResolvedSymbolTarget::kindSharedCache:
            assert(_dyldCacheAddress != nullptr);
            return (uintptr_t)_dyldCacheAddress + (uintptr_t)target.sharedCache.offset;

        case closure::Image::ResolvedSymbolTarget::kindImage:
            info = findImage(target.image.imageNum);
            assert(info != nullptr);
            return (uintptr_t)(info->loadedAddress()) + (uintptr_t)target.image.offset;

        case closure::Image::ResolvedSymbolTarget::kindAbsolute:
            if ( target.absolute.value & (1ULL << 62) )
                return (uintptr_t)(target.absolute.value | 0xC000000000000000ULL);
            else
                return (uintptr_t)target.absolute.value;
   }
    assert(0 && "malformed ResolvedSymbolTarget");
    return 0;
}


void Loader::completeAllDependents(Diagnostics& diag, bool& someCacheImageOverridden)
{
    bool iOSonMac = (_platform == Platform::iOSMac);
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    if ( _platform == Platform::iOS )
        iOSonMac = true;
#endif
    // accumulate all image overrides (512 is placeholder for max unzippered twins in dyld cache)
    STACK_ALLOC_ARRAY(ImageOverride, overrides, _existingImages.maxCount() + _newImages.maxCount() + 512);
    for (const auto anArray : _imagesArrays) {
        // ignore prebuilt Image* in dyld cache, except for MacCatalyst apps where unzipped twins can override each other
        if ( (anArray->startImageNum() < dyld3::closure::kFirstLaunchClosureImageNum) && !iOSonMac )
            continue;
        anArray->forEachImage(^(const dyld3::closure::Image* image, bool& stop) {
            ImageOverride overrideEntry;
            if ( image->isOverrideOfDyldCacheImage(overrideEntry.inCache) ) {
                someCacheImageOverridden = true;
                overrideEntry.replacement = image->imageNum();
                overrides.push_back(overrideEntry);
            }
        });
    }

    // make cache for fast lookup of already loaded images
    __block ImageNumSet alreadyLoaded;
    for (const LoadedImage& info : _existingImages) {
        alreadyLoaded.add(info.image()->imageNum());
    }
    alreadyLoaded.add(_newImages.begin()->image()->imageNum());

    // for each image in _newImages, starting at the top image, make sure its dependents are in _allImages
    uintptr_t index = 0;
    while ( (index < _newImages.count()) && diag.noError() ) {
        const closure::Image* image = _newImages[index].image();
        //dyld::log("completeAllDependents(): looking at dependents of %s\n", image->path());
        image->forEachDependentImage(^(uint32_t depIndex, closure::Image::LinkKind kind, closure::ImageNum depImageNum, bool& stop) {
            // check if imageNum needs to be changed to an override
            for (const ImageOverride& entry : overrides) {
                if ( entry.inCache == depImageNum ) {
                    depImageNum = entry.replacement;
                    break;
                }
            }
            // check if this dependent is already loaded
            if ( !alreadyLoaded.contains(depImageNum) ) {
                // if not, look in imagesArrays
                const closure::Image* depImage = closure::ImageArray::findImage(_imagesArrays, depImageNum);
                if ( depImage != nullptr ) {
                     //dyld::log("  load imageNum=0x%05X, image path=%s\n", depImageNum, depImage->path());
                     if ( _newImages.freeCount() == 0 ) {
                         diag.error("too many initial images");
                         stop = true;
                     }
                     else {
                         _newImages.push_back(LoadedImage::make(depImage));
                     }
                    alreadyLoaded.add(depImageNum);
                }
                else {
                    diag.error("unable to locate imageNum=0x%04X, depIndex=%d of %s", depImageNum, depIndex, image->path());
                    stop = true;
                }
            }
        });
        ++index;
    }
}

void Loader::mapAndFixupAllImages(Diagnostics& diag, bool processDOFs, bool fromOFI, bool* closureOutOfDate, bool* recoverable)
{
    *closureOutOfDate = false;
    *recoverable      = true;

    // scan array and map images not already loaded
    for (LoadedImage& info : _newImages) {
        if ( info.loadedAddress() != nullptr ) {
            // log main executable's segments
            if ( (info.loadedAddress()->filetype == MH_EXECUTE) && (info.state() == LoadedImage::State::mapped) ) {
                if ( _logSegments("dyld: mapped by kernel %s\n", info.image()->path()) ) {
                    info.image()->forEachDiskSegment(^(uint32_t segIndex, uint32_t fileOffset, uint32_t fileSize, int64_t vmOffset, uint64_t vmSize, uint8_t permissions, bool laterReadOnly, bool& stop) {
                        uint64_t start = (long)info.loadedAddress() + vmOffset;
                        uint64_t end   = start+vmSize-1;
                        if ( (segIndex == 0) && (permissions == 0) ) {
                            start = 0;
                        }
                        _logSegments("%14s (%c%c%c) 0x%012llX->0x%012llX \n", info.loadedAddress()->segmentName(segIndex),
                                    (permissions & PROT_READ) ? 'r' : '.', (permissions & PROT_WRITE) ? 'w' : '.', (permissions & PROT_EXEC) ? 'x' : '.' ,
                                    start, end);
                    });
                }
            }
            // skip over ones already loaded
            continue;
        }
        if ( info.image()->inDyldCache() ) {
            if ( info.image()->overridableDylib() ) {
                struct stat statBuf;
                if ( dyld3::stat(info.image()->path(), &statBuf) == 0 ) {
                    dyld3::closure::FileSystemPhysical fileSystem;
                    if ( _rootsChecker.onDiskFileIsRoot(info.image()->path(), (const DyldSharedCache*)_dyldCacheAddress, info.image(),
                                                        &fileSystem, statBuf.st_ino, statBuf.st_mtime) ) {
                        if ( ((const DyldSharedCache*)_dyldCacheAddress)->header.dylibsExpectedOnDisk ) {
                            diag.error("dylib file mtime/inode changed since closure was built for '%s'", info.image()->path());
                        } else {
                            diag.error("dylib file not expected on disk, must be a root '%s'", info.image()->path());
                        }
                        *closureOutOfDate = true;
                    }
                }
                else if ( (_dyldCacheAddress != nullptr) && ((dyld_cache_header*)_dyldCacheAddress)->dylibsExpectedOnDisk ) {
                    diag.error("dylib file missing, was in dyld shared cache '%s'", info.image()->path());
                    *closureOutOfDate = true;
                }
            }
            if ( diag.noError() ) {
                info.setLoadedAddress((MachOLoaded*)((uintptr_t)_dyldCacheAddress + info.image()->cacheOffset()));
                info.setState(LoadedImage::State::fixedUp);
                if ( _logSegments("dyld: Using from dyld cache %s\n", info.image()->path()) ) {
                    info.image()->forEachCacheSegment(^(uint32_t segIndex, uint64_t vmOffset, uint64_t vmSize, uint8_t permissions, bool &stop) {
                        _logSegments("%14s (%c%c%c) 0x%012lX->0x%012lX \n", info.loadedAddress()->segmentName(segIndex),
                                        (permissions & PROT_READ) ? 'r' : '.', (permissions & PROT_WRITE) ? 'w' : '.', (permissions & PROT_EXEC) ? 'x' : '.' ,
                                        (long)info.loadedAddress()+(long)vmOffset, (long)info.loadedAddress()+(long)vmOffset+(long)vmSize-1);
                     });
                }
            }
        }
        else {
            mapImage(diag, info, fromOFI, closureOutOfDate);
            if ( diag.hasError() )
                break; // out of for loop
        }

    }
    if ( diag.hasError() ) {
        // need to clean up by unmapping any images just mapped
        unmapAllImages();
        return;
    }

    // apply fixups to all but main executable
    LoadedImage* mainInfo = nullptr;
    for (LoadedImage& info : _newImages) {
        // images in shared cache do not need fixups applied
        if ( info.image()->inDyldCache() )
            continue;
        if ( info.loadedAddress()->filetype == MH_EXECUTE ) {
            mainInfo = &info;
            continue;
        }
        // previously loaded images were previously fixed up
        if ( info.state() < LoadedImage::State::fixedUp ) {
            applyFixupsToImage(diag, info);
            if ( diag.hasError() )
                break;
            info.setState(LoadedImage::State::fixedUp);
        }
    }
    if ( diag.hasError() ) {
        // need to clean up by unmapping any images just mapped
        unmapAllImages();
        return;
    }

    if ( mainInfo != nullptr ) {
        // now apply fixups to main executable
        // we do it in this order so that if there is a problem with the dylibs in the closure
        // the main executable is left untouched so the closure can be rebuilt
        applyFixupsToImage(diag, *mainInfo);
        if ( diag.hasError() ) {
            // need to clean up by unmapping any images just mapped
            unmapAllImages();
            // we have already started fixing up the main executable, so we cannot retry the launch again
            *recoverable = false;
            return;
        }
        mainInfo->setState(LoadedImage::State::fixedUp);
    }

    // find and register dtrace DOFs
    if ( processDOFs ) {
        STACK_ALLOC_OVERFLOW_SAFE_ARRAY(DOFInfo, dofImages, _newImages.count());
        for (LoadedImage& info : _newImages) {
            info.image()->forEachDOF(info.loadedAddress(), ^(const void* section) {
                DOFInfo dofInfo;
                dofInfo.dof            = section;
                dofInfo.imageHeader    = info.loadedAddress();
                dofInfo.imageShortName = info.image()->leafName();
                dofImages.push_back(dofInfo);
            });
        }
        registerDOFs(dofImages);
    }
}

void Loader::unmapAllImages()
{
    for (LoadedImage& info : _newImages) {
        if ( !info.image()->inDyldCache() && !info.leaveMapped() ) {
            if ( (info.state() == LoadedImage::State::mapped) || (info.state() == LoadedImage::State::fixedUp) ) {
                _logSegments("dyld: unmapping %s\n", info.image()->path());
                unmapImage(info);
            }
        }
    }
}

bool Loader::sandboxBlocked(const char* path, const char* kind)
{
#if TARGET_OS_SIMULATOR || TARGET_OS_DRIVERKIT
    // sandbox calls not yet supported in dyld_sim
    return false;
#else
    sandbox_filter_type filter = (sandbox_filter_type)(SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT);
    return ( sandbox_check(getpid(), kind, filter, path) > 0 );
#endif
}

bool Loader::sandboxBlockedMmap(const char* path)
{
    return sandboxBlocked(path, "file-map-executable");
}

bool Loader::sandboxBlockedOpen(const char* path)
{
    return sandboxBlocked(path, "file-read-data");
}

bool Loader::sandboxBlockedStat(const char* path)
{
    return sandboxBlocked(path, "file-read-metadata");
}

void Loader::mapImage(Diagnostics& diag, LoadedImage& info, bool fromOFI, bool* closureOutOfDate)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_MAP_IMAGE, info.image()->path(), 0, 0);

    const closure::Image*   image         = info.image();
    uint64_t                sliceOffset   = image->sliceOffsetInFile();
    const uint64_t          totalVMSize   = image->vmSizeToMap();
    uint32_t                codeSignFileOffset;
    uint32_t                codeSignFileSize;
    bool                    isCodeSigned  = image->hasCodeSignature(codeSignFileOffset, codeSignFileSize);

    // open file
    int fd = dyld3::open(info.image()->path(), O_RDONLY, 0);
    if ( fd == -1 ) {
        int openErr = errno;
        if ( (openErr == EPERM) && sandboxBlockedOpen(image->path()) )
            diag.error("file system sandbox blocked open(\"%s\", O_RDONLY)", image->path());
        else
            diag.error("open(\"%s\", O_RDONLY) failed with errno=%d", image->path(), openErr);
        return;
    }

    // get file info
    struct stat statBuf;
#if TARGET_OS_SIMULATOR
    if ( dyld3::stat(image->path(), &statBuf) != 0 ) {
#else
    if ( fstat(fd, &statBuf) != 0 ) {
#endif
        int statErr = errno;
        if ( (statErr == EPERM) && sandboxBlockedStat(image->path()) )
            diag.error("file system sandbox blocked stat(\"%s\")", image->path());
        else
            diag.error("stat(\"%s\") failed with errno=%d", image->path(), statErr);
        close(fd);
        return;
    }

    // verify file has not changed since closure was built
    uint64_t inode;
    uint64_t mtime;
    if ( image->hasFileModTimeAndInode(inode, mtime) ) {
        if ( (statBuf.st_mtime != mtime) || (statBuf.st_ino != inode) ) {
            diag.error("file mtime/inode changed since closure was built for '%s'", image->path());
            *closureOutOfDate = true;
            close(fd);
            return;
        }
    }

    // handle case on iOS where sliceOffset in closure is wrong because file was thinned after cache was built
    if ( (_dyldCacheAddress != nullptr) && !(((dyld_cache_header*)_dyldCacheAddress)->dylibsExpectedOnDisk) ) {
        if ( sliceOffset != 0 ) {
            if ( round_page_kernel(codeSignFileOffset+codeSignFileSize) == round_page_kernel(statBuf.st_size) ) {
                // file is now thin
                sliceOffset = 0;
            }
        }
    }

    if ( isCodeSigned && (sliceOffset == 0) ) {
        uint64_t expectedFileSize = round_page_kernel(codeSignFileOffset+codeSignFileSize);
        uint64_t actualFileSize = round_page_kernel(statBuf.st_size);
        if ( actualFileSize < expectedFileSize ) {
            diag.error("File size too small for code signature");
            *closureOutOfDate = true;
            close(fd);
            return;
        }
        if ( actualFileSize != expectedFileSize ) {
            diag.error("File size doesn't match code signature");
            *closureOutOfDate = true;
            close(fd);
            return;
        }
    }

    // register code signature
    uint64_t coveredCodeLength  = UINT64_MAX;
    if ( isCodeSigned ) {
        auto sigTimer = ScopedTimer(DBG_DYLD_TIMING_ATTACH_CODESIGNATURE, 0, 0, 0);
        fsignatures_t siginfo;
        siginfo.fs_file_start  = sliceOffset;                           // start of mach-o slice in fat file
        siginfo.fs_blob_start  = (void*)(long)(codeSignFileOffset);     // start of CD in mach-o file
        siginfo.fs_blob_size   = codeSignFileSize;                      // size of CD
        int result = fcntl(fd, F_ADDFILESIGS_RETURN, &siginfo);
        if ( result == -1 ) {
            int errnoCopy = errno;
            if ( (errnoCopy == EPERM) || (errnoCopy == EBADEXEC) ) {
                diag.error("code signature invalid (errno=%d) sliceOffset=0x%08llX, codeBlobOffset=0x%08X, codeBlobSize=0x%08X for '%s'",
                            errnoCopy, sliceOffset, codeSignFileOffset, codeSignFileSize, image->path());
#if BUILDING_LIBDYLD
                if ( errnoCopy == EBADEXEC ) {
                    // dlopen closures many be prebuilt in to the shared cache with a code signature, but the dylib is replaced
                    // with one without a code signature.  In that case, lets build a new closure
                    *closureOutOfDate = true;
                }
#endif
            }
            else {
                diag.error("fcntl(fd, F_ADDFILESIGS_RETURN) failed with errno=%d, sliceOffset=0x%08llX, codeBlobOffset=0x%08X, codeBlobSize=0x%08X for '%s'",
                            errnoCopy, sliceOffset, codeSignFileOffset, codeSignFileSize, image->path());
            }
            close(fd);
            return;
        }
        coveredCodeLength = siginfo.fs_file_start;
        if ( coveredCodeLength < codeSignFileOffset ) {
            diag.error("code signature does not cover entire file up to signature");
            close(fd);
            return;
        }
    }

    // <rdar://problem/41015217> dyld should use F_CHECK_LV even on unsigned binaries
    {
        // <rdar://problem/32684903> always call F_CHECK_LV to preflight
        fchecklv checkInfo;
        char  messageBuffer[512];
        messageBuffer[0] = '\0';
        checkInfo.lv_file_start = sliceOffset;
        checkInfo.lv_error_message_size = sizeof(messageBuffer);
        checkInfo.lv_error_message = messageBuffer;
        int res = fcntl(fd, F_CHECK_LV, &checkInfo);
        if ( res == -1 ) {
             diag.error("code signature in (%s) not valid for use in process: %s", image->path(), messageBuffer);
             close(fd);
             return;
        }
    }

    // reserve address range
    vm_address_t loadAddress = 0;
    kern_return_t r = vm_allocate(mach_task_self(), &loadAddress, (vm_size_t)totalVMSize, VM_FLAGS_ANYWHERE);
    if ( r != KERN_SUCCESS ) {
        diag.error("vm_allocate(size=0x%0llX) failed with result=%d", totalVMSize, r);
        close(fd);
        return;
    }

    if ( sliceOffset != 0 )
        _logSegments("dyld: Mapping %s (slice offset=%llu)\n", image->path(), sliceOffset);
    else
        _logSegments("dyld: Mapping %s\n", image->path());

    // map each segment
    __block bool           mmapFailure = false;
    __block const uint8_t* codeSignatureStartAddress = nullptr;
    __block const uint8_t* linkeditEndAddress = nullptr;
    __block bool           mappedFirstSegment = false;
    __block uint64_t       maxFileOffset = 0;
    image->forEachDiskSegment(^(uint32_t segIndex, uint32_t fileOffset, uint32_t fileSize, int64_t vmOffset, uint64_t vmSize, uint8_t permissions, bool laterReadOnly, bool& stop) {
        // <rdar://problem/32363581> Mapping zero filled segments fails with mmap of size 0
        if ( fileSize == 0 )
            return;
        void* segAddress = mmap((void*)(loadAddress+vmOffset), fileSize, permissions, MAP_FIXED | MAP_PRIVATE, fd, sliceOffset+fileOffset);
        int mmapErr = errno;
        if ( segAddress == MAP_FAILED ) {
            if ( mmapErr == EPERM ) {
                if ( sandboxBlockedMmap(image->path()) )
                    diag.error("file system sandbox blocked mmap() of '%s'", image->path());
                else
                    diag.error("code signing blocked mmap() of '%s'", image->path());
            }
            else {
                diag.error("mmap(addr=0x%0llX, size=0x%08X) failed with errno=%d for %s", loadAddress+vmOffset, fileSize, mmapErr, image->path());
            }
            mmapFailure = true;
            stop = true;
        }
        else if ( codeSignFileOffset > fileOffset ) {
            codeSignatureStartAddress = (uint8_t*)segAddress + (codeSignFileOffset-fileOffset);
            linkeditEndAddress = (uint8_t*)segAddress + vmSize;
        }
        // sanity check first segment is mach-o header
        if ( (segAddress != MAP_FAILED) && !mappedFirstSegment ) {
            mappedFirstSegment = true;
            const MachOFile* mf = (MachOFile*)segAddress;
            if ( !mf->isMachO(diag, fileSize) ) {
                mmapFailure = true;
                stop = true;
            }
        }
        if ( !mmapFailure ) {
            const MachOLoaded* lmo = (MachOLoaded*)loadAddress;
            _logSegments("%14s (%c%c%c) 0x%012lX->0x%012lX \n", lmo->segmentName(segIndex),
                         (permissions & PROT_READ) ? 'r' : '.', (permissions & PROT_WRITE) ? 'w' : '.', (permissions & PROT_EXEC) ? 'x' : '.' ,
                         (long)segAddress, (long)segAddress+(long)vmSize-1);
        }
        maxFileOffset = fileOffset + fileSize;
    });
    if ( mmapFailure ) {
        ::vm_deallocate(mach_task_self(), loadAddress, (vm_size_t)totalVMSize);
        ::close(fd);
        return;
    }

    // <rdar://problem/47163421> speculatively read whole slice
    fspecread_t specread = {} ;
    specread.fsr_offset = sliceOffset;
    specread.fsr_length = maxFileOffset;
    specread.fsr_flags  = 0;
    fcntl(fd, F_SPECULATIVE_READ, &specread);
    _logSegments("dyld: Speculatively read offset=0x%08llX, len=0x%08llX, path=%s\n", sliceOffset, maxFileOffset, image->path());

    // close file
    close(fd);

#if BUILDING_LIBDYLD
    // verify file has not changed since closure was built by checking code signature has not changed
    struct CDHashWrapper {
        uint8_t cdHash[20];
    };

    // Get all the hashes for the image
    STACK_ALLOC_OVERFLOW_SAFE_ARRAY(CDHashWrapper, expectedCDHashes, 1);
    image->forEachCDHash(^(const uint8_t *cdHash, bool &stop) {
        CDHashWrapper cdHashWrapper;
        memcpy(cdHashWrapper.cdHash, cdHash, sizeof(CDHashWrapper::cdHash));
        expectedCDHashes.push_back(cdHashWrapper);
    });

    if (!expectedCDHashes.empty()) {
        if (expectedCDHashes.count() != 1) {
            // We should only see a single hash for dylibs
            diag.error("code signature count invalid");
        } else if ( codeSignatureStartAddress == nullptr ) {
            diag.error("code signature missing");
        }
        else if ( codeSignatureStartAddress+codeSignFileSize > linkeditEndAddress ) {
            diag.error("code signature extends beyond end of __LINKEDIT");
        }
        else {
            // Get all the cd hashes for the macho
            STACK_ALLOC_OVERFLOW_SAFE_ARRAY(CDHashWrapper, foundCDHashes, 1);
            const MachOLoaded* lmo = (MachOLoaded*)loadAddress;
            lmo->forEachCDHashOfCodeSignature(codeSignatureStartAddress, codeSignFileSize,
                                              ^(const uint8_t *cdHash) {
                CDHashWrapper cdHashWrapper;
                memcpy(cdHashWrapper.cdHash, cdHash, sizeof(CDHashWrapper::cdHash));
                foundCDHashes.push_back(cdHashWrapper);
            });

            if (foundCDHashes.empty()) {
                diag.error("code signature format invalid");
            } else if (expectedCDHashes.count() != foundCDHashes.count()) {
                diag.error("code signature count invalid");
            } else {
                // We found a hash, so make sure its equal.
                if ( ::memcmp(foundCDHashes[0].cdHash, expectedCDHashes[0].cdHash, 20) != 0 )
                    diag.error("code signature changed since closure was built");
            }
        }
        if ( diag.hasError() ) {
            *closureOutOfDate = true;
            ::vm_deallocate(mach_task_self(), loadAddress, (vm_size_t)totalVMSize);
            return;
        }
    }

#endif

#if (__arm__ || __arm64__) && !TARGET_OS_SIMULATOR
    // tell kernel about fairplay encrypted regions
    uint32_t fpTextOffset;
    uint32_t fpSize;
    if ( image->isFairPlayEncrypted(fpTextOffset, fpSize) ) {
        const mach_header* mh = (mach_header*)loadAddress;
        int result = ::mremap_encrypted(((uint8_t*)mh) + fpTextOffset, fpSize, 1, mh->cputype, mh->cpusubtype);
        if ( result != 0 ) {
            diag.error("could not register fairplay decryption, mremap_encrypted() => %d", result);
            ::vm_deallocate(mach_task_self(), loadAddress, (vm_size_t)totalVMSize);
            return;
        }
    }
#endif

    _logLoads("dyld: load %s\n", image->path());

    timer.setData4((uint64_t)loadAddress);
    info.setLoadedAddress((MachOLoaded*)loadAddress);
    info.setState(LoadedImage::State::mapped);
}

void Loader::unmapImage(LoadedImage& info)
{
    assert(info.loadedAddress() != nullptr);
    ::vm_deallocate(mach_task_self(), (vm_address_t)info.loadedAddress(), (vm_size_t)(info.image()->vmSizeToMap()));
    info.setLoadedAddress(nullptr);
}

void Loader::registerDOFs(const Array<DOFInfo>& dofs)
{
    if ( dofs.empty() )
        return;

    int fd = ::open("/dev/" DTRACEMNR_HELPER, O_RDWR);
    if ( fd < 0 ) {
        _logDofs("can't open /dev/" DTRACEMNR_HELPER " to register dtrace DOF sections\n");
    }
    else {
        // allocate a buffer on the stack for the variable length dof_ioctl_data_t type
        uint8_t buffer[sizeof(dof_ioctl_data_t) + dofs.count()*sizeof(dof_helper_t)];
        dof_ioctl_data_t* ioctlData = (dof_ioctl_data_t*)buffer;

        // fill in buffer with one dof_helper_t per DOF section
        ioctlData->dofiod_count = dofs.count();
        for (unsigned int i=0; i < dofs.count(); ++i) {
            strlcpy(ioctlData->dofiod_helpers[i].dofhp_mod, dofs[i].imageShortName, DTRACE_MODNAMELEN);
            ioctlData->dofiod_helpers[i].dofhp_dof = (uintptr_t)(dofs[i].dof);
            ioctlData->dofiod_helpers[i].dofhp_addr = (uintptr_t)(dofs[i].dof);
        }

        // tell kernel about all DOF sections en mas
        // pass pointer to ioctlData because ioctl() only copies a fixed size amount of data into kernel
        user_addr_t val = (user_addr_t)(unsigned long)ioctlData;
        if ( ioctl(fd, DTRACEHIOC_ADDDOF, &val) != -1 ) {
            // kernel returns a unique identifier for each section in the dofiod_helpers[].dofhp_dof field.
            // Note, the closure marked the image as being never unload, so we don't need to keep the ID around
            // or support unregistering it later.
            for (unsigned int i=0; i < dofs.count(); ++i) {
                _logDofs("dyld: registering DOF section %p in %s with dtrace, ID=0x%08X\n",
                         dofs[i].dof, dofs[i].imageShortName, (int)(ioctlData->dofiod_helpers[i].dofhp_dof));
            }
        }
        else {
            _logDofs("dyld: ioctl to register dtrace DOF section failed\n");
        }
        close(fd);
    }
}

bool Loader::dtraceUserProbesEnabled()
{
#if !TARGET_OS_SIMULATOR
    uint8_t dofEnabled = *((uint8_t*)_COMM_PAGE_DTRACE_DOF_ENABLED);
    return ( (dofEnabled & 1) );
#else
    return false;
#endif
}


void Loader::vmAccountingSetSuspended(bool suspend, LogFunc logger)
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    // <rdar://problem/29099600> dyld should tell the kernel when it is doing fix-ups caused by roots
    logger("vm.footprint_suspend=%d\n", suspend);
    int newValue = suspend ? 1 : 0;
    int oldValue = 0;
    size_t newlen = sizeof(newValue);
    size_t oldlen = sizeof(oldValue);
    sysctlbyname("vm.footprint_suspend", &oldValue, &oldlen, &newValue, newlen);
#endif
}

static const char* targetString(const MachOAnalyzerSet::FixupTarget& target)
{
    switch (target.kind ) {
        case MachOAnalyzerSet::FixupTarget::Kind::rebase:
            return "rebase";
        case MachOAnalyzerSet::FixupTarget::Kind::bindAbsolute:
            return "abolute";
        case MachOAnalyzerSet::FixupTarget::Kind::bindToImage:
            return target.foundSymbolName;
        case MachOAnalyzerSet::FixupTarget::Kind::bindMissingSymbol:
            return "missing";
    }
    return "";
}

void Loader::applyFixupsToImage(Diagnostics& diag, LoadedImage& info)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_APPLY_FIXUPS, (uint64_t)info.loadedAddress(), 0, 0);
    closure::ImageNum       cacheImageNum;
    const char*             leafName         = info.image()->leafName();
    const closure::Image*   image            = info.image();
    const uint8_t*          imageLoadAddress = (uint8_t*)info.loadedAddress();
    uintptr_t               slide            = info.loadedAddress()->getSlide();
    bool                    overrideOfCache  = info.image()->isOverrideOfDyldCacheImage(cacheImageNum);
    
    if ( overrideOfCache )
        vmAccountingSetSuspended(true, _logFixups);
    if ( image->fixupsNotEncoded() ) {
        // make the cache writable for this block
        // We do this lazily, only if we find a symbol which needs to be overridden
        DyldSharedCache::DataConstLazyScopedWriter patcher((const DyldSharedCache*)_dyldCacheAddress, mach_task_self(), (DyldSharedCache::DataConstLogFunc)_logSegments);
        auto* patcherPtr = &patcher;

        WrappedMachO wmo((MachOAnalyzer*)info.loadedAddress(), this, (void*)info.image());
        wmo.forEachFixup(diag,
        ^(uint64_t fixupLocRuntimeOffset, PointerMetaData pmd, const FixupTarget& target, bool& stop) {
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + fixupLocRuntimeOffset);
            uintptr_t value;
            switch ( target.kind ) {
                case MachOAnalyzerSet::FixupTarget::Kind::rebase:
                case MachOAnalyzerSet::FixupTarget::Kind::bindToImage:
                    value = (uintptr_t)(target.foundInImage._mh) + target.offsetInImage;
                    break;
                case MachOAnalyzerSet::FixupTarget::Kind::bindAbsolute:
                    value = (uintptr_t)target.offsetInImage;
                    break;
                case MachOAnalyzerSet::FixupTarget::Kind::bindMissingSymbol:
                    if ( _launchErrorInfo ) {
                        _launchErrorInfo->kind              = DYLD_EXIT_REASON_SYMBOL_MISSING;
                        _launchErrorInfo->clientOfDylibPath = info.image()->path();
                        _launchErrorInfo->targetDylibPath   = target.foundInImage.path();
                        _launchErrorInfo->symbol            = target.requestedSymbolName;
                    }
                    // we have no value to set, and forEachFixup() is about to finish
                    return;
            }
    #if __has_feature(ptrauth_calls)
            if ( pmd.authenticated )
                value = MachOLoaded::ChainedFixupPointerOnDisk::Arm64e::signPointer(value, fixUpLoc, pmd.usesAddrDiversity, pmd.diversity, pmd.key);
    #endif
            if ( pmd.high8 )
                value |= ((uint64_t)pmd.high8 << 56);
            _logFixups("dyld: fixup: %s:%p = %p (%s)\n", leafName, fixUpLoc, (void*)value, targetString(target));
            *fixUpLoc = value;
        },
        ^(uint32_t cachedDylibIndex, uint32_t exportCacheOffset, const FixupTarget& target) {
#if BUILDING_LIBDYLD && __x86_64__
            // Full dlopen closures don't patch weak defs.  Bail out early if we are libdyld to match this behaviour
            return;
#endif
            patcherPtr->makeWriteable();
            ((const DyldSharedCache*)_dyldCacheAddress)->forEachPatchableUseOfExport(cachedDylibIndex, exportCacheOffset, ^(dyld_cache_patchable_location patchLoc) {
                uintptr_t* loc     = (uintptr_t*)(((uint8_t*)_dyldCacheAddress)+patchLoc.cacheOffset);
                uintptr_t  newImpl = (uintptr_t)(target.foundInImage._mh) + target.offsetInImage + DyldSharedCache::getAddend(patchLoc);
    #if __has_feature(ptrauth_calls)
                if ( patchLoc.authenticated )
                    newImpl = MachOLoaded::ChainedFixupPointerOnDisk::Arm64e::signPointer(newImpl, loc, patchLoc.usesAddressDiversity, patchLoc.discriminator, patchLoc.key);
    #endif
                // ignore duplicate patch entries
                if ( *loc != newImpl ) {
                    _logFixups("dyld: cache patch: %p = 0x%0lX\n", loc, newImpl);
                    *loc = newImpl;
                }
            });
        });
#if BUILDING_LIBDYLD && TARGET_OS_OSX
        // <rdar://problem/59265987> support old licenseware plugins on macOS using minimal closures
        __block bool oldBinary = true;
        info.loadedAddress()->forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
            if ( (platform == Platform::macOS) && (sdk >= 0x000A0F00) )
                oldBinary = false;
        });
        if ( oldBinary ) {
            // look for __DATA,__dyld section
            info.loadedAddress()->forEachSection(^(const MachOAnalyzer::SectionInfo& sectInfo, bool malformedSectionRange, bool& stop) {
                if ( (strcmp(sectInfo.sectName, "__dyld") == 0) && (strcmp(sectInfo.segInfo.segName, "__DATA") == 0) ) {
                    // dyld_func_lookup is second pointer in __dyld section
                    uintptr_t* dyldSection = (uintptr_t*)(sectInfo.sectAddr + (uintptr_t)info.loadedAddress());
                    _logFixups("dyld: __dyld section: %p = %p\n", &dyldSection[1], &dyld3::compatFuncLookup);
                    dyldSection[1] = (uintptr_t)&dyld3::compatFuncLookup;
                 }
            });
        }
#endif
    }
    else {
        if ( image->rebasesNotEncoded() ) {
            // <rdar://problem/56172089> some apps have so many rebases the closure file is too big, instead we go back to rebase opcodes
            ((MachOAnalyzer*)imageLoadAddress)->forEachRebase(diag, true, ^(uint64_t imageOffsetToRebase, bool& stop) {
                // this is a rebase, add slide
                uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToRebase);
                *fixUpLoc += slide;
                _logFixups("dyld: fixup: %s:%p += %p\n", leafName, fixUpLoc, (void*)slide);
            });
        }
        image->forEachFixup(^(uint64_t imageOffsetToRebase, bool& stop) {
            // this is a rebase, add slide
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToRebase);
            *fixUpLoc += slide;
            _logFixups("dyld: fixup: %s:%p += %p\n", leafName, fixUpLoc, (void*)slide);
        },
        ^(uint64_t imageOffsetToBind, closure::Image::ResolvedSymbolTarget bindTarget, bool& stop) {
            // this is a bind, set to target
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToBind);
            uintptr_t value = resolveTarget(bindTarget);
            _logFixups("dyld: fixup: %s:%p = %p\n", leafName, fixUpLoc, (void*)value);
            *fixUpLoc = value;
        },
        ^(uint64_t imageOffsetToStartsInfo, const Array<closure::Image::ResolvedSymbolTarget>& targets, bool& stop) {
            // this is a chain of fixups, fix up all
            STACK_ALLOC_OVERFLOW_SAFE_ARRAY(const void*, targetAddrs, 128);
            targetAddrs.reserve(targets.count());
            for (uint32_t i=0; i < targets.count(); ++i)
                targetAddrs.push_back((void*)resolveTarget(targets[i]));
            ((dyld3::MachOAnalyzer*)(info.loadedAddress()))->withChainStarts(diag, imageOffsetToStartsInfo, ^(const dyld_chained_starts_in_image* starts) {
                info.loadedAddress()->fixupAllChainedFixups(diag, starts, slide, targetAddrs, ^(void* loc, void* newValue) {
                    _logFixups("dyld: fixup: %s:%p = %p\n", leafName, loc, newValue);
                });
            });
        },
        ^(uint64_t imageOffsetToFixup) {
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToFixup);
            _logFixups("dyld: fixup objc image info: %s Setting objc image info for precomputed objc\n", leafName);

            MachOAnalyzer::ObjCImageInfo *imageInfo = (MachOAnalyzer::ObjCImageInfo *)fixUpLoc;
            ((MachOAnalyzer::ObjCImageInfo *)imageInfo)->flags |= MachOAnalyzer::ObjCImageInfo::dyldPreoptimized;
        },
        ^(uint64_t imageOffsetToBind, closure::Image::ResolvedSymbolTarget bindTarget, bool& stop) {
            // this is a bind, set to target
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToBind);
            uintptr_t value = resolveTarget(bindTarget);
#if __has_feature(ptrauth_calls)
            // Sign the ISA on arm64e.
            // Unfortunately a hard coded value here is not ideal, but this is ABI so we aren't going to change it
            // This matches the value in libobjc __objc_opt_ptrs: .quad x@AUTH(da, 27361, addr)
            value = MachOLoaded::ChainedFixupPointerOnDisk::Arm64e::signPointer(value, fixUpLoc, true, 27361, 2);
#endif
            _logFixups("dyld: fixup objc protocol: %s:%p = %p\n", leafName, fixUpLoc, (void*)value);
            *fixUpLoc = value;
        },
        ^(uint64_t imageOffsetToFixup, uint32_t selectorIndex, bool inSharedCache, bool &stop) {
            // fixupObjCSelRefs
            closure::Image::ResolvedSymbolTarget fixupTarget;
            if ( inSharedCache ) {
                const char* selectorString = _dyldCacheSelectorOpt->getEntryForIndex(selectorIndex);
                fixupTarget.sharedCache.kind     = closure::Image::ResolvedSymbolTarget::kindSharedCache;
                fixupTarget.sharedCache.offset   = (uint64_t)selectorString - (uint64_t)_dyldCacheAddress;
            } else {
                closure::ImageNum imageNum;
                uint64_t vmOffset;
                bool gotLocation = _closureSelectorOpt->getStringLocation(selectorIndex, _closureSelectorImages, imageNum, vmOffset);
                assert(gotLocation);
                fixupTarget.image.kind = closure::Image::ResolvedSymbolTarget::kindImage;
                fixupTarget.image.imageNum = imageNum;
                fixupTarget.image.offset = vmOffset;
            }
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToFixup);
            uintptr_t value = resolveTarget(fixupTarget);
            _logFixups("dyld: fixup objc selector: %s:%p(was '%s') = %p(now '%s')\n", leafName, fixUpLoc, (const char*)*fixUpLoc, (void*)value, (const char*)value);
            *fixUpLoc = value;
        }, ^(uint64_t imageOffsetToFixup, bool &stop) {
            // fixupObjCStableSwift
            // Class really is stable Swift, pretending to be pre-stable.
            // Fix its lie.
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToFixup);
            uintptr_t value = ((*fixUpLoc) | MachOAnalyzer::ObjCClassInfo::FAST_IS_SWIFT_STABLE) & ~MachOAnalyzer::ObjCClassInfo::FAST_IS_SWIFT_LEGACY;
            _logFixups("dyld: fixup objc stable Swift: %s:%p = %p\n", leafName, fixUpLoc, (void*)value);
            *fixUpLoc = value;
        }, ^(uint64_t imageOffsetToFixup, bool &stop) {
            // fixupObjCMethodList
            // Set the method list to have the uniqued bit set
            uint32_t* fixUpLoc = (uint32_t*)(imageLoadAddress + imageOffsetToFixup);
            uint32_t value = (*fixUpLoc) | MachOAnalyzer::ObjCMethodList::methodListIsUniqued;
            _logFixups("dyld: fixup objc method list: %s:%p = 0x%08x\n", leafName, fixUpLoc, value);
            *fixUpLoc = value;
        });

#if __i386__
        __block bool segmentsMadeWritable = false;
        image->forEachTextReloc(^(uint32_t imageOffsetToRebase, bool& stop) {
            if ( !segmentsMadeWritable )
                setSegmentProtects(info, true);
            uintptr_t* fixUpLoc = (uintptr_t*)(imageLoadAddress + imageOffsetToRebase);
            *fixUpLoc += slide;
            _logFixups("dyld: fixup: %s:%p += %p\n", leafName, fixUpLoc, (void*)slide);
         },
        ^(uint32_t imageOffsetToBind, closure::Image::ResolvedSymbolTarget bindTarget, bool& stop) {
            // FIXME
        });
        if ( segmentsMadeWritable )
            setSegmentProtects(info, false);
#endif
    }

    // make any read-only data segments read-only
    if ( image->hasReadOnlyData() && !image->inDyldCache() ) {
        image->forEachDiskSegment(^(uint32_t segIndex, uint32_t fileOffset, uint32_t fileSize, int64_t vmOffset, uint64_t vmSize, uint8_t permissions, bool laterReadOnly, bool& segStop) {
            if ( laterReadOnly ) {
                ::mprotect((void*)(imageLoadAddress+vmOffset), (size_t)vmSize, VM_PROT_READ);
            }
        });
    }

    if ( overrideOfCache )
        vmAccountingSetSuspended(false, _logFixups);
}

#if __i386__
void Loader::setSegmentProtects(const LoadedImage& info, bool write)
{
    info.image()->forEachDiskSegment(^(uint32_t segIndex, uint32_t fileOffset, uint32_t fileSize, int64_t vmOffset, uint64_t vmSize, uint8_t protections, bool laterReadOnly, bool& segStop) {
        if ( protections & VM_PROT_WRITE )
            return;
        uint32_t regionProt = protections;
        if ( write )
            regionProt = VM_PROT_WRITE | VM_PROT_READ;
        kern_return_t r = vm_protect(mach_task_self(), ((uintptr_t)info.loadedAddress())+(uintptr_t)vmOffset, (uintptr_t)vmSize, false, regionProt);
        assert( r == KERN_SUCCESS );
    });
}
#endif


void Loader::forEachImage(void (^handler)(const LoadedImage& li, bool& stop)) const
{
    bool stop = false;
    for (const LoadedImage& li : _existingImages) {
        handler(li, stop);
        if ( stop )
            return;
    }
    for (const LoadedImage& li : _newImages) {
        handler(li, stop);
        if ( stop )
            return;
    }
}

void Loader::mas_forEachImage(void (^handler)(const WrappedMachO& wmo, bool hidden, bool& stop)) const
{
    forEachImage(^(const LoadedImage& li, bool& stop) {
        WrappedMachO wmo((MachOAnalyzer*)li.loadedAddress(), this, (void*)li.image());
        handler(wmo, li.hideFromFlatSearch(), stop);
    });
}


bool Loader::wmo_missingSymbolResolver(const WrappedMachO* fromWmo, bool weakImport, bool lazyBind, const char* symbolName, const char* expectedInDylibPath, const char* clientPath, FixupTarget& target) const
{
    if ( weakImport ) {
        target.offsetInImage = 0;
        target.kind  = FixupTarget::Kind::bindAbsolute;
        return true;
    }

    if ( lazyBind && _allowMissingLazies ) {
        __block bool result = false;
        forEachImage(^(const LoadedImage& li, bool& stop) {
            if ( li.loadedAddress()->isDylib() && (strcmp(li.loadedAddress()->installName(), "/usr/lib/system/libdyld.dylib") == 0) ) {
                WrappedMachO libdyldWmo((MachOAnalyzer*)li.loadedAddress(), this, (void*)li.image());
                Diagnostics  diag;
                if ( libdyldWmo.findSymbolIn(diag, "__dyld_missing_symbol_abort", 0, target) ) {
                     // <rdar://problem/44315944> closures should bind missing lazy-bind symbols to a missing symbol handler in libdyld in flat namespace
                     result = true;
                }
                stop = true;
            }
        });
        return result;
    }

    // FIXME
    return false;
}


void Loader::mas_mainExecutable(WrappedMachO& mainWmo) const
{
    forEachImage(^(const LoadedImage& li, bool& stop) {
        if ( li.loadedAddress()->isMainExecutable() ) {
            WrappedMachO wmo((MachOAnalyzer*)li.loadedAddress(), this, (void*)li.image());
            mainWmo = wmo;
            stop = true;
        }
    });
}

void* Loader::mas_dyldCache() const
{
    return (void*)_dyldCacheAddress;
}


bool Loader::wmo_dependent(const WrappedMachO* wmo, uint32_t depIndex, WrappedMachO& childWmo, bool& missingWeakDylib) const
{
    const closure::Image* image = (closure::Image*)(wmo->_other);
    closure::ImageNum depImageNum = image->dependentImageNum(depIndex);
    if ( depImageNum == closure::kMissingWeakLinkedImage ) {
        missingWeakDylib = true;
        return true;
    }
    else {
        if ( LoadedImage* li = findImage(depImageNum) ) {
            WrappedMachO foundWmo((MachOAnalyzer*)li->loadedAddress(), this, (void*)li->image());
            missingWeakDylib = false;
            childWmo = foundWmo;
            return true;
        }
    }
    return false;
}


const char* Loader::wmo_path(const WrappedMachO* wmo) const
{
    const closure::Image* image = (closure::Image*)(wmo->_other);
    return image->path();
}



#if BUILDING_DYLD
LoadedImage* Loader::LaunchImagesCache::findImage(closure::ImageNum imageNum,
                                                  Array<LoadedImage>& images) const {
    if ( (imageNum < _firstImageNum) || (imageNum >= _lastImageNum) )
        return nullptr;

    unsigned int cacheIndex = imageNum - _firstImageNum;
    uint32_t imagesIndex = _imageIndices[cacheIndex];
    if ( imagesIndex == 0 )
        return nullptr;

    // Note the index is offset by 1 so that 0's are not yet set
    return &images[imagesIndex - 1];
}

void Loader::LaunchImagesCache::tryAddImage(closure::ImageNum imageNum, uint64_t allImagesIndex) const {
    if ( (imageNum < _firstImageNum) || (imageNum >= _lastImageNum) )
        return;

    unsigned int cacheIndex = imageNum - _firstImageNum;

    // Note the index is offset by 1 so that 0's are not yet set
    _imageIndices[cacheIndex] = (uint32_t)allImagesIndex + 1;
}
 #endif


void forEachLineInFile(const char* buffer, size_t bufferLen, void (^lineHandler)(const char* line, bool& stop))
{
    bool stop = false;
    const char* const eof = &buffer[bufferLen];
    for (const char* s = buffer; s < eof; ++s) {
        char lineBuffer[MAXPATHLEN];
        char* t = lineBuffer;
        char* tEnd = &lineBuffer[MAXPATHLEN];
        while ( (s < eof) && (t != tEnd) ) {
            if ( *s == '\n' )
            break;
            *t++ = *s++;
        }
        *t = '\0';
        lineHandler(lineBuffer, stop);
        if ( stop )
        break;
    }
}

void forEachLineInFile(const char* path, void (^lineHandler)(const char* line, bool& stop))
{
    int fd = dyld3::open(path, O_RDONLY, 0);
    if ( fd != -1 ) {
        struct stat statBuf;
        if ( fstat(fd, &statBuf) == 0 ) {
            const char* lines = (const char*)mmap(nullptr, (size_t)statBuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if ( lines != MAP_FAILED ) {
                forEachLineInFile(lines, (size_t)statBuf.st_size, lineHandler);
                munmap((void*)lines, (size_t)statBuf.st_size);
            }
        }
        close(fd);
    }
}


#if (BUILDING_LIBDYLD || BUILDING_DYLD)
    bool internalInstall()
    {
#if TARGET_OS_SIMULATOR
        return false;
#elif TARGET_OS_IPHONE
        uint32_t devFlags = *((uint32_t*)_COMM_PAGE_DEV_FIRM);
        return ( (devFlags & 1) == 1 );
#else
        return ( csr_check(CSR_ALLOW_APPLE_INTERNAL) == 0 );
#endif
    }
#endif

#if BUILDING_LIBDYLD
// hack because libdyld.dylib should not link with libc++.dylib
extern "C" void __cxa_pure_virtual() __attribute__((visibility("hidden")));
void __cxa_pure_virtual()
{
    abort();
}
#endif

} // namespace dyld3





