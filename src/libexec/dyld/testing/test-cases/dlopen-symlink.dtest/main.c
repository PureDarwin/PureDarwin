
// BUILD:  $CC foo.c -dynamiclib -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $SYMLINK libfoo.dylib $BUILD_DIR/libfoo-symlink.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen-symlink.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./dlopen-symlink.exe


#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // call dlopen() with a path that is a symlink
    void* handle = dlopen(RUN_DIR "/libfoo-symlink.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    // walk images to see if path was converted to real path
    const char* foundPath = NULL;
    int count = _dyld_image_count();
    for (int i=0; i < count; ++i) {
        const char* path = _dyld_get_image_name(i);
        LOG("path[%2d]=%s", i, path);
        if ( strstr(path, "libfoo") != NULL ) {
            if ( foundPath == NULL ) {
                foundPath = path;
            }
            else {
                FAIL("More than one libfoo found");
            }
        }
    }
    if ( foundPath == NULL ) {
        FAIL("No libfoo found");
    }
    if ( strstr(foundPath, "libfoo-symlink") != NULL ) {
        FAIL("Path is symlink not real path: %s", foundPath);
    }

    PASS("Success");
}

