#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that an non-Apple kext can bind to an Apple kext with a symbol set in the kext
# foo.kext exports foo and bar.kext uses it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-to-kext-symbol-set/main.kc", "/kext-bind-to-kext-symbol-set/main.kernel", "/kext-bind-to-kext-symbol-set/extensions", ["com.apple.foo", "not.com.apple.bar"], [])
    kernel_cache.analyze("/kext-bind-to-kext-symbol-set/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "not.com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x1C000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    kernel_cache.analyze("/kext-bind-to-kext-symbol-set/main.kc", ["-symbols", "-arch", "arm64"])
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][0]["vmAddr"] == "0x14020"

    # Check the fixups
    kernel_cache.analyze("/kext-bind-to-kext-symbol-set/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0x14020"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "not.com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> ../kext-symbols.sh SymbolSets.plist ./extensions/foo.kext/Info.plist exports.txt
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.c -o extensions/foo.kext/foo -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

