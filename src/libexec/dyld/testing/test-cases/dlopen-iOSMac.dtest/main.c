// BUILD(macos):  $CC main.c   -o $BUILD_DIR/dlopen-iOSMac.exe -DFOR_IOSMAC=1  -target x86_64-darwin-ios13.0-macabi
// BUILD(macos):  $CC main.c   -o $BUILD_DIR/dlopen-macOS.exe -DRUN_DIR="$RUN_DIR"
// BUILD(macos):  $CC cat.c -dynamiclib -install_name $RUN_DIR/libcat.dylib  -o $BUILD_DIR/libcat.dylib -target x86_64-darwin-ios13.0-macabi
// BUILD(macos):  $CC foo.c -dynamiclib -install_name $RUN_DIR/libtestnotincache.dylib  -o $BUILD_DIR/libtestnotincache.dylib $BUILD_DIR/libcat.dylib
// BUILD(macos):  $CC foo.c -dynamiclib -install_name $RUN_DIR/libtestincache.dylib  -o $BUILD_DIR/libtestincache.dylib -framework UIKit -F/System/iOSSupport/System/Library/Frameworks

// BUILD(ios,tvos,watchos,bridgeos):

// RUN:  ./dlopen-iOSMac.exe
// RUN:  ./dlopen-macOS.exe


#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "test_support.h"

int main(int arg, const char* argv[])
{

#if FOR_IOSMAC
    void* handle = dlopen("/System/Library/Frameworks/UIKit.framework/UIKit", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlopen-iOS-on-Mac, UIKit failed to dlopen(): %s", dlerror());
        return 0;
    }
#else
    void* handle = dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlopen-macOS, AppKit failed to dlopen(): %s", dlerror());
        return 0;
    }
    // verify that can't load a macOS dylib that links with a catalyst dylib
    handle = dlopen(RUN_DIR "/libtestnotincache.dylib", RTLD_LAZY);
    if ( handle != NULL ) {
        FAIL("dlopen-macOS, libtestnotincache.dylib should not be loaded");
        return 0;
    }
    // verify that can't load a macOS dylib that links with a catalyst dylib which is in the dyld cache
    handle = dlopen(RUN_DIR "/libtestincache.dylib", RTLD_LAZY);
    if ( handle != NULL ) {
        FAIL("dlopen-macOS, libtestincache.dylib should not be loaded");
        return 0;
    }
#endif

#if FOR_IOSMAC
    PASS("dlopen-iOS-on-Mac");
#else
    PASS("dlopen-macOS");
#endif

	return 0;
}

