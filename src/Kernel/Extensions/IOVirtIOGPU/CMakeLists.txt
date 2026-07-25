# IOVirtIOGPU: minimal virtio-gpu 2D scanout driver for QEMU's
# -device virtio-gpu-pci. Written from scratch against the public VIRTIO
# 1.1 spec (no Apple source exists for this device class). Polling-only:
# no MSI-X/interrupts, command completion and screen refresh are both
# polled (see IOVirtIOGPU.cpp for details).
add_kext_bundle(IOVirtIOGPU.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOVirtIOGPU BUNDLE_VERSION 1.0.0)
set_property(TARGET IOVirtIOGPU.kext PROPERTY OUTPUT_NAME IOVirtIOGPU)

target_sources(IOVirtIOGPU.kext PRIVATE
    IOVirtIOGPU.cpp
    IOVirtIOGPUUserClient.cpp
)

target_include_directories(IOVirtIOGPU.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily)
target_link_libraries(IOVirtIOGPU.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOVirtIOGPU.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IOVirtIOGPU.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET IOVirtIOGPU.kext PROPERTY CXX_STANDARD 14)
add_dependencies(IOVirtIOGPU.kext IOGOPFramebuffer.kext IOVirtIOFamily.kext)
install(TARGETS IOVirtIOGPU.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
