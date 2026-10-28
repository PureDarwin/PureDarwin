# SPDX-License-Identifier: MIT
# freebsd rtwn (realtek rtl8188e, 8192c, 8192e, 8723b, 8812a and 8821a usb wifi) on the fbsdkpi usb shim and net80211
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

# usbdevs.h comes from freebsd's usbdevs list, the haiku compat copy is too old for rtwn's ids
set(PDFBSDRTWN_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${PDFBSDRTWN_GEN}")
add_custom_command(OUTPUT "${PDFBSDRTWN_GEN}/usbdevs.h"
    COMMAND awk -v HEADERFILE=usbdevs.h
        -f "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/haiku/freebsd_network/tools/usbdevs2h.awk"
        "${CMAKE_CURRENT_SOURCE_DIR}/dev/usb/usbdevs" -h
    # the list's own netbsd id line is a comment, copied into the header's comment it would end it early
    COMMAND sed -i -e "/NetBSD: usbdevs/d" usbdevs.h
    WORKING_DIRECTORY "${PDFBSDRTWN_GEN}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dev/usb/usbdevs")
add_custom_target(PDFBSDrtwn_usbdevs DEPENDS "${PDFBSDRTWN_GEN}/usbdevs.h")

set(PDFBSDRTWN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
# the rtwn and if_rtwn_usb module sources from freebsd's sys/modules makefiles
foreach(src
    if_rtwn.c
    if_rtwn_tx.c
    if_rtwn_rx.c
    if_rtwn_beacon.c
    if_rtwn_calib.c
    if_rtwn_cam.c
    if_rtwn_task.c
    if_rtwn_efuse.c
    if_rtwn_fw.c
    rtl8192c/r92c_attach.c
    rtl8192c/r92c_beacon.c
    rtl8192c/r92c_calib.c
    rtl8192c/r92c_chan.c
    rtl8192c/r92c_fw.c
    rtl8192c/r92c_init.c
    rtl8192c/r92c_llt.c
    rtl8192c/r92c_rf.c
    rtl8192c/r92c_rom.c
    rtl8192c/r92c_rx.c
    rtl8192c/r92c_tx.c
    rtl8188e/r88e_beacon.c
    rtl8188e/r88e_calib.c
    rtl8188e/r88e_chan.c
    rtl8188e/r88e_fw.c
    rtl8188e/r88e_init.c
    rtl8188e/r88e_led.c
    rtl8188e/r88e_rf.c
    rtl8188e/r88e_rom.c
    rtl8188e/r88e_rx.c
    rtl8188e/r88e_tx.c
    rtl8192e/r92e_chan.c
    rtl8192e/r92e_fw.c
    rtl8192e/r92e_init.c
    rtl8192e/r92e_led.c
    rtl8192e/r92e_rf.c
    rtl8192e/r92e_rom.c
    rtl8192e/r92e_rx.c
    rtl8723b/r23b_calib.c
    rtl8723b/r23b_fw.c
    rtl8723b/r23b_init.c
    rtl8723b/r23b_led.c
    rtl8723b/r23b_rf.c
    rtl8723b/r23b_rom.c
    rtl8723b/r23b_rx.c
    rtl8812a/r12a_beacon.c
    rtl8812a/r12a_calib.c
    rtl8812a/r12a_caps.c
    rtl8812a/r12a_chan.c
    rtl8812a/r12a_fw.c
    rtl8812a/r12a_init.c
    rtl8812a/r12a_led.c
    rtl8812a/r12a_rf.c
    rtl8812a/r12a_rom.c
    rtl8812a/r12a_rx.c
    rtl8812a/r12a_tx.c
    rtl8821a/r21a_beacon.c
    rtl8821a/r21a_calib.c
    rtl8821a/r21a_chan.c
    rtl8821a/r21a_fw.c
    rtl8821a/r21a_init.c
    rtl8821a/r21a_led.c
    rtl8821a/r21a_rom.c
    rtl8821a/r21a_rx.c
    usb/rtwn_usb_attach.c
    usb/rtwn_usb_ep.c
    usb/rtwn_usb_reg.c
    usb/rtwn_usb_rx.c
    usb/rtwn_usb_tx.c
    rtl8188e/usb/r88eu_attach.c
    rtl8188e/usb/r88eu_init.c
    rtl8192c/usb/r92cu_attach.c
    rtl8192c/usb/r92cu_init.c
    rtl8192c/usb/r92cu_led.c
    rtl8192c/usb/r92cu_rx.c
    rtl8192c/usb/r92cu_tx.c
    rtl8192e/usb/r92eu_attach.c
    rtl8192e/usb/r92eu_init.c
    rtl8723b/usb/r23bu_attach.c
    rtl8723b/usb/r23bu_init.c
    rtl8723b/usb/r23bu_rom.c
    rtl8812a/usb/r12au_attach.c
    rtl8812a/usb/r12au_init.c
    rtl8812a/usb/r12au_rx.c
    rtl8812a/usb/r12au_tx.c
    rtl8821a/usb/r21au_attach.c
    rtl8821a/usb/r21au_init.c
    rtl8821a/usb/r21au_dfs.c
    )
    list(APPEND PDFBSDRTWN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dev/rtwn/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDrtwn
    CLASS PDFBSDrtwn
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDrtwn
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    USB
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${PDFBSDRTWN_GEN}"
    SOURCES ${PDFBSDRTWN_SOURCES}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)
add_dependencies(PDFBSDrtwn_driver PDFBSDrtwn_usbdevs)

# the iokit class is an IO80211Controller, so the apple80211 requests reach it
target_include_directories(PDFBSDrtwn.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDrtwn.kext PRIVATE PDFBSD_WLAN=1)
add_dependencies(PDFBSDrtwn.kext IO80211Family.kext)

# firmware comes from linux-firmware into /Library/Firmware under the names rtwn asks for, see rtwnFirmwareBuild in nix/image-contents.nix

# the few freebsd calls the haiku layer lacks, kept local to this driver
target_compile_options(PDFBSDrtwn_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_rtwn_compat.h")
