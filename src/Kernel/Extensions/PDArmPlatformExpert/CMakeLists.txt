add_kext_bundle(PDArmPlatformExpert INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDArmPlatformExpert BUNDLE_VERSION 1.0.0)

target_sources(PDArmPlatformExpert PRIVATE
    PDArmPlatformExpert.cpp
    PDArmCPU.cpp
    PDArmGIC.cpp
)

install(TARGETS PDArmPlatformExpert DESTINATION System/Library/Extensions COMPONENT BaseSystem)
