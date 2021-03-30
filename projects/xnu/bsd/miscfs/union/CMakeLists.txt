install_xnu_headers(TARGET_NAME xnu.headers.bsd_unionfs DESTINATION user SUBDIRECTORY miscfs/union FILES
    union.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_unionfs DESTINATION kernel SUBDIRECTORY miscfs/union FILES
    union.h
)
