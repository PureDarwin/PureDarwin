install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_pktsched DESTINATION user_private SUBDIRECTORY net/pktsched FILES
    pktsched.h pktsched_cbq.h pktsched_fairq.h pktsched_hfsc.h pktsched_priq.h pktsched_rmclass.h pktsched_fq_codel.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_net_pktsched DESTINATION kernel_private SUBDIRECTORY net/pktsched FILES
    pktsched.h pktsched_cbq.h pktsched_fairq.h pktsched_hfsc.h pktsched_priq.h pktsched_rmclass.h pktsched_fq_codel.h
)
