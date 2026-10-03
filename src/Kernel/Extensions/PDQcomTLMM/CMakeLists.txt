add_kext_bundle(PDQcomTLMM.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDQcomTLMM BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQcomTLMM.kext PROPERTY OUTPUT_NAME PDQcomTLMM)
target_sources(PDQcomTLMM.kext PRIVATE PDQcomTLMM.cpp)
target_link_libraries(PDQcomTLMM.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDQcomTLMM.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDQcomTLMM.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
