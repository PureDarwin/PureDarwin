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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <TargetConditionals.h>
#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_host.h>

#include "Array.h"
#include "MachOFile.h"
#include "SupportedArchs.h"

#if BUILDING_DYLD || BUILDING_LIBDYLD
    // define away restrict until rdar://60166935 is fixed
    #define restrict
    #include <subsystem.h>
#endif

namespace dyld3 {

////////////////////////////  posix wrappers ////////////////////////////////////////

// <rdar://problem/10111032> wrap calls to stat() with check for EAGAIN
int stat(const char* path, struct stat* buf)
{
    int result;
    do {
#if BUILDING_DYLD || BUILDING_LIBDYLD
        result = ::stat_with_subsystem(path, buf);
#else
        result = ::stat(path, buf);
#endif
    } while ((result == -1) && ((errno == EAGAIN) || (errno == EINTR)));

    return result;
}

// <rdar://problem/13805025> dyld should retry open() if it gets an EGAIN
int open(const char* path, int flag, int other)
{
    int result;
    do {
#if BUILDING_DYLD || BUILDING_LIBDYLD
        if (flag & O_CREAT)
            result = ::open(path, flag, other);
        else
            result = ::open_with_subsystem(path, flag);
#else
        result = ::open(path, flag, other);
#endif
    } while ((result == -1) && ((errno == EAGAIN) || (errno == EINTR)));

    return result;
}


////////////////////////////  FatFile ////////////////////////////////////////

const FatFile* FatFile::isFatFile(const void* fileStart)
{
    const FatFile* fileStartAsFat = (FatFile*)fileStart;
    if ( (fileStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC)) || (fileStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC_64)) )
        return fileStartAsFat;
    else
        return nullptr;
}

bool FatFile::isValidSlice(Diagnostics& diag, uint64_t fileLen, uint32_t sliceIndex,
                           uint32_t sliceCpuType, uint32_t sliceCpuSubType, uint64_t sliceOffset, uint64_t sliceLen) const {
    if ( greaterThanAddOrOverflow(sliceOffset, sliceLen, fileLen) ) {
        diag.error("slice %d extends beyond end of file", sliceIndex);
        return false;
    }
    const dyld3::MachOFile* mf = (const dyld3::MachOFile*)((uint8_t*)this+sliceOffset);
    if (!mf->isMachO(diag, sliceLen))
        return false;
    if ( (mf->cputype != (cpu_type_t)sliceCpuType) || (mf->cpusubtype != (cpu_subtype_t)sliceCpuSubType) ) {
        diag.error("cpu type/subtype mismatch");
        return false;
    }
    uint32_t pageSizeMask = mf->uses16KPages() ? 0x3FFF : 0xFFF;
    if ( (sliceOffset & pageSizeMask) != 0 ) {
        // slice not page aligned
        if ( strncmp((char*)this+sliceOffset, "!<arch>", 7) == 0 )
            diag.error("file is static library");
        else
            diag.error("slice is not page aligned");
        return false;
    }
    return true;
}

void FatFile::forEachSlice(Diagnostics& diag, uint64_t fileLen, void (^callback)(uint32_t sliceCpuType, uint32_t sliceCpuSubType, const void* sliceStart, uint64_t sliceSize, bool& stop)) const
{
	if ( this->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
        const uint64_t maxArchs = ((4096 - sizeof(fat_header)) / sizeof(fat_arch));
        const uint32_t numArchs = OSSwapBigToHostInt32(nfat_arch);
        if ( numArchs > maxArchs ) {
            diag.error("fat header too large: %u entries", numArchs);
            return;
        }
        bool stop = false;
        const fat_arch* const archs = (fat_arch*)(((char*)this)+sizeof(fat_header));
        for (uint32_t i=0; i < numArchs; ++i) {
            uint32_t cpuType    = OSSwapBigToHostInt32(archs[i].cputype);
            uint32_t cpuSubType = OSSwapBigToHostInt32(archs[i].cpusubtype);
            uint32_t offset     = OSSwapBigToHostInt32(archs[i].offset);
            uint32_t len        = OSSwapBigToHostInt32(archs[i].size);
            if (isValidSlice(diag, fileLen, i, cpuType, cpuSubType, offset, len))
                callback(cpuType, cpuSubType, (uint8_t*)this+offset, len, stop);
            if ( stop )
                break;
        }

        // Look for one more slice
        if ( numArchs != maxArchs ) {
            uint32_t cpuType    = OSSwapBigToHostInt32(archs[numArchs].cputype);
            uint32_t cpuSubType = OSSwapBigToHostInt32(archs[numArchs].cpusubtype);
            uint32_t offset     = OSSwapBigToHostInt32(archs[numArchs].offset);
            uint32_t len        = OSSwapBigToHostInt32(archs[numArchs].size);
            if ((cpuType == CPU_TYPE_ARM64) && ((cpuSubType == CPU_SUBTYPE_ARM64_ALL || cpuSubType == CPU_SUBTYPE_ARM64_V8))) {
                if (isValidSlice(diag, fileLen, numArchs, cpuType, cpuSubType, offset, len))
                    callback(cpuType, cpuSubType, (uint8_t*)this+offset, len, stop);
            }
        }
    }
    else if ( this->magic == OSSwapBigToHostInt32(FAT_MAGIC_64) ) {
        if ( OSSwapBigToHostInt32(nfat_arch) > ((4096 - sizeof(fat_header)) / sizeof(fat_arch)) ) {
            diag.error("fat header too large: %u entries", OSSwapBigToHostInt32(nfat_arch));
            return;
        }
        bool stop = false;
        const fat_arch_64* const archs = (fat_arch_64*)(((char*)this)+sizeof(fat_header));
        for (uint32_t i=0; i < OSSwapBigToHostInt32(nfat_arch); ++i) {
            uint32_t cpuType    = OSSwapBigToHostInt32(archs[i].cputype);
            uint32_t cpuSubType = OSSwapBigToHostInt32(archs[i].cpusubtype);
            uint64_t offset     = OSSwapBigToHostInt64(archs[i].offset);
            uint64_t len        = OSSwapBigToHostInt64(archs[i].size);
            if (isValidSlice(diag, fileLen, i, cpuType, cpuSubType, offset, len))
                callback(cpuType, cpuSubType, (uint8_t*)this+offset, len, stop);
            if ( stop )
                break;
        }
    }
    else {
        diag.error("not a fat file");
    }
}

bool FatFile::isFatFileWithSlice(Diagnostics& diag, uint64_t fileLen, const GradedArchs& archs, bool isOSBinary,
                                 uint64_t& sliceOffset, uint64_t& sliceLen, bool& missingSlice) const
{
    missingSlice = false;
    if ( (this->magic != OSSwapBigToHostInt32(FAT_MAGIC)) && (this->magic != OSSwapBigToHostInt32(FAT_MAGIC_64)) )
        return false;

    __block int bestGrade = 0;
    forEachSlice(diag, fileLen, ^(uint32_t sliceCpuType, uint32_t sliceCpuSubType, const void* sliceStart, uint64_t sliceSize, bool& stop) {
        if (int sliceGrade = archs.grade(sliceCpuType, sliceCpuSubType, isOSBinary)) {
            if ( sliceGrade > bestGrade ) {
                sliceOffset = (char*)sliceStart - (char*)this;
                sliceLen    = sliceSize;
                bestGrade   = sliceGrade;
            }
        }
    });
    if ( diag.hasError() )
        return false;

    if ( bestGrade == 0 )
        missingSlice = true;

    return (bestGrade != 0);
}


////////////////////////////  GradedArchs ////////////////////////////////////////


