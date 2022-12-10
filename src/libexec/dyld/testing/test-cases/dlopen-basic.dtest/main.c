
// BUILD:  $CC foo.c -dynamiclib           -o $BUILD_DIR/test.dylib
// BUILD:  $CC foo.c -bundle               -o $BUILD_DIR/test.bundle
// BUILD:  $CC main.c -DRUN_DIR="$RUN_DIR" -o $BUILD_DIR/dlopen-basic.exe

// RUN:  ./dlopen-basic.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

static void tryImage(const char* path)
{
    void* handle = dlopen(path, RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlopen(\"%s\"), dlerror()=%s", path, dlerror());
    }

    void* sym = dlsym(handle, "foo");
    if ( sym == NULL ) {
        FAIL("dlsym(\"foo\") for \"%s\" returned NULL, dlerror()=%s", path, dlerror());
    }

    int result = dlclose(handle);
    if ( result != 0 ) {
        FAIL("dlclose(\"%s\") returned %d, dlerror()=%s", path, result, dlerror());
    }
}



int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    tryImage(RUN_DIR "/test.bundle");
    tryImage(RUN_DIR "/test.dylib");
    PASS("Success");
}

