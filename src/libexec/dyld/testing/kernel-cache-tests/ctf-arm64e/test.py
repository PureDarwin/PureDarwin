#!/usr/bin/python2.7

import os
import KernelCollection

# This is the fixups-arm64e test, but with __CTF inserted so that we can see that CTF doesn't impact the result
# Note the ctf.txt file is 16k just to ensure that if its vm addr wasn't updated, we'd exceed the size of __LINKEDIT with the __CTF


def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64e", "/ctf-arm64e/main.kc", "/ctf-arm64e/main.kernel", "/ctf-arm64e/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/ctf-arm64e/main.kc", ["-layout", "-arch", "arm64e"])

    assert len(kernel_cache.dictionary()["cache-segments"]) == 6
    assert kernel_cache.dictionary()["cache-segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["cache-segments"][0]["vmAddr"] == "0xFFFFFFF007004000"
    assert kernel_cache.dictionary()["cache-segments"][1]["name"] == "__PRELINK_TEXT"
    assert kernel_cache.dictionary()["cache-segments"][1]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["cache-segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["cache-segments"][2]["vmAddr"] == "0xFFFFFFF007010000"
    assert kernel_cache.dictionary()["cache-segments"][3]["name"] == "__PRELINK_INFO"
    assert kernel_cache.dictionary()["cache-segments"][3]["vmAddr"] == "0xFFFFFFF00701C000"
    assert kernel_cache.dictionary()["cache-segments"][4]["name"] == "__DATA"
    assert kernel_cache.dictionary()["cache-segments"][4]["vmAddr"] == "0xFFFFFFF007020000"
    assert kernel_cache.dictionary()["cache-segments"][5]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["cache-segments"][5]["vmAddr"] == "0xFFFFFFF007030000"

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    # main.kernel
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert len(kernel_cache.dictionary()["dylibs"][0]["segments"]) == 5
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][0]["vmAddr"] == "0xFFFFFFF007010000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][1]["vmAddr"] == "0xFFFFFFF007020000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][2]["vmAddr"] == "0xFFFFFFF007014000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["name"] == "__CTF"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["vmSize"] == "0x0"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["sections"][0]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][3]["sections"][0]["vmSize"] == "0x0"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][4]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][0]["segments"][4]["vmAddr"] == "0xFFFFFFF007030000"
    # bar.kext
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()["dylibs"][1]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][0]["vmAddr"] == "0xFFFFFFF007008000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][1]["vmAddr"] == "0xFFFFFFF007018000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][2]["vmAddr"] == "0xFFFFFFF00702C000"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][1]["segments"][3]["vmAddr"] == "0xFFFFFFF007030000"
    # foo.kext
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert len(kernel_cache.dictionary()["dylibs"][2]["segments"]) == 4
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["name"] == "__TEXT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][0]["vmAddr"] == "0xFFFFFFF00700C000"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["name"] == "__TEXT_EXEC"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][1]["vmAddr"] == "0xFFFFFFF007018040"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["name"] == "__DATA"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][2]["vmAddr"] == "0xFFFFFFF00702C010"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["name"] == "__LINKEDIT"
    assert kernel_cache.dictionary()["dylibs"][2]["segments"][3]["vmAddr"] == "0xFFFFFFF007030000"

    # Check the fixups
    kernel_cache.analyze("/ctf-arm64e/main.kc", ["-fixups", "-arch", "arm64e"])
    assert len(kernel_cache.dictionary()["fixups"]) == 11
    # main.kernel: S s = { &func, &func, &g, &func, &g };
    assert kernel_cache.dictionary()["fixups"]["0x1C000"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x1C008"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x1C010"] == "kc(0) + 0xFFFFFFF00702802C"
    assert kernel_cache.dictionary()["fixups"]["0x20000"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x20008"] == "kc(0) + 0xFFFFFFF00702802C"
    # main.kernel: PackedS ps = { 0, &func, &func, 0, &g, 0, &g };
    assert kernel_cache.dictionary()["fixups"]["0x24004"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x2400C"] == "kc(0) + 0xFFFFFFF007014000 auth(IA !addr 0)"
    assert kernel_cache.dictionary()["fixups"]["0x24018"] == "kc(0) + 0xFFFFFFF00702802C"
    assert kernel_cache.dictionary()["fixups"]["0x24024"] == "kc(0) + 0xFFFFFFF00702802C"
    # bar.kext: __typeof(&bar) barPtr = &bar;
    assert kernel_cache.dictionary()["fixups"]["0x28000"] == "kc(0) + 0xFFFFFFF007018000 auth(IA !addr 0)"
    # foo.kext: int* gPtr = &g;
    assert kernel_cache.dictionary()["fixups"]["0x28010"] == "kc(0) + 0xFFFFFFF00702C018"
    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"] == "none"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"
    assert kernel_cache.dictionary()["dylibs"][2]["fixups"] == "none"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -std=c++11 -Wl,-static -mkernel -Wl,-fixup_chains -Wl,-kernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-image_base,0xfffffff007004000 -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack
# [~]> xcrun -sdk macosx.internal ctf_insert main.kernel -arch arm64e ctf.txt -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-fixup_chains bar.c -o extensions/bar.kext/bar
# [~]> rm -r extensions/*.kext/*.ld

