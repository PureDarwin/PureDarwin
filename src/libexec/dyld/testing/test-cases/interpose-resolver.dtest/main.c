
// BUILD:  $CC foo.c        -dynamiclib -o $BUILD_DIR/libfoo.dylib         -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC interposer.c -dynamiclib -o $BUILD_DIR/libmyinterpose.dylib -install_name $RUN_DIR/libmyinterpose.dylib $BUILD_DIR/libfoo.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/resolver-only.exe
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/interposed-resolver.exe -DINTERPOSED $BUILD_DIR/libmyinterpose.dylib

// RUN:  ./resolver-only.exe
// RUN:  ./interposed-resolver.exe


#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern int foo();
int (*pFoo)() = &foo;

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
#if INTERPOSED
    if ( foo() != 11 )
        FAIL("foo() != 11");
    else if ( (*pFoo)() != 11 )
        FAIL("*pFoo() != 11");
    else
        PASS("Success");
#else
    if ( foo() != 10 )
        FAIL(" foo() != 10");
    else if ( (*pFoo)() != 10 )
        FAIL(" *pFoo() != 10");
    else
        PASS("Success");
#endif
}
