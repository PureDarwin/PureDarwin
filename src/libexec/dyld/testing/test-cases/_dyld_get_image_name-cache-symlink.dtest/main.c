
// BUILD:  $CC foo.c -dynamiclib -install_name /System/Library/Frameworks/IOKit.framework/IOKit -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/_dyld_get_image_name-cache-symlink.exe $BUILD_DIR/libfoo.dylib

// RUN:  DYLD_LIBRARY_PATH=/usr/lib/system/introspection ./_dyld_get_image_name-cache-symlink.exe


#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // walk images to see if path was converted to real path
    const char* foundPath = NULL;
    int count = _dyld_image_count();
    for (int i=0; i < count; ++i) {
        const char* path = _dyld_get_image_name(i);
        LOG("path[%2d]=%s", i, path);
        if ( strstr(path, "/IOKit") != NULL ) {
            if ( foundPath == NULL ) {
                foundPath = path;
            }
            else {
                FAIL("More than one libfoo found");
            }
        }
    }
    if ( foundPath == NULL ) {
        FAIL("No IOKit found");
    }
    if ( strcmp(foundPath, "/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit") != 0 ) {
        FAIL("Path is symlink not real path: %s", foundPath);
    }

    PASS("Success");
}

