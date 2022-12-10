#!/usr/bin/python2.7

import os
import KernelCollection

# Test that the codeless bar.kext (with bundle id com.apple.bar) doesn't interfere with foo using the symbol set of the same bundle id

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/symbol-sets-and-codeless-kexts/main.kc", "/symbol-sets-and-codeless-kexts/main.kernel", "/symbol-sets-and-codeless-kexts/extensions", ["com.apple.foo", "com.apple.bar", "com.apple.baz"], [])
    kernel_cache.analyze("/symbol-sets-and-codeless-kexts/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x14000"

    # Find the address of the symbols to bind to
    kernel_cache.analyze("/symbol-sets-and-codeless-kexts/main.kc", ["-symbols", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["name"] == "_symbol_from_xnu"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["name"] == "_symbol_from_xnu_no_alias"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["vmAddr"] == "0xC00C"

    # Check the fixups
    kernel_cache.analyze("/symbol-sets-and-codeless-kexts/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 2
    assert kernel_cache.dictionary()["fixups"]["0x14000"] == "kc(0) + 0xC000"
    assert kernel_cache.dictionary()["fixups"]["0x14008"] == "kc(0) + 0xC00C"
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> rm -r extensions/foo.kext/*.ld

