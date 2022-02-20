#!/usr/bin/python2.7

import os
import KernelCollection

# codeless kext's are in the PRELINK_INFO in the pageableKC, but can be a dependency for auxKC kexts

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/auxkc-kext-bind-to-pageablekc-codeless-kext/main.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/main.kernel", None, [], [])
    kernel_cache.analyze("/auxkc-kext-bind-to-pageablekc-codeless-kext/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Now build an pageable cache using the baseline kernel collection
    kernel_cache.buildPageableKernelCollection("arm64", "/auxkc-kext-bind-to-pageablekc-codeless-kext/pageable.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/main.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/extensions-pageablekc", ["com.apple.bar", "com.apple.codeless"], [])
    kernel_cache.analyze("/auxkc-kext-bind-to-pageablekc-codeless-kext/pageable.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("arm64", "/auxkc-kext-bind-to-pageablekc-codeless-kext/aux.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/main.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/pageable.kc", "/auxkc-kext-bind-to-pageablekc-codeless-kext/extensions-auxkc", ["com.apple.foo"], [])
    kernel_cache.analyze("/auxkc-kext-bind-to-pageablekc-codeless-kext/aux.kc", ["-layout", "-arch", "arm64"])

    # Check the fixups
    kernel_cache.analyze("/auxkc-kext-bind-to-pageablekc-codeless-kext/aux.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x4000"] == "kc(1) + 0x8000"
    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions-auxkc/foo.kext/foo
# [~]> xcrun -sdk iphoneos cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions-pageablekc/bar.kext/bar
# [~]> rm -r extensions-*/*.kext/*.ld
