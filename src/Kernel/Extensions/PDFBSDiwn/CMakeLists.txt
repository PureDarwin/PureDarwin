# SPDX-License-Identifier: MIT
# freebsd iwn (intel wifi link 1000 to 6000 series, the cards iwlwifi drives with dvm) on fbsdkpi and net80211
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

add_fbsdkpi_driver(PDFBSDiwn
    SHARED
    CLASS PDFBSDiwn
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDiwn
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c" "${CMAKE_CURRENT_SOURCE_DIR}/dev/iwn/if_iwn.c"
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)

# the iokit class is an IO80211Controller, so the apple80211 requests reach it
target_include_directories(PDFBSDiwn.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDiwn.kext PRIVATE PDFBSD_WLAN=1)
add_dependencies(PDFBSDiwn.kext IO80211Family.kext)

# firmware comes from linux-firmware into /Library/Firmware under the names iwn asks for, see iwnFirmwareBuild in nix/image-contents.nix
