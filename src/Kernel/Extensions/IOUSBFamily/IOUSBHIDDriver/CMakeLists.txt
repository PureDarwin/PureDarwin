add_kext_bundle(IOUSBHIDDriver.kext KERNEL_PRIVATE INFO_PLIST Info.plist
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOUSBHIDDriver BUNDLE_VERSION 1.0.0)
set_property(TARGET IOUSBHIDDriver.kext PROPERTY OUTPUT_NAME IOUSBHIDDriver)

target_sources(IOUSBHIDDriver.kext PRIVATE
    IOUSBHIDKeyboard.cpp
    IOUSBHIDMouse.cpp
)

target_include_directories(IOUSBHIDDriver.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../../IOHIDFamily/include
)
target_link_libraries(IOUSBHIDDriver.kext PRIVATE IOUSBFamilyHeaders xnu_kernel_private_headers)
target_compile_options(IOUSBHIDDriver.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET IOUSBHIDDriver.kext PROPERTY CXX_STANDARD 14)
install(TARGETS IOUSBHIDDriver.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
