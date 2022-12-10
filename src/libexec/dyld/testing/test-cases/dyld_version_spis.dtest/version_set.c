// FIXME:  -Wl,-platform_version triggers linker warnings, we need to find a way to stop clang from emitting -platform_version
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,10.14,10.14     -o $BUILD_DIR/version_set_10.14.exe
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,10.14.9,10.14.9 -o $BUILD_DIR/version_set_10.14.9.exe
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,10.15,10.15     -o $BUILD_DIR/version_set_10.15.exe
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,10.15.1,10.15.1 -o $BUILD_DIR/version_set_10.15.1.exe
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,10.16,10.16     -o $BUILD_DIR/version_set_10.16.exe
// BUILD(macos):  $CC version_set.c -Wl,-platform_version,macos,11.0,11.0     -o $BUILD_DIR/version_set_11.exe
// RUN(macos):  ./version_set_10.14.exe
// RUN(macos):  ./version_set_10.14.9.exe
// RUN(macos):  ./version_set_10.15.exe
// RUN(macos):  ./version_set_10.15.1.exe
// RUN(macos):  ./version_set_10.16.exe
// RUN(macos):  ./version_set_11.exe

// BUILD(tvos):  $CC version_set.c -Wl,-platform_version,tvos,12.0,12.0 -o $BUILD_DIR/version_set_12.exe
// BUILD(tvos):  $CC version_set.c -Wl,-platform_version,tvos,12.9,12.9 -o $BUILD_DIR/version_set_12.9.exe
// BUILD(tvos):  $CC version_set.c -Wl,-platform_version,tvos,13.0,13.0 -o $BUILD_DIR/version_set_13.exe
// BUILD(tvos):  $CC version_set.c -Wl,-platform_version,tvos,13.1,13.1 -o $BUILD_DIR/version_set_13.1.exe
// BUILD(tvos):  $CC version_set.c -Wl,-platform_version,tvos,14.0,14.0 -o $BUILD_DIR/version_set_14.exe
// RUN(tvos):  ./version_set_12.exe
// RUN(tvos):  ./version_set_12.9.exe
// RUN(tvos):  ./version_set_13.exe
// RUN(tvos):  ./version_set_13.1.exe
// RUN(tvos):  ./version_set_14.exe

// BUILD(ios):  $CC version_set.c -Wl,-platform_version,ios,12.0,12.0 -o $BUILD_DIR/version_set_12.exe
// BUILD(ios):  $CC version_set.c -Wl,-platform_version,ios,12.9,12.9 -o $BUILD_DIR/version_set_12.9.exe
// BUILD(ios):  $CC version_set.c -Wl,-platform_version,ios,13.0,13.0 -o $BUILD_DIR/version_set_13.exe
// BUILD(ios):  $CC version_set.c -Wl,-platform_version,ios,13.1,13.1 -o $BUILD_DIR/version_set_13.1.exe
// BUILD(ios):  $CC version_set.c -Wl,-platform_version,ios,14.0,14.0 -o $BUILD_DIR/version_set_14.exe
// RUN(ios):  ./version_set_12.exe
// RUN(ios):  ./version_set_12.9.exe
// RUN(ios):  ./version_set_13.exe
// RUN(ios):  ./version_set_13.1.exe
// RUN(ios):  ./version_set_14.exe

// BUILD(watchos):  $CC version_set.c -Wl,-platform_version,watchos,5.0,5.0 -o $BUILD_DIR/version_set_5.exe
// BUILD(watchos):  $CC version_set.c -Wl,-platform_version,watchos,5.9,5.9 -o $BUILD_DIR/version_set_5.9.exe
// BUILD(watchos):  $CC version_set.c -Wl,-platform_version,watchos,6.0,6.0 -o $BUILD_DIR/version_set_6.exe
// BUILD(watchos):  $CC version_set.c -Wl,-platform_version,watchos,6.1,6.1 -o $BUILD_DIR/version_set_6.1.exe
// BUILD(watchos):  $CC version_set.c -Wl,-platform_version,watchos,7.0,7.0 -o $BUILD_DIR/version_set_7.exe
// RUN(watchos):  ./version_set_5.exe
// RUN(watchos):  ./version_set_5.9.exe
// RUN(watchos):  ./version_set_6.exe
// RUN(watchos):  ./version_set_6.1.exe
// RUN(watchos):  ./version_set_7.exe

// BUILD(bridgeos):

#include <stdio.h>
#include <string.h>
#include <mach-o/dyld_priv.h>
#include <dyld/for_dyld_priv.inc>

#include "test_support.h"