#define GRADE_i386        CPU_TYPE_I386,       CPU_SUBTYPE_I386_ALL,    false
#define GRADE_x86_64      CPU_TYPE_X86_64,     CPU_SUBTYPE_X86_64_ALL,  false
#define GRADE_x86_64h     CPU_TYPE_X86_64,     CPU_SUBTYPE_X86_64_H,    false
#define GRADE_armv7       CPU_TYPE_ARM,        CPU_SUBTYPE_ARM64_ALL,   false
#define GRADE_armv7s      CPU_TYPE_ARM,        CPU_SUBTYPE_ARM_V7,      false
#define GRADE_armv7k      CPU_TYPE_ARM,        CPU_SUBTYPE_ARM_V7K,     false
#define GRADE_arm64       CPU_TYPE_ARM64,      CPU_SUBTYPE_ARM64_ALL,   false
#define GRADE_arm64e      CPU_TYPE_ARM64,      CPU_SUBTYPE_ARM64E,      false
#define GRADE_arm64e_pb   CPU_TYPE_ARM64,      CPU_SUBTYPE_ARM64E,      true
#define GRADE_arm64_32    CPU_TYPE_ARM64_32,   CPU_SUBTYPE_ARM64_32_V8, false

const GradedArchs GradedArchs::i386              = { {{GRADE_i386,    1}} };
const GradedArchs GradedArchs::x86_64            = { {{GRADE_x86_64,  1}} };
const GradedArchs GradedArchs::x86_64h           = { {{GRADE_x86_64h, 2}, {GRADE_x86_64, 1}} };
const GradedArchs GradedArchs::arm64             = { {{GRADE_arm64,   1}} };
#if SUPPORT_ARCH_arm64e
const GradedArchs GradedArchs::arm64e_keysoff    = { {{GRADE_arm64e,    2}, {GRADE_arm64, 1}} };
const GradedArchs GradedArchs::arm64e_keysoff_pb = { {{GRADE_arm64e_pb, 2}, {GRADE_arm64, 1}} };
const GradedArchs GradedArchs::arm64e            = { {{GRADE_arm64e,    1}} };
const GradedArchs GradedArchs::arm64e_pb         = { {{GRADE_arm64e_pb, 1}} };
#endif
const GradedArchs GradedArchs::armv7             = { {{GRADE_armv7,   1}} };
const GradedArchs GradedArchs::armv7s            = { {{GRADE_armv7s,  2}, {GRADE_armv7, 1}} };
const GradedArchs GradedArchs::armv7k            = { {{GRADE_armv7k,  1}} };
#if SUPPORT_ARCH_arm64_32
const GradedArchs GradedArchs::arm64_32          = { {{GRADE_arm64_32, 1}} };
#endif

int GradedArchs::grade(uint32_t cputype, uint32_t cpusubtype, bool isOSBinary) const
{
    for (const CpuGrade* p = _orderedCpuTypes; p->type != 0; ++p) {
        if ( (p->type == cputype) && (p->subtype == (cpusubtype & ~CPU_SUBTYPE_MASK)) ) {
            if ( p->osBinary ) {
                if ( isOSBinary )
                    return p->grade;
            }
            else {
                return p->grade;
            }
        }
    }
    return 0;
}

const char* GradedArchs::name() const
{
    return MachOFile::archName(_orderedCpuTypes[0].type, _orderedCpuTypes[0].subtype);
}

#if __x86_64__
static bool isHaswell()
{
    // FIXME: figure out a commpage way to check this
    static bool sAlreadyDetermined = false;
    static bool sHaswell = false;
    if ( !sAlreadyDetermined ) {
        struct host_basic_info info;
        mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
        mach_port_t hostPort = mach_host_self();
        kern_return_t result = host_info(hostPort, HOST_BASIC_INFO, (host_info_t)&info, &count);
        mach_port_deallocate(mach_task_self(), hostPort);
        sHaswell = (result == KERN_SUCCESS) && (info.cpu_subtype == CPU_SUBTYPE_X86_64_H);
        sAlreadyDetermined = true;
    }
    return sHaswell;
}
#endif

const GradedArchs& GradedArchs::forCurrentOS(bool keysOff, bool osBinariesOnly)
{
#if __arm64e__
    if ( osBinariesOnly )
        return (keysOff ? arm64e_keysoff_pb : arm64e_pb);
    else
        return (keysOff ? arm64e_keysoff : arm64e);
#elif __ARM64_ARCH_8_32__
    return arm64_32;
#elif __arm64__
    return arm64;
#elif __ARM_ARCH_7K__
    return armv7k;
#elif __ARM_ARCH_7S__
    return armv7s;
#elif __ARM_ARCH_7A__
    return armv7;
#elif __x86_64__
    return isHaswell() ? x86_64h : x86_64;
#elif __i386__
    return i386;
#else
    #error unknown platform
#endif
}

const GradedArchs& GradedArchs::forName(const char* archName, bool keysOff)
{
    if (strcmp(archName, "x86_64h") == 0 )
        return x86_64h;
    else if (strcmp(archName, "x86_64") == 0 )
        return x86_64;
#if SUPPORT_ARCH_arm64e
    else if (strcmp(archName, "arm64e") == 0 )
        return keysOff ? arm64e_keysoff : arm64e;
#endif
    else if (strcmp(archName, "arm64") == 0 )
        return arm64;
    else if (strcmp(archName, "armv7k") == 0 )
        return armv7k;
    else if (strcmp(archName, "armv7s") == 0 )
        return armv7s;
    else if (strcmp(archName, "armv7") == 0 )
        return armv7;
#if SUPPORT_ARCH_arm64_32
    else if (strcmp(archName, "arm64_32") == 0 )
        return arm64_32;
#endif
    else if (strcmp(archName, "i386") == 0 )
        return i386;
    assert(0 && "unknown arch name");
}



////////////////////////////  MachOFile ////////////////////////////////////////


const MachOFile::ArchInfo MachOFile::_s_archInfos[] = {
    { "x86_64",   CPU_TYPE_X86_64,   CPU_SUBTYPE_X86_64_ALL  },
    { "x86_64h",  CPU_TYPE_X86_64,   CPU_SUBTYPE_X86_64_H    },
    { "i386",     CPU_TYPE_I386,     CPU_SUBTYPE_I386_ALL    },
    { "arm64",    CPU_TYPE_ARM64,    CPU_SUBTYPE_ARM64_ALL   },
#if SUPPORT_ARCH_arm64e
    { "arm64e",   CPU_TYPE_ARM64,    CPU_SUBTYPE_ARM64E     },
#endif
#if SUPPORT_ARCH_arm64_32
    { "arm64_32", CPU_TYPE_ARM64_32, CPU_SUBTYPE_ARM64_32_V8 },
#endif
    { "armv7k",   CPU_TYPE_ARM,      CPU_SUBTYPE_ARM_V7K     },
    { "armv7s",   CPU_TYPE_ARM,      CPU_SUBTYPE_ARM_V7S     },
    { "armv7",    CPU_TYPE_ARM,      CPU_SUBTYPE_ARM_V7      }
};

const MachOFile::PlatformInfo MachOFile::_s_platformInfos[] = {
    { "macOS",       Platform::macOS,             LC_VERSION_MIN_MACOSX   },
    { "iOS",         Platform::iOS,               LC_VERSION_MIN_IPHONEOS },
    { "tvOS",        Platform::tvOS,              LC_VERSION_MIN_TVOS     },
    { "watchOS",     Platform::watchOS,           LC_VERSION_MIN_WATCHOS  },
    { "bridgeOS",    Platform::bridgeOS,          LC_BUILD_VERSION        },
    { "MacCatalyst", Platform::iOSMac,            LC_BUILD_VERSION        },
    { "iOS-sim",     Platform::iOS_simulator,     LC_BUILD_VERSION        },
    { "tvOS-sim",    Platform::tvOS_simulator,    LC_BUILD_VERSION        },
    { "watchOS-sim", Platform::watchOS_simulator, LC_BUILD_VERSION        },
};



bool MachOFile::is64() const
{
    return (this->magic == MH_MAGIC_64);
}

size_t MachOFile::machHeaderSize() const
{
    return is64() ? sizeof(mach_header_64) : sizeof(mach_header);
}

uint32_t MachOFile::maskedCpuSubtype() const
{
    return (this->cpusubtype & ~CPU_SUBTYPE_MASK);
}

uint32_t MachOFile::pointerSize() const
{
    if (this->magic == MH_MAGIC_64)
        return 8;
    else
        return 4;
}

