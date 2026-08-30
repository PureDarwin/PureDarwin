if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
    add_compile_options(-include "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
endif()

# The legacy extension sources use the MacOSX branch of the XNU KPI headers.
add_compile_definitions(XNU_PLATFORM_MacOSX=1)

if(NOT PUREDARWIN_ARM64 AND NOT PUREDARWIN_ARM32)
    add_subdirectory(IOACPIFamily)
    add_subdirectory(PDACPIPlatform)
    add_subdirectory(AppleAPIC)
    add_subdirectory(AppleI386GenericPlatform)
    add_subdirectory(AppleI386PCI)
    add_subdirectory(AppleIntelPIIXATA)
    add_subdirectory(ApplePS2Controller)
    add_subdirectory(IOATAFamily)
    add_subdirectory(IOATABlockStorage)
else()
    add_subdirectory(PDArmPlatformExpert)
    if(PUREDARWIN_ARM64)
        add_subdirectory(PDArmPCI)
    endif()
endif()

add_subdirectory(IOPCIFamily)
add_subdirectory(IOStorageFamily)
add_subdirectory(IOCDStorageFamily)
add_subdirectory(IODVDStorageFamily)
add_subdirectory(IOBDStorageFamily)
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
add_subdirectory(IOIntelFramebuffer)
add_subdirectory(IOVirtIONet)
add_subdirectory(IOVirtIOBlock)
add_subdirectory(IOHIDFamily)
add_subdirectory(IONetworkingFamily)
add_subdirectory(PDE1000)
add_subdirectory(PDRealtek8111)
add_subdirectory(IOAudioFamily)
add_subdirectory(RavynHDAudio)
add_subdirectory(corecrypto)
add_subdirectory(ext4)
add_subdirectory(msdosfs)
add_subdirectory(apfs)
add_subdirectory(HFSEncodings)
add_subdirectory(hfs)
add_subdirectory(AppleFileSystemDriver)
add_subdirectory(Ext4FileSystemDriver)
add_subdirectory(PDBcm2835SD)

add_custom_target(kexts)
add_dependencies(kexts
    IOPCIFamily ApplePIODMA IOStorageFamily IOCDStorageFamily.kext
    IODVDStorageFamily.kext IOBDStorageFamily.kext IONVMEFamily.kext
    IOUSBFamily IOUSBCompositeDriver.kext AppleUSBMergeNub.kext
    IOUSBHIDDriver.kext
    AppleUSBEHCI.kext AppleUSBOHCI.kext AppleUSBUHCI.kext
    RavynAHCIPort.kext RavynXHCIPort.kext
    IOGOPFramebuffer.kext IOVirtIOFamily.kext IOVirtIOGPU.kext IOVirtIONet.kext
    IOVirtIOBlock.kext
    IOIntelFramebuffer.kext
    IOHIDFamily.kext IONetworkingFamily.kext PDE1000.kext PDRealtek8111.kext
    IOAudioFamily.kext RavynHDAudio.kext
    corecrypto.kext ext4.kext msdosfs.kext apfs.kext HFSEncodings.kext hfs.kext
    AppleFileSystemDriver.kext Ext4FileSystemDriver.kext
)
if(PUREDARWIN_ARM64 OR PUREDARWIN_ARM32)
    add_dependencies(kexts PDArmPlatformExpert)
endif()
if(PUREDARWIN_ARM32)
    add_dependencies(kexts PDBcm2835SD.kext)
endif()
if(PUREDARWIN_ENABLE_IOGRAPHICS_FAMILY)
    add_dependencies(kexts IOGraphicsFamily.kext)
endif()
if(NOT PUREDARWIN_ARM64 AND NOT PUREDARWIN_ARM32)
    add_dependencies(kexts
        AppleAPIC AppleI386GenericPlatform AppleI386PCI AppleIntelPIIXATA
        ApplePS2Controller IOACPIFamily IOATAFamily IOATABlockStorage.kext
        PDACPIPlatform
    )
endif()

add_subdirectory(pthread)
add_dependencies(kexts pthread.kext)

# Permissive AppleMobileFileIntegrity stand-in: PureDarwin does not enforce
# Apple code signing, so this registers the amfi_t/TrustCache vtables that
# xnu 12377 requires a real AMFI kext to provide, with allow-all semantics.
add_subdirectory(amfi)
add_dependencies(kexts amfi.kext)
