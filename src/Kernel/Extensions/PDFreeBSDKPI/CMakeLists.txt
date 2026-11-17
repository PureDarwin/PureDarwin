# the fbsdkpi layer once, for every driver built SHARED against it
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

add_fbsdkpi_library(PDFreeBSDKPI
    BUNDLE_IDENTIFIER org.puredarwin.kpi.freebsd
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