extern struct mach_header __dso_handle;

#if (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 101400)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "macOS 10.14"
#elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 101409)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "macOS 10.14.9"
#elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 101500)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "macOS 10.15"
#elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 101501)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "macOS 10.15.1"
#elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 101600)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 true
#define VERSION_NAME "macOS 10.16"
#elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == 110000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 true
#define VERSION_NAME "macOS 11"
#elif (__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ == 120000)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "tvOS 12"
#elif (__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ == 120900)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "tvOS 12.9"
#elif (__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ == 130000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "tvOS 13"
#elif (__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ == 130100)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "tvOS 13.1"
#elif (__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ == 140000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 true
#define VERSION_NAME "tvOS 14"
#elif (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ == 120000)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "iOS 12"
#elif (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ == 120900)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "iOS 12.9"
#elif (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ == 130000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "iOS 13"
#elif (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ == 130100)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "iOS 13.1"
#elif (__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ == 140000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 true
#define VERSION_NAME "iOS 14"
#elif (__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ == 50000)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "watchOS 5"
#elif (__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ == 50900)
#define FALL_2018 true
#define FALL_2019 false
#define FALL_2020 false
#define VERSION_NAME "watchOS 5.9"
#elif (__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ == 60000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "watchOS 6"
#elif (__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ == 60100)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 false
#define VERSION_NAME "watchOS 6.1"
#elif (__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ == 70000)
#define FALL_2018 true
#define FALL_2019 true
#define FALL_2020 true
#define VERSION_NAME "watchOS 7"
#else
#error Unknown version
#endif

void testVersionChecks(const char* versionName, dyld_build_version_t testVersion, bool expected) {
    if (expected != dyld_minos_at_least(&__dso_handle, testVersion)) {
        FAIL(VERSION_NAME "should be %s than %s for dyld_minos_at_least()", versionName, expected ? "newer" : "older");
    }
    if (expected != dyld_sdk_at_least(&__dso_handle, testVersion)) {
        FAIL(VERSION_NAME "should be %s than %s for dyld_sdk_at_least()", versionName, expected ? "newer" : "older");
    }
    if (expected != dyld_program_minos_at_least(testVersion)) {
        FAIL(VERSION_NAME "should be %s than %s for dyld_program_minos_at_least()", versionName, expected ? "newer" : "older");
    }
    if (expected != dyld_program_sdk_at_least(testVersion)) {
        FAIL(VERSION_NAME "should be %s than %s for dyld_program_sdk_at_least()", versionName, expected ? "newer" : "older");
    }
}

int main(void) {
    testVersionChecks("dyld_fall_2018_os_versions", dyld_fall_2018_os_versions, FALL_2018);
    testVersionChecks("dyld_fall_2019_os_versions", dyld_fall_2019_os_versions, FALL_2019);
    testVersionChecks("dyld_fall_2020_os_versions", dyld_fall_2020_os_versions, FALL_2020);

#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
    testVersionChecks("dyld_platform_version_macOS_10_14", dyld_platform_version_macOS_10_14, FALL_2018);
    testVersionChecks("dyld_platform_version_macOS_10_15", dyld_platform_version_macOS_10_15, FALL_2019);
    testVersionChecks("dyld_platform_version_macOS_10_16", dyld_platform_version_macOS_10_16, FALL_2020);
#elif defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
    testVersionChecks("dyld_platform_version_tvOS_12_0", dyld_platform_version_tvOS_12_0, FALL_2018);
    testVersionChecks("dyld_platform_version_tvOS_13_0", dyld_platform_version_tvOS_13_0, FALL_2019);
    testVersionChecks("dyld_platform_version_tvOS_14_0", dyld_platform_version_tvOS_14_0, FALL_2020);
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    testVersionChecks("dyld_platform_version_iOS_12_0", dyld_platform_version_iOS_12_0, FALL_2018);
    testVersionChecks("dyld_platform_version_iOS_13_0", dyld_platform_version_iOS_13_0, FALL_2019);
    testVersionChecks("dyld_platform_version_iOS_14_0", dyld_platform_version_iOS_14_0, FALL_2020);
#elif defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__)
    testVersionChecks("dyld_platform_version_watchOS_5_0", dyld_platform_version_watchOS_5_0, FALL_2018);
    testVersionChecks("dyld_platform_version_watchOS_6_0", dyld_platform_version_watchOS_6_0, FALL_2019);
    testVersionChecks("dyld_platform_version_watchOS_7_0", dyld_platform_version_watchOS_7_0, FALL_2020);
#endif

    PASS("Success");
};
