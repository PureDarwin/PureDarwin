
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen-realpath.exe
// BUILD:  $SYMLINK ./IOKit.framework/IOKit  $BUILD_DIR/IOKit
// BUILD:  $SYMLINK /System/Library/Frameworks/IOKit.framework  $BUILD_DIR/IOKit.framework

//FIXME: Use something besides IOKit so we do not need to copy it into the chroot
// R:  DYLD_FALLBACK_LIBRARY_PATH=/baz  ./dlopen-realpath.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

static void tryImage(const char* path)
{
    void* handle = dlopen(path, RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlerror(\"%s\"): %s", path, dlerror());
    }

    int result = dlclose(handle);
    if ( result != 0 ) {
        FAIL("dlclose(\"%s\"): %s", path, dlerror());
    }
}



int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    tryImage("./IOKit.framework/IOKit");
    tryImage("./././IOKit/../IOKit.framework/IOKit");
    tryImage("./IOKit");

    // Also try libSystem which has an alias in the OS to /usr/lib/libSystem.B.dylib
    tryImage("/usr/lib/libSystem.dylib");

    // Also try using non-canonical path
    // This requires DYLD_FALLBACK_LIBRARY_PATH to be disabled, otherwise it is found that way
    tryImage("//usr/lib/libSystem.dylib");
    tryImage("/usr/./lib/libSystem.dylib");

    PASS("Success");
}