bool MachOFile::uses16KPages() const
{
    switch (this->cputype) {
        case CPU_TYPE_ARM64:
        case CPU_TYPE_ARM64_32:
            return true;
        case CPU_TYPE_ARM:
            // iOS is 16k aligned for armv7/armv7s and watchOS armv7k is 16k aligned
            return this->cpusubtype == CPU_SUBTYPE_ARM_V7K;
        default:
            return false;
    }
}

bool MachOFile::isArch(const char* aName) const
{
    return (strcmp(aName, archName(this->cputype, this->cpusubtype)) == 0);
}

const char* MachOFile::archName(uint32_t cputype, uint32_t cpusubtype)
{
    for (const ArchInfo& info : _s_archInfos) {
        if ( (cputype == info.cputype) && ((cpusubtype & ~CPU_SUBTYPE_MASK) == info.cpusubtype) ) {
            return info.name;
        }
    }
    return "unknown";
}

uint32_t MachOFile::cpuTypeFromArchName(const char* archName)
{
    for (const ArchInfo& info : _s_archInfos) {
        if ( strcmp(archName, info.name) == 0 ) {
            return info.cputype;
        }
    }
    return 0;
}

uint32_t MachOFile::cpuSubtypeFromArchName(const char* archName)
{
    for (const ArchInfo& info : _s_archInfos) {
        if ( strcmp(archName, info.name) == 0 ) {
            return info.cpusubtype;
        }
    }
    return 0;
}

const char* MachOFile::archName() const
{
    return archName(this->cputype, this->cpusubtype);
}

static void appendDigit(char*& s, unsigned& num, unsigned place, bool& startedPrinting)
{
    if ( num >= place ) {
        unsigned dig = (num/place);
        *s++ = '0' + dig;
        num -= (dig*place);
        startedPrinting = true;
    }
    else if ( startedPrinting ) {
        *s++ = '0';
    }
}

static void appendNumber(char*& s, unsigned num)
{
    assert(num < 99999);
    bool startedPrinting = false;
    appendDigit(s, num, 10000, startedPrinting);
    appendDigit(s, num,  1000, startedPrinting);
    appendDigit(s, num,   100, startedPrinting);
    appendDigit(s, num,    10, startedPrinting);
    appendDigit(s, num,     1, startedPrinting);
    if ( !startedPrinting )
        *s++ = '0';
}

void MachOFile::packedVersionToString(uint32_t packedVersion, char versionString[32])
{
    // sprintf(versionString, "%d.%d.%d", (packedVersion >> 16), ((packedVersion >> 8) & 0xFF), (packedVersion & 0xFF));
    char* s = versionString;
    appendNumber(s, (packedVersion >> 16));
    *s++ = '.';
    appendNumber(s, (packedVersion >> 8) & 0xFF);
    *s++ = '.';
    appendNumber(s, (packedVersion & 0xFF));
    *s++ = '\0';
}

bool MachOFile::builtForPlatform(Platform reqPlatform, bool onlyOnePlatform) const
{
    __block bool foundRequestedPlatform = false;
    __block bool foundOtherPlatform     = false;
    forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
        if ( platform == reqPlatform )
            foundRequestedPlatform = true;
        else
            foundOtherPlatform = true;
    });
    // if checking that this binary is built for exactly one platform, fail if more
    if ( foundOtherPlatform && onlyOnePlatform )
        return false;
    if ( foundRequestedPlatform )
        return true;

    // binary has no explict load command to mark platform
    // could be an old macOS binary, look at arch
    if  ( !foundOtherPlatform && (reqPlatform == Platform::macOS) ) {
        if ( this->cputype == CPU_TYPE_X86_64 )
            return true;
        if ( this->cputype == CPU_TYPE_I386 )
            return true;
    }

#if BUILDING_DYLDINFO
    // Allow offline tools to analyze binaries dyld doesn't load, ie, those with platforms
    if ( !foundOtherPlatform && (reqPlatform == Platform::unknown) )
        return true;
#endif

    return false;
}

bool MachOFile::loadableIntoProcess(Platform processPlatform, const char* path) const
{
    if ( this->builtForPlatform(processPlatform) )
        return true;

    // Some host macOS dylibs can be loaded into simulator processes
    if ( MachOFile::isSimulatorPlatform(processPlatform) && this->builtForPlatform(Platform::macOS)) {
        static const char* macOSHost[] = {
            "/usr/lib/system/libsystem_kernel.dylib",
            "/usr/lib/system/libsystem_platform.dylib",
            "/usr/lib/system/libsystem_pthread.dylib",
            "/usr/lib/system/libsystem_platform_debug.dylib",
            "/usr/lib/system/libsystem_pthread_debug.dylib",
            "/usr/lib/system/host/liblaunch_sim.dylib",
        };
        for (const char* libPath : macOSHost) {
            if (strcmp(libPath, path) == 0)
                return true;
        }
    }

    // If this is being called on main executable where we expect a macOS program, Catalyst programs are also runnable
    if ( (this->filetype == MH_EXECUTE) && (processPlatform == Platform::macOS) && this->builtForPlatform(Platform::iOSMac, true) )
        return true;
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    if ( (this->filetype == MH_EXECUTE) && (processPlatform == Platform::macOS) && this->builtForPlatform(Platform::iOS, true) )
        return true;
#endif

    bool iOSonMac = (processPlatform == Platform::iOSMac);
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    // allow iOS binaries in iOSApp
    if ( processPlatform == Platform::iOS ) {
        // can load Catalyst binaries into iOS process
        if ( this->builtForPlatform(Platform::iOSMac) )
            return true;
        iOSonMac = true;
    }
#endif
    // macOS dylibs can be loaded into iOSMac processes
    if ( (iOSonMac) && this->builtForPlatform(Platform::macOS, true) )
        return true;

    return false;
}

bool MachOFile::isZippered() const
{
    __block bool macOS = false;
    __block bool iOSMac = false;
    forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
        if ( platform == Platform::macOS )
            macOS = true;
        else if ( platform == Platform::iOSMac )
            iOSMac = true;
    });
    return macOS && iOSMac;
}

bool MachOFile::inDyldCache() const {
    return (this->flags & 0x80000000);
}

Platform MachOFile::currentPlatform()
{

#if TARGET_OS_SIMULATOR
#if TARGET_OS_WATCH
    return Platform::watchOS_simulator;
#elif TARGET_OS_TV
    return Platform::tvOS_simulator;
#else
    return Platform::iOS_simulator;
#endif
#elif TARGET_OS_BRIDGE
    return Platform::bridgeOS;
#elif TARGET_OS_WATCH
    return Platform::watchOS;
#elif TARGET_OS_TV
    return Platform::tvOS;
#elif TARGET_OS_IOS
    return Platform::iOS;
#elif TARGET_OS_OSX
    return Platform::macOS;
#elif TARGET_OS_DRIVERKIT
    return Platform::driverKit;
#else
    #error unknown platform
#endif
}


const char* MachOFile::currentArchName()
{
#if __ARM_ARCH_7K__
    return "armv7k";
#elif __ARM_ARCH_7A__
    return "armv7";
#elif __ARM_ARCH_7S__
    return "armv7s";
#elif __arm64e__
    return "arm64e";
#elif __arm64__
#if __LP64__
    return "arm64";
#else
    return "arm64_32";
#endif
#elif __x86_64__
    return isHaswell() ? "x86_64h" : "x86_64";
#elif __i386__
    return "i386";
#else
    #error unknown arch
#endif
}

bool MachOFile::isSimulatorPlatform(Platform platform)
{
    return ( (platform == Platform::iOS_simulator) ||
             (platform == Platform::watchOS_simulator) ||
             (platform == Platform::tvOS_simulator) );
}

bool MachOFile::isDylib() const
{
    return (this->filetype == MH_DYLIB);
}

bool MachOFile::isBundle() const
{
    return (this->filetype == MH_BUNDLE);
}

bool MachOFile::isMainExecutable() const
{
    return (this->filetype == MH_EXECUTE);
}

bool MachOFile::isDynamicExecutable() const
{
    if ( this->filetype != MH_EXECUTE )
        return false;

    // static executables do not have dyld load command
    return hasLoadCommand(LC_LOAD_DYLINKER);
}

