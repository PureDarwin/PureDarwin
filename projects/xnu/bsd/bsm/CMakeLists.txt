install_xnu_headers(TARGET_NAME xnu.headers.bsd_bsm DESTINATION user SUBDIRECTORY bsm FILES
    audit.h audit_domain.h audit_errno.h audit_fcntl.h audit_internal.h
    audit_kevents.h audit_record.h audit_socket_type.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_bsm DESTINATION kernel SUBDIRECTORY bsm FILES audit.h)
