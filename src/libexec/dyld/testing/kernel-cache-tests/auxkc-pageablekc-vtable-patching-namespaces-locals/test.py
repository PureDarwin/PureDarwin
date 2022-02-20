#!/usr/bin/python2.7

import os
import KernelCollection

# This is the same as auxkc-pageablekc-vtable-patching-namespaces
# but this test has all local symbols instead of global symbols


# The kernel has class OSObject and subclass KernelClass
# foo.kext sublclasses KernelClass to get Foo1, and subclasses that to get Foo2
# bar.kext sublclasses Foo1 to get Bar1, and subclasses that to get Bar2

# In KernelClass the vtable layout is:
# [ ..., foo() kernelClassUsed0() ]

# In Foo1, the layout is:
# [ ..., foo() kernelClass_RESERVED0(), foo1Used0(), foo1Used1() ]

# In Foo2, the layout is:
# [ ..., foo() kernelClass_RESERVED0(), foo1Used0(), foo1_RESERVED1(), foo1_RESERVED2(), foo1_RESERVED3() ]

# In Bar1, the layout is:
# [ ..., foo() kernelClass_RESERVED0(), foo1Used0(), foo1_RESERVED1(), foo1_RESERVED2(), foo1_RESERVED3() ]

# In Bar2, the layout is:
# [ ..., foo() kernelClass_RESERVED0(), foo1Used0(), foo1_RESERVED1(), foo1_RESERVED2(), foo1_RESERVED3() ]

# All kext's will end up getting the vtable entry after foo() patched to kernelClassUsed0()
# Foo2, Bar1, Bar2, will also get the vtable entry after foo1Used0() patched to foo1Used1()

def findGlobalSymbolVMAddr(kernel_cache, dylib_index, symbol_name):
    for symbol_and_addr in kernel_cache.dictionary()["dylibs"][dylib_index]["global-symbols"]:
        if symbol_and_addr["name"] == symbol_name:
            return symbol_and_addr["vmAddr"]
    return None

def findLocalSymbolVMAddr(kernel_cache, dylib_index, symbol_name):
    for symbol_and_addr in kernel_cache.dictionary()["dylibs"][dylib_index]["local-symbols"]:
        if symbol_and_addr["name"] == symbol_name:
            return symbol_and_addr["vmAddr"]
    return None

def findFixupVMAddr(kernel_cache, fixup_name):
    for fixup_vmaddr, fixup_target in kernel_cache.dictionary()["fixups"].iteritems():
        if fixup_target == fixup_name:
            return fixup_vmaddr
    return None

def findPagableFixupVMAddr(kernel_cache, dylib_index, fixup_name):
    for fixup_vmaddr, fixup_target in kernel_cache.dictionary()["dylibs"][dylib_index]["fixups"].iteritems():
        if fixup_target == fixup_name:
            return fixup_vmaddr
    return None

def findAuxFixupVMAddr(kernel_cache, dylib_index, fixup_name):
    for fixup_vmaddr, fixup_target in kernel_cache.dictionary()["dylibs"][dylib_index]["fixups"].iteritems():
        if fixup_target == fixup_name:
            return fixup_vmaddr
    return None

def offsetVMAddr(vmAddr, offset):
    het_int = int(vmAddr, 16)
    het_int = het_int + offset
    return ''.join([ '0x', hex(het_int).upper()[2:] ])

