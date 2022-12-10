#!/usr/bin/python2.7

import os
import KernelCollection

# __DATA should be packed at sub-page alignment
# But only in the kexts.  The kernel pages should not be packed

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/packed-kext-text/main.kc", "/packed-kext-text/main.kernel", "/packed-kext-text/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/packed-kext-text/main.kc", ["-layout", "-arch", "arm64"])

    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmSize"] == "0xC000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmSize"] == "0x4000"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmSize"] == "0x20"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmAddr"] == "0x10020"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmSize"] == "0xC"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

