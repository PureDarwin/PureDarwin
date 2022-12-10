#!/usr/bin/python2.7

import os
import KernelCollection

# Verify that we can build an auxKC from third party kext's without split seg

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("x86_64", "/auxkc-no-split-seg/main.kc", "/auxkc-no-split-seg/main.kernel", "/auxkc-no-split-seg/extensions", [], [])
    kernel_cache.analyze("/auxkc-no-split-seg/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__HIB"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0x14000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__HIB"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x14000"

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/auxkc-no-split-seg/aux.kc", "/auxkc-no-split-seg/main.kc", "", "/auxkc-no-split-seg/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/auxkc-no-split-seg/aux.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 8
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__REGION0"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__REGION1"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x9000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__REGION2"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0xA000"
    assert kernel_cache.dictionary()["cache-segments"][6]["name"] == "__REGION3"
    assert kernel_cache.dictionary()["cache-segments"][6]["vmAddr"] == "0xB000"
    assert kernel_cache.dictionary()["cache-segments"][7]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][7]["vmAddr"] == "0xC000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x9000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0xC000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0xA000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0xB000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0xC000"

    # Check the fixups
    kernel_cache.analyze("/auxkc-no-split-seg/aux.kc", ["-fixups", "-arch", "x86_64"])
    assert kernel_cache.dictionary()["fixups"] == ""
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["fixups"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x5008"] == "kc(3) + 0x9000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][1]["fixups"]) == 1
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"]["0x5008"] == "kc(3) + 0xB000"


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -Wl,-segprot,__HIB,r-x,r-x -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib bar.c -o extensions/bar.kext/bar

