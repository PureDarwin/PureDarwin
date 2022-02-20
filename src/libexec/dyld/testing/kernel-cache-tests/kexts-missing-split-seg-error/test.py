#!/usr/bin/python2.7

import os
import KernelCollection

# Check errors from canBePlacedInKernelCollection()
# We use a lack of split seg here as arm64 requires it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kexts-missing-split-seg-error/main.kc", "/kexts-missing-split-seg-error/main.kernel", "/kexts-missing-split-seg-error/extensions", ["com.apple.foo", "com.apple.bar"], ["-json-errors"])
    assert len(kernel_cache.dictionary()) == 2
    # bar
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "cannot be placed in kernel collection because: Missing split seg v2"
    # foo
    assert kernel_cache.dictionary()[1]["id"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()[1]["errors"]) == 1
    assert kernel_cache.dictionary()[1]["errors"][0] == "cannot be placed in kernel collection because: Missing split seg v2"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const -mios-version-min=8.0 foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const -mios-version-min=8.0 bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

