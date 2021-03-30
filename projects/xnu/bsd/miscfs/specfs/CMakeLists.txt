install_xnu_headers(TARGET_NAME xnu.headers.bsd_specfs DESTINATION user SUBDIRECTORY miscfs/specfs FILES
    specdev.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_specfs DESTINATION kernel SUBDIRECTORY miscfs/specfs FILES
    specdev.h
)
