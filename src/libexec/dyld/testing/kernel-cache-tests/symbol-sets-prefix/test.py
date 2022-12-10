#!/usr/bin/python2.7

import os
import KernelCollection

# Symbol sets can have a prefix with an implicit * wildcard on the end which re-exports anything from xnu with that name

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/symbol-sets-prefix/main.kc", "/symbol-sets-prefix/main.kernel", "/symbol-sets-prefix/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/symbol-sets-prefix/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA_CONST"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0x14000"

    # Check the symbols
    kernel_cache.analyze("/symbol-sets-prefix/main.kc", ["-symbols", "-arch", "arm64"])
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["global-symbols"]) == 6
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][0]["name"] == "__mh_execute_header"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][1]["name"] == "__start"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["name"] == "_symbol_from_xnu0"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][2]["vmAddr"] == "0xC000"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["name"] == "_symbol_from_xnu1"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["vmAddr"] == "0xC00C"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][4]["name"] == "_symbol_from_xnu2"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][4]["vmAddr"] == "0xC018"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][5]["name"] == "_symbol_from_xnu3"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][5]["vmAddr"] == "0xC024"

    # Check the fixups
    kernel_cache.analyze("/symbol-sets-prefix/main.kc", ["-fixups", "-arch", "arm64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 4
    assert kernel_cache.dictionary()["fixups"]["0x14000"] == "kc(0) + 0xC000"
    assert kernel_cache.dictionary()["fixups"]["0x14008"] == "kc(0) + 0xC00C"
    assert kernel_cache.dictionary()["fixups"]["0x14010"] == "kc(0) + 0xC018"
    assert kernel_cache.dictionary()["fixups"]["0x14018"] == "kc(0) + 0xC024"
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> rm -r extensions/foo.kext/*.ld

