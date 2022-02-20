#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that a kext can bind to another kext
# foo.kext exports foo and bar.kext uses it
# This is the same as kext-vtable-patching but checks that overrides of methods in parent classes
# are propagated to child classes
# Foo defines fooOverride().  

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/kext-vtable-patching-overrides/main.kc", "/kext-vtable-patching-overrides/main.kernel", "/kext-vtable-patching-overrides/extensions", ["com.apple.foo", "com.apple.bar"], [])
    kernel_cache.analyze("/kext-vtable-patching-overrides/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 3
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.foo"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/kext-vtable-patching-overrides/main.kc", ["-symbols", "-arch", "x86_64"])
    
    # From bar, find the vtable and its override of foo()
    # Bar::foo()
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["name"] == "__ZN3Bar3fooEv"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["vmAddr"] == "0x24F10"
    
    # From foo, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    # Foo::fooOverride()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][3]["name"] == "__ZN3Foo11fooOverrideEv"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][3]["vmAddr"] == "0x26BA0"
    # Foo::foo()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][7]["name"] == "__ZN3Foo3fooEv"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][7]["vmAddr"] == "0x26B80"
    # Foo::fooUsed0()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][8]["name"] == "__ZN3Foo8fooUsed0Ev"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][8]["vmAddr"] == "0x26BC0"

    # From foo
    # FooSub::fooOverride()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][21]["name"] == "__ZN6FooSub11fooOverrideEv"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][21]["vmAddr"] == "0x26EA0"
    # Foo::foo()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][22]["name"] == "__ZN6FooSub3fooEv"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][22]["vmAddr"] == "0x26E80"


    # Check the fixups
    kernel_cache.analyze("/kext-vtable-patching-overrides/main.kc", ["-fixups", "-arch", "x86_64"])
    
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x23150"] == "kc(0) + 0x26B80"
    # Then the following fixup should be to Foo::fooOverride()
    assert kernel_cache.dictionary()["fixups"]["0x23158"] == "kc(0) + 0x26BA0"
    # Then the following fixup should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x23160"] == "kc(0) + 0x26BC0"
    
    # In vtable for FooSub, we match the entry for FooSub::foo() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x233A8"] == "kc(0) + 0x26E80"
    # Then the following fixup should be to FooSub::fooOverride()
    assert kernel_cache.dictionary()["fixups"]["0x233B0"] == "kc(0) + 0x26EA0"
    # Then the following fixup should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x233B8"] == "kc(0) + 0x26BC0"

    # Now in bar, again match the entry for its Bar::foo() symbol
    assert kernel_cache.dictionary()["fixups"]["0x21150"] == "kc(0) + 0x24F10"
    # Then the following fixup should be to FooSub::fooOverride()
    assert kernel_cache.dictionary()["fixups"]["0x21158"] == "kc(0) + 0x26EA0"
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x21160"] == "kc(0) + 0x26BC0"


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-no_data_const foo.cpp -o extensions/foo.kext/foo -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -DFOO_USED=1 -DFOO_OVERRIDE=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

