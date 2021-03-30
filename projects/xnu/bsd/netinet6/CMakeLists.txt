install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet6 DESTINATION user SUBDIRECTORY netinet6 FILES
    ah.h ipsec.h esp.h in6.h ipcomp.h raw_ip6.h scope6_var.h in6_var.h nd6.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet6 DESTINATION user_private SUBDIRECTORY netinet6 FILES
    in6.h in6_pcb.h in6_var.h ip6_var.h mld6_var.h nd6.h scope6_var.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet6 DESTINATION kernel SUBDIRECTORY netinet6 FILES
    ah.h ipsec.h esp.h in6.h ipcomp.h raw_ip6.h scope6_var.h in6_var.h nd6.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_netinet6 DESTINATION kernel_private SUBDIRECTORY netinet6 FILES
    in6.h in6_pcb.h in6_var.h ip6_var.h mld6_var.h nd6.h scope6_var.h
    ah6.h esp6.h esp_rijndael.h esp_chachapoly.h in6_gif.h in6_ifattach.h
    ip6_ecn.h ip6protosw.h ipsec6.h tcp6_var.h udp6_var.h
)
