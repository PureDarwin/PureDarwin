
// BUILD:  $CC foo.c -dynamiclib  -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/dyld_images_for_addresses.exe

// RUN:  ./dyld_images_for_addresses.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <uuid/uuid.h>
#include <mach-o/dyld_priv.h>

#include "test_support.h"

extern void* __dso_handle;

extern int foo1();
extern int foo2();
extern int foo3();


int myfunc()
{
    return 3;
}

int myfunc2()
{
    return 3;
}

static int mystaticfoo()
{
    return 3;
}

int mydata = 5;
int myarray[10];


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    int mylocal;

    const void* addresses[12];
    addresses[0] = &myfunc;
    addresses[1] = &myfunc2;
    addresses[2] = &mystaticfoo;
    addresses[3] = &__dso_handle;
    addresses[4] = &mydata;
    addresses[5] = &myarray;
    addresses[6] = &mylocal;    // not owned by dyld, so coresponding dyld_image_uuid_offset should be all zeros
    addresses[7] = &foo1;
    addresses[8] = &foo2;
    addresses[9] = &foo3;
    addresses[10] = &fopen;
    addresses[11] = &fclose;

    struct dyld_image_uuid_offset infos[12];
    _dyld_images_for_addresses(12, addresses, infos);

    for (int i=0; i < 12; ++i) {
        uuid_string_t str;
        uuid_unparse_upper(infos[i].uuid, str);
        LOG("0x%09llX 0x%08llX %s", (long long)infos[i].image, infos[i].offsetInImage, str);
    }

    if ( infos[0].image != infos[1].image )
        FAIL("0 vs 1");
    else if ( infos[0].image != infos[2].image )
        FAIL("0 vs 2");
    else if ( infos[0].image != infos[3].image )
        FAIL("0 vs 3");
    else if ( infos[0].image != infos[4].image )
        FAIL("0 vs 4");
    else if ( infos[0].image != infos[5].image )
        FAIL("0 vs 5");
    else if ( infos[6].image != NULL )
        FAIL("6 vs null ");
    else if ( infos[7].image != infos[8].image )
        FAIL("7 vs 8");
    else if ( infos[7].image != infos[9].image )
        FAIL("7 vs 9");
    else if ( infos[0].image == infos[7].image )
        FAIL("0 vs 7");
    else if ( infos[10].image != infos[11].image )
        FAIL("10 vs 11");
    else if ( uuid_compare(infos[0].uuid, infos[1].uuid) != 0  )
        FAIL("uuid 0 vs 1");
    else if ( uuid_compare(infos[0].uuid, infos[2].uuid) != 0 )
        FAIL("uuid 0 vs 2");
    else if ( uuid_compare(infos[0].uuid, infos[3].uuid) != 0 )
        FAIL("uuid 0 vs 3");
    else if ( uuid_compare(infos[0].uuid, infos[4].uuid) != 0 )
        FAIL("uuid 0 vs 4");
    else if ( uuid_compare(infos[0].uuid, infos[5].uuid) != 0 )
        FAIL("uuid 0 vs 5");
    else if ( uuid_is_null(infos[6].uuid) == 0 )
        FAIL("uuid 6 vs null");
    else if ( uuid_compare(infos[7].uuid, infos[8].uuid) != 0 )
        FAIL("uuid 7 vs 8");
    else if ( uuid_compare(infos[7].uuid, infos[9].uuid) != 0 )
        FAIL("uuid 7 vs 9");
    else if ( uuid_compare(infos[0].uuid, infos[7].uuid) == 0 )
        FAIL("uuid 0 vs 7");
    else if ( uuid_compare(infos[10].uuid, infos[11].uuid) != 0 )
        FAIL("uuid 10 vs 11");
    else
        PASS("Success");
}

