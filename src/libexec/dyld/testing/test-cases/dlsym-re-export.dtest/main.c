
// BUILD:  $CC sub1.c -dynamiclib -install_name @rpath/libsub1.dylib   -o $BUILD_DIR/sub1/libsub1.dylib
// BUILD:  $CC sub2.c -dynamiclib -install_name @rpath/libsub2.dylib   -o $BUILD_DIR/sub2/libsub2.dylib
// BUILD:  $CC foo.c  -dynamiclib -install_name $RUN_DIR/libfoo.dylib  -o $BUILD_DIR/libfoo.dylib -rpath @loader_path/sub1 -Wl,-reexport_library,$BUILD_DIR/sub1/libsub1.dylib -Wl,-reexport_library,$BUILD_DIR/sub2/libsub2.dylib
// BUILD:  $CC main.c  -o $BUILD_DIR/dlsym-reexport.exe -DRUN_DIR="$RUN_DIR" -rpath @loader_path/sub2

// RUN:  ./dlsym-reexport.exe

// rpath for sub1 is found in libfoo.dylib.  rpath for sub2 is found in dlsym-reexport.exe

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // RTLD_FIRST means dlsym() should only search libfoo.dylib (and any re-exports)
    void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_FIRST);
    if ( handle == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    void* sym1 = dlsym(handle, "sub1");
    if ( sym1 == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    void* sym2 = dlsym(handle, "sub2");
    if ( sym2 == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    PASS("Success");
}

