
// BUILD:  $CC main.c            -o $BUILD_DIR/_dyld_shared_cache_contains_path.exe

// RUN:  ./_dyld_shared_cache_contains_path.exe

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

    if ( _dyld_shared_cache_contains_path("/usr/lib/libSystem.B.dylib") != hasCache ) {
        if ( hasCache )
            FAIL("libSystem.B.dylib is not in dyld cache");
        else
            FAIL("no cache, but libSystem.B.dylib is in dyld cache");
    }
    
    if ( _dyld_shared_cache_contains_path("/System/Library/Frameworks/Foundation.framework/Foundation") != hasCache ) {
         if ( hasCache )
             FAIL("Foundation.framework is not in dyld cache");
         else
             FAIL("no cache, but Foundation.framework is in dyld cache");
     }

#if TARGET_OS_OSX
    if ( _dyld_shared_cache_contains_path("/System/Library/Frameworks/Foundation.framework/Versions/Current/Foundation") != hasCache ) {
        if ( hasCache )
            FAIL("Current/Foundation.framework is not in dyld cache");
        else
            FAIL("no cache, but Current/Foundation.framework is in dyld cache");
     }
#endif


    PASS("SUCCESS");
}

