# Stand-in for the AppleKeyStore service keychain daemons open
add_kext_bundle(PDKeyStore.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDKeyStore BUNDLE_VERSION 1.0.0)
set_property(TARGET PDKeyStore.kext PROPERTY OUTPUT_NAME PDKeyStore)

target_sources(PDKeyStore.kext PRIVATE
    PDKeyStore.cpp
)

target_link_libraries(PDKeyStore.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDKeyStore.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDKeyStore.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
