
// BUILD:  $CC foo.c -bundle     -o $BUILD_DIR/test.bundle
// BUILD:  $CC main.c            -o $BUILD_DIR/image-list-uuid.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./image-list-uuid.exe


#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_images.h>
#include <uuid/uuid.h>

#include "test_support.h"

extern const struct mach_header __dso_handle;

static void printUUIDs(const struct dyld_all_image_infos* infos)
{
    if ( infos->uuidArray != NULL ) {
        for (int i=0; i < infos->uuidArrayCount; ++i) {
            const struct dyld_uuid_info* nonCacheInfo = &infos->uuidArray[i];
            uuid_string_t uuidStr;
            uuid_unparse_upper(nonCacheInfo->imageUUID, uuidStr);
            LOG("%p %s", nonCacheInfo->imageLoadAddress, uuidStr);
          }
    }
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // NOTE: dyld_all_image_infos is private, but currently looked at by kernel during stackshots
    // This test is to validate that the data available to the kernel is correct

    task_dyld_info_data_t task_dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if ( task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count) ) {
        FAIL("task_info() failed");
    }
    const struct dyld_all_image_infos* infos = (struct dyld_all_image_infos*)(uintptr_t)task_dyld_info.all_image_info_addr;


    if ( infos->uuidArray == NULL ) {
        FAIL("infos->uuidArray == NULL");
    }
    if ( infos->uuidArrayCount < 2 ) {
        // expect to contain main executable and dyld
        FAIL("infos->uuidArrayCount != 2 (is %lu)", infos->uuidArrayCount);
    }
    printUUIDs(infos);
    uint32_t initialCount = infos->uuidArrayCount;

    bool foundMain = false;
    bool foundDyld = false;
    for (int i=0; i < infos->uuidArrayCount; ++i) {
        const struct dyld_uuid_info* nonCacheInfo = &infos->uuidArray[i];
        if ( nonCacheInfo->imageLoadAddress == &__dso_handle )
            foundMain = true;
        if ( nonCacheInfo->imageLoadAddress->filetype == MH_DYLINKER )
            foundDyld = true;
    }
    if ( !foundMain ) {
        FAIL("image_infos-uuids uuid array does not contain main program");
    }
    if ( !foundDyld ) {
        FAIL("image_infos-uuids uuid array does not contain dyld");
    }

    void* handle = dlopen(RUN_DIR "/test.bundle", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("image_infos-uuids %s", dlerror());
    }
    LOG("loaded test.bundle");

    // now expect UUID list to be three
    if ( infos->uuidArrayCount != initialCount+1 ) {
        // expect to contain main executable and dyld
        FAIL("infos->uuidArrayCount was not incremented (is %lu)", infos->uuidArrayCount);
    }
    printUUIDs(infos);

    PASS("Success");
}

