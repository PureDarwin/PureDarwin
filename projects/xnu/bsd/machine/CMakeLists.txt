install_xnu_headers(TARGET_NAME xnu.headers.bsd_machine DESTINATION user SUBDIRECTORY machine FILES
    byte_order.h endian.h fasttrap_isa.h limits.h param.h profile.h signal.h types.h
    vmparam.h _types.h _limits.h _param.h _mcontext.h machine_types.modulemap
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_machine DESTINATION user_private SUBDIRECTORY machine FILES disklabel.h)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_machine DESTINATION kernel SUBDIRECTORY machine FILES
    disklabel.h byte_order.h endian.h limits.h param.h profile.h signal.h types.h
    vmparam.h _types.h _limits.h _param.h _mcontext.h
)
