#!/usr/bin/python2.7

import os
import KernelCollection

# This tests that kexts can bind to each other using DYLD_CHAINED_PTR_64_OFFSET

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-to-kext-arm64-chains/main.kc", "/kext-bind-to-kext-arm64-chains/main.kernel", "/kext-bind-to-kext-arm64-chains/extensions", ["com.apple.foo", "com.apple.bar"], [])

    # layout
    kernel_cache.analyze("/kext-bind-to-kext-arm64-chains/main.kc", ["-layout", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 4
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0xFFFFFFF00701C000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Symbols
    kernel_cache.analyze("/kext-bind-to-kext-arm64-chains/main.kc", ["-symbols", "-arch", "arm64"])
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["vmAddr"] == "0xFFFFFFF007018020"

    # Check the fixups
    kernel_cache.analyze("/kext-bind-to-kext-arm64-chains/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 2
    # bar.kext: extern int foo();
    assert kernel_cache.dictionary()["fixups"]["0x20000"] == "kc(0) + 0xFFFFFFF007014000"
    # main.kernel: __typeof(&func) funcPtr = &func;
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0xFFFFFFF007018020"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -Wl,-fixup_chains -Wl,-kernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-image_base,0xfffffff007004000 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