bool MachOFile::isStaticExecutable() const
{
    if ( this->filetype != MH_EXECUTE )
        return false;

    // static executables do not have dyld load command
    return !hasLoadCommand(LC_LOAD_DYLINKER);
}

bool MachOFile::isKextBundle() const
{
    return (this->filetype == MH_KEXT_BUNDLE);
}

bool MachOFile::isFileSet() const
{
    return (this->filetype == MH_FILESET);
}

bool MachOFile::isPIE() const
{
    return (this->flags & MH_PIE);
}

bool MachOFile::isPreload() const
{
    return (this->filetype == MH_PRELOAD);
}

const char* MachOFile::platformName(Platform reqPlatform)
{
    for (const PlatformInfo& info : _s_platformInfos) {
        if ( info.platform == reqPlatform )
            return info.name;
    }
    return "unknown platform";
}

void MachOFile::forEachSupportedPlatform(void (^handler)(Platform platform, uint32_t minOS, uint32_t sdk)) const
{
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        const build_version_command* buildCmd = (build_version_command *)cmd;
        const version_min_command*   versCmd  = (version_min_command*)cmd;
        switch ( cmd->cmd ) {
            case LC_BUILD_VERSION:
                handler((Platform)(buildCmd->platform), buildCmd->minos, buildCmd->sdk);
                break;
            case LC_VERSION_MIN_MACOSX:
                handler(Platform::macOS, versCmd->version, versCmd->sdk);
                break;
            case LC_VERSION_MIN_IPHONEOS:
                if ( (this->cputype == CPU_TYPE_X86_64) || (this->cputype == CPU_TYPE_I386) )
                    handler(Platform::iOS_simulator, versCmd->version, versCmd->sdk); // old sim binary
                else
                    handler(Platform::iOS, versCmd->version, versCmd->sdk);
                break;
            case LC_VERSION_MIN_TVOS:
                if ( this->cputype == CPU_TYPE_X86_64 )
                    handler(Platform::tvOS_simulator, versCmd->version, versCmd->sdk); // old sim binary
                else
                    handler(Platform::tvOS, versCmd->version, versCmd->sdk);
                break;
            case LC_VERSION_MIN_WATCHOS:
                if ( (this->cputype == CPU_TYPE_X86_64) || (this->cputype == CPU_TYPE_I386) )
                    handler(Platform::watchOS_simulator, versCmd->version, versCmd->sdk); // old sim binary
                else
                    handler(Platform::watchOS, versCmd->version, versCmd->sdk);
                break;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
}


bool MachOFile::isMachO(Diagnostics& diag, uint64_t fileSize) const
{
    if ( !hasMachOMagic() ) {
        // old PPC slices are not currently valid "mach-o" but should not cause an error
        if ( !hasMachOBigEndianMagic() )
            diag.error("file does not start with MH_MAGIC[_64]");
        return false;
    }
    if ( this->sizeofcmds + machHeaderSize() > fileSize ) {
        diag.error("load commands exceed length of first segment");
        return false;
    }
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) { });
    return diag.noError();
}

bool MachOFile::hasMachOMagic() const
{
    return ( (this->magic == MH_MAGIC) || (this->magic == MH_MAGIC_64) );
}

bool MachOFile::hasMachOBigEndianMagic() const
{
    return ( (this->magic == MH_CIGAM) || (this->magic == MH_CIGAM_64) );
}


void MachOFile::forEachLoadCommand(Diagnostics& diag, void (^callback)(const load_command* cmd, bool& stop)) const
{
    bool stop = false;
    const load_command* startCmds = nullptr;
    if ( this->magic == MH_MAGIC_64 )
        startCmds = (load_command*)((char *)this + sizeof(mach_header_64));
    else if ( this->magic == MH_MAGIC )
        startCmds = (load_command*)((char *)this + sizeof(mach_header));
    else if ( hasMachOBigEndianMagic() )
        return;  // can't process big endian mach-o
    else {
        const uint32_t* h = (uint32_t*)this;
        diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return;  // not a mach-o file
    }
    const load_command* const cmdsEnd = (load_command*)((char*)startCmds + this->sizeofcmds);
    const load_command* cmd = startCmds;
    for (uint32_t i = 0; i < this->ncmds; ++i) {
        const load_command* nextCmd = (load_command*)((char *)cmd + cmd->cmdsize);
        if ( cmd->cmdsize < 8 ) {
            diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) too small", i, this->ncmds, cmd, this, cmd->cmdsize);
            return;
        }
        if ( (nextCmd > cmdsEnd) || (nextCmd < startCmds) ) {
            diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) is too large, load commands end at %p", i, this->ncmds, cmd, this, cmd->cmdsize, cmdsEnd);
            return;
        }
        callback(cmd, stop);
        if ( stop )
            return;
        cmd = nextCmd;
    }
}

void MachOFile::removeLoadCommand(Diagnostics& diag, void (^callback)(const load_command* cmd, bool& remove, bool& stop))
{
    bool stop = false;
    const load_command* startCmds = nullptr;
    if ( this->magic == MH_MAGIC_64 )
        startCmds = (load_command*)((char *)this + sizeof(mach_header_64));
    else if ( this->magic == MH_MAGIC )
        startCmds = (load_command*)((char *)this + sizeof(mach_header));
    else if ( hasMachOBigEndianMagic() )
        return;  // can't process big endian mach-o
    else {
        const uint32_t* h = (uint32_t*)this;
        diag.error("file does not start with MH_MAGIC[_64]: 0x%08X 0x%08X", h[0], h [1]);
        return;  // not a mach-o file
    }
    const load_command* const cmdsEnd = (load_command*)((char*)startCmds + this->sizeofcmds);
    auto cmd = (load_command*)startCmds;
    const uint32_t origNcmds = this->ncmds;
    unsigned bytesRemaining = this->sizeofcmds;
    for (uint32_t i = 0; i < origNcmds; ++i) {
        bool remove = false;
        auto nextCmd = (load_command*)((char *)cmd + cmd->cmdsize);
        if ( cmd->cmdsize < 8 ) {
            diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) too small", i, this->ncmds, cmd, this, cmd->cmdsize);
            return;
        }
        if ( (nextCmd > cmdsEnd) || (nextCmd < startCmds) ) {
            diag.error("malformed load command #%d of %d at %p with mh=%p, size (0x%X) is too large, load commands end at %p", i, this->ncmds, cmd, this, cmd->cmdsize, cmdsEnd);
            return;
        }
        callback(cmd, remove, stop);
        if ( remove ) {
            this->sizeofcmds -= cmd->cmdsize;
            ::memmove((void*)cmd, (void*)nextCmd, bytesRemaining);
            this->ncmds--;
        } else {
            bytesRemaining -= cmd->cmdsize;
            cmd = nextCmd;
        }
        if ( stop )
            break;
    }
    if ( cmd )
     ::bzero(cmd, bytesRemaining);
}

const char* MachOFile::installName() const
{
    const char*  name;
    uint32_t     compatVersion;
    uint32_t     currentVersion;
    if ( getDylibInstallName(&name, &compatVersion, &currentVersion) )
        return name;
    return nullptr;
}

bool MachOFile::getDylibInstallName(const char** installName, uint32_t* compatVersion, uint32_t* currentVersion) const
{
    Diagnostics diag;
    __block bool found = false;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == LC_ID_DYLIB ) {
            const dylib_command*  dylibCmd = (dylib_command*)cmd;
            *compatVersion  = dylibCmd->dylib.compatibility_version;
            *currentVersion = dylibCmd->dylib.current_version;
            *installName    = (char*)dylibCmd + dylibCmd->dylib.name.offset;
            found = true;
            stop = true;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
    return found;
}

bool MachOFile::getUuid(uuid_t uuid) const
{
    Diagnostics diag;
    __block bool found = false;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == LC_UUID ) {
            const uuid_command* uc = (const uuid_command*)cmd;
            memcpy(uuid, uc->uuid, sizeof(uuid_t));
            found = true;
            stop = true;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
    if ( !found )
        bzero(uuid, sizeof(uuid_t));
    return found;
}

