#!/usr/bin/python2.7

import os
import KernelCollection

# This tests that kexts can bind to each other using DYLD_CHAINED_PTR_ARM64E_KERNEL

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64e", "/kext-bind-to-kext-arm64e-chains/main.kc", "/kext-bind-to-kext-arm64e-chains/main.kernel", "/kext-bind-to-kext-arm64e-chains/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/kext-bind-to-kext-arm64e-chains/main.kc", ["-layout", "-arch", "arm64e"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["vmAddr"] == "0xFFFFFFF00701C000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Symbols
    kernel_cache.analyze("/kext-bind-to-kext-arm64e-chains/main.kc", ["-symbols", "-arch", "arm64e"])
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][1]["name"] == "_fooPtr"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][1]["vmAddr"] == "0xFFFFFFF007028000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["name"] == "_f"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["vmAddr"] == "0xFFFFFFF007028008"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][1]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][1]["vmAddr"] == "0xFFFFFFF007018070"

    # Check the fixups
    kernel_cache.analyze("/kext-bind-to-kext-arm64e-chains/main.kc", ["-fixups", "-arch", "arm64e"])
    assert len(kernel_cache.dictionary()["fixups"]) == 4
    # __DATA_CONST
    # bar.kext: extern int foo();
    # bar.kext: extern int f;
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0xFFFFFFF007018070 auth(IA addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x18008"] == "kc(0) + 0xFFFFFFF007028008"
    # __DATA
    # main.kernel: __typeof(&func) funcPtr = &func;
    assert kernel_cache.dictionary()["fixups"]["0x20000"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    # bar.kext: __typeof(&foo) fooPtr = &foo;
    assert kernel_cache.dictionary()["fixups"]["0x24000"] == "kc(0) + 0xFFFFFFF007018070 auth(IA !addr 0)"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-static -mkernel -Wl,-fixup_chains -Wl,-kernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-image_base,0xfffffff007004000 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

