#!/usr/bin/python2.7

import os
import KernelCollection

# OSMetaClass in the kernel has a vtable which has to be patched in to Foo::MetaClass

def check(kernel_cache):
    kernel_cache.buildKernelCollection("x86_64", "/auxkc-vtable-metaclass-patching/main.kc", "/auxkc-vtable-metaclass-patching/main.kernel", None, [], [])
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/main.kc", ["-symbols", "-arch", "x86_64"])
    
    # From OSMetaClass, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][11]["name"] == "__ZN15OSMetaClassBase11placeholderEv"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][11]["vmAddr"] == "0x14BE0"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][12]["name"] == "__ZN15OSMetaClassBase18metaclassBaseUsed4Ev"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][12]["vmAddr"] == "0x14BF0"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][13]["name"] == "__ZN15OSMetaClassBase18metaclassBaseUsed5Ev"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][13]["vmAddr"] == "0x14C10"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][14]["name"] == "__ZN15OSMetaClassBase18metaclassBaseUsed6Ev"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][14]["vmAddr"] == "0x14C30"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][15]["name"] == "__ZN15OSMetaClassBase18metaclassBaseUsed7Ev"
    assert kernel_cache.dictionary()["dylibs"][0]["global-symbols"][15]["vmAddr"] == "0x14C50"


    # Check the fixups
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/main.kc", ["-fixups", "-arch", "x86_64"])
    
    # In vtable for OSMetaClass, we match the entry for OSMetaClass::placeholder() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x14168"] == "kc(0) + 0x14BE0 : pointer64"
    # Then the following fixup should be to OSMetaClass::metaclassBaseUsed4()
    assert kernel_cache.dictionary()["fixups"]["0x14170"] == "kc(0) + 0x14BF0 : pointer64"
    # Then OSMetaClass::metaclassBaseUsed5()
    assert kernel_cache.dictionary()["fixups"]["0x14178"] == "kc(0) + 0x14C10 : pointer64"
    # Then OSMetaClass::metaclassBaseUsed6()
    assert kernel_cache.dictionary()["fixups"]["0x14180"] == "kc(0) + 0x14C30 : pointer64"
    # Then OSMetaClass::metaclassBaseUsed7()
    assert kernel_cache.dictionary()["fixups"]["0x14188"] == "kc(0) + 0x14C50 : pointer64"


    # -----------------------------------------------------------
    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/auxkc-vtable-metaclass-patching/aux.kc", "/auxkc-vtable-metaclass-patching/main.kc", "", "/auxkc-vtable-metaclass-patching/extensions", ["com.apple.foo"], [])
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/aux.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.foo"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/aux.kc", ["-symbols", "-arch", "x86_64"])
    
    # From foo.kext, find the vtable and its override of placeholder()
    # Foo::placeholder()
    assert kernel_cache.dictionary()["dylibs"][0]["local-symbols"][4]["name"] == "__ZN3Foo11placeholderEv"
    assert kernel_cache.dictionary()["dylibs"][0]["local-symbols"][4]["vmAddr"] == "0xCF90"


    # Check the fixups
    kernel_cache.analyze("/auxkc-vtable-metaclass-patching/aux.kc", ["-fixups", "-arch", "x86_64"])

    # Now in foo.kext, match the entry for its Foo::placeholder() symbol
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x50"] == "kc(3) + 0xCF90"
    # And if the patching was correct, then following entry should be to OSMetaClass::metaclassBaseUsed4()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x58"] == "kc(0) + 0x10BF0"
    # Then OSMetaClass::metaclassBaseUsed5()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x60"] == "kc(0) + 0x10C10"
    # Then OSMetaClass::metaclassBaseUsed6()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x68"] == "kc(0) + 0x10C30"
    # Then OSMetaClass::metaclassBaseUsed7()
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"]["0x70"] == "kc(0) + 0x10C50"


# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r-- -DMETACLASS_BASE_USED=1
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.cpp -o extensions/foo.kext/foo -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

