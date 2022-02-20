#!/usr/bin/python2.7

import os
import KernelCollection

# Try to patch a vtable, but the parent vtable is too small

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kext-vtable-patching-error-small-vtable/main.kc", "/kext-vtable-patching-error-small-vtable/main.kernel", "/kext-vtable-patching-error-small-vtable/extensions", ["com.apple.foo", "com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 1
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "Malformed vtable.  Super class '__ZTV3Foo' has 40 entries vs subclass '__ZTV3Bar' with 37 entries"

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.cpp -o extensions/foo.kext/foo -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -DFOO_USED=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

