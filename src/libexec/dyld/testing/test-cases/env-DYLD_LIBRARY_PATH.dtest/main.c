
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/door1/libfoo.dylib -install_name $RUN_DIR/door1/libfoo.dylib -DVALUE=1
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/door2/libfoo.dylib -install_name $RUN_DIR/door2/libfoo.dylib -DVALUE=42
// BUILD:  $CC main.c            -o $BUILD_DIR/env-DYLD_LIBRARY_PATH.exe $BUILD_DIR/door1/libfoo.dylib
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_LIBRARY_PATH.exe

// RUN:  ./env-DYLD_LIBRARY_PATH.exe
// RUN:  DYLD_LIBRARY_PATH=$RUN_DIR/door2/ ./env-DYLD_LIBRARY_PATH.exe

#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern int foo();

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    int expected = (getenv("DYLD_LIBRARY_PATH") != NULL) ? 42 : 1;

	if ( foo() == expected )
        PASS("Success");
    else
        FAIL("Wrong dylib loaded");
}

