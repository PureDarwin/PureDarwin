#!/usr/bin/python2.7

import os
import KernelCollection

# Update the kmod_info entry for the files here

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kmod-info-local-symbols/main.kc", "/kmod-info-local-symbols/main.kernel", "/kmod-info-local-symbols/extensions", ["com.apple.foo", "com.apple.bar"], [])

    # Check the layout
    kernel_cache.analyze("/kmod-info-local-symbols/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Find the address and size of each kext in the layout and check that these match their kmod info values
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0x205000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmSize"] == "0xFF8"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["vmAddr"] == "0x206000"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["vmSize"] == "0xFF8"

    # Check the kmod info
    kernel_cache.analyze("/kmod-info-local-symbols/main.kc", ["-kmod", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()) == 3
    assert kernel_cache.dictionary()[0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()[0]["kmod_info"] == "none"

    # bar.kext
    assert kernel_cache.dictionary()[1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()[1]["kmod_info"]["info-version"] == "1"
    assert kernel_cache.dictionary()[1]["kmod_info"]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()[1]["kmod_info"]["version"] == "1.0.0"
    assert kernel_cache.dictionary()[1]["kmod_info"]["address"] == "0x205000"
    assert kernel_cache.dictionary()[1]["kmod_info"]["size"] == "0xFF8"

    # foo.kext
    assert kernel_cache.dictionary()[2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()[2]["kmod_info"]["info-version"] == "1"
    assert kernel_cache.dictionary()[2]["kmod_info"]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()[2]["kmod_info"]["version"] == "1.0.0"
    assert kernel_cache.dictionary()[2]["kmod_info"]["address"] == "0x206000"
    assert kernel_cache.dictionary()[2]["kmod_info"]["size"] == "0xFF8"

# [~]> xcrun -sdk macosx.internal cc -fvisibility=hidden -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x200000 -Wl,-segaddr,__HIB,0x100000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk macosx.internal cc -fvisibility=hidden -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -fvisibility=hidden -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

