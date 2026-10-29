if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
    add_compile_options(-include "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
endif()

if(PD_CORECRYPTO_DIR)
    include(${PD_CORECRYPTO_DIR}/pd/corecrypto-pd.cmake)
endif()

# The legacy extension sources use the MacOSX branch of the XNU KPI headers.
add_compile_definitions(XNU_PLATFORM_MacOSX=1)

# riscv64 kexts share the kernel's soft-float lp64 abi and medany code model,
# float arguments would otherwise land in registers the kernel never reads
if(PUREDARWIN_RISCV64)
    add_compile_options(-march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany)
endif()

if(PUREDARWIN_ARCH STREQUAL "x86_64")
    add_subdirectory(IOACPIFamily)
    add_subdirectory(PDACPIPlatform)
    add_subdirectory(AppleAPIC)
    add_subdirectory(AppleI386GenericPlatform)
    add_subdirectory(AppleI386PCI)
    add_subdirectory(AppleIntelPIIXATA)
    add_subdirectory(ApplePS2Controller)
    add_subdirectory(IOATAFamily)
    add_subdirectory(IOATABlockStorage)
elseif(PUREDARWIN_RISCV64)
    add_subdirectory(PDRiscvPlatformExpert)
    add_subdirectory(PDEcamPCI)
else()
    add_subdirectory(PDArmPlatformExpert)
    if(PUREDARWIN_ARM64)
        add_subdirectory(PDArmPCI)
        add_subdirectory(PDEcamPCI)
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
add_subdirectory(PDVirtualFramebuffer)
add_subdirectory(IOVirtIOFamily)
add_subdirectory(IOVirtIOGPU)
add_subdirectory(PDRadeonFramebuffer)
add_subdirectory(PDAmdGPU)
add_subdirectory(IOVirtIONet)
add_subdirectory(IOVirtIOBlock)
if(PUREDARWIN_ARCH STREQUAL "x86_64")
    add_subdirectory(PDHyperV)
endif()
add_subdirectory(IOHIDFamily)
add_subdirectory(PDIOSurface)
add_subdirectory(IONetworkingFamily)
add_subdirectory(PDE1000)
# FreeBSD network drivers on the fbsdkpi layer for arm64 boards
if(PUREDARWIN_ARM64)
    add_subdirectory(PDFBSDem)
    add_subdirectory(PDFBSDigc)
    add_subdirectory(PDFBSDrge)
endif()
# FreeBSD drivers on the fbsdkpi layer, x86_64 only for now
if(PUREDARWIN_ARCH STREQUAL "x86_64")
    add_subdirectory(PDFBSDem)
    add_subdirectory(PDFBSDigc)
    add_subdirectory(PDFBSDix)
    add_subdirectory(PDFBSDrge)
    add_subdirectory(PDFBSDrtw89)
    add_subdirectory(PDFBSDrtwn)
    add_subdirectory(PDFBSDrun)
    add_subdirectory(PDDRMDri)
    # amdgpu through drm-kmod; the drm-kmod tree comes from nix, so it is optional
    if(PD_DRMKMOD_DIR)
        add_subdirectory(PDDRMamdgpu)
        # only a drm-kmod tree that kept i915 can build it
        if(EXISTS "${PD_DRMKMOD_DIR}/drivers/gpu/drm/i915")
            add_subdirectory(PDDRMi915)
        endif()
        # nouveau comes from linux into its own copy of the drm-kmod tree
        if(EXISTS "${PD_DRMKMOD_DIR}/drivers/gpu/drm/nouveau")
            add_subdirectory(PDDRMnouveau)
        endif()
        # radeon for terascale and gcn 1 and 2, from the drm-kmod tree that kept it
        if(EXISTS "${PD_DRMKMOD_DIR}/drivers/gpu/drm/radeon")
            add_subdirectory(PDDRMradeon)
        endif()
    endif()
elseif(PUREDARWIN_ARM64)
    # arm64 bring-up of the layer: platform-device drm drivers from the device tree
    add_subdirectory(PDDRMDri)
    if(PD_DRMKMOD_DIR)
        add_subdirectory(PDDRMmsm)
    endif()
endif()
add_subdirectory(PDRealtek8111)
add_subdirectory(IO80211Family)
add_subdirectory(PDRalinkUSB)
add_subdirectory(PDQwx)
add_subdirectory(IOAudioFamily)
add_subdirectory(RavynHDAudio)
add_subdirectory(corecrypto)
add_subdirectory(ext4)
add_subdirectory(cd9660)
add_subdirectory(PDAuxHello)
add_subdirectory(msdosfs)
add_subdirectory(apfs)
add_subdirectory(HFSEncodings)
add_subdirectory(hfs)
add_subdirectory(AppleFileSystemDriver)
add_subdirectory(Ext4FileSystemDriver)
add_subdirectory(ApfsFileSystemDriver)
add_subdirectory(PDBcm2835SD)
# HID over I2C: ACPI laptops on x86, device tree boards on arm64
add_subdirectory(PDI2CFamily)
add_subdirectory(PDI2CHID)
add_subdirectory(PDDesignWareI2C)
add_subdirectory(PDUSBNCM)
if(PUREDARWIN_ARM64)
    add_subdirectory(PDSun50iMMC)
    add_subdirectory(PDPowerVR)
    add_subdirectory(PDSun60iDisplay)
    add_subdirectory(PDSg2002SD)
    add_subdirectory(PDSg2002Eth)
    add_subdirectory(PDQcomTLMM)
    add_subdirectory(PDQcomGeniI2C)
    add_subdirectory(PDGadgetFamily)
    add_subdirectory(PDQcomUSB)
    add_subdirectory(PDTegraUSB)
endif()

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
    PDRadeonFramebuffer.kext PDAmdGPU.kext
    IOHIDFamily.kext IONetworkingFamily.kext PDE1000.kext PDRealtek8111.kext
    IO80211Family.kext PDRalinkUSB.kext PDQwx.kext
    IOAudioFamily.kext RavynHDAudio.kext
    corecrypto.kext ext4.kext msdosfs.kext cd9660.kext PDAuxHello.kext apfs.kext HFSEncodings.kext hfs.kext
    AppleFileSystemDriver.kext Ext4FileSystemDriver.kext ApfsFileSystemDriver.kext
)
if(PUREDARWIN_ARCH STREQUAL "x86_64")
    add_dependencies(kexts PDFBSDem.kext PDFBSDigc.kext PDFBSDix.kext PDFBSDrge.kext PDFBSDrtw89.kext PDFBSDrtwn.kext PDFBSDrun.kext PDDRMDri.kext)
