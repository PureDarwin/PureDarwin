add_kext_bundle(PDQcomGeniI2C.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDQcomGeniI2C BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQcomGeniI2C.kext PROPERTY OUTPUT_NAME PDQcomGeniI2C)
target_sources(PDQcomGeniI2C.kext PRIVATE PDQcomGeniI2C.cpp)
target_link_libraries(PDQcomGeniI2C.kext PRIVATE PDI2CFamilyHeaders xnu_kernel_private_headers)
set_property(TARGET PDQcomGeniI2C.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDQcomGeniI2C.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
