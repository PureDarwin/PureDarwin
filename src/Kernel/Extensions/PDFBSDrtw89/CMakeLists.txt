include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDRTW89_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
foreach(src
    core.c chan.c mac80211.c mac.c mac_be.c phy.c phy_be.c fw.c cam.c
    efuse.c efuse_be.c regd.c sar.c coex.c ps.c ser.c util.c debug.c acpi.c
    rtw8852c.c rtw8852c_table.c rtw8852c_rfk.c rtw8852c_rfk_table.c
    rtw8852b_common.c rtw8852b.c rtw8852b_table.c rtw8852b_rfk.c
    rtw8852b_rfk_table.c
    pci.c pci_be.c rtw8852ce.c rtw8852be.c)
    list(APPEND PDFBSDRTW89_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rtw89/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDrtw89
    CLASS PDFBSDrtw89
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDrtw89
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/rtw89"
    DEFINITIONS KBUILD_MODNAME=\"rtw89\" LINUXKPI_VERSION=70000
        CONFIG_RTW89_DEBUGMSG
        # test-only, off by default: PD_RTW89_VM_LINK_GEN1 takes gen1 when a vm hides the link
        ${PD_RTW89_TEST_DEFINES}
    SOURCES ${PDFBSDRTW89_SOURCES}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)

# the iokit class is an IO80211Controller, so the apple80211 requests reach it
target_include_directories(PDFBSDrtw89.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDrtw89.kext PRIVATE PDFBSD_WLAN=1)
add_dependencies(PDFBSDrtw89.kext IO80211Family.kext)

# firmware stays in the bundle and loads from disk, the kernel collection has a 64 MB cap
install(FILES firmware/rtw89/rtw8852b_fw-1.bin firmware/rtw89/rtw8852c_fw-2.bin
    DESTINATION System/Library/Extensions/PDFBSDrtw89.kext/Contents/Resources/firmware/rtw89
    COMPONENT BaseSystem)
install(FILES firmware/LICENCE.rtlwifi_firmware.txt
    DESTINATION System/Library/Extensions/PDFBSDrtw89.kext/Contents/Resources
    COMPONENT BaseSystem)
