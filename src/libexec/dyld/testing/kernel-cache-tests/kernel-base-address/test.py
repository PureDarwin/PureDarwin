#!/usr/bin/python2.7

import os
import KernelCollection


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kernel-base-address/main.kc", "/kernel-base-address/main.kernel", None, [], [])

    # Check the layout
    kernel_cache.analyze("/kernel-base-address/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0xFFFFFFF007004000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0xFFFFFFF007010000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0xFFFFFFF007014000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0xFFFFFFF00701C000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0xFFFFFFF007014000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0xFFFFFFF00700C000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0xFFFFFFF00701C000"

    # Check the entry point
    kernel_cache.analyze("/kernel-base-address/main.kc", ["-entrypoint", "-arch", "arm64"])
    assert kernel_cache.dictionary()["entrypoint"] == "0xFFFFFFF00700C000"

    # Check the fixups
    kernel_cache.analyze("/kernel-base-address/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 4
    assert kernel_cache.dictionary()["fixups"]["0x10000"] == "kc(0) + 0xFFFFFFF007014028"
    assert kernel_cache.dictionary()["fixups"]["0x10008"] == "kc(0) + 0xFFFFFFF007014028"
    assert kernel_cache.dictionary()["fixups"]["0x10018"] == "kc(0) + 0xFFFFFFF00701402C"
    assert kernel_cache.dictionary()["fixups"]["0x10020"] == "kc(0) + 0xFFFFFFF00701402D"
    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -Wl,-image_base,0xfffffff007004000