def check(kernel_cache):
    enableLogging = False
    kernel_cache.buildKernelCollection("x86_64", "/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kernel", "", [], [])
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 1
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.kernel"

    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", ["-symbols", "-arch", "x86_64"])
    
    # From kernel, we want to know where the vtable is, and the foo() and kernelClassUsed0() slots in that vtable
    # KernelClass::foo()
    kernelClassFooVMAddr = findGlobalSymbolVMAddr(kernel_cache, 0, "__ZN1X11KernelClass3fooEv")
    if enableLogging:
        print "kernelClassFooVMAddr: " + kernelClassFooVMAddr

    # KernelClass::kernelClassUsed0()
    kernelClassUsed0VMAddr = findGlobalSymbolVMAddr(kernel_cache, 0, "__ZN1X11KernelClass16kernelClassUsed0Ev")
    if enableLogging:
        print "kernelClassUsed0VMAddr: " + kernelClassUsed0VMAddr


    # Check the fixups
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", ["-fixups", "-arch", "x86_64"])
    
    # In vtable for Foo, we match the entry for Foo::foo() by looking for its value on the RHS of the fixup
    kernelFooFixupAddr = findFixupVMAddr(kernel_cache, "kc(0) + " + kernelClassFooVMAddr + " : pointer64")
    if enableLogging:
        print "kernelFooFixupAddr: " + kernelFooFixupAddr
    # Then the following fixup should be to KernelClass::kernelClassUsed0()
    kernelFooNextFixupAddr = offsetVMAddr(kernelFooFixupAddr, 8)
    if enableLogging:
        print "kernelFooNextFixupAddr: " + kernelFooNextFixupAddr
    assert kernel_cache.dictionary()["fixups"][kernelFooNextFixupAddr] == "kc(0) + " + kernelClassUsed0VMAddr + " : pointer64"

    # From this point on, the vmAddr for __ZN1X11KernelClass16kernelClassUsed0Ev is an offset in to kc(0)
    # so we want to turn it from a vmAddr to vmOffset by subtracting the base address of 0x4000 which is on __HIB
    kernelClassUsed0VMOffset = offsetVMAddr(kernelClassUsed0VMAddr, -0x4000)
    if enableLogging:
        print "kernelClassUsed0VMOffset: " + kernelClassUsed0VMOffset

    # -----------------------------------------------------------
    # Now build an pageable cache using the baseline kernel collection
    kernel_cache.buildPageableKernelCollection("x86_64", "/auxkc-pageablekc-vtable-patching-namespaces-locals/pageable.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/extensions", ["com.apple.foo1", "com.apple.foo2"], [])
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/pageable.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.foo1"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.foo2"


    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/pageable.kc", ["-symbols", "-arch", "x86_64"])
    
    # From foo1, find the vtable and its override of foo()
    # Foo1::foo()
    pageableFoo1FooVMAddr = findLocalSymbolVMAddr(kernel_cache, 0, "__ZN1X4Foo13fooEv")
    if enableLogging:
        print "pageableFoo1FooVMAddr: " + pageableFoo1FooVMAddr

    pageableFoo1FooUsed0VMAddr = findLocalSymbolVMAddr(kernel_cache, 0, "__ZN1X4Foo19foo1Used0Ev")
    if enableLogging:
        print "pageableFoo1FooUsed0VMAddr: " + pageableFoo1FooUsed0VMAddr

    pageableFoo1FooUsed1VMAddr = findLocalSymbolVMAddr(kernel_cache, 0, "__ZN1X4Foo19foo1Used1Ev")
    if enableLogging:
        print "pageableFoo1FooUsed1VMAddr: " + pageableFoo1FooUsed1VMAddr

    # From foo2, find the vtable and its override of foo()
    # Foo2::foo()
    pageableFoo2FooVMAddr = findLocalSymbolVMAddr(kernel_cache, 1, "__ZN1X4Foo23fooEv")
    if enableLogging:
        print "pageableFoo2FooVMAddr: " + pageableFoo2FooVMAddr
    # Also find Foo2::foo1Used0() as it overrides foo1Used0 from the superclass
    pageableFoo2FooUsed0VMAddr = findLocalSymbolVMAddr(kernel_cache, 1, "__ZN1X4Foo29foo1Used0Ev")
    if enableLogging:
        print "pageableFoo2FooUsed0VMAddr: " + pageableFoo2FooUsed0VMAddr


    # Check the fixups
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/pageable.kc", ["-fixups", "-arch", "x86_64"])
    kernel_cache.dictionary()["fixups"] == "none"

    # --- foo1.kext ---
    # The vtable we have is [ ..., foo(), kernelClass_RESERVED0(), foo1Used0(), foo1Used1() ]
    # and we want           [ ..., foo(), kernelClassUsed0(),      foo1Used0(), foo1Used1() ]

    # In vtable for Foo1, we match the entry for Foo1::foo() by looking for its value on the RHS of the fixup
    pageableFoo1FooFixupAddr = findPagableFixupVMAddr(kernel_cache, 0, "kc(1) + " + pageableFoo1FooVMAddr)
    if enableLogging:
        print "pageableFoo1FooFixupAddr: " + pageableFoo1FooFixupAddr

    # Then the following fixup should be to KernelClass::kernelClassUsed0()
    pageableFoo1FooNextFixupAddr = offsetVMAddr(pageableFoo1FooFixupAddr, 8)
    if enableLogging:
        print "pageableFoo1FooNextFixupAddr: " + pageableFoo1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][pageableFoo1FooNextFixupAddr] == "kc(0) + " + kernelClassUsed0VMOffset

    # Then we should have foo1Used0()
    pageableFoo1FooNextFixupAddr = offsetVMAddr(pageableFoo1FooFixupAddr, 16)
    if enableLogging:
        print "pageableFoo1FooNextFixupAddr: " + pageableFoo1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][pageableFoo1FooNextFixupAddr] == "kc(1) + " + pageableFoo1FooUsed0VMAddr

    # And then foo1Used1()
    pageableFoo1FooNextFixupAddr = offsetVMAddr(pageableFoo1FooFixupAddr, 24)
    if enableLogging:
        print "pageableFoo1FooNextFixupAddr: " + pageableFoo1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][pageableFoo1FooNextFixupAddr] == "kc(1) + " + pageableFoo1FooUsed1VMAddr

    # --- foo2.kext ---
    # The vtable we have is [ ..., foo(), kernelClass_RESERVED0(), foo1Used0(), foo1_RESERVED1() ]
    # and we want           [ ..., foo(), kernelClassUsed0(),      foo1Used0(), foo1Used1() ]

    # In vtable for Foo2, we match the entry for Foo2::foo() by looking for its value on the RHS of the fixup
    pageableFoo2FooFixupAddr = findPagableFixupVMAddr(kernel_cache, 1, "kc(1) + " + pageableFoo2FooVMAddr)
    if enableLogging:
        print "pageableFoo2FooFixupAddr: " + pageableFoo2FooFixupAddr

    # Then the following fixup should be to KernelClass::kernelClassUsed0()
    pageableFoo2FooNextFixupAddr = offsetVMAddr(pageableFoo2FooFixupAddr, 8)
    if enableLogging:
        print "pageableFoo2FooNextFixupAddr: " + pageableFoo2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][pageableFoo2FooNextFixupAddr] == "kc(0) + " + kernelClassUsed0VMOffset

    # Then we should have foo1Used0(), but Foo2 overrides that, so it should be the Foo2 implementation, not the Foo1 implementation
    pageableFoo2FooNextFixupAddr = offsetVMAddr(pageableFoo2FooFixupAddr, 16)
    if enableLogging:
        print "pageableFoo2FooNextFixupAddr: " + pageableFoo2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][pageableFoo2FooNextFixupAddr] == "kc(1) + " + pageableFoo2FooUsed0VMAddr

    # And then foo1Used1()
    pageableFoo2FooNextFixupAddr = offsetVMAddr(pageableFoo2FooFixupAddr, 24)
    if enableLogging:
        print "pageableFoo2FooNextFixupAddr: " + pageableFoo2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][pageableFoo2FooNextFixupAddr] == "kc(1) + " + pageableFoo1FooUsed1VMAddr


    # -----------------------------------------------------------
    # Now build an aux cache using the baseline kernel collection
    kernel_cache.buildAuxKernelCollection("x86_64", "/auxkc-pageablekc-vtable-patching-namespaces-locals/aux.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/main.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/pageable.kc", "/auxkc-pageablekc-vtable-patching-namespaces-locals/extensions", ["com.apple.bar1", "com.apple.bar2"], [])
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/aux.kc", ["-layout", "-arch", "x86_64"])

    assert len(kernel_cache.dictionary()["dylibs"]) == 2
    assert kernel_cache.dictionary()["dylibs"][0]["name"] == "com.apple.bar1"
    assert kernel_cache.dictionary()["dylibs"][1]["name"] == "com.apple.bar2"


    # Get the addresses for the symbols we are looking at.  This will make it easier to work out the fixup slots
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/aux.kc", ["-symbols", "-arch", "x86_64"])

    # From bar1, find the vtable and its override of foo()
    # Bar1::foo()
    auxBar1FooVMAddr = findLocalSymbolVMAddr(kernel_cache, 0, "__ZN1X4Bar13fooEv")
    if enableLogging:
        print "auxBar1FooVMAddr: " + auxBar1FooVMAddr

    # From bar2, find the vtable and its override of foo()
    # Bar1::foo()
    auxBar2FooVMAddr = findLocalSymbolVMAddr(kernel_cache, 1, "__ZN4Bar23fooEv")
    if enableLogging:
        print "auxBar2FooVMAddr: " + auxBar2FooVMAddr


    # Check the fixups
    kernel_cache.analyze("/auxkc-pageablekc-vtable-patching-namespaces-locals/aux.kc", ["-fixups", "-arch", "x86_64"])

    # --- foo1.kext ---
    # The vtable we have is [ ..., foo(), kernelClass_RESERVED0(), foo1Used0(), foo1Used1() ]
    # and we want           [ ..., foo(), kernelClassUsed0(),      foo1Used0(), foo1Used1() ]

    # In vtable for Bar1, we match the entry for Bar1::foo() by looking for its value on the RHS of the fixup
    auxBar1FooFixupAddr = findAuxFixupVMAddr(kernel_cache, 0, "kc(3) + " + auxBar1FooVMAddr)
    if enableLogging:
        print "auxBar1FooFixupAddr: " + auxBar1FooFixupAddr

    # Then the following fixup should be to KernelClass::kernelClassUsed0()
    auxBar1FooNextFixupAddr = offsetVMAddr(auxBar1FooFixupAddr, 8)
    if enableLogging:
        print "auxBar1FooNextFixupAddr: " + auxBar1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][auxBar1FooNextFixupAddr] == "kc(0) + " + kernelClassUsed0VMOffset

    # Then we should have foo1Used0() from Foo2 as it overrides it from Foo1
    auxBar1FooNextFixupAddr = offsetVMAddr(auxBar1FooFixupAddr, 16)
    if enableLogging:
        print "auxBar1FooNextFixupAddr: " + auxBar1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][auxBar1FooNextFixupAddr] == "kc(1) + " + pageableFoo2FooUsed0VMAddr

    # And then foo1Used1()
    auxBar1FooNextFixupAddr = offsetVMAddr(auxBar1FooFixupAddr, 24)
    if enableLogging:
        print "auxBar1FooNextFixupAddr: " + auxBar1FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][0]["fixups"][auxBar1FooNextFixupAddr] == "kc(1) + " + pageableFoo1FooUsed1VMAddr

    # --- bar2.kext ---
    # The vtable we have is [ ..., foo(), kernelClass_RESERVED0(), foo1Used0(), foo1_RESERVED1() ]
    # and we want           [ ..., foo(), kernelClassUsed0(),      foo1Used0(), foo1Used1() ]

    # In vtable for Foo2, we match the entry for Foo2::foo() by looking for its value on the RHS of the fixup
    auxBar2FooFixupAddr = findAuxFixupVMAddr(kernel_cache, 1, "kc(3) + " + auxBar2FooVMAddr)
    if enableLogging:
        print "auxBar2FooFixupAddr: " + auxBar2FooFixupAddr

    # Then the following fixup should be to KernelClass::kernelClassUsed0()
    auxBar2FooNextFixupAddr = offsetVMAddr(auxBar2FooFixupAddr, 8)
    if enableLogging:
        print "auxBar2FooNextFixupAddr: " + auxBar2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][auxBar2FooNextFixupAddr] == "kc(0) + " + kernelClassUsed0VMOffset

    # Then we should have foo1Used0() from Foo2 as it overrides it from Foo1
    auxBar2FooNextFixupAddr = offsetVMAddr(auxBar2FooFixupAddr, 16)
    if enableLogging:
        print "auxBar2FooNextFixupAddr: " + auxBar2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][auxBar2FooNextFixupAddr] == "kc(1) + " + pageableFoo2FooUsed0VMAddr

    # And then foo1Used1()
    auxBar2FooNextFixupAddr = offsetVMAddr(auxBar2FooFixupAddr, 24)
    if enableLogging:
        print "auxBar2FooNextFixupAddr: " + auxBar2FooNextFixupAddr
    assert kernel_cache.dictionary()["dylibs"][1]["fixups"][auxBar2FooNextFixupAddr] == "kc(1) + " + pageableFoo1FooUsed1VMAddr

# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-static -mkernel -nostdlib -Wl,-e,__start -Wl,-pie main.cpp kernel.cpp -Wl,-pagezero_size,0x0 -o main.kernel -Wl,-image_base,0x10000 -Wl,-segaddr,__HIB,0x4000 -Wl,-add_split_seg_info -Wl,-install_name,/usr/lib/swift/split.seg.v2.hack -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -Wl,-sectcreate,__LINKINFO,__symbolsets,SymbolSets.plist -Wl,-segprot,__LINKINFO,r--,r-- -std=c++11 -DKERNEL_USED=1 -Wl,-exported_symbol,__ZN1X11KernelClass10gMetaClassE -Wl,-exported_symbol,__ZN8OSObject10gMetaClassE -Wl,-exported_symbol,__ZNK11OSMetaClass19instanceConstructedEv
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo1.cpp -o extensions/foo1.kext/foo1 -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -std=c++11 -DFOO1_USED0=1  -DFOO1_USED1=1 -Wl,-exported_symbol,__ZN1X4Foo110gMetaClassE -Wl,-exported_symbol,__ZN1X4Foo1C2EPK11OSMetaClass -Wl,-exported_symbol,__ZTVN1X4Foo1E -Wl,-exported_symbol,__ZN1X4Foo1D2Ev
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-no_data_const foo2.cpp -o extensions/foo2.kext/foo2 -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -std=c++11 -DFOO1_USED0=1 -Wl,-exported_symbol,__ZN1X4Foo210gMetaClassE -Wl,-exported_symbol,__ZN1X4Foo2C2EPK11OSMetaClass -Wl,-exported_symbol,__ZTVN1X4Foo2E -Wl,-exported_symbol,__ZN1X4Foo2D2Ev
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-data_const bar1.cpp -o extensions/bar1.kext/bar1 -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -std=c++11 -DFOO1_USED0=1 -Wl,-exported_symbol,__ZN1X4Bar110gMetaClassE -Wl,-exported_symbol,__ZN1X4Bar1C2EPK11OSMetaClass -Wl,-exported_symbol,__ZTVN1X4Bar1E -Wl,-exported_symbol,__ZN1X4Bar1D2Ev
# [~]> xcrun -sdk macosx.internal cc -arch x86_64 -Wl,-kext -mkernel -nostdlib -Wl,-add_split_seg_info -Wl,-data_const bar2.cpp -o extensions/bar2.kext/bar2 -iwithsysroot /System/Library/Frameworks/Kernel.framework/Headers -std=c++11 -DFOO1_USED0=1

