
// BUILD:  $CC foo.c -dynamiclib -Wl,-U,_gInitialisersCalled                                         -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC bar.c -dynamiclib -Wl,-U,_gInitialisersCalled $BUILD_DIR/libfoo.dylib -flat_namespace -install_name $RUN_DIR/libbar.dylib -o $BUILD_DIR/libbar.dylib -Wl,-w
// BUILD:  $CC main.c -DRUN_DIR="$RUN_DIR"                                                                                               -o $BUILD_DIR/dlopen-flat.exe

// RUN:  ./dlopen-flat.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int gInitialisersCalled = 0;

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    int result;

    // Foo exports foo()
    void* fooHandle = 0;
    {
        fooHandle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_LAZY);
        if (!fooHandle) {
            FAIL("dlopen(\"" RUN_DIR "/libfoo.dylib\") failed with error: %s", dlerror());
        }
        if (gInitialisersCalled != 1) {
            FAIL("gInitialisersCalled != 1");
        }
    }
    // Now unload foo which should do something.
    result = dlclose(fooHandle);
    if (result != 0) {
        FAIL("dlclose() returned %c", result);
    }

    // Open foo again which should do something.
    {
        fooHandle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_LAZY);
        if (!fooHandle) {
            FAIL("dlopen failed with error: %s", dlerror());
        }
        if (gInitialisersCalled != 2) {
            FAIL("gInitialisersCalled != 2");
        }
    }

    // Bar is going to resolve foo()
    void* barHandle = 0;
    {
        barHandle = dlopen(RUN_DIR "/libbar.dylib", RTLD_LAZY);
        if (!barHandle) {
            FAIL("dlopen(\"" RUN_DIR "/libbar.dylib\" failed with error: %s", dlerror());
        }
        if (gInitialisersCalled != 3) {
            FAIL("gInitialisersCalled != 3");
        }
    }
    // Now unload foo which shouldn't do anything.
    result = dlclose(fooHandle);
    if (result != 0) {
        FAIL("dlclose(\"" RUN_DIR "/libfoo.dylib\") returned %c", result);
    }

    // Open foo again which shouldn't do anything.
    {
        fooHandle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_LAZY);
        if (!fooHandle) {
            FAIL("dlopen(\"" RUN_DIR "/libfoo.dylib\" failed with error: %s", dlerror());
        }
        if (gInitialisersCalled != 3) {
            FAIL("gInitialisersCalled != 3");
        }
    }

    PASS("Success");
}

