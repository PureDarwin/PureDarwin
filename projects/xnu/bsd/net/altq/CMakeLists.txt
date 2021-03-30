install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_altq DESTINATION user_private SUBDIRECTORY net/altq FILES
    altq.h altq_cbq.h altq_fairq.h altq_hfsc.h altq_priq.h altq_qfq.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_altq DESTINATION kernel_private SUBDIRECTORY net/altq FILES
    altq.h altq_cbq.h altq_fairq.h altq_hfsc.h altq_priq.h altq_qfq.h
)
