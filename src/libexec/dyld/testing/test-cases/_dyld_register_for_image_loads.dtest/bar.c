
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "test_support.h"

__attribute__((constructor))
void bar(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    void* handle = dlopen(RUN_DIR "/libbaz.dylib", RTLD_FIRST);
    if ( handle == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libbaz.dylib", dlerror());
        exit(0);
    }
}
