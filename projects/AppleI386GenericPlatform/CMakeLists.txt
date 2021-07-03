add_kext_bundle(AppleI386GenericPlatform INFO_PLIST Info.plist KERNEL_PRIVATE)

target_sources(AppleI386GenericPlatform PRIVATE
    AppleI386CPU.cpp
    AppleI386PlatformExpert.cpp
)

install(TARGETS AppleI386GenericPlatform DESTINATION /System/Library/Extensions COMPONENT BaseSystem)
