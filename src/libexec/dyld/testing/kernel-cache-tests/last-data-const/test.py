#!/usr/bin/python2.7

import os
import KernelCollection

# Last data const should be folded in under data const and allowed to have fixups

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/last-data-const/main.kc", "/last-data-const/main.kernel", "/last-data-const/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/last-data-const/main.kc", ["-layout", "-arch", "arm64"])

    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmSize"] == "0x8000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__LASTDATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0x18000"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Check we have the correct addresses of the symbols being bound to
    kernel_cache.analyze("/last-data-const/main.kc", ["-symbols", "-arch", "arm64"])
    # main.kernel
    # int g = &x;
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["name"] == "_x"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["vmAddr"] == "0x24000"
    # foo.kext
    # foo()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["vmAddr"] == "0x14030"

    kernel_cache.analyze("/last-data-const/main.kc", ["-fixups", "-arch", "arm64"])
    # main.kernel
    # int g = &x;
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0x24000"
    # foo.kext
    # foo()
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0x14030"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie -Wl,-rename_section,__DATA,__data,__LASTDATA_CONST,__data -Wl,-segprot,__LASTDATA_CONST,r--,r-- main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

