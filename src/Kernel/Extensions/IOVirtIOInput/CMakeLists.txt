# IOVirtIOInput: QEMU's virtio tablet, mouse and keyboard from the public VIRTIO 1.2 spec,
# into the raw HID queues the USB and PS/2 drivers feed
add_kext_bundle(IOVirtIOInput.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOVirtIOInput BUNDLE_VERSION 1.0.0)
set_property(TARGET IOVirtIOInput.kext PROPERTY OUTPUT_NAME IOVirtIOInput)

target_sources(IOVirtIOInput.kext PRIVATE
    IOVirtIOInput.cpp
)

target_include_directories(IOVirtIOInput.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(IOVirtIOInput.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOVirtIOInput.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IOVirtIOInput.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET IOVirtIOInput.kext PROPERTY CXX_STANDARD 14)
add_dependencies(IOVirtIOInput.kext IOVirtIOFamily.kext IOHIDFamily.kext)
install(TARGETS IOVirtIOInput.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
