#!/usr/bin/python2.7

import os
import KernelCollection


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/extra-prelink-info/main.kc", "/extra-prelink-info/main.kernel", None, [],
                                       ["-prelink-info-extra", "$PWD/extra-prelink-info/prelink-info.plist"])

    # Check the layout
    kernel_cache.analyze("/extra-prelink-info/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0x0"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0x4000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0x4000"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack

