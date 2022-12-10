
// BOOT_ARGS: amfi=3 cs_enforcement_disable=1

// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $CC main.c -o $BUILD_DIR/kernel-hello-world.exe -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie -Wl,-pagezero_size,0x0 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -fno-stack-protector -fno-builtin -ffreestanding -Wl,-segprot,__HIB,rx,rx -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000  -fno-ptrauth-function-pointer-type-discrimination -ftrivial-auto-var-init=uninitialized
// BUILD(macos,ios,tvos,bridgeos|x86_64,arm64,arm64e):  $APP_CACHE_UTIL -create-kernel-collection $BUILD_DIR/kernel-hello-world.kc -kernel $BUILD_DIR/kernel-hello-world.exe -platform kernel

// BUILD(watchos):

// RUN_STATIC:    $RUN_STATIC ./kernel-hello-world.kc

#include "../kernel-test-runner.h"

typedef unsigned long long uint64_t;
extern int printf(const char*, ...);

#define printf(...) funcs->printf(__VA_ARGS__)

#if __x86_64__
__attribute__((section(("__HIB, __text"))))
#else
__attribute__((section(("__TEXT_EXEC, __text"))))
#endif
int _start(const TestRunnerFunctions* funcs)
{
    setFuncs(funcs);
    PASS("Success");
    return 0;
}


