
// BOOT_ARGS: dyld_flags=2

// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libmissing.dylib -install_name @rpath/libmissing.dylib
// BUILD:  $CC foo.c -dynamiclib -Wl,-weak_library,$BUILD_DIR/libmissing.dylib -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib -rpath $RUN_DIR
// BUILD:  $CC main.c -o $BUILD_DIR/rpath-weak-missing.exe -DRUN_DIR="$RUN_DIR"

// BUILD: $SKIP_INSTALL $BUILD_DIR/libmissing.dylib

// RUN:  ./rpath-weak-missing.exe
// RUN:  DYLD_AMFI_FAKE=0 ./rpath-weak-missing.exe

// main prog dlopen()s libfoo.dylib which weak links to @rpath/libmissing.dylib

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int main()
{
    void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("rpath-weak-missing dlopen(\"%s/libfoo.dylib\") - %s", RUN_DIR, dlerror());
    }

    PASS("rpath-weak-missing");
}


