install_xnu_headers(TARGET_NAME xnu.headers.bsd_audit DESTINATION user SUBDIRECTORY security/audit FILES
    audit_ioctl.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_audit DESTINATION kernel SUBDIRECTORY security/audit FILES
    audit_ioctl.h audit.h audit_bsd.h audit_private.h
)
