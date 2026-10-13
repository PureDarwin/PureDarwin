add_kext_bundle(PDDesignWareI2C.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDDesignWareI2C BUNDLE_VERSION 1.0.0)
set_property(TARGET PDDesignWareI2C.kext PROPERTY OUTPUT_NAME PDDesignWareI2C)
target_sources(PDDesignWareI2C.kext PRIVATE PDDesignWareI2C.cpp)
target_link_libraries(PDDesignWareI2C.kext PRIVATE PDI2CFamilyHeaders xnu_kernel_private_headers)
set_property(TARGET PDDesignWareI2C.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDDesignWareI2C.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
