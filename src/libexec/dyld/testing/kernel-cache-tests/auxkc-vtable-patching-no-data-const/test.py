#!/usr/bin/python2.7

import os
import KernelCollection

# Note, this is the same as auxkc-vtable-patching but without __DATA_CONST.  This tests that we get the correct offsets
# even when the vtables are in __DATA which will be mapped lower than the auxKC mach_header

# This verifies that a kext can bind to another kext
# foo.kext exports foo and bar.kext uses it
# We put foo.kext in the base KC so that the patch slot in bar.kext has to know to use the correct fixup level in the fixup chain

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/auxkc-vtable-patching-no-data-const/main.kc", "/auxkc-vtable-patching-no-data-const/main.kernel", "/auxkc-vtable-patching-no-data-const/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/main.kc", ["-symbols", "-arch", "x86_64"])
    
    # From foo, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    # Foo::foo()
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][6]["name"] == "__ZN3Foo3fooEv"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][6]["vmAddr"] == "0x16ED0"
    # Foo::fooUsed0()
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][7]["name"] == "__ZN3Foo8fooUsed0Ev"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][7]["vmAddr"] == "0x16EF0"


    # Check the fixups
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/main.kc", ["-fixups", "-arch", "x86_64"])
    
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x1D150"] == "kc(0) + 0x16ED0"
    # Then the following fixup should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x1D158"] == "kc(0) + 0x16EF0"


    # -----------------------------------------------------------
    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/auxkc-vtable-patching-no-data-const/aux.kc", "/auxkc-vtable-patching-no-data-const/main.kc", "", "/auxkc-vtable-patching-no-data-const/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/aux.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar"


    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/aux.kc", ["-symbols", "-arch", "x86_64"])
    
    # From bar, find the vtable and its override of foo()
    # Bar::foo()
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["name"] == "__ZN3Bar3fooEv"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][3]["vmAddr"] == "0xCF10"


    # Check the fixups
    kernel_cache.analyze("/auxkc-vtable-patching-no-data-const/aux.kc", ["-fixups", "-arch", "x86_64"])

    # Now in bar, again match the entry for its Bar::foo() symbol
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x150"] == "kc(3) + 0xCF10"
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x158"] == "kc(0) + 0x12EF0"


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.cpp -o extensions/foo.kext/foo -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -DFOO_USED=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-no_data_const
# [~]> rm -r extensions/*.kext/*.ld

