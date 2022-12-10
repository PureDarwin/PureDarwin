#!/usr/bin/python2.7

import os
import KernelCollection

# Check that we get sensible errors on the bad kext

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-missing-dep/main.kc", "/kext-bind-missing-dep/main.kernel", "/kext-bind-missing-dep/extensions", ["com.apple.foo"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 1
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "Failed to bind '_bar' as could not find a kext with 'com.apple.bar' bundle-id"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> rm -r extensions/*.kext/*.ld

