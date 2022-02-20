// BUILD_ONLY: MacOSX

// BUILD:           $CC bar.c -dynamiclib -install_name $RUN_DIR/libmissing.dylib -o $BUILD_DIR/libbar.dylib
// BUILD:           $CC foo.c -dynamiclib $BUILD_DIR/libbar.dylib -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:           $CC main.c -o $BUILD_DIR/MATLAB -DRUN_DIR="$RUN_DIR"

// RUN:  ./MATLAB

// libfoo.dylib was weak-def exported symbols.  This tests that it can fail to load cleanly.

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"



int main(int argc, const char* argv[])
{
    // force weak-def table to be generated
    void* handle = dlopen("/usr/lib/libc++.dylib", 0);
    if ( handle == NULL ) {
        FAIL("weak-def-unload, expected dlopen(libc++.dylib) to succeed");
	}

    // try to dlopen something with weak symbols that fails to load
    handle = dlopen(RUN_DIR "/libfoo.dylib", 0);
    if ( handle != NULL ) {
        FAIL("weak-def-unload, expected dlopen to fail");
	}

    PASS("weak-def-unload");
}

