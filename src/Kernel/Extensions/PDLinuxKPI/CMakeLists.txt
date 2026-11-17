# linuxkpi and net80211 once, for every wifi driver built SHARED against it
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

add_fbsdkpi_lkpi_library(PDLinuxKPI
    BUNDLE_IDENTIFIER org.puredarwin.kpi.linux
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
