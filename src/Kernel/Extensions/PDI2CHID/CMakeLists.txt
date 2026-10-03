add_kext_bundle(PDI2CHID.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDI2CHID BUNDLE_VERSION 1.0.0)
set_property(TARGET PDI2CHID.kext PROPERTY OUTPUT_NAME PDI2CHID)
target_sources(PDI2CHID.kext PRIVATE PDI2CHIDDevice.cpp)

target_include_directories(PDI2CHID.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily
)
target_link_libraries(PDI2CHID.kext PRIVATE PDI2CFamilyHeaders xnu_kernel_private_headers)
target_compile_options(PDI2CHID.kext PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include/pd_hid_compat.h"
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDI2CHID.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDI2CHID.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