endif()
add_dependencies(kexts PDI2CFamily.kext PDI2CHID.kext PDDesignWareI2C.kext PDUSBNCM.kext)
if(PUREDARWIN_ARM64 OR PUREDARWIN_ARM32)
    add_dependencies(kexts PDArmPlatformExpert)
endif()
if(PUREDARWIN_ARM32)
    add_dependencies(kexts PDBcm2835SD.kext)
endif()
if(PUREDARWIN_ARM64)
    add_dependencies(kexts PDEcamPCI)
    add_dependencies(kexts PDSun50iMMC.kext)
    add_dependencies(kexts PDPowerVR.kext)
    add_dependencies(kexts PDSun60iDisplay.kext)
    add_dependencies(kexts PDSg2002SD.kext)
    add_dependencies(kexts PDSg2002Eth.kext)
    add_dependencies(kexts PDQcomTLMM.kext)
    add_dependencies(kexts PDQcomGeniI2C.kext)
    add_dependencies(kexts PDGadgetFamily.kext)
    add_dependencies(kexts PDQcomUSB.kext)
    add_dependencies(kexts PDTegraUSB.kext)
endif()
if(PUREDARWIN_ENABLE_IOGRAPHICS_FAMILY)
    add_dependencies(kexts IOGraphicsFamily.kext)
endif()
if(PUREDARWIN_RISCV64)
    add_dependencies(kexts PDRiscvPlatformExpert PDEcamPCI)
endif()
if(PUREDARWIN_ARCH STREQUAL "x86_64")
    add_dependencies(kexts
        AppleAPIC AppleI386GenericPlatform AppleI386PCI AppleIntelPIIXATA
        ApplePS2Controller IOACPIFamily IOATAFamily IOATABlockStorage.kext
        PDACPIPlatform PDHyperV.kext
    )
endif()

add_subdirectory(pthread)
add_dependencies(kexts pthread.kext)

# Permissive AppleMobileFileIntegrity stand-in: PureDarwin does not enforce
# Apple code signing, so this registers the amfi_t/TrustCache vtables that
# xnu 12377 requires a real AMFI kext to provide, with allow-all semantics.
add_subdirectory(amfi)
add_dependencies(kexts amfi.kext)

# MACF policy behind sandbox_init(3); loaded at boot as a security extension.
add_subdirectory(Sandbox)
add_dependencies(kexts Sandbox.kext)

# IOWatchdog service that watchdogd opens at boot
add_subdirectory(PDWatchdog)
add_dependencies(kexts PDWatchdog.kext)
add_subdirectory(PDDiskImages2)
add_dependencies(kexts PDDiskImages2.kext)
# FSKit's lifs user client fskitd opens at start
add_subdirectory(PDLifs)
add_dependencies(kexts PDLifs.kext)
add_subdirectory(PDSerialHID)
add_dependencies(kexts PDSerialHID.kext)
add_subdirectory(PDKeyStore)
add_dependencies(kexts PDKeyStore.kext)
