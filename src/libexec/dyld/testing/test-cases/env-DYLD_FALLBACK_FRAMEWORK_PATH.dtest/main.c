
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/Foo.framework/Foo                     -install_name $RUN_DIR/Foo.framework/Foo -DVALUE=1
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/FallbackFrameworks/Foo.framework/Foo -install_name $RUN_DIR/Foo.framework/Foo -DVALUE=42
// BUILD:  $CC main.c            -o $BUILD_DIR/env-DYLD_FALLBACK_FRAMEWORK_PATH.exe $BUILD_DIR/Foo.framework/Foo
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_FALLBACK_FRAMEWORK_PATH.exe

// BUILD: $SKIP_INSTALL $BUILD_DIR/Foo.framework/Foo

// RUN:  DYLD_FALLBACK_FRAMEWORK_PATH=$RUN_DIR/FallbackFrameworks/ ./env-DYLD_FALLBACK_FRAMEWORK_PATH.exe

#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern int foo();

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( foo() == 42 )
        PASS("Success");
    else
        FAIL("foo() was not 42");
}
