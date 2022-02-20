
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib           -install_name $RUN_DIR/libfoo.dylib -DVALUE=1
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/fallback/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib -DVALUE=42
// BUILD:  $CC main.c            -o $BUILD_DIR/env-DYLD_FALLBACK_LIBRARY_PATH.exe $BUILD_DIR/libfoo.dylib
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_FALLBACK_LIBRARY_PATH.exe

// BUILD: $SKIP_INSTALL $BUILD_DIR/libfoo.dylib

// RUN:  DYLD_FALLBACK_LIBRARY_PATH=$RUN_DIR/fallback/ ./env-DYLD_FALLBACK_LIBRARY_PATH.exe

#include <stdio.h>

#include "test_support.h"

extern int foo();

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( foo() == 42 )
        PASS("Success");
    else
        FAIL("libfoo.dylib incorrectly used fallback");
}

