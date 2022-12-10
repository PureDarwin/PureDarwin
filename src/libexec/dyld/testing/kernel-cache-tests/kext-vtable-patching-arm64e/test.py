#!/usr/bin/python2.7

import os
import KernelCollection

# This verifies that a kext can bind to another kext
# foo.kext exports foo and bar.kext uses it
# bar.kext then exports foo and baz.kext binds to it

# Note this is the same as the kext-vtable-patching test, just with arm64e so ptrauth on the fixups

def check(kernel_cache):
    kernel_cache.buildKernelCollection("arm64e", "/kext-vtable-patching-arm64e/main.kc", "/kext-vtable-patching-arm64e/main.kernel", "/kext-vtable-patching-arm64e/extensions", ["com.apple.foo", "com.apple.bar", "com.apple.baz"], [])
    kernel_cache.analyze("/kext-vtable-patching-arm64e/main.kc", ["-layout", "-arch", "arm64e"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 4
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"
    assert kernel_cache.dictionary()["dylibs"][2]["name"] == "com.apple.baz"
    assert kernel_cache.dictionary()["dylibs"][3]["name"] == "com.apple.foo"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/kext-vtable-patching-arm64e/main.kc", ["-symbols", "-arch", "arm64e"])
    
    # From bar, find the vtable and its override of foo()
    # Bar::foo()
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["name"] == "__ZN3Bar3fooEv"
    assert kernel_cache.dictionary()["dylibs"][1]["global-symbols"][3]["vmAddr"] == "0x2036C"
    
    # From baz, find the vtable and its override of foo()
    # Baz::foo()
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][3]["name"] == "__ZN3Baz3fooEv"
    assert kernel_cache.dictionary()["dylibs"][2]["global-symbols"][3]["vmAddr"] == "0x2085C"
    
    # From foo, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    # Foo::foo()
    assert kernel_cache.dictionary()["dylibs"][3]["global-symbols"][6]["name"] == "__ZN3Foo3fooEv"
    assert kernel_cache.dictionary()["dylibs"][3]["global-symbols"][6]["vmAddr"] == "0x20DE8"
    # Foo::fooUsed0()
    assert kernel_cache.dictionary()["dylibs"][3]["global-symbols"][7]["name"] == "__ZN3Foo8fooUsed0Ev"
    assert kernel_cache.dictionary()["dylibs"][3]["global-symbols"][7]["vmAddr"] == "0x20E00"


    # Check the fixups
    kernel_cache.analyze("/kext-vtable-patching-arm64e/main.kc", ["-fixups", "-arch", "arm64e"])
    
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    assert kernel_cache.dictionary()["fixups"]["0x20480"] == "kc(0) + 0x20DE8 auth(IA addr 49764)"
    # Then the following fixup should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x20488"] == "kc(0) + 0x20E00 auth(IA addr 61962)"

    # Now in bar, again match the entry for its Bar::foo() symbol
    assert kernel_cache.dictionary()["fixups"]["0x200E8"] == "kc(0) + 0x2036C auth(IA addr 49764)"
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x200F0"] == "kc(0) + 0x20E00 auth(IA addr 61962)"

    # Now in baz, again match the entry for its Baz::foo() symbol
    assert kernel_cache.dictionary()["fixups"]["0x202B0"] == "kc(0) + 0x2085C auth(IA addr 49764)"
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    assert kernel_cache.dictionary()["fixups"]["0x202B8"] == "kc(0) + 0x20E00 auth(IA addr 61962)"


# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r-- -Wl,-fixup_chains -Wl,-kernel
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo.cpp -o extensions/foo.kext/foo -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -DFOO_USED=1
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const baz.cpp -o extensions/baz.kext/baz -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers
# [~]> rm -r extensions/*.kext/*.ld

