
// BOOT_ARGSxx: amfi=3 cs_enforcement_disable=1
// FIXME: re-enable for macOS when it work
// xxBUILDxx(macos|x86_64):  $CC main.c -o $BUILD_DIR/kernel-fixups.exe -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie -Wl,-pagezero_size,0x0 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-stack-protector -fno-builtin -ffreestanding -Wl,-segprot,__HIB,rx,rx -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000
// xxBUILDxx(macos|x86_64):  $APP_CACHE_UTIL -create-kernel-collection $BUILD_DIR/kernel.kc -kernel $BUILD_DIR/kernel-fixups.exe

// BUILDxx(macos,ios,tvos,watchos,bridgeos):

// xxRUN_STATIC:    $RUN_STATIC ./kernel.kc

// This tests that unaligned fixups work in x86_64

#include "../kernel-test-runner.h"
#include "../kernel-fixups.h"
#include "../kernel-classic-relocs.h"

#define printf(...) funcs->printf(__VA_ARGS__)

int x = 1;

struct __attribute__((packed)) __attribute__((aligned((4096)))) PackedS {
    int     i0; // aligned to 8
    int*    p0; // aligned to 4
    int     i1; // aligned to 4
    int*    p1; // aligned to 8
    char    i2; // aligned to 8
    int*    p2; // aligned to 1
};
struct PackedS ps = { 0, &x, 0, &x, 0, &x };

__attribute__((section(("__HIB, __text"))))
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
        FAIL("slide = %d\n", slideReturnCode);
        return 0;
    }

    int slideClassicReturnCode = slideClassic(funcs->mhs[0], funcs->printf);
    if ( slideClassicReturnCode != 0 ) {
        FAIL("mhs[0] slide classic = %d\n", slideClassicReturnCode);
        return 0;
    }

    LOG("Done sliding");

    if ( ps.p0[0] != x ) {
    	FAIL("ps.p1[0] != x, %d != %d\n", ps.p0[0], x);
    	return 0;
    }

    if ( ps.p1[0] != x ) {
        FAIL("ps.p1[0] != x, %d != %d\n", ps.p1[0], x);
        return 0;
    }

    if ( ps.p2[0] != x ) {
        FAIL("ps.p2[0] != x, %d != %d\n", ps.p2[0], x);
        return 0;
    }

    PASS("Success");
    return 0;
}