void MachOFile::forEachDependentDylib(void (^callback)(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop)) const
{
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
         switch ( cmd->cmd ) {
            case LC_LOAD_DYLIB:
            case LC_LOAD_WEAK_DYLIB:
            case LC_REEXPORT_DYLIB:
            case LC_LOAD_UPWARD_DYLIB: {
                const dylib_command* dylibCmd = (dylib_command*)cmd;
                const char* loadPath = (char*)dylibCmd + dylibCmd->dylib.name.offset;
                callback(loadPath, (cmd->cmd == LC_LOAD_WEAK_DYLIB), (cmd->cmd == LC_REEXPORT_DYLIB), (cmd->cmd == LC_LOAD_UPWARD_DYLIB),
                                    dylibCmd->dylib.compatibility_version, dylibCmd->dylib.current_version, stop);
            }
            break;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
}

void MachOFile::forDyldEnv(void (^callback)(const char* envVar, bool& stop)) const
{
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
         if ( cmd->cmd == LC_DYLD_ENVIRONMENT ) {
            const dylinker_command* envCmd = (dylinker_command*)cmd;
            const char* keyEqualsValue = (char*)envCmd + envCmd->name.offset;
            // only process variables that start with DYLD_ and end in _PATH
            if ( (strncmp(keyEqualsValue, "DYLD_", 5) == 0) ) {
                const char* equals = strchr(keyEqualsValue, '=');
                if ( equals != NULL ) {
                    if ( strncmp(&equals[-5], "_PATH", 5) == 0 ) {
                        callback(keyEqualsValue, stop);
                    }
                }
            }
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
}

bool MachOFile::enforceCompatVersion() const
{
    __block bool result = true;
    forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
        switch ( platform ) {
            case Platform::macOS:
                if ( minOS >= 0x000A0E00 )  // macOS 10.14
                    result = false;
                break;
            case Platform::iOS:
            case Platform::tvOS:
            case Platform::iOS_simulator:
            case Platform::tvOS_simulator:
                if ( minOS >= 0x000C0000 )  // iOS 12.0
                    result = false;
                break;
            case Platform::watchOS:
            case Platform::watchOS_simulator:
                if ( minOS >= 0x00050000 )  // watchOS 5.0
                    result = false;
                break;
            case Platform::bridgeOS:
                if ( minOS >= 0x00030000 )  // bridgeOS 3.0
                    result = false;
                break;
            case Platform::driverKit:
            case Platform::iOSMac:
                result = false;
                break;
            case Platform::unknown:
                break;
        }
    });
    return result;
}

const thread_command* MachOFile::unixThreadLoadCommand() const {
    Diagnostics diag;
    __block const thread_command* command = nullptr;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == LC_UNIXTHREAD ) {
            command = (const thread_command*)cmd;
            stop = true;
        }
    });
    return command;
}


uint32_t MachOFile::entryAddrRegisterIndexForThreadCmd() const
{
    switch ( this->cputype ) {
        case CPU_TYPE_I386:
            return 10; // i386_thread_state_t.eip
        case CPU_TYPE_X86_64:
            return 16; // x86_thread_state64_t.rip
        case CPU_TYPE_ARM:
            return 15; // arm_thread_state_t.pc
        case CPU_TYPE_ARM64:
            return 32; // arm_thread_state64_t.__pc
    }
    return ~0U;
}


uint64_t MachOFile::entryAddrFromThreadCmd(const thread_command* cmd) const
{
    assert(cmd->cmd == LC_UNIXTHREAD);
    const uint32_t* regs32 = (uint32_t*)(((char*)cmd) + 16);
    const uint64_t* regs64 = (uint64_t*)(((char*)cmd) + 16);

    uint32_t index = entryAddrRegisterIndexForThreadCmd();
    if (index == ~0U)
        return 0;

    return is64() ? regs64[index] : regs32[index];
}


void MachOFile::forEachSegment(void (^callback)(const SegmentInfo& info, bool& stop)) const
{
    Diagnostics diag;
    const bool intel32 = (this->cputype == CPU_TYPE_I386);
    __block uint32_t segIndex = 0;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == LC_SEGMENT_64 ) {
            const segment_command_64* segCmd = (segment_command_64*)cmd;
            uint64_t sizeOfSections = segCmd->vmsize;
            uint8_t p2align = 0;
            const section_64* const sectionsStart = (section_64*)((char*)segCmd + sizeof(struct segment_command_64));
            const section_64* const sectionsEnd   = &sectionsStart[segCmd->nsects];
            for (const section_64* sect=sectionsStart; sect < sectionsEnd; ++sect) {
                sizeOfSections = sect->addr + sect->size - segCmd->vmaddr;
                if ( sect->align > p2align )
                    p2align = sect->align;
            }
            SegmentInfo info;
            info.fileOffset        = segCmd->fileoff;
            info.fileSize          = segCmd->filesize;
            info.vmAddr            = segCmd->vmaddr;
            info.vmSize            = segCmd->vmsize;
            info.sizeOfSections    = sizeOfSections;
            info.segName           = segCmd->segname;
            info.loadCommandOffset = (uint32_t)((uint8_t*)segCmd - (uint8_t*)this);
            info.protections       = segCmd->initprot;
            info.textRelocs        = false;
            info.readOnlyData      = ((segCmd->flags & SG_READ_ONLY) != 0);
            info.isProtected       = (segCmd->flags & SG_PROTECTED_VERSION_1) ? 1 : 0;
            info.p2align           = p2align;
            info.segIndex          = segIndex;
            callback(info, stop);
            ++segIndex;
        }
        else if ( cmd->cmd == LC_SEGMENT ) {
            const segment_command* segCmd = (segment_command*)cmd;
            uint64_t sizeOfSections = segCmd->vmsize;
            uint8_t p2align = 0;
            bool  hasTextRelocs = false;
            const section* const sectionsStart = (section*)((char*)segCmd + sizeof(struct segment_command));
            const section* const sectionsEnd   = &sectionsStart[segCmd->nsects];
            for (const section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
                sizeOfSections = sect->addr + sect->size - segCmd->vmaddr;
                if ( sect->align > p2align )
                    p2align = sect->align;
                if ( sect->flags & (S_ATTR_EXT_RELOC|S_ATTR_LOC_RELOC) )
                    hasTextRelocs = true;
           }
            SegmentInfo info;
            info.fileOffset        = segCmd->fileoff;
            info.fileSize          = segCmd->filesize;
            info.vmAddr            = segCmd->vmaddr;
            info.vmSize            = segCmd->vmsize;
            info.sizeOfSections    = sizeOfSections;
            info.segName           = segCmd->segname;
            info.loadCommandOffset = (uint32_t)((uint8_t*)segCmd - (uint8_t*)this);
            info.protections       = segCmd->initprot;
            info.textRelocs        = intel32 && !info.writable() && hasTextRelocs;
            info.readOnlyData      = ((segCmd->flags & SG_READ_ONLY) != 0);
            info.isProtected       = (segCmd->flags & SG_PROTECTED_VERSION_1) ? 1 : 0;
            info.p2align           = p2align;
            info.segIndex          = segIndex;
            callback(info, stop);
            ++segIndex;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
}

