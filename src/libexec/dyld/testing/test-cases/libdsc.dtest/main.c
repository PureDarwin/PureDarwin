
// BUILD:  $CC main.c      -o $BUILD_DIR/libdsc-test.exe -ldsc

// RUN: ./libdsc-test.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <mach-o/dyld_priv.h>
#include <dlfcn.h>

#include "test_support.h"
#include "shared-cache/dsc_iterator.h"

// This program links libdsc.a and verifies that dyld_shared_cache_iterate() works

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    size_t cacheLen;
    const void* cacheStart = _dyld_get_shared_cache_range(&cacheLen);

    if ( cacheStart != NULL ) {
        dyld_shared_cache_iterate(cacheStart, cacheLen, ^(const dyld_shared_cache_dylib_info* dylibInfo, const dyld_shared_cache_segment_info* segInfo) {
            if ( false ) {
                printf("%p %s\n", dylibInfo->machHeader, dylibInfo->path);
                printf("    dylib.version=%d\n", dylibInfo->version);
                printf("    dylib.isAlias=%d\n", dylibInfo->isAlias);
                printf("    dylib.inode=%lld\n",   dylibInfo->inode);
                printf("    dylib.modTime=%lld\n", dylibInfo->modTime);
                printf("    segment.name=         %s\n", segInfo->name);
                printf("    segment.fileOffset=   0x%08llX\n", segInfo->fileOffset);
                printf("    segment.fileSize=     0x%08llX\n", segInfo->fileSize);
                printf("    segment.address=      0x%08llX\n", segInfo->address);
                printf("    segment.addressOffset=0x%08llX\n", segInfo->addressOffset);
            }
        });
    }

    PASS("Success");
}
