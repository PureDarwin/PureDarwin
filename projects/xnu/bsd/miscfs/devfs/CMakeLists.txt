install_xnu_headers(TARGET_NAME xnu.headers.bsd_devfs DESTINATION user SUBDIRECTORY miscfs/devfs FILES
    devfs.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_devfs DESTINATION user_private SUBDIRECTORY miscfs/devfs FILES
    devfs.h
    devfs_proto.h
    defsdefs.h
    fdesc.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_devfs DESTINATION kernel SUBDIRECTORY miscfs/devfs FILES
    devfs.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_devfs DESTINATION kernel_private SUBDIRECTORY miscfs/devfs FILES
    devfs.h
    devfs_proto.h
    defsdefs.h
)
