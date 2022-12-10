#!/usr/bin/python2.7

import os
import KernelCollection

# The non-auxKC can still be 1GB.

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("arm64", "/large-auxkc-no-errors/main.kc", "/large-auxkc-no-errors/main.kernel", "/large-auxkc-no-errors/extensions", [], [])
    kernel_cache.analyze("/large-auxkc-no-errors/main.kc", ["-layout", "-arch", "arm64"])

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("arm64", "/large-auxkc-no-errors/aux.kc", "/large-auxkc-no-errors/main.kc", "", "/large-auxkc-no-errors/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/large-auxkc-no-errors/main.kc", ["-layout", "-arch", "arm64"])


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

