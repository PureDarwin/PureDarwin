# CMakeLists.txt - wcn36xx (ISC, from linux drivers/net/wireless/ath/wcn36xx) on linuxkpi, over PDQcomWCNSS
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDWCN36XX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_wcnss_shim.c")
foreach(src main.c dxe.c txrx.c smd.c pmc.c firmware.c)
    list(APPEND PDFBSDWCN36XX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wcn36xx/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDwcn36xx
    CLASS PDFBSDwcn36xx
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDwcn36xx
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/wcn36xx"
    DEFINITIONS KBUILD_MODNAME=\"wcn36xx\" LINUXKPI_VERSION=70000
    # linux builds with zeroed stack variables (INIT_STACK_ALL_ZERO), and wcn36xx relies on it:
    # config bss copies a partly filled stack struct into the hal message
    LKPI_OPTIONS -ftrivial-auto-var-init=zero
    SOURCES ${PDFBSDWCN36XX_SOURCES}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)

# the iokit class is an IO80211Controller on a device-tree provider, PDQcomWCNSS
target_include_directories(PDFBSDwcn36xx.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDwcn36xx.kext PRIVATE PDFBSD_WLAN=1 PDFBSD_PLATFORM=1)
add_dependencies(PDFBSDwcn36xx.kext IO80211Family.kext PDQcomWCNSS.kext)
