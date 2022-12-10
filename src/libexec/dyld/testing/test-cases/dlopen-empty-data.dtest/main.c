
// BUILD:  $CC foo.c -dynamiclib  -install_name $RUN_DIR/libfoo-static.dylib  -o $BUILD_DIR/libfoo-static.dylib
// BUILD:  $CC foo.c -dynamiclib  -install_name $RUN_DIR/libfoo-dynamic.dylib -o $BUILD_DIR/libfoo-dynamic.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo-static.dylib -o $BUILD_DIR/dlopen-empty-data.exe -DRUN_DIR="$RUN_DIR"


// RUN:  ./dlopen-empty-data.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

// libfoo-static.dylib and libfoo-dynamic.dylib each have an empty (no disk size) DATA segment

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    void* handle = dlopen(RUN_DIR "/libfoo-dynamic.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("libfoo-dynamic.dylib could not be loaded: %s", dlerror());
    }

    PASS("Success");
}

