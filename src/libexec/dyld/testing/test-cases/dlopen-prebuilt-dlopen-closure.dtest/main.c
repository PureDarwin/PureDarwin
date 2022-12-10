
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen-prebuilt-dlopen-closure.exe

// RUN:  ./dlopen-prebuilt-dlopen-closure.exe

// /usr/lib/libobjc-trampolines.dylib is not in the shared cache, but gets a prebuilt
// closure.  On embedded platforms, this is validated using the cd-hash of the file vs
// what is in the pre-built closure.

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    void* handle = dlopen("/usr/lib/libobjc-trampolines.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    PASS("Success");
}

