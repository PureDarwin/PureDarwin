#!/usr/bin/python2.7

import os
import KernelCollection


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kernel-uuid/main.kc", "/kernel-uuid/main.kernel", None, [], [])

    # Check the UUID
    kernel_cache.analyze("/kernel-uuid/main.kc", ["-uuid", "-arch", "arm64"])
    assert kernel_cache.dictionary()["uuid"] != "00000000-0000-0000-0000-000000000000"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack

