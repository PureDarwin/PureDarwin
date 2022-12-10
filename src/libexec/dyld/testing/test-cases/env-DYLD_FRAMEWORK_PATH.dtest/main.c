
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/Frameworks/Foo.framework/Foo     -install_name $RUN_DIR/Frameworks/Foo.framework/Foo -DVALUE=1
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/Frameworks-alt/Foo.framework/Foo -install_name $RUN_DIR/Frameworks/Foo.framework/Foo -DVALUE=42
// BUILD:  $CC main.c            -o $BUILD_DIR/env-DYLD_FRAMEWORK_PATH.exe $BUILD_DIR/Frameworks/Foo.framework/Foo
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/env-DYLD_FRAMEWORK_PATH.exe

// RUN:  ./env-DYLD_FRAMEWORK_PATH.exe
// RUN:  DYLD_FRAMEWORK_PATH=$RUN_DIR/Frameworks-alt/ ./env-DYLD_FRAMEWORK_PATH.exe

#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern int foo();

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    int expected = (getenv("DYLD_FRAMEWORK_PATH") != NULL) ? 42 : 1;

	if ( foo() == expected )
        PASS("Success");
    else
        FAIL("Incorrect libfoo.dylib loaded");
}

