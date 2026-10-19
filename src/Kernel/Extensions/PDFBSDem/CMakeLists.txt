include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDEM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c")
foreach(src
    e1000_80003es2lan.c e1000_82540.c e1000_82541.c e1000_82542.c
    e1000_82543.c e1000_82571.c e1000_82575.c e1000_api.c e1000_base.c
    e1000_i210.c e1000_ich8lan.c e1000_mac.c e1000_manage.c e1000_mbx.c
    e1000_nvm.c e1000_osdep.c e1000_phy.c e1000_vf.c em_txrx.c if_em.c
    igb_txrx.c)
    list(APPEND PDFBSDEM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dev/e1000/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDem
    CLASS PDFBSDem
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDem
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    IFLIB
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/dev/e1000"
    SOURCES ${PDFBSDEM_SOURCES})
