#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that an Apple kext can bind to KPI.private with a symbol set in the kext
# com.apple.kpi.private exports foo and bar.kext uses it

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-bind-to-kpi-private-symbol-set/main.kc", "/kext-bind-to-kpi-private-symbol-set/main.kernel", "/kext-bind-to-kpi-private-symbol-set/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/kext-bind-to-kpi-private-symbol-set/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x18000"

    kernel_cache.analyze("/kext-bind-to-kpi-private-symbol-set/main.kc", ["-symbols", "-arch", "arm64"])
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["name"] == "_foo"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["vmAddr"] == "0xC00C"

    # Check the fixups
    kernel_cache.analyze("/kext-bind-to-kpi-private-symbol-set/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 1
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0xC00C"
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

