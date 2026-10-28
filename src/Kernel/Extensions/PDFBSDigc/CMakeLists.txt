# SPDX-License-Identifier: MIT
# freebsd igc (intel i225 and i226 2.5g) on the fbsdkpi iflib layer, laid out like PDFBSDem
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDIGC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
foreach(src if_igc.c igc_api.c igc_base.c igc_i225.c igc_mac.c igc_nvm.c igc_phy.c igc_txrx.c)
    list(APPEND PDFBSDIGC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dev/igc/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDigc
    CLASS PDFBSDigc
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDigc
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    IFLIB
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/dev/igc"
    SOURCES ${PDFBSDIGC_SOURCES})

# the few freebsd calls the haiku layer lacks, kept local to this driver
target_compile_options(PDFBSDigc_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_igc_compat.h")
