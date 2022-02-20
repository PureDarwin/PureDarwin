#!/usr/bin/python2.7

import os
import KernelCollection

# Check errors from canBePlacedInKernelCollection()
# All arm64* binaries cannot use RWX permissions

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/rwx-permissions-error/main.kc", "/rwx-permissions-error/main.kernel", "", [], ["-json-errors"])
    assert len(kernel_cache.dictionary()) == 1
    # kernel
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "cannot be placed in kernel collection because: Segments are not allowed to be both writable and executable"


# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie -segprot __RWX rwx rwx main.c -o main.kernel

