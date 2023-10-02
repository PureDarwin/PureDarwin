add_kext_bundle(AppleAPIC INFO_PLIST Info.plist BUNDLE_IDENTIFIER com.apple.driver.AppleAPIC BUNDLE_VERSION 1.4)
target_sources(AppleAPIC PRIVATE
    Apple8259PIC.cpp
    AppleAPIC.cpp
)
target_compile_options(AppleAPIC PRIVATE -Wno-implicit-int-conversion -Wno-sign-conversion)

install(TARGETS AppleAPIC DESTINATION System/Library/Extensions COMPONENT BaseSystem)
