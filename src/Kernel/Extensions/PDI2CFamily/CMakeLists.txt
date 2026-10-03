add_kext_bundle(PDI2CFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.iokit.PDI2CFamily BUNDLE_VERSION 1.0.0)
set_property(TARGET PDI2CFamily.kext PROPERTY OUTPUT_NAME PDI2CFamily)

target_sources(PDI2CFamily.kext PRIVATE
    PDI2CController.cpp
)

target_link_libraries(PDI2CFamily.kext PRIVATE
    PDI2CFamilyHeaders
    xnu_kernel_private_headers
)
set_property(TARGET PDI2CFamily.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDI2CFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)

# controllers and clients include PDI2CController.h through this
add_library(PDI2CFamilyHeaders INTERFACE)
target_include_directories(PDI2CFamilyHeaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

