#!/usr/bin/python2.7

import os
import KernelCollection

# Code-less kext's have a plist, but no mach-o.  We still need to put them in the prelink info


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/codeless-kexts/main.kc", "/codeless-kexts/main.kernel", "/codeless-kexts/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/codeless-kexts/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 5
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x10000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x10000"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -std=c++11 -Wl,-static -mkernel -Wl,-fixup_chains -Wl,-kernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack

