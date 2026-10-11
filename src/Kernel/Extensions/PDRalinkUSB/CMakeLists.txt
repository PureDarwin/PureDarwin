# PDRalinkUSB: Ralink RT2870/RT3070/RT3572/RT5370 USB 802.11 adapters on
# IO80211Family. The radio is OpenBSD's run(4), ISC licensed (LICENSE.run).
# rt2870.bin is Ralink's firmware, redistributable unmodified under
# Resources/LICENCE.ralink-firmware.txt.
add_kext_bundle(PDRalinkUSB.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRalinkUSB BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRalinkUSB.kext PROPERTY OUTPUT_NAME PDRalinkUSB)

# the firmware goes into the binary byte for byte
set(PDRALINK_FW_C "${CMAKE_CURRENT_BINARY_DIR}/rt2870_fw.c")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Resources/rt2870.bin" PDRALINK_FW_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," PDRALINK_FW_BYTES "${PDRALINK_FW_HEX}")
file(WRITE "${PDRALINK_FW_C}" "#include <stddef.h>\n#include <stdint.h>\nconst uint8_t pd_rt2870_fw[] = { ${PDRALINK_FW_BYTES} };\nconst size_t pd_rt2870_fw_len = sizeof(pd_rt2870_fw);\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/rt2870.bin")

set(PDRALINK_C_SOURCES if_run.c pd_run_glue.c)
target_sources(PDRalinkUSB.kext PRIVATE ${PDRALINK_C_SOURCES} "${PDRALINK_FW_C}" PDRalinkUSB.cpp)
set_source_files_properties(${PDRALINK_C_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat/pd80211_compat.h;-Wno-sign-compare;-Wno-unused-parameter;-Wno-unused-function;-Wno-unused-variable;-Wno-strict-prototypes;-ferror-limit=0")

target_include_directories(PDRalinkUSB.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family
    ${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family/compat
    ${CMAKE_CURRENT_BINARY_DIR}/../IO80211Family/shim-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDRalinkUSB.kext PRIVATE IOUSBFamilyHeaders xnu_kernel_private_headers)
target_compile_definitions(PDRalinkUSB.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDRalinkUSB.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDRalinkUSB.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDRalinkUSB.kext IO80211Family.kext)
install(TARGETS PDRalinkUSB.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
install(FILES Resources/LICENCE.ralink-firmware.txt
    DESTINATION System/Library/Extensions/PDRalinkUSB.kext/Contents/Resources COMPONENT BaseSystem)
