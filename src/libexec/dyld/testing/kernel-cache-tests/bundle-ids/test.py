#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that we are able to build a kernel collection with a kernel and kext's referenced as bundle id's

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/bundle-ids/main.kc", "/bundle-ids/main.kernel", "/bundle-ids/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/bundle-ids/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x1C000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0x20000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"].endswith("com.apple.kernel")
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x20000"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"].endswith("bar")
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0x14000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x1C000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["vmAddr"] == "0x20000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"].endswith("foo")
    assert len(kernel_cache.dictionary()["dylibs"][2]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmAddr"] == "0x14030"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["vmAddr"] == "0x1C010"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["vmAddr"] == "0x20000"

    # Check the fixups
    kernel_cache.analyze("/bundle-ids/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 2
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0x14000"
    assert kernel_cache.dictionary()["fixups"]["0x1C010"] == "kc(0) + 0x1C018"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"].endswith("com.apple.kernel")
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"].endswith("bar")
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"].endswith("foo")
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

