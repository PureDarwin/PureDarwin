// This tests DYLD_VERSIONED_LIBRARY_PATH where the library we are doing a versioned check against is only in the shared
// cache and not on disk.  This is the case when macOS moves to MRM.

// libz.1.dylib was chosen as (on 10.15) it had a current version of 1.2.11 so we can write test dylibs with higer/lower versions

// BUILD(macos):  $CC foo.c -dynamiclib -DRESULT="1.0.0"  -current_version 1.0.0  -install_name /usr/lib/libz.1.dylib -o $BUILD_DIR/alt-1.0.0/libz.1.dylib
// BUILD(macos):  $CC foo.c -dynamiclib -DRESULT="2000.0.0"  -current_version 2000.0.0  -install_name /usr/lib/libz.1.dylib -o $BUILD_DIR/alt-2000.0.0/libz.1.dylib

// BUILD(macos):  $CC main.c            -o $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe -lz
// BUILD(macos):  $CC main.c            -o $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-1.0.0.exe -lz -Wl,-dyld_env,DYLD_VERSIONED_LIBRARY_PATH=@loader_path/alt-1.0.0
// BUILD(macos):  $CC main.c            -o $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-2000.0.0.exe -lz -Wl,-dyld_env,DYLD_VERSIONED_LIBRARY_PATH=@loader_path/alt-2000.0.0
// BUILD(macos):  $CC main.c            -o $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe -DUSE_DLOPEN

// BUILD(macos):  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe
// BUILD(macos):  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe

// BUILD(ios,tvos,watchos,bridgeos):

// Use the host when we have no ENV variable, or we check against the 1.0.0 test dylib which is lower than the host
// RUN: ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe "host"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-1.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe "host"
// RUN: ./env-DYLD_VERSIONED_LIBRARY_PATH-1.0.0.exe "host"
// RUN: ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe "host"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-1.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe "host"

// Use the 2000.0.0 dylib when its present
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-2000.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe "2000.0.0"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-2000.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-1.0.0.exe "2000.0.0"
// RUN: ./env-DYLD_VERSIONED_LIBRARY_PATH-2000.0.0.exe "2000.0.0"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-2000.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe "2000.0.0"

// 2000.0.0 should also override the 1.0.0 version if we specify both (in either order)
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-1.0.0:$RUN_DIR/alt-2000.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe "2000.0.0"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-2000.0.0:$RUN_DIR/alt-1.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs.exe "2000.0.0"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-1.0.0:$RUN_DIR/alt-2000.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe "2000.0.0"
// RUN: DYLD_VERSIONED_LIBRARY_PATH=$RUN_DIR/alt-2000.0.0:$RUN_DIR/alt-1.0.0 ./env-DYLD_VERSIONED_LIBRARY_PATH-missing-dylibs-dlopen.exe "2000.0.0"

#include <stdio.h>  // fprintf(), NULL
#include <stdlib.h> // exit(), EXIT_SUCCESS
#include <string.h>
#include <stdlib.h> // for atoi()

#include "test_support.h"

#if USE_DLOPEN
#include <dlfcn.h>
#else
extern const char* zlibVersion(); // returns "1.2.11" on the host dylib
#endif

int main(int argc, const char* argv[])
{
	const char* expectedResult = argv[1];

#if USE_DLOPEN
    void * handle = dlopen("/usr/lib/libz.1.dylib", RTLD_LAZY);
    if (!handle) {
        FAIL("dlopen(\"%s\") failed with error \"%s\"", "/usr/lib/libz.1.dylib", dlerror());
    }
    const char* (*zlibVersion)() = dlsym(handle, "zlibVersion");
    if (!zlibVersion) {
        FAIL("dlsym(\"zlibVersion\") failed with error \"%s\"", dlerror());
    }
#endif

    const char* actualResult = zlibVersion();
    if ( !strcmp(expectedResult, "host") ) {
        // We don't know what the host version is, but we know we wanted it instead of the test dylibs
        if ( !strcmp(actualResult, "1.0.0") ) {
            FAIL("Using wrong dylib. zlibVersion() returned %s, expected host version", actualResult);
        } else {
            PASS("Success");
        }
    } else {
        if ( strcmp(actualResult, expectedResult) ) {
            FAIL("Using wrong dylib. zlibVersion() returned %s, expected %s", actualResult, expectedResult);
        } else {
            PASS("Success");
        }
    }
	return 0;
}

