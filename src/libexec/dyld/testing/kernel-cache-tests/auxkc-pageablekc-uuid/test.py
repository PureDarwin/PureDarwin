#!/usr/bin/python2.7

import os
import KernelCollection

# Verify that an auxKC references the UUID of the base KC
# And also the UUID of the pageable KC

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("arm64", "/auxkc-pageablekc-uuid/main.kc", "/auxkc-pageablekc-uuid/main.kernel", "/auxkc-pageablekc-uuid/extensions", [], [])
    kernel_cache.analyze("/auxkc-pageablekc-uuid/main.kc", ["-layout", "-arch", "arm64"])

    # Check the kernel UUID
    kernel_cache.analyze("/auxkc-pageablekc-uuid/main.kc", ["-uuid", "-arch", "arm64"])
    kernelUUID = kernel_cache.dictionary()["uuid"]
    assert kernelUUID != "00000000-0000-0000-0000-000000000000"
    assert kernelUUID == kernel_cache.dictionary()["prelink-info-uuid"]

    # Now build a pageable cache using the baseline kernel collection
    kernel_cache.buildPageableKernelCollection("arm64", "/auxkc-pageablekc-uuid/pageable.kc", "/auxkc-pageablekc-uuid/main.kc", "/auxkc-pageablekc-uuid/extensions", ["com.apple.foo", "com.apple.bar"], [])

    # Check the pageable UUID
    kernel_cache.analyze("/auxkc-pageablekc-uuid/pageable.kc", ["-uuid", "-arch", "arm64"])
    pageableUUID = kernel_cache.dictionary()["uuid"]
    assert pageableUUID != "00000000-0000-0000-0000-000000000000"
    assert kernel_cache.dictionary()["prelink-info-base-uuid"] == kernelUUID

    # Now build an aux cache using the baseline and pageable kernel collections
    kernel_cache.buildAuxKernelCollection("arm64", "/auxkc-pageablekc-uuid/aux.kc", "/auxkc-pageablekc-uuid/main.kc", "/auxkc-pageablekc-uuid/pageable.kc", "/auxkc-pageablekc-uuid/extensions", ["com.apple.foo", "com.apple.bar"], [])

    # Check the aux UUID
    kernel_cache.analyze("/auxkc-pageablekc-uuid/aux.kc", ["-uuid", "-arch", "arm64"])
    assert kernel_cache.dictionary()["uuid"] != "00000000-0000-0000-0000-000000000000"
    assert kernel_cache.dictionary()["prelink-info-base-uuid"] == kernelUUID
    assert kernel_cache.dictionary()["prelink-info-pageable-uuid"] == pageableUUID


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

