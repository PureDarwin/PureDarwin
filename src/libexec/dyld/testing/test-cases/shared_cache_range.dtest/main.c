
// BUILD:  $CC main.c            -o $BUILD_DIR/shared_cache_range.exe

// RUN:  ./shared_cache_range.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <mach-o/dyld_priv.h>
#include <dlfcn.h>

#if __has_feature(ptrauth_calls)
    #include <ptrauth.h>
#endif

#include "test_support.h"

static const void *stripPointer(const void *ptr) {
#if __has_feature(ptrauth_calls)
    return __builtin_ptrauth_strip(ptr, ptrauth_key_asia);
#else
    return ptr;
#endif
}


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // see if image containing malloc is in the dyld cache
    Dl_info info;
    if ( dladdr(&malloc, &info) == 0 ) {
        FAIL("shared_cache_range: dladdr(&malloc, xx) fail");
    }
    const struct mach_header* mh = (struct mach_header*)info.dli_fbase;
    LOG("image with malloc=%p", mh);
    if ( mh == NULL ) {
        FAIL("shared_cache_range: dladdr(&malloc, xx) => dli_fbase==NULL");
    }
    bool haveSharedCache = (mh->flags & 0x80000000);
    LOG("haveSharedCache=%d", haveSharedCache);

    size_t cacheLen;
    const void* cacheStart = _dyld_get_shared_cache_range(&cacheLen);

    if ( haveSharedCache ) {
        if ( cacheStart == NULL ) {
            FAIL("_dyld_get_shared_cache_range() returned NULL even though we have a cache");
        }
        LOG("shared cache start=%p, len=0x%0lX", cacheStart, cacheLen);
        const void* cacheEnd = (char*)cacheStart + cacheLen;

        // verify malloc is in shared cache
        if ( (stripPointer((void*)&malloc) < cacheStart) || (stripPointer((void*)&malloc) > cacheEnd) ) {
            FAIL("shared_cache_range: malloc is outside range of cache");
        }
    }
    else {
        if ( cacheStart != NULL ) {
            FAIL("returned non-NULL even though we don't have a cache");
        }
    }

    PASS("Success");
}

