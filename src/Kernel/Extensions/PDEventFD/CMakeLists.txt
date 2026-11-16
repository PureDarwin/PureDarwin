# /dev/eventfd for elftrans guests, a real kernel fd so it crosses SCM_RIGHTS
add_kext_bundle(PDEventFD.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDEventFD BUNDLE_VERSION 1.0.0)
set_property(TARGET PDEventFD.kext PROPERTY OUTPUT_NAME PDEventFD)

target_sources(PDEventFD.kext PRIVATE PDEventFD.c PDEventFDService.cpp)
set_property(TARGET PDEventFD.kext PROPERTY CXX_STANDARD 14)
target_link_libraries(PDEventFD.kext PRIVATE xnu_kernel_private_headers)

install(TARGETS PDEventFD.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