void MachOFile::forEachSection(void (^callback)(const SectionInfo& sectInfo, bool malformedSectionRange, bool& stop)) const
{
    Diagnostics diag;
    BLOCK_ACCCESSIBLE_ARRAY(char, sectNameCopy, 20);  // read as:  char sectNameCopy[20];
    const bool intel32 = (this->cputype == CPU_TYPE_I386);
    __block uint32_t segIndex = 0;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        SectionInfo sectInfo;
        if ( cmd->cmd == LC_SEGMENT_64 ) {
            const segment_command_64* segCmd = (segment_command_64*)cmd;
            uint64_t sizeOfSections = segCmd->vmsize;
            uint8_t p2align = 0;
            const section_64* const sectionsStart = (section_64*)((char*)segCmd + sizeof(struct segment_command_64));
            const section_64* const sectionsEnd   = &sectionsStart[segCmd->nsects];
            for (const section_64* sect=sectionsStart; sect < sectionsEnd; ++sect) {
                sizeOfSections = sect->addr + sect->size - segCmd->vmaddr;
                if ( sect->align > p2align )
                    p2align = sect->align;
            }
            sectInfo.segInfo.fileOffset        = segCmd->fileoff;
            sectInfo.segInfo.fileSize          = segCmd->filesize;
            sectInfo.segInfo.vmAddr            = segCmd->vmaddr;
            sectInfo.segInfo.vmSize            = segCmd->vmsize;
            sectInfo.segInfo.sizeOfSections    = sizeOfSections;
            sectInfo.segInfo.segName           = segCmd->segname;
            sectInfo.segInfo.loadCommandOffset = (uint32_t)((uint8_t*)segCmd - (uint8_t*)this);
            sectInfo.segInfo.protections       = segCmd->initprot;
            sectInfo.segInfo.textRelocs        = false;
            sectInfo.segInfo.readOnlyData      = ((segCmd->flags & SG_READ_ONLY) != 0);
            sectInfo.segInfo.isProtected       = (segCmd->flags & SG_PROTECTED_VERSION_1) ? 1 : 0;
            sectInfo.segInfo.p2align           = p2align;
            sectInfo.segInfo.segIndex          = segIndex;
            for (const section_64* sect=sectionsStart; !stop && (sect < sectionsEnd); ++sect) {
                const char* sectName = sect->sectname;
                if ( sectName[15] != '\0' ) {
                    strlcpy(sectNameCopy, sectName, 17);
                    sectName = sectNameCopy;
                }
                bool malformedSectionRange = (sect->addr < segCmd->vmaddr) || greaterThanAddOrOverflow(sect->addr, sect->size, segCmd->vmaddr + segCmd->filesize);
                sectInfo.sectName       = sectName;
                sectInfo.sectFileOffset = sect->offset;
                sectInfo.sectFlags      = sect->flags;
                sectInfo.sectAddr       = sect->addr;
                sectInfo.sectSize       = sect->size;
                sectInfo.sectAlignP2    = sect->align;
                sectInfo.reserved1      = sect->reserved1;
                sectInfo.reserved2      = sect->reserved2;
                callback(sectInfo, malformedSectionRange, stop);
            }
            ++segIndex;
        }
        else if ( cmd->cmd == LC_SEGMENT ) {
            const segment_command* segCmd = (segment_command*)cmd;
            uint64_t sizeOfSections = segCmd->vmsize;
            uint8_t p2align = 0;
            bool  hasTextRelocs = false;
            const section* const sectionsStart = (section*)((char*)segCmd + sizeof(struct segment_command));
            const section* const sectionsEnd   = &sectionsStart[segCmd->nsects];
            for (const section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
                sizeOfSections = sect->addr + sect->size - segCmd->vmaddr;
                if ( sect->align > p2align )
                    p2align = sect->align;
                if ( sect->flags & (S_ATTR_EXT_RELOC|S_ATTR_LOC_RELOC) )
                    hasTextRelocs = true;
            }
            sectInfo.segInfo.fileOffset        = segCmd->fileoff;
            sectInfo.segInfo.fileSize          = segCmd->filesize;
            sectInfo.segInfo.vmAddr            = segCmd->vmaddr;
            sectInfo.segInfo.vmSize            = segCmd->vmsize;
            sectInfo.segInfo.sizeOfSections    = sizeOfSections;
            sectInfo.segInfo.segName           = segCmd->segname;
            sectInfo.segInfo.loadCommandOffset = (uint32_t)((uint8_t*)segCmd - (uint8_t*)this);
            sectInfo.segInfo.protections       = segCmd->initprot;
            sectInfo.segInfo.textRelocs        = intel32 && !sectInfo.segInfo.writable() && hasTextRelocs;
            sectInfo.segInfo.readOnlyData      = ((segCmd->flags & SG_READ_ONLY) != 0);
            sectInfo.segInfo.isProtected       = (segCmd->flags & SG_PROTECTED_VERSION_1) ? 1 : 0;
            sectInfo.segInfo.p2align           = p2align;
            sectInfo.segInfo.segIndex          = segIndex;
            for (const section* sect=sectionsStart; !stop && (sect < sectionsEnd); ++sect) {
                const char* sectName = sect->sectname;
                if ( sectName[15] != '\0' ) {
                    strlcpy(sectNameCopy, sectName, 17);
                    sectName = sectNameCopy;
                }
                bool malformedSectionRange = (sect->addr < segCmd->vmaddr) || greaterThanAddOrOverflow(sect->addr, sect->size, segCmd->vmaddr + segCmd->filesize);
                sectInfo.sectName       = sectName;
                sectInfo.sectFileOffset = sect->offset;
                sectInfo.sectFlags      = sect->flags;
                sectInfo.sectAddr       = sect->addr;
                sectInfo.sectSize       = sect->size;
                sectInfo.sectAlignP2    = sect->align;
                sectInfo.reserved1      = sect->reserved1;
                sectInfo.reserved2      = sect->reserved2;
                callback(sectInfo, malformedSectionRange, stop);
            }
            ++segIndex;
        }
    });
    diag.assertNoError();   // any malformations in the file should have been caught by earlier validate() call
}

bool MachOFile::hasWeakDefs() const
{
    return (this->flags & MH_WEAK_DEFINES);
}

bool MachOFile::hasThreadLocalVariables() const
{
    return (this->flags & MH_HAS_TLV_DESCRIPTORS);
}

static bool endsWith(const char* str, const char* suffix)
{
    size_t strLen    = strlen(str);
    size_t suffixLen = strlen(suffix);
    if ( strLen < suffixLen )
        return false;
    return (strcmp(&str[strLen-suffixLen], suffix) == 0);
}

bool MachOFile::isSharedCacheEligiblePath(const char* dylibName) {
    return (   (strncmp(dylibName, "/usr/lib/", 9) == 0)
            || (strncmp(dylibName, "/System/Library/", 16) == 0)
            || (strncmp(dylibName, "/System/iOSSupport/usr/lib/", 27) == 0)
            || (strncmp(dylibName, "/System/iOSSupport/System/Library/", 34) == 0)
            || (strncmp(dylibName, "/Library/Apple/usr/lib/", 23) == 0)
            || (strncmp(dylibName, "/Library/Apple/System/Library/", 30) == 0) );
}

static bool startsWith(const char* buffer, const char* valueToFind) {
    return strncmp(buffer, valueToFind, strlen(valueToFind)) == 0;
}

static bool platformExcludesSharedCache_macOS(const char* installName) {
    // Note: This function basically matches dontCache() from update dyld shared cache

    if ( startsWith(installName, "/usr/lib/system/introspection/") )
        return true;
    if ( startsWith(installName, "/System/Library/QuickTime/") )
        return true;
    if ( startsWith(installName, "/System/Library/Tcl/") )
        return true;
    if ( startsWith(installName, "/System/Library/Perl/") )
        return true;
    if ( startsWith(installName, "/System/Library/MonitorPanels/") )
        return true;
    if ( startsWith(installName, "/System/Library/Accessibility/") )
        return true;
    if ( startsWith(installName, "/usr/local/") )
        return true;
    if ( startsWith(installName, "/usr/lib/pam/") )
        return true;
    // We no longer support ROSP, so skip all paths which start with the special prefix
    if ( startsWith(installName, "/System/Library/Templates/Data/") )
        return true;

    // anything inside a .app bundle is specific to app, so should not be in shared cache
    if ( strstr(installName, ".app/") != NULL )
        return true;

    return false;
}

static bool platformExcludesSharedCache_iOS(const char* installName) {
    if ( strcmp(installName, "/System/Library/Caches/com.apple.xpc/sdk.dylib") == 0 )
        return true;
    if ( strcmp(installName, "/System/Library/Caches/com.apple.xpcd/xpcd_cache.dylib") == 0 )
        return true;
    return false;
}

static bool platformExcludesSharedCache_tvOS(const char* installName) {
    return platformExcludesSharedCache_iOS(installName);
}

static bool platformExcludesSharedCache_watchOS(const char* installName) {
    return platformExcludesSharedCache_iOS(installName);
}

static bool platformExcludesSharedCache_bridgeOS(const char* installName) {
    return platformExcludesSharedCache_iOS(installName);
}

