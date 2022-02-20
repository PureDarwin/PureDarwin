

// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/dylib-static-link.exe

// RUN:  ./dylib-static-link.exe


#include <stdio.h>

#include "test_support.h"

extern int foo;


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( foo == 42 )
        PASS("Success");
    else
        FAIL("Wrong value");
}


