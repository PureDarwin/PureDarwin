install_xnu_headers(TARGET_NAME xnu.headers.bsd_pthread DESTINATION user_private SUBDIRECTORY pthread FILES
    bsdthread_private.h priority_private.h workqueue.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_pthread DESTINATION kernel SUBDIRECTORY pthread FILES
    keysock.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_pthread DESTINATION kernel_private SUBDIRECTORY pthread FILES
    keysock.h key_debug.h keydb.h key_var.h key.h
)
