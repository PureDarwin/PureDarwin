#!/usr/bin/python2.7

import os
import KernelCollection

# This tests verifies that the vtable in bar.kext is patched
# But also that this can be done against a subclass in the kernel, not just 

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kernel-vtable-patching/main.kc", "/kernel-vtable-patching/main.kernel", "/kernel-vtable-patching/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/kernel-vtable-patching/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/kernel-vtable-patching/main.kc", ["-symbols", "-arch", "x86_64"])
    
    # From foo, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    # Foo::foo()
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][27]["name"] == "__ZN3Foo3fooEv"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][27]["vmAddr"] == "0x15EC0"
    # Foo::fooUsed0()
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][28]["name"] == "__ZN3Foo8fooUsed0Ev"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][28]["vmAddr"] == "0x15EE0"
    
    # From bar, find the vtable and its override of foo()
    # Bar::foo()
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["name"] == "__ZN3Bar3fooEv"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["vmAddr"] == "0x16F10"


    # Check the fixups
    kernel_cache.analyze("/kernel-vtable-patching/main.kc", ["-fixups", "-arch", "x86_64"])
    
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x14500"] == "kc(0) + 0x15EC0 : pointer64"
    # Then the following fixup should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x14508"] == "kc(0) + 0x15EE0 : pointer64"

    # Now in bar, again match the entry for its Bar::foo() symbol
    assert kernel_cache.dictionary()["fixups"]["0x1D150"] == "kc(0) + 0x16F10"
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x1D158"] == "kc(0) + 0x15EE0"


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp foo.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--  -DFOO_USED=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

