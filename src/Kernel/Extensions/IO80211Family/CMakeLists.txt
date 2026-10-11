# IO80211Family: the 802.11 family. IO80211Controller and IO80211Interface
# follow Apple's family, the 802.11 protocol engine underneath is OpenBSD's
# net80211 (ISC and BSD licensed, see LICENSE) on a small compat layer.
add_kext_bundle(IO80211Family.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IO80211Family BUNDLE_VERSION 1.0.0)
set_property(TARGET IO80211Family.kext PROPERTY OUTPUT_NAME IO80211Family)

file(GLOB PD80211_NET80211_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/net80211/*.c)
set(PD80211_C_SOURCES
    compat/pd80211_compat.c
    compat/pd_if_media.c
    compat/pd_subr_tree.c
    pd80211_glue.c
    crypto/aes.c
    crypto/arc4.c
    crypto/cmac.c
    crypto/hmac.c
    crypto/key_wrap.c
    crypto/michael.c
    ${PD80211_NET80211_SOURCES}
)
target_sources(IO80211Family.kext PRIVATE ${PD80211_C_SOURCES}
    IO80211Controller.cpp
    IO80211Interface.cpp
)

# the family includes <IOKit/network/...>, linked in from IONetworkingFamily as PDRealtek8111 does
set(PD80211_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
set(PD80211_IONF_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../IONetworkingFamily")
file(MAKE_DIRECTORY "${PD80211_SHIM_DIR}/IOKit/network")
foreach(hdr
    IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
    IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
    IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
    IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
    IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
    IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
    IOOutputQueue.h IOPacketQueue.h)
    file(CREATE_LINK "${PD80211_IONF_SRC_DIR}/${hdr}" "${PD80211_SHIM_DIR}/IOKit/network/${hdr}" SYMBOLIC)
endforeach()

# the openbsd sources see the compat header in place of their kernel headers
set_source_files_properties(${PD80211_C_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/compat/pd80211_compat.h;-Wno-sign-compare;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-shadow;-Wno-cast-qual;-Wno-unused-function;-Wno-unused-variable;-Wno-strict-prototypes;-ferror-limit=0")

target_include_directories(IO80211Family.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${PD80211_SHIM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_compile_definitions(IO80211Family.kext PRIVATE __PRIVATE_SPI__)
set_property(TARGET IO80211Family.kext PROPERTY CXX_STANDARD 14)
add_dependencies(IO80211Family.kext IONetworkingFamily.kext)
target_link_libraries(IO80211Family.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(IO80211Family.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
install(TARGETS IO80211Family.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
