add_kext_bundle(ApplePIODMA INFO_PLIST Info.plist BUNDLE_IDENTIFIER com.apple.driver.ApplePIODMA BUNDLE_VERSION 1.0.0d1)
target_sources(ApplePIODMA PRIVATE
    ApplePIODMA.cpp
    ApplePIODMADebug.cpp
    ApplePIODMARequest.cpp
    ApplePIODMARequestPool.cpp
)

set_property(TARGET ApplePIODMA PROPERTY CXX_STANDARD 11)
target_link_libraries(ApplePIODMA PRIVATE IOPCIFamilyHeaders)
install(TARGETS ApplePIODMA DESTINATION System/Library/Extensions COMPONENT BaseSystem)
