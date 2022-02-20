#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that a kext can bind to another kext
# foo.kext exports foo and bar.kext uses it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/auxkc-bind-to-pageablekc/main.kc", "/auxkc-bind-to-pageablekc/main.kernel", "", [], [])
    kernel_cache.analyze("/auxkc-bind-to-pageablekc/main.kc", ["-layout", "-arch", "arm64"])

    # Now build an pageable cache using the baseline kernel collection
    kernel_cache.buildPageableKernelCollection("arm64", "/auxkc-bind-to-pageablekc/pageable.kc", "/auxkc-bind-to-pageablekc/main.kc", "/auxkc-bind-to-pageablekc/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/auxkc-bind-to-pageablekc/pageable.kc", ["-symbols", "-arch", "arm64"])

    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][0]["vmAddr"] == "0x8000"

    # Now build an aux cache using the baseline and pageable kernel collections
    kernel_cache.buildAuxKernelCollection("arm64", "/auxkc-bind-to-pageablekc/aux.kc", "/auxkc-bind-to-pageablekc/main.kc", "/auxkc-bind-to-pageablekc/pageable.kc", "/auxkc-bind-to-pageablekc/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/auxkc-bind-to-pageablekc/aux.kc", ["-fixups", "-arch", "arm64"])

    # bar.kext
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    # extern int foo()
    assert kernel_cache.dictionary()["fixups"]["0x4000"] == "kc(1) + 0x8000"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

