#!/usr/bin/python2.7

import os
import KernelCollection

# The arm64e auxKC has a lower memory limit than other KCs.  Verify that we get an error with only 64MB in there.

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("arm64e", "/large-auxkc-errors/main.kc", "/large-auxkc-errors/main.kernel", "/large-auxkc-errors/extensions", [], [])
    kernel_cache.analyze("/large-auxkc-errors/main.kc", ["-layout", "-arch", "arm64e"])

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("arm64e", "/large-auxkc-errors/aux.kc", "/large-auxkc-errors/main.kc", "", "/large-auxkc-errors/extensions", ["com.apple.foo", "com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 1
    assert "kernel collection size exceeds maximum size of 67108864" in kernel_cache.dictionary()[0]


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

