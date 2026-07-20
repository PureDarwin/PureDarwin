# IOVirtIONet: minimal IOEthernetController for QEMU's virtio-net-pci.
# Written from scratch against the public VIRTIO 1.1 spec (no Apple
# source exists for this device class). Polling-only, matching PDE1000's
# shape: no MSI-X/interrupts, RX is timer-polled.
add_kext_bundle(IOVirtIONet.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOVirtIONet BUNDLE_VERSION 1.0.0)
set_property(TARGET IOVirtIONet.kext PROPERTY OUTPUT_NAME IOVirtIONet)

target_sources(IOVirtIONet.kext PRIVATE
    IOVirtIONet.cpp
)

# IOVirtIONet.cpp includes <IOKit/network/Foo.h>; mirror PDE1000's shim
# trick so it can see those headers without installing them system-wide.
set(PUREDARWIN_VNET_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
set(PUREDARWIN_IONF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily")
file(MAKE_DIRECTORY "${PUREDARWIN_VNET_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${PUREDARWIN_IONF_SRC_DIR}/${hdr}"
        "${PUREDARWIN_VNET_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(IOVirtIONet.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUREDARWIN_VNET_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(IOVirtIONet.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOVirtIONet.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IOVirtIONet.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations)
set_property(TARGET IOVirtIONet.kext PROPERTY CXX_STANDARD 14)
add_dependencies(IOVirtIONet.kext IONetworkingFamily.kext IOVirtIOFamily.kext)
install(TARGETS IOVirtIONet.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
