install_xnu_headers(TARGET_NAME xnu.headers.bsd_routefs DESTINATION kernel SUBDIRECTORY miscfs/routefs FILES
    routefs.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_routefs DESTINATION kernel_private SUBDIRECTORY miscfs/routefs FILES
    routefs.h
)
