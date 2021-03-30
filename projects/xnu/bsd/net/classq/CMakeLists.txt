install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_classq DESTINATION user_private SUBDIRECTORY net/classq FILES
    classq.h classq_blue.h classq_red.h classq_rio.h classq_sfb.h if_classq.h classq_fq_codel.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_classq DESTINATION kernel_private SUBDIRECTORY net/classq FILES
    classq.h classq_blue.h classq_red.h classq_rio.h classq_sfb.h if_classq.h classq_fq_codel.h
)
