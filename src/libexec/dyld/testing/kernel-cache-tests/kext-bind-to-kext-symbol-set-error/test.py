#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that an non-Apple kext cannot bind to another Apple kext with a symbol set in the kext
# foo.kext exports foo and bar.kext uses it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-to-kext-symbol-set-error/main.kc", "/kext-bind-to-kext-symbol-set-error/main.kernel", "/kext-bind-to-kext-symbol-set-error/extensions", ["com.apple.foo", "not.com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 1
    assert kernel_cache.dictionary()[0]["id"] == "not.com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "Failed to bind '_foo' in 'not.com.apple.bar' (at offset 0x0 in __DATA, __got) as could not find a kext which exports this symbol"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

