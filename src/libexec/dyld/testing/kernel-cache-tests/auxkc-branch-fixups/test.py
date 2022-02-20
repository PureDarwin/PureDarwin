#!/usr/bin/python2.7

import os
import KernelCollection

# Check that we can link x86_64 kext's with branch relocations

def check(kernel_cache):
    # First build a kernel collection
    kernel_cache.buildKernelCollection("x86_64", "/auxkc-branch-fixups/main.kc", "/auxkc-branch-fixups/main.kernel", "/auxkc-branch-fixups/extensions", [], [])
    kernel_cache.analyze("/auxkc-branch-fixups/main.kc", ["-layout", "-arch", "x86_64"])

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/auxkc-branch-fixups/aux.kc", "/auxkc-branch-fixups/main.kc", "", "/auxkc-branch-fixups/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/auxkc-branch-fixups/aux.kc", ["-layout", "-arch", "x86_64"])


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__HIB,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -Wl,-segprot,__HIB,r-x,r-x -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib bar.c -o extensions/bar.kext/bar

