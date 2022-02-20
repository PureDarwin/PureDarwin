#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that we look in the foo.kext Plugins subdirectory for more kexts

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64", "/plugins/main.kc", "/plugins/main.kernel", "/plugins/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/plugins/main.kc", ["-layout", "-arch", "arm64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -Wl,-e,__start -Wl,-pagezero_size,0x0 -Wl,-pie main.c -o main.kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info foo.c -o extensions/foo.kext/foo
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info bar.c -o extensions/foo.kext/PlugIns/bar.kext/bar
# [~]> rm -r extensions/foo.kext/*.ld
# [~]> rm -r extensions/foo.kext/PlugIns/bar.kext/*.ld

