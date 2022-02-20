

// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib -install_name libfoo.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/cwd-load.exe
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/cwd-load.exe

// RUN:  ./cwd-load.exe

// libfoo.dylib is loaded from the current directory (not an absolute path)


#include <stdio.h>

#include "test_support.h"

extern int foo;


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( foo == 42 )
        PASS("cwd-relative-load");
    else
        FAIL("cwd-relative-load, wrong value");
}


