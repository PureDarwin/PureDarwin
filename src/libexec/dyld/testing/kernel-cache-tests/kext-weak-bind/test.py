#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that weak binds point to the same symbol

# FIXME: This should be re-enabled once we know how to handle classic relocs combined with split seg v2.

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-weak-bind/main.kc", "/kext-weak-bind/main.kernel", "/kext-weak-bind/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/kext-weak-bind/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Check the fixups
    kernel_cache.analyze("/kext-weak-bind/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 2
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0x1C018"
    assert kernel_cache.dictionary()["fixups"]["0x1C010"] == "kc(0) + 0x1C018"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

