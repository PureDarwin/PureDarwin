
// BUILD:  $CP bad.txt $BUILD_DIR/libnota.dylib
// BUILD:  $CC main.c  -o $BUILD_DIR/dlopen-bad-file.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./dlopen-bad-file.exe

#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // try to dlopen() a text file
    void* handle = dlopen(RUN_DIR "/libnota.dylib", RTLD_FIRST);
    if ( handle != NULL ) {
        FAIL("Should have failed on non-mach-o file %s", RUN_DIR "/libnota.dylib");
    }
    const char* message = dlerror();
    if ( (strstr(message, "mach-o") == NULL) && (strstr(message, "too short") == NULL) ) {
        FAIL("dlerror() message '%s' did not contain 'mach-o'", message);
    }

    // try to dlopen() a directory
    handle = dlopen(RUN_DIR, RTLD_FIRST);
    if ( handle != NULL ) {
        FAIL("Should have failed on dir %s", RUN_DIR);
    }
    message = dlerror();
    if ( strstr(message, "not a file") == NULL ) {
        FAIL("dlerror() message '%s' did not contain 'not a file", message);
    }

    PASS("Success");
}

