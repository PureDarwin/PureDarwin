#!/usr/bin/python2.7

import os
import KernelCollection

# This tests verifies that the vtable in bar.kext is patched
# But also that this can be done against a subclass in the kernel, not just

# Note this is the same as kernel-vtable-patching but with a large base address and chained fixups

def findGlobalSymbolVMAddr(kernel_cache, dylib_index, symbol_name):
    for symbol_and_addr in kernel_cache.dictionary()["dylibs"][dylib_index]["global-symbols"]:
        if symbol_and_addr["name"] == symbol_name:
            return symbol_and_addr["vmAddr"]
    return None

def findFixupVMAddr(kernel_cache, fixup_name):
    for fixup_vmaddr, fixup_target in kernel_cache.dictionary()["fixups"].iteritems():
        if fixup_target == fixup_name:
            return fixup_vmaddr
    return None

def offsetVMAddr(vmAddr, offset):
    het_int = int(vmAddr, 16)
    het_int = het_int + offset
    return ''.join([ '0x', hex(het_int).upper()[2:] ])

def check(kernel_cache):
    enableLogging = False
    kernel_cache.buildKernelCollection("arm64e", "/kernel-vtable-patching-arm64e/main.kc", "/kernel-vtable-patching-arm64e/main.kernel", "/kernel-vtable-patching-arm64e/extensions", ["com.apple.bar"], [])
    kernel_cache.analyze("/kernel-vtable-patching-arm64e/main.kc", ["-layout", "-arch", "arm64e"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/kernel-vtable-patching-arm64e/main.kc", ["-symbols", "-arch", "arm64e"])
    
    # From foo, we want to know where the vtable is, and the foo() and fooUsed0() slots in that vtable
    # Foo::foo()
    fooClassFooVMAddr = findGlobalSymbolVMAddr(kernel_cache, 0, "__ZN3Foo3fooEv")
    if enableLogging:
        print "fooClassFooVMAddr: " + fooClassFooVMAddr

    # Foo::fooUsed0()
    fooClassUsed0VMAddr = findGlobalSymbolVMAddr(kernel_cache, 0, "__ZN3Foo8fooUsed0Ev")
    if enableLogging:
        print "fooClassUsed0VMAddr: " + fooClassUsed0VMAddr
    
    # From bar, find the vtable and its override of foo()
    # Bar::foo()
    barClassFooVMAddr = findGlobalSymbolVMAddr(kernel_cache, 1, "__ZN3Bar3fooEv")
    if enableLogging:
        print "barClassFooVMAddr: " + barClassFooVMAddr


    # Check the fixups
    kernel_cache.analyze("/kernel-vtable-patching-arm64e/main.kc", ["-fixups", "-arch", "arm64e"])
    
    # foo.kext
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    fooFooFixupAddr = findFixupVMAddr(kernel_cache, "kc(0) + " + fooClassFooVMAddr + " auth(IA addr 49764)")
    if enableLogging:
        print "fooFooFixupAddr: " + fooFooFixupAddr
    # Then the following fixup should be to Foo::fooUsed0()
    fooFooNextFixupAddr = offsetVMAddr(fooFooFixupAddr, 8)
    if enableLogging:
        print "fooFooNextFixupAddr: " + fooFooNextFixupAddr
    assert kernel_cache.dictionary()["fixups"][fooFooNextFixupAddr] == "kc(0) + " + fooClassUsed0VMAddr + " auth(IA addr 61962)"

    # bar.kext
    # Now in bar, again match the entry for its Bar::foo() symbol
    barFooFixupAddr = findFixupVMAddr(kernel_cache, "kc(0) + " + barClassFooVMAddr + " auth(IA addr 49764)")
    if enableLogging:
        print "barFooFixupAddr: " + barFooFixupAddr
    # And if the patching was correct, then following entry should be to Foo::fooUsed0()
    barFooNextFixupAddr = offsetVMAddr(barFooFixupAddr, 8)
    if enableLogging:
        print "barFooNextFixupAddr: " + barFooNextFixupAddr
    assert kernel_cache.dictionary()["fixups"][barFooNextFixupAddr] == "kc(0) + " + fooClassUsed0VMAddr + " auth(IA addr 61962)"

# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-static -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-e,__start -Wl,-pie main.cpp foo.cpp -Wl,-pagezero_size,0x0 -Wl,-rename_section,__TEXT,__text,__TEXT_EXEC,__text -o main.kernel  -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -Wl,-image_base,0xfffffff000000000 -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r--  -DFOO_USED=1 -Wl,-kernel -Wl,-fixup_chains
# [~]> xcrun -sdk iphoneos.internal cc -arch arm64e -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const bar.cpp -o extensions/bar.kext/bar -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-fixup_chains
# [~]> rm -r extensions/*.kext/*.ld

