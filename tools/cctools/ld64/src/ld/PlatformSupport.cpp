/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-*
 *
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#include <mach-o/loader.h>
#include "PlatformSupport.h"

//FIXME: Only needed until we move VersionSet into PlatformSupport
#include "ld.hpp"
#include "Options.h"

namespace ld {

const Platform basePlatform(Platform platform)
{
    switch(platform) {
        case ld::Platform::iOSMac:
        case ld::Platform::iOS_simulator:
            return ld::Platform::iOS;

        case ld::Platform::watchOS_simulator:
            return Platform::watchOS;

        case ld::Platform::tvOS_simulator:
            return ld::Platform::tvOS;

        default:
            return platform;
    }
}

static const PlatformInfo sAllSupportedPlatforms[] = {
    { Platform::unknown,           Platform::unknown,      "unknown",           "",                                NULL,                        0x00000000, 0x00000000, 0,                       false, true,  PlatEnforce::error,   PlatEnforce::error },
    { Platform::macOS,             Platform::macOS,        "macOS",             "-macos_version_min",             "MACOSX_DEPLOYMENT_TARGET",   0x000A0E00, 0x000A0400, LC_VERSION_MIN_MACOSX,   false, true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::iOS,               Platform::iOS,          "iOS",               "-ios_version_min",               "IPHONEOS_DEPLOYMENT_TARGET", 0x000C0000, 0x00070000, LC_VERSION_MIN_IPHONEOS, true,  true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::tvOS,              Platform::tvOS,         "tvOS",              "-tvos_version_min",              "TVOS_DEPLOYMENT_TARGET",     0x000C0000, 0x00070000, LC_VERSION_MIN_TVOS,     true,  true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::watchOS,           Platform::watchOS,      "watchOS",           "-watchos_version_min",           "WATCHOS_DEPLOYMENT_TARGET",  0x00050000, 0x00020000, LC_VERSION_MIN_WATCHOS,  true,  true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::bridgeOS,          Platform::bridgeOS,     "bridgeOS",          "-bridgeos_version_min",          "BRIDGEOS_DEPLOYMENT_TARGET", 0x00010000, 0x00010000, 0,                       false, true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::iOSMac,            Platform::iOSMac,       "Mac Catalyst",      "-maccatalyst_version_min",       NULL,                         0x000D0000, 0x000D0000, 0,                       false, true,  PlatEnforce::warnInternalErrorExternal, PlatEnforce::warnInternalErrorExternal },
    { Platform::iOS_simulator,     Platform::iOS,          "iOS Simulator",     "-ios_simulator_version_min",     NULL,                         0x000D0000, 0x00080000, LC_VERSION_MIN_IPHONEOS, false, true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::tvOS_simulator,    Platform::tvOS,         "tvOS Simulator",    "-tvos_simulator_version_min",    NULL,                         0x000D0000, 0x00080000, LC_VERSION_MIN_TVOS,     false, true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::watchOS_simulator, Platform::watchOS,      "watchOS Simulator", "-watchos_simulator_version_min", NULL,                         0x00060000, 0x00020000, LC_VERSION_MIN_WATCHOS,  false, true,  PlatEnforce::warning, PlatEnforce::warning },
    { Platform::driverKit,         Platform::driverKit,    "DriverKit",         "-driverkit_version_min",         NULL,                         0x00130000, 0x00130000, 0,                       false, true,  PlatEnforce::error,   PlatEnforce::error },

    { Platform::freestanding,     Platform::freestanding,  "free standing",     "-preload",                       NULL,                         0x00000000, 0,          0,                       false, false, PlatEnforce::allow,   PlatEnforce::allow   },
};

static void versionToString(uint32_t value, char buffer[32])
{
    if ( value & 0xFF )
        sprintf(buffer, "%d.%d.%d", value >> 16, (value >> 8) & 0xFF, value & 0xFF);
    else
        sprintf(buffer, "%d.%d", value >> 16, (value >> 8) & 0xFF);
}

void VersionSet::checkObjectCrosslink(const VersionSet& objectPlatforms, const std::string& targetPath, bool internalSDK,
                                      bool bitcode) const {
    // Check platform cross-linking.
    __block bool warned = false;
    forEach(^(ld::Platform cmdLinePlatform, uint32_t cmdLineMinVersion, uint32_t cmdLineSDKVersion, bool& stop) {
        // <rdar://50990574> if .o file is old and has no platform specified, don't complain (unless building for iOSMac)
        if ( objectPlatforms.empty() && !contains(ld::Platform::iOSMac))
            return;

        if ( !objectPlatforms.contains(cmdLinePlatform) ) {
            if (bitcode) {
                throwf("building for %s, but linking in object file (%s) built for %s,",
                       to_str().c_str(), targetPath.c_str(), objectPlatforms.to_str().c_str());
            } else if ( (count() == 2) && (cmdLinePlatform == ld::Platform::iOSMac) )  {
                // clang is not emitting second LC_BUILD_VERSION in zippered .o files
            }
            else {
                auto enforce = platformInfo(cmdLinePlatform)._linkingObjectFiles;
                if (enforce == PlatEnforce::warnInternalErrorExternal) {
                    if (internalSDK) {
                        enforce = PlatEnforce::warning;
                    } else {
                        enforce = PlatEnforce::error;
                    }
                }
                switch (enforce) {
                    case PlatEnforce::allow:
                        break;
                    case PlatEnforce::warnBI:
                        // only warn during B&I builds
                        if ( (getenv("RC_XBS") != NULL) && (getenv("RC_BUILDIT") == NULL) )
                            break;
                    case PlatEnforce::warning: {
                        if ( !warned ) {
                            warning("building for %s, but linking in object file (%s) built for %s",
                                    to_str().c_str(), targetPath.c_str(), objectPlatforms.to_str().c_str());
                            warned = true;
                        }
                    } break;
                    case PlatEnforce::error:
                        throwf("building for %s, but linking in object file built for %s,",
                               to_str().c_str(), objectPlatforms.to_str().c_str());
                        break;
                    case PlatEnforce::warnInternalErrorExternal:
                        assert(0);
                        break;
                }
            }
        }
        else if ( (cmdLineMinVersion != 0) && (minOS(cmdLinePlatform) > cmdLineMinVersion) ) {
            char t1[32];
            char t2[32];
            versionToString(minOS(cmdLinePlatform), t1);
            versionToString(cmdLineMinVersion, t2);
            warning("object file (%s) was built for newer %s version (%s) than being linked (%s)",
                    targetPath.c_str(), platformInfo(cmdLinePlatform).printName, t1, t2);
        }
    });
}

void VersionSet::checkDylibCrosslink(const VersionSet& dylibPlatforms, const std::string& targetPath,
                                     const std::string& dylibType, bool internalSDK, bool indirectDylib,
                                     bool bitcode) const {
    // Check platform cross-linking.
    __block bool warned = false;
    forEach(^(ld::Platform cmdLinePlatform, uint32_t cmdLineMinVersion, uint32_t cmdLineSDKVersion, bool& stop) {
        // <rdar://51768462> if dylib is old and has no platform specified, don't complain (unless building for iOSMac)
        if ( dylibPlatforms.empty() && !contains(ld::Platform::iOSMac))
            return;
        if ( !dylibPlatforms.contains(cmdLinePlatform) ) {
            // Normally linked dylibs need to support all the platforms we are building for,
            // with the exception of zippered binaries linking to macOS only libraries in the OS.
            // To handle that case we exit early when the commandline platform is iOSMac, if
            // the platforms also contain macOS.
            if (cmdLinePlatform == ld::Platform::iOSMac && contains(ld::Platform::macOS))
                return;
            // <rdar://problem/48416765> spurious warnings when iOSMac binary is built links with zippered dylib that links with macOS dylib
            // <rdar://problem/50517845> iOSMac app links simulator frameworks without warning/error
            if  ( indirectDylib && dylibPlatforms.contains(ld::Platform::macOS) && contains(ld::Platform::iOSMac) )
                return;
            if ( bitcode ) {
                throwf("building for %s, but linking in %s file (%s) built for %s,",
                       to_str().c_str(),  dylibType.c_str(), targetPath.c_str(), dylibPlatforms.to_str().c_str());
            } else if ( (count() == 2) && (cmdLinePlatform == ld::Platform::iOSMac) )  {
                // clang is not emitting second LC_BUILD_VERSION in zippered .o files
            }
            else {
                auto enforce = platformInfo(cmdLinePlatform)._linkingObjectFiles;
                if (enforce == PlatEnforce::warnInternalErrorExternal) {
                    if (internalSDK) {
                        enforce = PlatEnforce::warning;
                    } else {
                        enforce = PlatEnforce::error;
                    }
                }
                switch (enforce) {
                    case PlatEnforce::allow:
                        break;
                    case PlatEnforce::warnBI:
                        // only warn during B&I builds
                        if ( (getenv("RC_XBS") != NULL) && (getenv("RC_BUILDIT") == NULL) )
                            break;
                    case PlatEnforce::warning: {
                        if ( !warned ) {
                            warning("building for %s, but linking in %s file (%s) built for %s",
                                    to_str().c_str(),  dylibType.c_str(), targetPath.c_str(), dylibPlatforms.to_str().c_str());
                            warned = true;
                        }
                    } break;
                    case PlatEnforce::error:
                        throwf("building for %s, but linking in %s built for %s,",
                               to_str().c_str(),  dylibType.c_str(),  dylibPlatforms.to_str().c_str());
                        break;
                    case PlatEnforce::warnInternalErrorExternal:
                        assert(0);
                        break;
                }
            }
        }
    });
}

const PlatformInfo& platformInfo(Platform platform)
{
    for (const PlatformInfo& info : sAllSupportedPlatforms) {
        if ( info.platform == platform )
            return info;
    }
    assert(0 && "unknown platform");
    abort();                 // ld64-port
    __builtin_unreachable(); // ld64-port
}

void forEachSupportedPlatform(void (^handler)(const PlatformInfo& info, bool& stop))
{
    bool stop = false;
    for (const PlatformInfo& info : sAllSupportedPlatforms) {
        handler(info, stop);
        if ( stop )
            break;
    }
}

Platform platformForLoadCommand(uint32_t lc, const mach_header* mh)
{
    Platform result = Platform::unknown;
    bool isIntel = ((mh->cputype & ~CPU_ARCH_MASK) == CPU_TYPE_I386);
    for (const PlatformInfo& info : sAllSupportedPlatforms) {
        if ( info.loadCommandIfNotUsingBuildVersionLC == lc ) {
            // some LC_ are in multiple rows, disabiguate by only setting sim row on intel
            bool isSim = (info.platform != info.basePlatform);
            if ( !isSim || isIntel  )
                result = info.platform;
        }
    }
    return result;
}

// clang puts PLATFORM_IOS into sim .o files instead of PLATFORM_IOSSIMULATOR
Platform platformFromBuildVersion(uint32_t plat, const mach_header* mh)
{
    bool isIntel = ((mh->cputype & ~CPU_ARCH_MASK) == CPU_TYPE_I386);
    if ( (Platform)plat == Platform::iOS ) {
        if (isIntel)
            return Platform::iOS_simulator;
    }
    else if ( (Platform)plat == Platform::tvOS ) {
        if (isIntel)
            return Platform::tvOS_simulator;
    }
    else if ( (Platform)plat == Platform::watchOS ) {
        if (isIntel)
            return Platform::watchOS_simulator;
    }
    return (Platform)plat;
}



} // namespace
