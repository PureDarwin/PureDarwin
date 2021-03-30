install_xnu_headers(TARGET_NAME xnu.headers.bsd_i386 DESTINATION user SUBDIRECTORY i386 FILES
    endian.h fasttrap_isa.h param.h profile.h signal.h limits.h
    _limits.h types.h vmparam.h _types.h _param.h _mcontext.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_i386 DESTINATION kernel SUBDIRECTORY i386 FILES
    endian.h param.h profile.h signal.h limits.h _limits.h
    types.h vmparam.h _types.h _param.h _mcontext.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_i386 DESTINATION kernel_private SUBDIRECTORY i386 FILES disklabel.h)
