
// BOOT_ARGS: amfi=3 cs_enforcement_disable=1

// Create the base kernel collection
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CC main.c -o $BUILD_DIR/kernel-auxkc-fixups.exe -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie -Wl,-pagezero_size,0x0 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-stack-protector -fno-builtin -ffreestanding -Wl,-segprot,__HIB,rx,rx -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000 -fno-ptrauth-function-pointer-type-discrimination -ftrivial-auto-var-init=uninitialized
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $APP_CACHE_UTIL -create-kernel-collection $BUILD_DIR/kernel.kc -kernel $BUILD_DIR/kernel-auxkc-fixups.exe

// Create the pageable kernel collection
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CP extensions/pageable.kext/Info.plist $BUILD_DIR/extensions/pageable-kext/Info.plist
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CC pageable.c -o $BUILD_DIR/extensions/pageable-kext/pageable -Wl,-kext -Wl,-kext_objects_dir,$BUILD_DIR/KextObjects -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-ptrauth-function-pointer-type-discrimination
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $APP_CACHE_UTIL -create-pageable-kernel-collection $BUILD_DIR/pageable.kc -kernel-collection $BUILD_DIR/kernel.kc -extensions $BUILD_DIR/extensions -bundle-id com.apple.pageable $DEPENDS_ON $BUILD_DIR/extensions/pageable-kext/Info.plist $DEPENDS_ON $BUILD_DIR/extensions/pageable-kext/pageable

// Create the aux kernel collection
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CP extensions/foo.kext/Info.plist $BUILD_DIR/extensions/foo-kext/Info.plist
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CP extensions/bar.kext/Info.plist $BUILD_DIR/extensions/bar-kext/Info.plist
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CC foo.c -o $BUILD_DIR/extensions/foo-kext/foo -Wl,-kext -Wl,-kext_objects_dir,$BUILD_DIR/KextObjects -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-ptrauth-function-pointer-type-discrimination
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CC bar.c -o $BUILD_DIR/extensions/bar-kext/bar -Wl,-kext -Wl,-kext_objects_dir,$BUILD_DIR/KextObjects -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-ptrauth-function-pointer-type-discrimination
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $APP_CACHE_UTIL -create-aux-kernel-collection $BUILD_DIR/aux.kc -kernel-collection $BUILD_DIR/kernel.kc -pageable-collection $BUILD_DIR/pageable.kc -extensions $BUILD_DIR/extensions  -bundle-id com.apple.foo $DEPENDS_ON $BUILD_DIR/extensions/foo-kext/Info.plist $DEPENDS_ON $BUILD_DIR/extensions/bar-kext/Info.plist $DEPENDS_ON $BUILD_DIR/extensions/foo-kext/foo $DEPENDS_ON $BUILD_DIR/extensions/bar-kext/bar

// BUILD(watchos):

// RUN_STATIC:    $RUN_STATIC $RUN_DIR/kernel.kc $RUN_DIR/pageable.kc - $RUN_DIR/aux.kc

#include "../kernel-test-runner.h"
#include "../kernel-fixups.h"
#include "../kernel-classic-relocs.h"
#include "../kernel-helpers.h"

#define printf(...) funcs->printf(__VA_ARGS__)

int x = 1;
int *g = &x;

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
int _start(const TestRunnerFunctions* funcs)
{
    setFuncs(funcs);

    const void* slideBasePointers[4];
    slideBasePointers[0] = funcs->basePointers[0];
    slideBasePointers[1] = funcs->basePointers[1];
    slideBasePointers[2] = funcs->basePointers[2];
    slideBasePointers[3] = funcs->basePointers[3];
    int slideReturnCode = slide(funcs->mhs[0], slideBasePointers, funcs->printf);
    if ( slideReturnCode != 0 ) {
        FAIL("mhs[0] slide = %d\n", slideReturnCode);
        return 0;
    }

    int slideClassicReturnCode = slideClassic(funcs->mhs[0], funcs->printf);
    if ( slideClassicReturnCode != 0 ) {
        FAIL("mhs[0] slide classic = %d\n", slideClassicReturnCode);
        return 0;
    }

    if ( g[0] != x ) {
    	FAIL("g[0] != x, %d != %d\n", g[0], x);
    	return 0;
    }

    // First slide the pageableKC using the top level fixups.  These handle the branch GOTs
    slideReturnCode = slide(funcs->mhs[1], slideBasePointers, funcs->printf);
    if ( slideReturnCode != 0 ) {
        FAIL("mhs[1] slide = %d\n", slideReturnCode);
        return 0;
    }

    // Then slide pageable using the fixups attached to the kexts own mach headers
    slideReturnCode = slideKextsInsideKernelCollection(funcs->mhs[1], slideBasePointers, funcs->printf, funcs);
    if ( slideReturnCode != 0 ) {
        FAIL("mhs[1] slide = %d\n", slideReturnCode);
        return 0;
    }

    // Slide the auc KC
    slideReturnCode = slide(funcs->mhs[3], slideBasePointers, funcs->printf);
    if ( slideReturnCode != 0 ) {
        FAIL("mhs[3] slide = %d\n", slideReturnCode);
        return 0;
    }

  #if __x86_64__
      // On x86 only, slide the auxKC individually
      // Then slide pageable using the fixups attached to the kexts own mach headers
      slideReturnCode = slideKextsInsideKernelCollection(funcs->mhs[3], slideBasePointers, funcs->printf, funcs);
      if ( slideReturnCode != 0 ) {
          FAIL("mhs[3] slide = %d\n", slideReturnCode);
          return 0;
      }
  #endif

    // If we have any mod init funcs, then lets run them now
    // These are the tests inside the auxKC kexts
    int runModInitFuncs = runAllModInitFunctionsForAppCache(funcs->mhs[3], funcs->printf, funcs);
    if ( runModInitFuncs != 0 ) {
        FAIL("runModInitFuncs = %d\n", runModInitFuncs);
        return 0;
    }

    PASS("Success");
    return 0;
}


