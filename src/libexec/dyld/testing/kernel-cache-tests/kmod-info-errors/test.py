#!/usr/bin/python2.7

import os
import KernelCollection

# Error for bad kmod info

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kmod-info-errors/main.kc", "/kmod-info-errors/main.kernel", "/kmod-info-errors/extensions", ["com.apple.foo", "com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 2
    # bar
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "unsupported kmod_info version of 2"
    # foo
    assert kernel_cache.dictionary()[1]["id"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()[1]["errors"]) == 1
    assert kernel_cache.dictionary()[1]["errors"][0] == "unsupported kmod_info version of 2"

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.c -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x200000 -Wl,-segaddr,__HIB,0x100000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

