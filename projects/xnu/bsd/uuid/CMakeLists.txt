install_xnu_headers(TARGET_NAME xnu.headers.bsd_uuid DESTINATION user SUBDIRECTORY uuid FILES
    uuid.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_uuid DESTINATION kernel SUBDIRECTORY uuid FILES
    uuid.h
)
