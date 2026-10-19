/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_NET_BPF_H_
#define _FBSD_COMPAT_NET_BPF_H_


#define DLT_EN10MB		1		/* Ethernet (10Mb) */
#define DLT_IEEE802_11	105		/* IEEE 802.11 wireless */


struct bpf_if;


#define bpf_mtap(bpf_if, mbuf) do { } while (0)
#define BPF_MTAP(ifp, m) do { } while (0)
#define BPF_TAP(ifp, pkt, pktlen) do { } while (0)
#define bpf_mtap2(bpf_if, data, dlen, mbuf) do { } while (0)
#define bpfattach(ifnet, dlt, hdrlen) do { } while (0);
#define bpfattach2(ifnet, dlt, hdrlen, bpf_if) do { } while (0)
#define bpfdetach(ifnet) do { } while (0);


static inline int
bpf_peers_present(struct bpf_if *bpf)
{
	return 0;
}

#endif	/* _FBSD_COMPAT_NET_BPF_H_ */
