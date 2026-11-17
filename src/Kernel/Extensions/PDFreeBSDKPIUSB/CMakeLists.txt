# haiku's usb shim once, for every usb driver built SHARED against it
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

add_fbsdkpi_usb_library(PDFreeBSDKPIUSB
    BUNDLE_IDENTIFIER org.puredarwin.kpi.freebsd.usb
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
