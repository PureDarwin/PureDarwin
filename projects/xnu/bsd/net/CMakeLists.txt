install_xnu_headers(TARGET_NAME xnu.headers.bsd_net DESTINATION user SUBDIRECTORY net FILES
    bpf.h dlil.h ethernet.h if.h if_arp.h if_dl.h if_llc.h if_media.h if_mib.h
    if_types.h if_var.h if_utun.h kext_net.h ndrv.h pfkeyv2.h route.h net_kev.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_net DESTINATION user_private SUBDIRECTORY net FILES
    bpf.h dlil.h ethernet.h if.h if_arp.h if_dl.h if_llc.h if_media.h if_mib.h
    if_types.h if_var.h if_utun.h kext_net.h ndrv.h pfkeyv2.h route.h net_kev.h
    kpi_interface.h kpi_interfacefilter.h kpi_protocol.h if_ether.h init.h radix.h

    content_filter.h etherdefs.h firewire.h if_bond_var.h if_bond_internal.h if_bridgevar.h
    if_ipsec.h if_llreach.h if_pflog.h if_ports_used.h if_ppp.h if_vlan_var.h if_fake_var.h
    if_6lowpan_var.h iptap.h lacp.h multi_layer_pkt_log.h ndrv_var.h necp.h net_api_stats.h
    net_log_common.h netsrc.h network_agent.h ntstat.h packet_mangler.h pfkeyv2.h pfvar.h
    pktap.h ppp_defs.h raw_cb.h net_perf.h sixxlowpan.h frame802154.h nat464_utils.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_net DESTINATION kernel SUBDIRECTORY net FILES
    bpfdesc.h ppp_comp.h zlib.h bpf_compat.h net_osdep.h flowadv.h net_perf.h
)

install_xnu_headers(TARGET_NAME xnu.headers.bsd_net DESTINATION kernel_private SUBDIRECTORY net FILES
    bpf.h dlil.h ethernet.h if.h if_arp.h if_dl.h if_llc.h if_media.h if_mib.h
    if_types.h if_var.h if_utun.h kext_net.h ndrv.h pfkeyv2.h route.h net_kev.h
    bpfdesc.h ppp_comp.h zlib.h bpf_compat.h net_osdep.h flowadv.h net_perf.h

    content_filter.h etherdefs.h firewire.h if_bond_var.h if_bond_internal.h if_bridgevar.h
    if_ipsec.h if_llreach.h if_pflog.h if_ports_used.h if_ppp.h if_vlan_var.h if_fake_var.h
    if_6lowpan_var.h iptap.h lacp.h multi_layer_pkt_log.h ndrv_var.h necp.h net_api_stats.h
    net_log_common.h netsrc.h network_agent.h ntstat.h packet_mangler.h pfkeyv2.h pfvar.h
    pktap.h ppp_defs.h raw_cb.h net_perf.h sixxlowpan.h frame802154.h nat464_utils.h
)

add_subdirectory(altq)
add_subdirectory(classq)
add_subdirectory(pktsched)
