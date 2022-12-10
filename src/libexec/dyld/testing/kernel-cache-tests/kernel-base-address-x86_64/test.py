#!/usr/bin/python2.7

import os
import KernelCollection


def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kernel-base-address-x86_64/main.kc", "/kernel-base-address-x86_64/main.kernel", None, [], [])

    # Check the layout
    kernel_cache.analyze("/kernel-base-address-x86_64/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 7
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0xFFFFFF8000200000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0xFFFFFF8000204000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xFFFFFF8000204000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0xFFFFFF8000208000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0xFFFFFF800020C000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__HIB"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0xFFFFFF8000100000"
    assert kernel_cache.dictionary()["cache-segments"][6]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][6]["vmAddr"] == "0xFFFFFF8000210000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xFFFFFF8000204000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0xFFFFFF800020C000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__HIB"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0xFFFFFF8000100000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0xFFFFFF8000210000"

    # Check the entry point
    kernel_cache.analyze("/kernel-base-address-x86_64/main.kc", ["-entrypoint", "-arch", "x86_64"])
    assert kernel_cache.dictionary()["entrypoint"] == "0xFFFFFF8000100000"

    # Check the fixups
    kernel_cache.analyze("/kernel-base-address-x86_64/main.kc", ["-fixups", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 2
    assert kernel_cache.dictionary()["fixups"]["0x10C008"] == "kc(0) + 0xFFFFFF8000204FF0 : pointer64"
    assert kernel_cache.dictionary()["fixups"]["0x30"] == "kc(0) + 0xFFFFFF8000100028 : pointer64"
    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0xffffff8000200000 -Wl,-segaddr,__HIB,0xffffff8000100000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack

