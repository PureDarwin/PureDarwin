#!/usr/bin/python2.7

import os
import KernelCollection

# Bar has a superclass in Foo, but we don't export that symbol.  This causes the vtable patcher to fail

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kernel-vtable-patching-error/main.kc", "/kernel-vtable-patching-error/main.kernel", "/kernel-vtable-patching-error/extensions", ["com.apple.bar"], ["-json-errors"])

    assert len(kernel_cache.dictionary()) == 1
    # bar
    assert kernel_cache.dictionary()[0]["id"] == "com.apple.bar"
    assert len(kernel_cache.dictionary()[0]["errors"]) == 1
    assert kernel_cache.dictionary()[0]["errors"][0] == "Cannot find symbol for metaclass pointed to by '__ZN3Bar10superClassE'.  Expected symbol '__ZN3Foo10gMetaClassE' to be defined in another kext"

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--  -DFOO_USED=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

