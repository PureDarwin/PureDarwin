// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC present.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/dylib-static-weak-present.exe
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo2.dylib -install_name $RUN_DIR/libfoomissing.dylib
// BUILD:  $CC missing.c $BUILD_DIR/libfoo2.dylib -o $BUILD_DIR/dylib-static-weak-missing.exe

// BUILD: $SKIP_INSTALL $BUILD_DIR/libfoo2.dylib

// RUN:  ./dylib-static-weak-present.exe
// RUN:  ./dylib-static-weak-missing.exe


#include <stddef.h>
#include <stdio.h>

#include "test_support.h"

extern int foo __attribute__((weak_import));


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // dylib will be found at runtime, so &foo should never be NULL
    if ( &foo != NULL ) {
       if ( foo == 42 )
            PASS("Success");
        else
            FAIL("Wrong value");
    }
    else {
        FAIL("&foo == NULL");
    }
}


