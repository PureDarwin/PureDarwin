add_kext_bundle(PDRiscvPlatformExpert INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDRiscvPlatformExpert BUNDLE_VERSION 1.0.0)

target_sources(PDRiscvPlatformExpert PRIVATE
    PDRiscvPlatformExpert.cpp
    PDRiscvCPU.cpp
    PDRiscvPLIC.cpp
    PDRiscvDT.cpp
)

install(TARGETS PDRiscvPlatformExpert DESTINATION System/Library/Extensions COMPONENT BaseSystem)
