# PDQwx: Qualcomm WCN6855 (QCNFA765, 17cb:1103) PCIe 802.11ax on IO80211Family.
# The radio is OpenBSD's qwx(4), ISC and BSD-3-Clause-Clear licensed (see the
# file headers). Firmware is not built in, it loads from /Library/Firmware/qwx.
add_kext_bundle(PDQwx.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDQwx BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQwx.kext PROPERTY OUTPUT_NAME PDQwx)

set(PDQWX_C_SOURCES qwx.c if_qwx_pci.c pd_qwx_glue.c)
target_sources(PDQwx.kext PRIVATE ${PDQWX_C_SOURCES} PDQwx.cpp)
set_source_files_properties(${PDQWX_C_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat/pd80211_compat.h;-Wno-sign-compare;-Wno-unused-parameter;-Wno-unused-function;-Wno-unused-variable;-Wno-unused-but-set-variable;-Wno-strict-prototypes;-Wno-missing-field-initializers;-Wno-shadow;-Wno-cast-qual;-ferror-limit=0")

target_include_directories(PDQwx.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat
    ${CMAKE_CURRENT_BINARY_DIR}/../IO80211Family/shim-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDQwx.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDQwx.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDQwx.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDQwx.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDQwx.kext IO80211Family.kext)
install(TARGETS PDQwx.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
