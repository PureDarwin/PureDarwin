# SPDX-License-Identifier: MIT
# freebsd run (ralink rt2700 to rt5592 usb wifi) on the fbsdkpi usb shim and net80211
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

# usbdevs.h comes from freebsd's usbdevs list, the haiku compat copy is too old for run's ids
set(PDFBSDRUN_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${PDFBSDRUN_GEN}")
add_custom_command(OUTPUT "${PDFBSDRUN_GEN}/usbdevs.h"
    COMMAND awk -v HEADERFILE=usbdevs.h
        -f "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/haiku/freebsd_network/tools/usbdevs2h.awk"
        "${CMAKE_CURRENT_SOURCE_DIR}/dev/usb/usbdevs" -h
    # the list's own netbsd id line is a comment, copied into the header's comment it would end it early
    COMMAND sed -i -e "/NetBSD: usbdevs/d" usbdevs.h
    WORKING_DIRECTORY "${PDFBSDRUN_GEN}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dev/usb/usbdevs")
add_custom_target(PDFBSDrun_usbdevs DEPENDS "${PDFBSDRUN_GEN}/usbdevs.h")

set(PDFBSDRUN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/dev/usb/wlan/if_run.c")

add_fbsdkpi_driver(PDFBSDrun
    CLASS PDFBSDrun
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDrun
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    USB
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${PDFBSDRUN_GEN}"
    SOURCES ${PDFBSDRUN_SOURCES}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)
add_dependencies(PDFBSDrun_driver PDFBSDrun_usbdevs)

# the iokit class is an IO80211Controller, so the apple80211 requests reach it
target_include_directories(PDFBSDrun.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDrun.kext PRIVATE PDFBSD_WLAN=1)
add_dependencies(PDFBSDrun.kext IO80211Family.kext)

# runfw comes from linux-firmware's rt2870.bin into /Library/Firmware, see runFirmwareBuild in nix/image-contents.nix

# the few freebsd calls the haiku layer lacks, kept local to this driver
target_compile_options(PDFBSDrun_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_run_compat.h")
