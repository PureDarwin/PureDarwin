# PDUSBNCM: USB CDC NCM Ethernet, from the USB-IF CDC 1.2 and NCM 1.0 specifications
add_kext_bundle(PDUSBNCM.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDUSBNCM BUNDLE_VERSION 1.0.0)
set_property(TARGET PDUSBNCM.kext PROPERTY OUTPUT_NAME PDUSBNCM)

target_sources(PDUSBNCM.kext PRIVATE
    PDUSBNCM.cpp
)

# PDUSBNCM.cpp includes <IOKit/network/Foo.h>, mirror the same shim trick
# IONetworkingFamily's CMakeLists.txt uses so it can see those headers
# without installing them system-wide.
set(PUREDARWIN_PDUSBNCM_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
set(PUREDARWIN_IONF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily")
file(MAKE_DIRECTORY "${PUREDARWIN_PDUSBNCM_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${PUREDARWIN_IONF_SRC_DIR}/${hdr}"
        "${PUREDARWIN_PDUSBNCM_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(PDUSBNCM.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUREDARWIN_PDUSBNCM_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDUSBNCM.kext PRIVATE IOUSBFamilyHeaders xnu_kernel_private_headers)
target_compile_definitions(PDUSBNCM.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDUSBNCM.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations)
set_property(TARGET PDUSBNCM.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDUSBNCM.kext IONetworkingFamily.kext)
install(TARGETS PDUSBNCM.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
