/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_NET_IF_H_
#define _FBSD_COMPAT_NET_IF_H_


#include <posix/net/if.h>

#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/time.h>


#define IF_Kbps(x)				((uintmax_t)(x) * 1000)
#define IF_Mbps(x)				(IF_Kbps((x) * 1000))
#define IF_Gbps(x)				(IF_Mbps((x) * 1000))

/* Capabilities that interfaces can advertise. */
#define	IFCAP_RXCSUM		0x00001  /* can offload checksum on RX */
#define	IFCAP_TXCSUM		0x00002  /* can offload checksum on TX */
#define	IFCAP_NETCONS		0x00004  /* can be a network console */
#define	IFCAP_VLAN_MTU		0x00008	/* VLAN-compatible MTU */
#define	IFCAP_VLAN_HWTAGGING	0x00010	/* hardware VLAN tag support */
#define	IFCAP_JUMBO_MTU		0x00020	/* 9000 byte MTU supported */
#define	IFCAP_POLLING		0x00040	/* driver supports polling */
#define	IFCAP_VLAN_HWCSUM	0x00080	/* can do IFCAP_HWCSUM on VLANs */
#define	IFCAP_TSO4		0x00100	/* can do TCP Segmentation Offload */
#define	IFCAP_TSO6		0x00200	/* can do TCP6 Segmentation Offload */
#define	IFCAP_LRO		0x00400	/* can do Large Receive Offload */
#define	IFCAP_WOL_UCAST		0x00800	/* wake on any unicast frame */
#define	IFCAP_WOL_MCAST		0x01000	/* wake on any multicast frame */
#define	IFCAP_WOL_MAGIC		0x02000	/* wake on any Magic Packet */
#define	IFCAP_TOE4		0x04000	/* interface can offload TCP */
#define	IFCAP_TOE6		0x08000	/* interface can offload TCP6 */
#define	IFCAP_VLAN_HWFILTER	0x10000 /* interface hw can filter vlan tag */
/* 	available		0x20000 */
#define	IFCAP_VLAN_HWTSO	0x40000 /* can do IFCAP_TSO on VLANs */
#define	IFCAP_LINKSTATE		0x80000 /* the runtime link state is dynamic */
#define	IFCAP_NETMAP		0x100000 /* netmap mode supported/enabled */
#define	IFCAP_RXCSUM_IPV6	0x200000  /* can offload checksum on IPv6 RX */
#define	IFCAP_TXCSUM_IPV6	0x400000  /* can offload checksum on IPv6 TX */
#define	IFCAP_HWSTATS		0x800000 /* manages counters internally */
#define	IFCAP_TXRTLMT		0x1000000 /* hardware supports TX rate limiting */
#define	IFCAP_HWRXTSTMP		0x2000000 /* hardware rx timestamping */
#define	IFCAP_MEXTPG		0x4000000 /* understands M_EXTPG mbufs */
#define	IFCAP_TXTLS4		0x8000000 /* can do TLS encryption and segmentation for TCP */
#define	IFCAP_TXTLS6		0x10000000 /* can do TLS encryption and segmentation for TCP6 */
#define	IFCAP_VXLAN_HWCSUM	0x20000000 /* can do IFCAN_HWCSUM on VXLANs */
#define	IFCAP_VXLAN_HWTSO	0x40000000 /* can do IFCAP_TSO on VXLANs */
#define	IFCAP_TXTLS_RTLMT	0x80000000 /* can do TLS with rate limiting */

#define IFCAP_HWCSUM_IPV6	(IFCAP_RXCSUM_IPV6 | IFCAP_TXCSUM_IPV6)

#define IFCAP_HWCSUM	(IFCAP_RXCSUM | IFCAP_TXCSUM)
#define	IFCAP_TSO	(IFCAP_TSO4 | IFCAP_TSO6)
#define	IFCAP_WOL	(IFCAP_WOL_UCAST | IFCAP_WOL_MCAST | IFCAP_WOL_MAGIC)
#define	IFCAP_TOE	(IFCAP_TOE4 | IFCAP_TOE6)

#define	IFCAP_CANTCHANGE	(IFCAP_NETMAP)

