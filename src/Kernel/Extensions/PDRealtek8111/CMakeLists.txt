# PDRealtek8111: IOEthernetController for the Realtek RTL8111/8168/8411 PCIe
# gigabit family. Register layout and init sequence follow NetBSD's re(4)
# (BSD-licensed, Bill Paul) - see LICENSE.re.
add_kext_bundle(PDRealtek8111.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRealtek8111 BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRealtek8111.kext PROPERTY OUTPUT_NAME PDRealtek8111)

target_sources(PDRealtek8111.kext PRIVATE
    PDRealtek8111.cpp
)

# PDRealtek8111.cpp includes <IOKit/network/Foo.h>; mirror the same shim trick
# IONetworkingFamily's CMakeLists.txt uses so it can see those headers
# without installing them system-wide.
set(PUREDARWIN_PDRealtek8111_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
set(PUREDARWIN_IONF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily")
file(MAKE_DIRECTORY "${PUREDARWIN_PDRealtek8111_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${PUREDARWIN_IONF_SRC_DIR}/${hdr}"
        "${PUREDARWIN_PDRealtek8111_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(PDRealtek8111.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUREDARWIN_PDRealtek8111_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDRealtek8111.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDRealtek8111.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDRealtek8111.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations)
set_property(TARGET PDRealtek8111.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDRealtek8111.kext IONetworkingFamily.kext)
install(TARGETS PDRealtek8111.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
