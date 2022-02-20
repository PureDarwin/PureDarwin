
// BUILD:  $CC main.c -DRUN_DIR="$RUN_DIR" -o $BUILD_DIR/dlopen_preflight.exe

// RUN:  ./dlopen_preflight.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    if ( !dlopen_preflight("/System/Library/Frameworks/Foundation.framework/Foundation") ) {
        FAIL("Foundation.framework cannot be found");
    }

#if TARGET_OS_OSX
    if ( !dlopen_preflight("/System/Library/Frameworks/Foundation.framework/Versions/Current/Foundation") ) {
        FAIL("Foundation.framework via symlink cannot be found");
    }
#endif

    PASS("SUCCESS");
}

