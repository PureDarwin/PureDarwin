# IONetworkingFamily-151.40.1, vendored from apple-oss-distributions. Provides
# IONetworkController/IONetworkInterface/IOEthernetController/IOEthernetInterface
# and friends - the family every NIC driver (e.g. a future PDE1000) attaches
# under, plus IONetworkStack which publishes en0 etc into BSD's ifnet layer.
add_kext_bundle(IONetworkingFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IONetworkingFamily BUNDLE_VERSION 151.40.1)
set_property(TARGET IONetworkingFamily.kext PROPERTY OUTPUT_NAME IONetworkingFamily)

target_sources(IONetworkingFamily.kext PRIVATE
    IOEthernetController.cpp
    IOEthernetInterface.cpp
    IOKernelDebugger.cpp
    IOMbufMemoryCursor.cpp
    IONetworkController.cpp
    IONetworkData.cpp
    IONetworkInterface.cpp
    IONetworkMedium.cpp
    IONetworkStack.cpp
    IONetworkUserClient.cpp
    IOOutputQueue.cpp
    IOPacketQueue.cpp)

# Sources #include these as <IOKit/network/Foo.h> (matching their real
# Kernel.framework install location) but the vendored tree keeps them flat.
# Mirror the public headers into a local IOKit/network/ shim dir at configure
# time instead of installing them system-wide (they're not needed anywhere
# else yet).
set(PUREDARWIN_IONF_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
file(MAKE_DIRECTORY "${PUREDARWIN_IONF_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/${hdr}"
        "${PUREDARWIN_IONF_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

target_include_directories(IONetworkingFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUREDARWIN_IONF_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/bsd)
target_link_libraries(IONetworkingFamily.kext PRIVATE xnu_kernel_private_headers)
# The .cpp sources define members (outputStart, pollInputPackets,
# actionInputCtl, ...) that the public header only declares when
# __PRIVATE_SPI__ is set (kIONetworkWorkLoopSynchronous/IOMbufServiceClass
# and friends are gated the same way) - Apple builds this family with the
# private SPI enabled, matching how it ships as part of the OS.
target_compile_definitions(IONetworkingFamily.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IONetworkingFamily.kext PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/bsd/net/if_private.h
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/bsd/sys/sockio_private.h
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-deprecated-declarations -Wno-reorder -Wno-unused-private-field)
set_property(TARGET IONetworkingFamily.kext PROPERTY CXX_STANDARD 14)
install(TARGETS IONetworkingFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
