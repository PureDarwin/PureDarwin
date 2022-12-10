
// BUILD:  $CC main.c            -o $BUILD_DIR/shared_cache_optimized.exe

// RUN:  ./shared_cache_optimized.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <mach-o/dyld_priv.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // tests run on internal installs which use un-optimzed dyld cache
    if ( _dyld_shared_cache_optimized() )
        FAIL("unexpectedly returned true");
    else
        PASS("Success");
}

