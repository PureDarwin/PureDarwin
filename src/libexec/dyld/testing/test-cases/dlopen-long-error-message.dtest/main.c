
// BUILD:  $CC main.c  -o $BUILD_DIR/dlopen-long-error-message.exe

// RUN:  ./dlopen-long-error-message.exe

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    for (int i=0; i < 10; ++i) {
        void* handle = dlopen("/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/bogus/path/libbogus.dylib", RTLD_FIRST);
        if ( handle != NULL ) {
            FAIL("Should have failed on non-existent file");
            return 0;
        }
        free(strdup("hello there"));
    }

    PASS("Success");
}

