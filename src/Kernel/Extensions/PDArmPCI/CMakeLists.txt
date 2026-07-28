add_kext_bundle(PDArmPCI INFO_PLIST Info.plist BUNDLE_IDENTIFIER org.puredarwin.driver.PDArmPCI BUNDLE_VERSION 1.0)
target_sources(PDArmPCI PRIVATE PDArmPCI.cpp)
target_link_libraries(PDArmPCI PRIVATE IOPCIFamilyHeaders)
install(TARGETS PDArmPCI DESTINATION System/Library/Extensions COMPONENT BaseSystem)
