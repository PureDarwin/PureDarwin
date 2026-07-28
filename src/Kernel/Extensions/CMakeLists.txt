if(NOT PUREDARWIN_ARM64)
    add_subdirectory(IOACPIFamily)
    add_subdirectory(AppleAPIC)
    add_subdirectory(AppleI386GenericPlatform)
    add_subdirectory(AppleI386PCI)
    add_subdirectory(AppleIntelPIIXATA)
    add_subdirectory(ApplePS2Controller)
    add_subdirectory(IOATAFamily)
    add_subdirectory(IOATABlockStorage)
else()
    add_subdirectory(PDArmPlatformExpert)
    add_subdirectory(PDArmPCI)
endif()

add_subdirectory(IOPCIFamily)
add_subdirectory(IOStorageFamily)
add_subdirectory(IONVMEFamily)
add_subdirectory(IOUSBFamily)
add_subdirectory(RavynAHCIPort)
add_subdirectory(RavynXHCIPort)
option(PUREDARWIN_ENABLE_IOGRAPHICS_FAMILY "Build the full IOGraphicsFamily kext" ON)
if(PUREDARWIN_ENABLE_IOGRAPHICS_FAMILY)
    add_subdirectory(IOGraphicsFamily)
endif()
add_subdirectory(IOGOPFramebuffer)
add_subdirectory(IOVirtIOFamily)
add_subdirectory(IOVirtIOGPU)
add_subdirectory(IOIntelGen9Framebuffer)
add_subdirectory(IOVirtIONet)
add_subdirectory(IOHIDFamily)
add_subdirectory(IONetworkingFamily)
add_subdirectory(PDE1000)
add_subdirectory(RavynHDAudio)
add_subdirectory(corecrypto)
add_subdirectory(ext4)
add_subdirectory(msdosfs)
add_subdirectory(apfs)
add_subdirectory(HFSEncodings)
add_subdirectory(hfs)
add_subdirectory(AppleFileSystemDriver)
add_subdirectory(Ext4FileSystemDriver)
add_subdirectory(pthread)

add_custom_target(kexts)
add_dependencies(kexts
    IOPCIFamily ApplePIODMA IOStorageFamily IONVMEFamily.kext
    IOUSBFamily IOUSBCompositeDriver.kext AppleUSBMergeNub.kext
    IOUSBHIDDriver.kext
    AppleUSBEHCI.kext AppleUSBOHCI.kext AppleUSBUHCI.kext
    RavynAHCIPort.kext RavynXHCIPort.kext
    IOGOPFramebuffer.kext IOVirtIOFamily.kext IOVirtIOGPU.kext IOVirtIONet.kext
    IOIntelGen9Framebuffer.kext
    IOHIDFamily.kext IONetworkingFamily.kext PDE1000.kext
    RavynHDAudio.kext
    corecrypto.kext ext4.kext msdosfs.kext apfs.kext HFSEncodings.kext hfs.kext
    AppleFileSystemDriver.kext Ext4FileSystemDriver.kext pthread.kext
)
if(PUREDARWIN_ARM64)
    add_dependencies(kexts PDArmPlatformExpert)
endif()
if(PUREDARWIN_ENABLE_IOGRAPHICS_FAMILY)
    add_dependencies(kexts IOGraphicsFamily.kext)
endif()
if(NOT PUREDARWIN_ARM64)
    add_dependencies(kexts
        AppleAPIC AppleI386GenericPlatform AppleI386PCI AppleIntelPIIXATA
        ApplePS2Controller IOACPIFamily IOATAFamily IOATABlockStorage.kext
    )
endif()
