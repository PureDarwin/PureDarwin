#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that we use the symbol set when resolving symbols from aux KC to the kernel

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/auxkc-kext-bind-to-kernel/main.kc", "/auxkc-kext-bind-to-kernel/main.kernel", "/auxkc-kext-bind-to-kernel/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/auxkc-kext-bind-to-kernel/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("arm64", "/auxkc-kext-bind-to-kernel/aux.kc", "/auxkc-kext-bind-to-kernel/main.kc", "", "/auxkc-kext-bind-to-kernel/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/auxkc-kext-bind-to-kernel/aux.kc", ["-layout", "-arch", "arm64"])

    # Check the fixups
    kernel_cache.analyze("/auxkc-kext-bind-to-kernel/aux.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x4000"] == "kc(0) + 0x8000"
    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> rm -r extensions/foo.kext/*.ld

