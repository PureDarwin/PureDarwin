#!/usr/bin/python2.7

import os
import KernelCollection

# Check that weak binds can be missing, so long as we check for the magic symbol

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/kext-missing-weak-bind/main.kc", "/kext-missing-weak-bind/main.kernel", "/kext-missing-weak-bind/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/kext-missing-weak-bind/main.kc", ["-layout", "-arch", "arm64"])
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0x18000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Symbols
    kernel_cache.analyze("/kext-missing-weak-bind/main.kc", ["-symbols", "-arch", "arm64"])
    # kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["name"] == "_gOSKextUnresolved"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["vmAddr"] == "0x20000"

    # Check the fixups
    kernel_cache.analyze("/kext-missing-weak-bind/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 4
    assert kernel_cache.dictionary()["fixups"]["0x18000"] == "kc(0) + 0x20000"
    assert kernel_cache.dictionary()["fixups"]["0x18008"] == "kc(0) + 0x20000"
    assert kernel_cache.dictionary()["fixups"]["0x18010"] == "kc(0) + 0x20000"
    assert kernel_cache.dictionary()["fixups"]["0x18018"] == "kc(0) + 0x20000"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r-- main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/bar.kext/bar -Wl,-fixup_chains
# [~]> rm -r extensions/*.kext/*.ld

