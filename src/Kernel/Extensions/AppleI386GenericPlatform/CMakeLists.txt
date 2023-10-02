add_kext_bundle(AppleI386GenericPlatform INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.AppleI386GenericPlatform BUNDLE_VERSION 1.0.0d1)

target_sources(AppleI386GenericPlatform PRIVATE
    AppleI386CPU.cpp
    AppleI386PlatformExpert.cpp
)

install(TARGETS AppleI386GenericPlatform DESTINATION System/Library/Extensions COMPONENT BaseSystem)