// Returns true if the current platform requires that this install name be excluded from the shared cache
// Note that this overrides any exclusion from anywhere else.
static bool platformExcludesSharedCache(Platform platform, const char* installName) {
    switch (platform) {
        case dyld3::Platform::unknown:
            return false;
        case dyld3::Platform::macOS:
            return platformExcludesSharedCache_macOS(installName);
        case dyld3::Platform::iOS:
            return platformExcludesSharedCache_iOS(installName);
        case dyld3::Platform::tvOS:
            return platformExcludesSharedCache_tvOS(installName);
        case dyld3::Platform::watchOS:
            return platformExcludesSharedCache_watchOS(installName);
        case dyld3::Platform::bridgeOS:
            return platformExcludesSharedCache_bridgeOS(installName);
        case dyld3::Platform::iOSMac:
            return platformExcludesSharedCache_macOS(installName);
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



bool MachOFile::canBePlacedInDyldCache(const char* path, void (^failureReason)(const char*)) const
{

    if ( !isSharedCacheEligiblePath(path) ) {
        // Dont spam the user with an error about paths when we know these are never eligible.
        return false;
    }

    // only dylibs can go in cache
    if ( this->filetype != MH_DYLIB ) {
        failureReason("Not MH_DYLIB");
        return false; // cannot continue, installName() will assert() if not a dylib
    }

    // only dylibs built for /usr/lib or /System/Library can go in cache

    const char* dylibName = installName();
    if ( dylibName[0] != '/' ) {
        failureReason("install name not an absolute path");
        // Don't continue as we don't want to spam the log with errors we don't need.
        return false;
    }
    else if ( strcmp(dylibName, path) != 0 ) {
        failureReason("install path does not match install name");
        return false;
    }
    else if ( strstr(dylibName, "//") != 0 ) {
        failureReason("install name should not include //");
        return false;
    }
    else if ( strstr(dylibName, "./") != 0 ) {
        failureReason("install name should not include ./");
        return false;
    }

    __block bool platformExcludedFile = false;
    forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
        if ( platformExcludedFile )
            return;
        if ( platformExcludesSharedCache(platform, dylibName) ) {
            platformExcludedFile = true;
            return;
        }
    });
    if ( platformExcludedFile ) {
        failureReason("install name is not shared cache eligible on platform");
        return false;
    }

    bool retval = true;

    // flat namespace files cannot go in cache
    if ( (this->flags & MH_TWOLEVEL) == 0 ) {
        retval = false;
        failureReason("Not built with two level namespaces");
    }

    // don't put debug variants into dyld cache
    if ( endsWith(path, "_profile.dylib") || endsWith(path, "_debug.dylib") || endsWith(path, "_profile") || endsWith(path, "_debug") || endsWith(path, "/CoreADI") ) {
        retval = false;
        failureReason("Variant image");
    }

    // dylib must have extra info for moving DATA and TEXT segments apart
    __block bool hasExtraInfo = false;
    __block bool hasDyldInfo = false;
    __block bool hasExportTrie = false;
    __block bool hasLazyLoad = false;
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == LC_SEGMENT_SPLIT_INFO )
            hasExtraInfo = true;
        if ( cmd->cmd == LC_DYLD_INFO_ONLY )
            hasDyldInfo = true;
        if ( cmd->cmd == LC_DYLD_EXPORTS_TRIE )
            hasExportTrie = true;
        if ( cmd->cmd == LC_LAZY_LOAD_DYLIB )
            hasLazyLoad = true;
    });
    if ( !hasExtraInfo ) {
        retval = false;
        failureReason("Missing split seg info");
    }
    if ( !hasDyldInfo && !hasExportTrie ) {
        retval = false;
        failureReason("Old binary, missing dyld info or export trie");
    }
    if ( hasLazyLoad ) {
        retval = false;
        failureReason("Has lazy load");
    }

    // dylib can only depend on other dylibs in the shared cache
    __block bool allDepPathsAreGood = true;
    forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
        if ( !isSharedCacheEligiblePath(loadPath) ) {
            allDepPathsAreGood = false;
            stop = true;
        }
    });
    if ( !allDepPathsAreGood ) {
        retval = false;
        failureReason("Depends on dylibs ineligable for dyld cache");
    }

    // dylibs with interposing info cannot be in cache
    if ( hasInterposingTuples() ) {
        retval = false;
        failureReason("Has interposing tuples");
    }

    // Temporarily kick out swift binaries out of dyld cache on watchOS simulators as they have missing split seg
    if ( (this->cputype == CPU_TYPE_I386) && builtForPlatform(Platform::watchOS_simulator) ) {
        if ( strncmp(dylibName, "/usr/lib/swift/", 15) == 0 ) {
            retval = false;
            failureReason("i386 swift binary");
        }
    }

    return retval;
}

#if BUILDING_APP_CACHE_UTIL
bool MachOFile::canBePlacedInKernelCollection(const char* path, void (^failureReason)(const char*)) const
{
    // only dylibs and the kernel itself can go in cache
    if ( this->filetype == MH_EXECUTE ) {
        // xnu
    } else if ( this->isKextBundle() ) {
        // kext's
    } else {
        failureReason("Not MH_KEXT_BUNDLE");
        return false;
    }

    if ( this->filetype == MH_EXECUTE ) {
        // xnu

        // two-level namespace binaries cannot go in cache
        if ( (this->flags & MH_TWOLEVEL) != 0 ) {
            failureReason("Built with two level namespaces");
            return false;
        }

        // xnu kernel cannot have a page zero
        __block bool foundPageZero = false;
        forEachSegment(^(const SegmentInfo &segmentInfo, bool &stop) {
            if ( strcmp(segmentInfo.segName, "__PAGEZERO") == 0 ) {
                foundPageZero = true;
                stop = true;
            }
        });
        if (foundPageZero) {
            failureReason("Has __PAGEZERO");
            return false;
        }

        // xnu must have an LC_UNIXTHREAD to point to the entry point
        __block bool foundMainLC = false;
        __block bool foundUnixThreadLC = false;
        Diagnostics diag;
        forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
            if ( cmd->cmd == LC_MAIN ) {
                foundMainLC = true;
                stop = true;
            }
            else if ( cmd->cmd == LC_UNIXTHREAD ) {
                foundUnixThreadLC = true;
            }
        });
        if (foundMainLC) {
            failureReason("Found LC_MAIN");
            return false;
        }
        if (!foundUnixThreadLC) {
            failureReason("Expected LC_UNIXTHREAD");
            return false;
        }

        if (diag.hasError()) {
            failureReason("Error parsing load commands");
            return false;
        }

        // The kernel should be a static executable, not a dynamic one
        if ( !isStaticExecutable() ) {
            failureReason("Expected static executable");
            return false;
        }

        // The kernel must be built with -pie
        if ( !isPIE() ) {
            failureReason("Expected pie");
            return false;
        }
    }

    if ( isArch("arm64e") && isKextBundle() && !hasChainedFixups() ) {
        failureReason("Missing fixup information");
        return false;
    }

    // dylibs with interposing info cannot be in cache
    __block bool hasInterposing = false;
    forEachSection(^(const SectionInfo& info, bool malformedSectionRange, bool &stop) {
        if ( ((info.sectFlags & SECTION_TYPE) == S_INTERPOSING) || ((strcmp(info.sectName, "__interpose") == 0) && (strcmp(info.segInfo.segName, "__DATA") == 0)) )
            hasInterposing = true;
    });
    if ( hasInterposing ) {
        failureReason("Has interposing tuples");
        return false;
    }

    // Only x86_64 is allowed to have RWX segments
    if ( !isArch("x86_64") && !isArch("x86_64h") ) {
        __block bool foundBadSegment = false;
        forEachSegment(^(const SegmentInfo &info, bool &stop) {
            if ( (info.protections & (VM_PROT_WRITE | VM_PROT_EXECUTE)) == (VM_PROT_WRITE | VM_PROT_EXECUTE) ) {
                failureReason("Segments are not allowed to be both writable and executable");
                foundBadSegment = true;
                stop = true;
            }
        });
        if ( foundBadSegment )
            return false;
    }

    return true;
}
#endif

