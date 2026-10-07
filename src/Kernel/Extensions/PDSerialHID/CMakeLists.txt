# A keyboard and an absolute pointer driven over the serial console, for boards with no USB input
add_kext_bundle(PDSerialHID.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDSerialHID BUNDLE_VERSION 1.0.0)
set_property(TARGET PDSerialHID.kext PROPERTY OUTPUT_NAME PDSerialHID)

target_sources(PDSerialHID.kext PRIVATE
    PDSerialHID.cpp
)

target_include_directories(PDSerialHID.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily
)
target_link_libraries(PDSerialHID.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDSerialHID.kext PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/include/pd_hid_compat.h"
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDSerialHID.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDSerialHID.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
