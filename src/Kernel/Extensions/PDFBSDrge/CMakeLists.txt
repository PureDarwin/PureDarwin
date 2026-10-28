# SPDX-License-Identifier: MIT
# freebsd rge (rtl8125/8126/8127) on the fbsdkpi network layer, plain ifnet without iflib
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDRGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
foreach(src if_rge.c if_rge_hw.c if_rge_stats.c if_rge_sysctl.c)
    list(APPEND PDFBSDRGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dev/rge/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDrge
    CLASS PDFBSDrge
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDrge
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/dev/rge"
    SOURCES ${PDFBSDRGE_SOURCES})

# the few freebsd calls the haiku layer lacks, kept local to this driver
target_compile_options(PDFBSDrge_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_rge_compat.h")
