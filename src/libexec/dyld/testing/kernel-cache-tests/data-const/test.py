#!/usr/bin/python2.7

import os
import KernelCollection

# __DATA_CONST is r/o in xnu and r/w in the kext's but should be r/o in the final image

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/data-const/main.kc", "/data-const/main.kernel", "/data-const/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/data-const/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 7
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmSize"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmSize"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmSize"] == "0xC000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmSize"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x20000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0x24000"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmSize"] == "0x8000"
    assert kernel_cache.dictionary()["cache-segments"][6]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][6]["vmAddr"] == "0x2C000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 5
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x24000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][4]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][4]["vmAddr"] == "0x2C000"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 5
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0x14000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x28000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["vmAddr"] == "0x1C000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][4]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][4]["vmAddr"] == "0x2C000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][2]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmAddr"] == "0x14030"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["vmAddr"] == "0x28004"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["vmAddr"] == "0x2C000"

    # Check we have the correct addresses of the symbols being bound to
    kernel_cache.analyze("/data-const/main.kc", ["-symbols", "-arch", "arm64"])
    # int g = &x;
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["name"] == "_x"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["vmAddr"] == "0x24000"
    # foo()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["vmAddr"] == "0x14030"

    kernel_cache.analyze("/data-const/main.kc", ["-fixups", "-arch", "arm64"])
    # int g = &x;
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0x24000"
    # foo()
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0x14030"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie -Wl,-rename_section,__DATA,__data,__DATA_CONST,__data -Wl,-segprot,__DATA_CONST,r--,r-- main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

