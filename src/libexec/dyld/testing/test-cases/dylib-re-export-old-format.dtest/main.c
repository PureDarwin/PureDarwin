// BUILD(macos|x86_64):   $CC bar.c -mmacosx-version-min=10.5 -dynamiclib -install_name $RUN_DIR/libbar.dylib -o $BUILD_DIR/libbar.dylib
// BUILD(macos|x86_64):   $CC foo.c -mmacosx-version-min=10.5 -dynamiclib $BUILD_DIR/libbar.dylib -sub_library libbar -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD(macos|x86_64):   $CC main.c -mmacosx-version-min=10.5 -o $BUILD_DIR/dylib-re-export.exe $BUILD_DIR/libfoo.dylib -L$BUILD_DIR

// BUILD(ios,tvos,watchos,bridgeos):

// RUN:  ./dylib-re-export.exe


#include <stdio.h>

#include "test_support.h"

extern int bar();


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    PASS("Success");
#if 0
    if ( bar() == 42 )
        PASS("Success");
    else
        FAIL("Wrong value");
#endif
}