static bool platformExcludesPrebuiltClosure_macOS(const char* path) {
    // We no longer support ROSP, so skip all paths which start with the special prefix
    if ( startsWith(path, "/System/Library/Templates/Data/") )
        return true;

    // anything inside a .app bundle is specific to app, so should not get a prebuilt closure
    if ( strstr(path, ".app/") != NULL )
        return true;

    return false;
}

static bool platformExcludesPrebuiltClosure_iOS(const char* path) {
    if ( strcmp(path, "/System/Library/Caches/com.apple.xpc/sdk.dylib") == 0 )
        return true;
    if ( strcmp(path, "/System/Library/Caches/com.apple.xpcd/xpcd_cache.dylib") == 0 )
        return true;
    return false;
}

static bool platformExcludesPrebuiltClosure_tvOS(const char* path) {
    return platformExcludesPrebuiltClosure_iOS(path);
}

static bool platformExcludesPrebuiltClosure_watchOS(const char* path) {
    return platformExcludesPrebuiltClosure_iOS(path);
}

static bool platformExcludesPrebuiltClosure_bridgeOS(const char* path) {
    return platformExcludesPrebuiltClosure_iOS(path);
}

// Returns true if the current platform requires that this install name be excluded from the shared cache
// Note that this overrides any exclusion from anywhere else.
static bool platformExcludesPrebuiltClosure(Platform platform, const char* path) {
    switch (platform) {
        case dyld3::Platform::unknown:
            return false;
        case dyld3::Platform::macOS:
            return platformExcludesPrebuiltClosure_macOS(path);
        case dyld3::Platform::iOS:
            return platformExcludesPrebuiltClosure_iOS(path);
        case dyld3::Platform::tvOS:
            return platformExcludesPrebuiltClosure_tvOS(path);
        case dyld3::Platform::watchOS:
            return platformExcludesPrebuiltClosure_watchOS(path);
        case dyld3::Platform::bridgeOS:
            return platformExcludesPrebuiltClosure_bridgeOS(path);
        case dyld3::Platform::iOSMac:
            return platformExcludesPrebuiltClosure_macOS(path);
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

bool MachOFile::canHavePrecomputedDlopenClosure(const char* path, void (^failureReason)(const char*)) const
{
    __block bool retval = true;

    // only dylibs can go in cache
    if ( (this->filetype != MH_DYLIB) && (this->filetype != MH_BUNDLE) ) {
        retval = false;
        failureReason("not MH_DYLIB or MH_BUNDLE");
    }

    // flat namespace files cannot go in cache
    if ( (this->flags & MH_TWOLEVEL) == 0 ) {
        retval = false;
        failureReason("not built with two level namespaces");
    }

    // can only depend on other dylibs with absolute paths
    __block bool allDepPathsAreGood = true;
    forEachDependentDylib(^(const char* loadPath, bool isWeak, bool isReExport, bool isUpward, uint32_t compatVersion, uint32_t curVersion, bool& stop) {
        if ( loadPath[0] != '/' ) {
            allDepPathsAreGood = false;
            stop = true;
        }
    });
    if ( !allDepPathsAreGood ) {
        retval = false;
        failureReason("depends on dylibs that are not absolute paths");
    }

    __block bool platformExcludedFile = false;
    forEachSupportedPlatform(^(Platform platform, uint32_t minOS, uint32_t sdk) {
        if ( platformExcludedFile )
            return;
        if ( platformExcludesPrebuiltClosure(platform, path) ) {
            platformExcludedFile = true;
            return;
        }
    });
    if ( platformExcludedFile ) {
        failureReason("file cannot get a prebuilt closure on this platform");
        return false;
    }

    // dylibs with interposing info cannot have dlopen closure pre-computed
    if ( hasInterposingTuples() ) {
        retval = false;
        failureReason("has interposing tuples");
    }

    // special system dylib overrides cannot have closure pre-computed
    if ( strncmp(path, "/usr/lib/system/introspection/", 30) == 0 ) {
        retval = false;
        failureReason("override of OS dylib");
    }

    return retval;
}

bool MachOFile::hasInterposingTuples() const
{
    __block bool hasInterposing = false;
    forEachSection(^(const SectionInfo& info, bool malformedSectionRange, bool &stop) {
        if ( ((info.sectFlags & SECTION_TYPE) == S_INTERPOSING) || ((strcmp(info.sectName, "__interpose") == 0) && (strcmp(info.segInfo.segName, "__DATA") == 0)) )
            hasInterposing = true;
    });
    return hasInterposing;
}

bool MachOFile::isFairPlayEncrypted(uint32_t& textOffset, uint32_t& size) const
{
    if ( const encryption_info_command* encCmd = findFairPlayEncryptionLoadCommand() ) {
       if ( encCmd->cryptid == 1 ) {
            // Note: cryptid is 0 in just-built apps.  The AppStore sets cryptid to 1
            textOffset = encCmd->cryptoff;
            size       = encCmd->cryptsize;
            return true;
        }
    }
    textOffset = 0;
    size = 0;
    return false;
}

bool MachOFile::canBeFairPlayEncrypted() const
{
    return (findFairPlayEncryptionLoadCommand() != nullptr);
}

const encryption_info_command* MachOFile::findFairPlayEncryptionLoadCommand() const
{
    __block const encryption_info_command* result = nullptr;
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
         if ( (cmd->cmd == LC_ENCRYPTION_INFO) || (cmd->cmd == LC_ENCRYPTION_INFO_64) ) {
            result = (encryption_info_command*)cmd;
            stop = true;
        }
    });
    if ( diag.noError() )
        return result;
    else
        return nullptr;
}


bool MachOFile::hasLoadCommand(uint32_t cmdNum) const
{
    __block bool hasLC = false;
    Diagnostics diag;
    forEachLoadCommand(diag, ^(const load_command* cmd, bool& stop) {
        if ( cmd->cmd == cmdNum ) {
            hasLC = true;
            stop = true;
        }
    });
    return hasLC;
}

bool MachOFile::allowsAlternatePlatform() const
{
    __block bool result = false;
    forEachSection(^(const SectionInfo& info, bool malformedSectionRange, bool& stop) {
        if ( (strcmp(info.sectName, "__allow_alt_plat") == 0) && (strncmp(info.segInfo.segName, "__DATA", 6) == 0) ) {
            result = true;
            stop = true;
        }
    });
    return result;
}

bool MachOFile::hasChainedFixups() const
{
#if SUPPORT_ARCH_arm64e
    // arm64e always uses chained fixups
    if ( (this->cputype == CPU_TYPE_ARM64) && (this->maskedCpuSubtype() == CPU_SUBTYPE_ARM64E) ) {
        // Not all binaries have fixups at all so check for the load commands
        return hasLoadCommand(LC_DYLD_INFO_ONLY) || hasLoadCommand(LC_DYLD_CHAINED_FIXUPS);
    }
#endif
    return hasLoadCommand(LC_DYLD_CHAINED_FIXUPS);
}

bool MachOFile::hasChainedFixupsLoadCommand() const
{
    return hasLoadCommand(LC_DYLD_CHAINED_FIXUPS);
}

uint64_t MachOFile::read_uleb128(Diagnostics& diag, const uint8_t*& p, const uint8_t* end)
{
    uint64_t result = 0;
    int         bit = 0;
    do {
        if ( p == end ) {
            diag.error("malformed uleb128");
            break;
        }
        uint64_t slice = *p & 0x7f;

        if ( bit > 63 ) {
            diag.error("uleb128 too big for uint64");
            break;
        }
        else {
            result |= (slice << bit);
            bit += 7;
        }
    }
    while (*p++ & 0x80);
    return result;
}


int64_t MachOFile::read_sleb128(Diagnostics& diag, const uint8_t*& p, const uint8_t* end)
{
    int64_t  result = 0;
    int      bit = 0;
    uint8_t  byte = 0;
    do {
        if ( p == end ) {
            diag.error("malformed sleb128");
            break;
        }
        byte = *p++;
        result |= (((int64_t)(byte & 0x7f)) << bit);
        bit += 7;
    } while (byte & 0x80);
    // sign extend negative numbers
    if ( ((byte & 0x40) != 0) && (bit < 64) )
        result |= (~0ULL) << bit;
    return result;
}


} // namespace dyld3





