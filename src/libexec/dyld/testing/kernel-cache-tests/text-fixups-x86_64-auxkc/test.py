#!/usr/bin/python2.7

import os
import KernelCollection


def findGlobalSymbolVMAddr(kernel_cache, dylib_index, symbol_name):
    for symbol_and_addr in kernel_cache.dictionary()["dylibs"][dylib_index]["global-symbols"]:
        if symbol_and_addr["name"] == symbol_name:
            return symbol_and_addr["vmAddr"]
    return None

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/text-fixups-x86_64-auxkc/main.kc", "/text-fixups-x86_64-auxkc/main.kernel", "/text-fixups-x86_64-auxkc/extensions", [], [])
    kernel_cache.analyze("/text-fixups-x86_64-auxkc/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/text-fixups-x86_64-auxkc/aux.kc", "/text-fixups-x86_64-auxkc/main.kc", "", "/text-fixups-x86_64-auxkc/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/text-fixups-x86_64-auxkc/aux.kc", ["-layout", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0x8000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0x9000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0xC000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 3
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0xA000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0xB000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0xC000"

    # Find the sybmols the fixups point to
    kernel_cache.analyze("/text-fixups-x86_64-auxkc/aux.kc", ["-symbols", "-arch", "x86_64"])
    barAddress = findGlobalSymbolVMAddr(kernel_cache, 0, "_bar")
    gAddress = findGlobalSymbolVMAddr(kernel_cache, 1, "_g")

    # Check the fixups
    kernel_cache.analyze("/text-fixups-x86_64-auxkc/aux.kc", ["-fixups", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()["dylibs"]) == 2

    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][0]["fixups"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x4FF0"] == "kc(3) + " + barAddress

    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    
    assert len(kernel_cache.dictionary()["dylibs"][1]["fixups"]) == 1
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"]["0x4FF0"] == "kc(3) + " + gAddress

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-kernel -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0xffffff8000200000 -Wl,-segaddr,__HIB,0xffffff8000100000 -Wl,-add_split_seg_info  -Wl,-read_only_relocs,suppress -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib bar.c -o extensions/bar.kext/bar