/* interface flags -- these extend the Haiku-native ones from posix/net/if.h */
#define IFF_DRV_RUNNING		0x00010000
#define IFF_DRV_OACTIVE		0x00020000
#define IFF_LINK0			0x00040000		/* per link layer defined bit */
#define	IFF_LINK1			0x00080000		/* per link layer defined bit */
#define	IFF_LINK2			0x00100000		/* per link layer defined bit */
#define IFF_DEBUG			0x00200000
#define	IFF_MONITOR			0x00400000		/* (n) user-requested monitor mode */
#define	IFF_PPROMISC		0x00800000		/* (n) user-requested promisc mode */
#define	IFF_NOGROUP			0x01000000		/* (n) interface is not part of any groups */
#define	IFF_NEEDSGIANT		0x02000000		/* (i) hold Giant over hook calls (OpenBSD compatibility) */

#define LINK_STATE_UNKNOWN	0
#define LINK_STATE_DOWN		1
#define LINK_STATE_UP		2

#define IFQ_MAXLEN			50


/*
 * Structure describing information about an interface
 * which may be of interest to management entities.
 */
struct if_data {
	/* generic interface information */
	uint8_t	ifi_type;			/* ethernet, tokenring, etc */
	uint8_t	ifi_physical;		/* e.g., AUI, Thinnet, 10base-T, etc */
	uint8_t	ifi_addrlen;		/* media address length */
	uint8_t	ifi_hdrlen;			/* media header length */
	uint8_t	ifi_link_state;		/* current link state */
	uint8_t	ifi_recvquota;		/* polling quota for receive intrs */
	uint8_t	ifi_xmitquota;		/* polling quota for xmit intrs */
	uint16_t	ifi_datalen;		/* length of this data struct */
	uint32_t	ifi_mtu;			/* maximum transmission unit */
	uint32_t	ifi_metric;			/* routing metric (external only) */
	uint64_t	ifi_baudrate;		/* linespeed */
	/* volatile statistics */
	uint64_t	ifi_ipackets;		/* packets received on interface */
	uint64_t	ifi_ierrors;		/* input errors on interface */
	uint64_t	ifi_opackets;		/* packets sent on interface */
	uint64_t	ifi_oerrors;		/* output errors on interface */
	uint64_t	ifi_collisions;		/* collisions on csma interfaces */
	uint64_t	ifi_ibytes;			/* total number of octets received */
	uint64_t	ifi_obytes;			/* total number of octets sent */
	uint64_t	ifi_imcasts;		/* packets received via multicast */
	uint64_t	ifi_omcasts;		/* packets sent via multicast */
	uint64_t	ifi_iqdrops;		/* dropped on input, this interface */
	uint64_t	ifi_oqdrops;		/* dropped on output, this interface */
	uint64_t	ifi_noproto;		/* destined for unsupported protocol */
	uint64_t	ifi_hwassist;		/* HW offload capabilities */
	time_t	ifi_epoch;			/* uptime at attach or stat reset */
#ifdef __alpha__
	u_int	ifi_timepad;		/* time_t is int, not long on alpha */
#endif
	struct	timeval ifi_lastchange;	/* time of last administrative change */
};

struct ifmediareq {
	char	ifm_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	int	ifm_current;		/* current media options */
	int	ifm_mask;		/* don't care mask */
	int	ifm_status;		/* media status */
	int	ifm_active;		/* active options */
	int	ifm_count;		/* # entries in ifm_ulist array */
	int	*ifm_ulist;		/* media words */
};

struct  ifdrv {
	char			ifd_name[IFNAMSIZ];     /* if name, e.g. "en0" */
	unsigned long	ifd_cmd;
	size_t			ifd_len;
	void*			ifd_data;
};

/*
 * Structure used to request i2c data
 * from interface transceivers.
 */
struct ifi2creq {
	uint8_t dev_addr;	/* i2c address (0xA0, 0xA2) */
	uint8_t offset;		/* read offset */
	uint8_t len;		/* read length */
	uint8_t spare0;
	uint32_t spare1;
	uint8_t data[8];	/* read buffer */
};

#ifdef _KERNEL
/* TODO: this should go away soon. */
#	include <net/if_var.h>
#endif


#endif	/* _FBSD_COMPAT_NET_IF_H_ */
