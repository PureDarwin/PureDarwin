# Hyper-V guest support (hypercalls, SynIC, VMBus), for WSL2 and Hyper-V VMs
add_kext_bundle(PDHyperV.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDHyperV BUNDLE_VERSION 1.0.0)
set_property(TARGET PDHyperV.kext PROPERTY OUTPUT_NAME PDHyperV)

target_sources(PDHyperV.kext PRIVATE
    PDHyperV.cpp
    PDHVStorage.cpp
    PDHVSock.cpp
    PDHVNetwork.cpp
    hypercall.s
)

# PDHVNetwork includes <IOKit/network/...>, so link IONetworkingFamily's headers under that path
set(PDHYPERV_NET_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
file(MAKE_DIRECTORY "${PDHYPERV_NET_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily/${hdr}"
        "${PDHYPERV_NET_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(PDHyperV.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PDHYPERV_NET_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_compile_definitions(PDHyperV.kext PRIVATE __PRIVATE_SPI__)
target_link_libraries(PDHyperV.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDHyperV.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual -Wno-packed
    -Wno-deprecated-declarations)
set_property(TARGET PDHyperV.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDHyperV.kext IOStorageFamily IONetworkingFamily.kext)
install(TARGETS PDHyperV.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
