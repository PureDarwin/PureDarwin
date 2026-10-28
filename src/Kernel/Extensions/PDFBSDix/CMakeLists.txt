# SPDX-License-Identifier: MIT
# freebsd ix (intel 82598, 82599, x540, x550 and e610 10g) on the fbsdkpi iflib layer, laid out like PDFBSDigc
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDIX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
foreach(src
    if_ix.c if_bypass.c if_fdir.c if_sriov.c ix_txrx.c ixgbe_osdep.c
    ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_mbx.c ixgbe_vf.c
    ixgbe_dcb.c ixgbe_dcb_82598.c ixgbe_dcb_82599.c
    ixgbe_82598.c ixgbe_82599.c ixgbe_x540.c ixgbe_x550.c ixgbe_e610.c
    ixgbe_fw_logging.c)
    list(APPEND PDFBSDIX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dev/ixgbe/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDix
    CLASS PDFBSDix
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDix
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    IFLIB
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/dev/ixgbe"
    SOURCES ${PDFBSDIX_SOURCES})

# the few freebsd calls the haiku layer lacks, kept local to this driver
target_compile_options(PDFBSDix_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_ix_compat.h"
    -Werror=implicit-function-declaration)
