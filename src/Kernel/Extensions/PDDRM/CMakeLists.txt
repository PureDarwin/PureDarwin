# the drm core and ttm once, for every drm driver built SHARED against it
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

add_fbsdkpi_drm_library(PDDRM
    BUNDLE_IDENTIFIER org.puredarwin.kpi.drm
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
