#!/usr/bin/python2.7

import os
import KernelCollection

# Test unaligned fixups in x86_64 kexts

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/fixups-x86_64-unaligned/main.kc", "/fixups-x86_64-unaligned/main.kernel", "/fixups-x86_64-unaligned/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/fixups-x86_64-unaligned/main.kc", ["-symbols", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"
    # int foo();
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["name"] == "_foo"
    foo_vmaddr_foo = kernel_cache.dictionary()["dylibs"][1]["global-symbols"][0]["vmAddr"]
    # int g;
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][1]["name"] == "_g"
    foo_vmaddr_g = kernel_cache.dictionary()["dylibs"][1]["global-symbols"][1]["vmAddr"]
    # PackedS ps;
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][2]["name"] == "_ps"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][2]["vmAddr"] == "0x20C000"
    # int func();
    assert kernel_cache.dictionary()["dylibs"][1]["local-symbols"][0]["name"] == "_func"
    foo_vmaddr_func = kernel_cache.dictionary()["dylibs"][1]["local-symbols"][0]["vmAddr"]

    # Check the fixups
    kernel_cache.analyze("/fixups-x86_64-unaligned/main.kc", ["-fixups", "-arch", "x86_64"])
    assert len(kernel_cache.dictionary()["fixups"]) == 20
    # foo.kext: PackedS ps = { 0, &func, &func, 0, &g, 0, &g };
    # _ps is at 0x20C000 which is offset 0x10C000 from __HIB
    assert kernel_cache.dictionary()["fixups"]["0x10C004"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10C00C"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10C018"] == "kc(0) + " + foo_vmaddr_g
    assert kernel_cache.dictionary()["fixups"]["0x10C021"] == "kc(0) + " + foo_vmaddr_g
    # foo.kext: PackedS ps_array = { { 0, &func, &func, 0, &g, 0, &g }, ... }
    # _ps_array[0] is at 0x20D000 which is offset 0x10D000 from __HIB
    assert kernel_cache.dictionary()["fixups"]["0x10D004"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10D00C"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10D018"] == "kc(0) + " + foo_vmaddr_g
    assert kernel_cache.dictionary()["fixups"]["0x10D021"] == "kc(0) + " + foo_vmaddr_g
    # _ps_array[1] is at 0x20E000 which is offset 0x10D000 from __HIB
    assert kernel_cache.dictionary()["fixups"]["0x10E004"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10E00C"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10E018"] == "kc(0) + " + foo_vmaddr_g
    assert kernel_cache.dictionary()["fixups"]["0x10E021"] == "kc(0) + " + foo_vmaddr_g
    # _ps_array[2] is at 0x20F000 which is offset 0x10D000 from __HIB
    assert kernel_cache.dictionary()["fixups"]["0x10F004"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10F00C"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x10F018"] == "kc(0) + " + foo_vmaddr_g
    assert kernel_cache.dictionary()["fixups"]["0x10F021"] == "kc(0) + " + foo_vmaddr_g
    # _ps_array[3] is at 0x210000 which is offset 0x10D000 from __HIB
    assert kernel_cache.dictionary()["fixups"]["0x110004"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x11000C"] == "kc(0) + " + foo_vmaddr_func
    assert kernel_cache.dictionary()["fixups"]["0x110018"] == "kc(0) + " + foo_vmaddr_g
    assert kernel_cache.dictionary()["fixups"]["0x110021"] == "kc(0) + " + foo_vmaddr_g

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-kernel -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x200000 -Wl,-segaddr,__HIB,0x100000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo

