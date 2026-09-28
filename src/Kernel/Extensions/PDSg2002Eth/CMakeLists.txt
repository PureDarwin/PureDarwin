# PDSg2002Eth: the sg2002 designware gmac and its internal 10/100 phy, polled like IOVirtIONet
add_kext_bundle(PDSg2002Eth.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDSg2002Eth BUNDLE_VERSION 1.0.0)
set_property(TARGET PDSg2002Eth.kext PROPERTY OUTPUT_NAME PDSg2002Eth)

target_sources(PDSg2002Eth.kext PRIVATE
    PDSg2002Eth.cpp
)

# IONetworkingFamily headers through a shim dir, the same trick as PDE1000
set(PUREDARWIN_SGETH_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
set(PUREDARWIN_IONF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily")
file(MAKE_DIRECTORY "${PUREDARWIN_SGETH_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${PUREDARWIN_IONF_SRC_DIR}/${hdr}"
        "${PUREDARWIN_SGETH_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(PDSg2002Eth.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUREDARWIN_SGETH_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDSg2002Eth.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDSg2002Eth.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDSg2002Eth.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations)
set_property(TARGET PDSg2002Eth.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDSg2002Eth.kext IONetworkingFamily.kext)
install(TARGETS PDSg2002Eth.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
