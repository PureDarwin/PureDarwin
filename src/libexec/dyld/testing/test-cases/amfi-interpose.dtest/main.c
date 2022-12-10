// BOOT_ARGS: dyld_flags=2

// BUILD:  $CC interposer.c -dynamiclib -o $BUILD_DIR/libmyalloc.dylib -install_name $RUN_DIR/libmyalloc.dylib
// BUILD:  $CC main.c $BUILD_DIR/libmyalloc.dylib  -o $BUILD_DIR/amfi-interpose.exe
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/amfi-interpose.exe

// RUN:  DYLD_AMFI_FAKE=0x7F  ./amfi-interpose.exe
// RUN:  DYLD_AMFI_FAKE=0x3F  ./amfi-interpose.exe

//
// Tests that AMFI_DYLD_OUTPUT_ALLOW_LIBRARY_INTERPOSING bit from AMFI blocks interposing
//

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libamfi.h>

#include "test_support.h"

int main()
{
    // interposed malloc() doubles alloction size and prefills allocation with '#'
    char* p1 = malloc(10);
    bool interposed = (strncmp(p1, "####################", 20) == 0);

    const char* amfiBits = getenv("DYLD_AMFI_FAKE");
    if ( amfiBits == NULL ) {
        FAIL("amfi-interpose: DYLD_AMFI_FAKE not set");
    }
#ifdef AMFI_RETURNS_INTERPOSING_FLAG
    bool allowInterposing = (strcmp(amfiBits, "0x7F") == 0);
#else
    bool allowInterposing = true;
#endif

    if ( interposed == allowInterposing )
        PASS("Success");
    else if ( interposed )
        FAIL("amfi-interpose: malloc interposed, but amfi said to block it");
    else
        FAIL("amfi-interpose: malloc not interposed, but amfi said to allow it");
}
