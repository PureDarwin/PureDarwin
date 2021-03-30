install_xnu_headers(TARGET_NAME xnu.headers.bsd_netkey DESTINATION user SUBDIRECTORY netkey FILES
    keysock.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netkey DESTINATION user_private SUBDIRECTORY netkey FILES
    key_debug.h keydb.h key_var.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netkey DESTINATION kernel SUBDIRECTORY netkey FILES
    keysock.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netkey DESTINATION kernel_private SUBDIRECTORY netkey FILES
    keysock.h key_debug.h keydb.h key_var.h key.h
)
