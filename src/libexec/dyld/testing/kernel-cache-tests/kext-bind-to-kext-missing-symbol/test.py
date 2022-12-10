#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that we throw errors on each kext which is missing a symbol

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-to-kext-missing-symbol/main.kc", "/kext-bind-to-kext-missing-symbol/main.kernel", "/kext-bind-to-kext-missing-symbol/extensions", ["com.apple.foo", "com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 2
    # bar.kext
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "Failed to bind '_baz' in 'com.apple.bar' (at offset 0x0 in __DATA, __got) as could not find a kext which exports this symbol"
    # bar.kext
    assert kernel_cache.dictionary()[1]["id"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()[1]["errors"]) == 1
    assert kernel_cache.dictionary()[1]["errors"][0] == "Failed to bind '_baz' in 'com.apple.foo' (at offset 0x0 in __DATA, __got) as could not find a kext which exports this symbol"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

