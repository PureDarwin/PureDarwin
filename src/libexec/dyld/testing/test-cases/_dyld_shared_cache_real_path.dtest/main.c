
// BUILD:  $CC main.c            -o $BUILD_DIR/_dyld_shared_cache_real_path.exe

// RUN:  ./_dyld_shared_cache_real_path.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <mach-o/dyld_priv.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    size_t length;
    bool hasCache = ( _dyld_get_shared_cache_range(&length) != NULL );
    if ( hasCache ) {
        const char* path = _dyld_shared_cache_real_path("/usr/lib/libSystem.dylib");
        if ( path == NULL )
            FAIL("libSystem.dylib is not in dyld cache");
        else if ( strcmp(path, "/usr/lib/libSystem.B.dylib") != 0 )
            FAIL("libSystem.B.dylib != %s", path);

#if TARGET_OS_OSX
        // actual path
        path = _dyld_shared_cache_real_path("/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation");
        if ( path == NULL )
            FAIL("Foundation is not in dyld cache");
        else if ( strcmp(path, "/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation") != 0 )
            FAIL("Foundation != %s", path);

        // symlink inside the shared cache
        path = _dyld_shared_cache_real_path("/System/Library/Frameworks/Foundation.framework/Foundation");
        if ( path == NULL )
            FAIL("Foundation is not in dyld cache");
        else if ( strcmp(path, "/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation") != 0 )
            FAIL("Foundation != %s", path);

        // symlink not in the shared cache (as we don't handle directory symlinks today)
        path = _dyld_shared_cache_real_path("/System/Library/Frameworks/Foundation.framework/Versions/Current/Foundation");
        if ( path == NULL )
            FAIL("Foundation is not in dyld cache");
        else if ( strcmp(path, "/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation") != 0 )
            FAIL("Foundation != %s", path);
#endif
    } else {
        const char* path = _dyld_shared_cache_real_path("/usr/lib/libSystem.B.dylib");
        if ( path != NULL )
            FAIL("no cache, but libSystem.B.dylib is in dyld cache");
    }

#if 0
    if (  != hasCache ) {
        if ( hasCache )
            FAIL("libSystem.B.dylib is not in dyld cache");
        else
            FAIL("no cache, but libSystem.B.dylib is in dyld cache");
    }
    
    if ( _dyld_shared_cache_real_path("/System/Library/Frameworks/Foundation.framework/Foundation") != hasCache ) {
         if ( hasCache )
             FAIL("Foundation.framework is not in dyld cache");
         else
             FAIL("no cache, but Foundation.framework is in dyld cache");
     }

#if TARGET_OS_OSX
    if ( _dyld_shared_cache_real_path("/System/Library/Frameworks/Foundation.framework/Versions/Current/Foundation") != hasCache ) {
        if ( hasCache )
            FAIL("Current/Foundation.framework is not in dyld cache");
        else
            FAIL("no cache, but Current/Foundation.framework is in dyld cache");
     }
#endif

#endif

    PASS("SUCCESS");
}

