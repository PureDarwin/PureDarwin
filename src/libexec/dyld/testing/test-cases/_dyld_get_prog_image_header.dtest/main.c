// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC main.c            -o $BUILD_DIR/_dyld_get_prog_image_header.exe
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/_dyld_get_prog_image_header.exe

// RUN:  ./_dyld_get_prog_image_header.exe
// RUN:  DYLD_INSERT_LIBRARIES=libfoo.dylib  ./_dyld_get_prog_image_header.exe


#include <mach-o/dyld_priv.h>

#include "test_support.h"

int main(int argc, const char* argv[]) {
    uint32_t i = 0;
    const struct mach_header* mhA = NULL;
    do {
        mhA = _dyld_get_image_header(i++);
    }
    while (mhA->filetype != MH_EXECUTE);
    
    const struct mach_header* mhB = _dyld_get_prog_image_header();
    if ( mhA != mhB )
        FAIL("Incorrect mach header address (%p)", mhA);

    PASS("Success");
}
