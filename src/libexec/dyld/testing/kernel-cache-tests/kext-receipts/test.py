#!/usr/bin/python2.7

import os
import KernelCollection


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-receipts/main.kc", "/kext-receipts/main.kernel", None, [],
                                       ["-sectcreate", "__SMALL", "", "$PWD/kext-receipts/small.txt",
                                        "-sectcreate", "__SIXTEEN_CHARSS", "__sixteen_chaars", "$PWD/kext-receipts/large.txt",
                                        "-sectcreate", "__SIXTEEN_CHARSS", "__large2", "$PWD/kext-receipts/large2.txt"])

    # Check the layout
    kernel_cache.analyze("/kext-receipts/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 8
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x4000"

    # small.txt
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__SMALL"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0xC000"
    assert not "sections" in kernel_cache.dictionary()["cache-segments"][3]

    # large.txt, then large2.txt
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__SIXTEEN_CHARSS"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0x10000"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmSize"] == "0x4000"
    assert len(kernel_cache.dictionary()["cache-segments"][4]["sections"]) == 2
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][0]["name"] == "__sixteen_chaars";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][0]["vmAddr"] == "0x10000";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][0]["vmEnd"] == "0x10124";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][0]["vmSize"] == "0x124";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][1]["name"] == "__large2";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][1]["vmAddr"] == "0x10124";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][1]["vmEnd"] == "0x1012E";
    assert kernel_cache.dictionary()["cache-segments"][4]["sections"][1]["vmSize"] == "0xA";

    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0x14000"
    assert kernel_cache.dictionary()["cache-segments"][6]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][6]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["cache-segments"][7]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][7]["vmAddr"] == "0x20000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x18000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0x20000"

    # Check the entry point
    kernel_cache.analyze("/kext-receipts/main.kc", ["-entrypoint", "-arch", "arm64"])
    assert kernel_cache.dictionary()["entrypoint"] == "0x8000"

    # Check the fixups
    kernel_cache.analyze("/kext-receipts/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 4
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0x18028"
    assert kernel_cache.dictionary()["fixups"]["0x18008"] == "kc(0) + 0x18028"
    assert kernel_cache.dictionary()["fixups"]["0x18018"] == "kc(0) + 0x1802C"
    assert kernel_cache.dictionary()["fixups"]["0x18020"] == "kc(0) + 0x1802D"
    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack

