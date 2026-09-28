add_kext_bundle(PDEcamPCI INFO_PLIST Info.plist BUNDLE_IDENTIFIER org.puredarwin.driver.PDEcamPCI BUNDLE_VERSION 1.0)
target_sources(PDEcamPCI PRIVATE PDEcamPCI.cpp)
target_link_libraries(PDEcamPCI PRIVATE IOPCIFamilyHeaders)
install(TARGETS PDEcamPCI DESTINATION System/Library/Extensions COMPONENT BaseSystem)
