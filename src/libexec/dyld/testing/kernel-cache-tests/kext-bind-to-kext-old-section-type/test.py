#!/usr/bin/python2.7

import os
import KernelCollection

# This tests that the very old bar.kext can be parsed.  It's __got section has a S_NON_LAZY_SYMBOL_POINTERS type,
# but newer linkers changed to just S_REGULAR.

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kext-bind-to-kext-old-section-type/main.kc", "/kext-bind-to-kext-old-section-type/main.kernel", "/kext-bind-to-kext-old-section-type/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/kext-bind-to-kext-old-section-type/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][2]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmAddr"] == "0x19000"

    kernel_cache.analyze("/kext-bind-to-kext-old-section-type/main.kc", ["-symbols", "-arch", "x86_64"])
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["vmAddr"] == "0x14000"

    # Check the fixups
    kernel_cache.analyze("/kext-bind-to-kext-old-section-type/main.kc", ["-fixups", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x15000"] == "kc(0) + 0x14000"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# Note, bar.kext has to be linked with a very old linker from 10.7 to get the __got section with S_NON_LAZY_SYMBOL_POINTERS.
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -Wl,-kernel -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__HIB,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -Wl,-segprot,__HIB,r-x,r-x -Wl,-image_base,0x8000 -Wl,-segaddr,__HIB,0x4000
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar -fuse-ld=...

