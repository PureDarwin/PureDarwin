
// BUILD:  $CC main.c  -o $BUILD_DIR/dyld_launch_mode.exe

// RUN:  ./dyld_launch_mode.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <mach-o/dyld_priv.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    const char* modeStr = getenv("DYLD_USE_CLOSURES");
    if ( modeStr == NULL ) {
        FAIL("dyld_launch_mode: DYLD_USE_CLOSURES env var not set");
    }

    uint32_t expectedFlags;
    uint32_t launchFlags = _dyld_launch_mode();
    fprintf(stderr, "launchFlags=0x%08x\n", launchFlags);

    if ( strcmp(modeStr, "0") == 0 ) {
        expectedFlags = 0;
    }
    else if ( strcmp(modeStr, "1") == 0 ) {
        expectedFlags = DYLD_LAUNCH_MODE_USING_CLOSURE | DYLD_LAUNCH_MODE_BUILT_CLOSURE_AT_LAUNCH;
    }
    else if ( strcmp(modeStr, "2") == 0 ) {
        expectedFlags = DYLD_LAUNCH_MODE_USING_CLOSURE | DYLD_LAUNCH_MODE_BUILT_CLOSURE_AT_LAUNCH | DYLD_LAUNCH_MODE_MINIMAL_CLOSURE;
    }
    else {
        FAIL("dyld_launch_mode: DYLD_USE_CLOSURES value unknown");
    }


    if ( launchFlags == expectedFlags )
        PASS("dyld_launch_mode");
    else
        FAIL("dyld_launch_mode: expected flags to be 0x%08X but were 0x%08X", expectedFlags, launchFlags);

    return 0;
}

