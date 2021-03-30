install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet DESTINATION user SUBDIRECTORY netinet FILES
    bootp.h icmp6.h if_ether.h icmp_var.h igmp.h igmp_var.h in.h in_pcb.h 
    in_systm.h in_var.h ip.h ip6.h ip_icmp.h ip_var.h tcp.h tcp_fsm.h tcp_seq.h
    tcp_timer.h tcp_var.h tcpip.h udp.h udp_var.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet DESTINATION user_private SUBDIRECTORY netinet FILES
    flow_divert_proto.h igmp_var.h in.h in_gif.h in_pcb.h in_tclass.h ip.h ip_compat.h
    ip_dummynet.h ip_flowid.h mptcp_var.h tcp.h tcp_cc.h tcp_debug.h tcp_var.h tcp_cache.h
    udp.h in_stat.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet DESTINATION kernel SUBDIRECTORY netinet FILES
    kpi_ipfilter.h in_arp.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet DESTINATION kernel_private SUBDIRECTORY netinet FILES
    kpi_ipfilter.h in_arp.h ip_ecn.h ip_encap.h tcp_log.h
)
