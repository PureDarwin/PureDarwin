
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/librotest.dylib -install_name $RUN_DIR/librotest.dylib -Wl,-data_const
// BUILD:  $CC main.c            -o $BUILD_DIR/read-only-data.exe $BUILD_DIR/librotest.dylib -Wl,-data_const -DRUN_DIR="$RUN_DIR"
// BUILD:  $CC foo.c -bundle     -o $BUILD_DIR/test.bundle -DBUNDLE=1  -Wl,-data_const


// RUN:  ./read-only-data.exe

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>
#include <mach-o/getsect.h>

#include "test_support.h"

extern bool isReadOnly(const void* addr);
extern bool testLib();

const void* const funcs[] = { &malloc, &free, &strcmp, &printf };

typedef bool (*TestFunc)(void);

static void notify(const struct mach_header* mh, intptr_t slide)
{
    // only look at images not in dyld shared cache
    if ( (mh->flags & 0x80000000) == 0 ) {
        LOG("mh=%p flags=0x%08X", mh, mh->flags);
        const char* path = dyld_image_path_containing_address(mh);
        bool inTestDir = (strstr(path, RUN_DIR) != NULL);
        unsigned long size;
    #if __LP64__
        uint8_t* p = getsectiondata((struct mach_header_64*)mh, "__DATA_CONST", "__const", &size);
    #else
        uint8_t* p = getsectiondata(mh, "__DATA_CONST", "__const", &size);
    #endif
        if ( (p != NULL) && inTestDir && !isReadOnly(p) ) {
            FAIL("read-only-data __DATA_CONST,__const section not read-only in %p %s", mh, path);
        }
    }
}


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // test __DATA_CONST in main is read-only
    if ( !isReadOnly(&funcs[2]) ) {
        FAIL("main executable not read-only");
    }

    // test __DATA_CONST in linked dylib is read-only
    if ( !testLib() ) {
        FAIL("librotest.dylib not read-only");
    }

    _dyld_register_func_for_add_image(&notify);

    // test __DATA_CONST in dlopen'ed bundle is read-only
    void* h = dlopen(RUN_DIR "/test.bundle", 0);
    if ( h == NULL ) {
        FAIL("test.bundle not loaded");
    }
    TestFunc tb = (TestFunc)dlsym(h, "testBundle");
    if ( !tb() ) {
        FAIL("test.bundle not read-only");
    }

    PASS("Success");
}

