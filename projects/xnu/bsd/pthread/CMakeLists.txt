install_xnu_headers(TARGET_NAME xnu.headers.bsd_pthread DESTINATION user_private SUBDIRECTORY pthread FILES
    bsdthread_private.h priority_private.h workqueue_internal.h
)
