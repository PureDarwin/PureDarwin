#!/usr/bin/python2.7

import os
import KernelCollection

# Verify that an auxKC has a reverse vmAddr order

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("arm64", "/hello-world-auxkc/main.kc", "/hello-world-auxkc/main.kernel", "/hello-world-auxkc/extensions", [], [])
    kernel_cache.analyze("/hello-world-auxkc/main.kc", ["-layout", "-arch", "arm64"])

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

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("arm64", "/hello-world-auxkc/aux.kc", "/hello-world-auxkc/main.kc", "", "/hello-world-auxkc/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/hello-world-auxkc/aux.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x14000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0x18000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x14000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0x18000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0x14020"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x18000"

    # Check the fixups
    kernel_cache.analyze("/hello-world-auxkc/aux.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x4000"] == "kc(3) + 0x14020"
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

