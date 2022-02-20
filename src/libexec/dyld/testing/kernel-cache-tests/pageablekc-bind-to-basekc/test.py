#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that a kext can bind to another kext
# foo.kext exports foo and bar.kext uses it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/pageablekc-bind-to-basekc/main.kc", "/pageablekc-bind-to-basekc/main.kernel", "/pageablekc-bind-to-basekc/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/pageablekc-bind-to-basekc/main.kc", ["-symbols", "-arch", "arm64"])

    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["vmAddr"] == "0x10000"

    # Now build an pageable cache using the baseline kernel collection
    kernel_cache.buildPageableKernelCollection("arm64", "/pageablekc-bind-to-basekc/pageable.kc", "/pageablekc-bind-to-basekc/main.kc", "/pageablekc-bind-to-basekc/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/pageablekc-bind-to-basekc/pageable.kc", ["-fixups", "-arch", "arm64"])

    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["fixups"]) == 1
    # extern int foo()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0xC000"] == "kc(0) + 0x10000"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

