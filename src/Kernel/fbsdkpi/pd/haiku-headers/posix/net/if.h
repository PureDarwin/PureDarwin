/*
 * Copyright 2006-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NET_IF_H
#define _NET_IF_H


#include <net/route.h>
#include <sys/socket.h>


#define IF_NAMESIZE	32

/* BSD specific/proprietary part */

#define IFNAMSIZ IF_NAMESIZE

struct ifreq_stream_stats {
	uint32_t	packets;
	uint32_t	errors;
	uint64_t	bytes;
	uint32_t	multicast_packets;
	uint32_t	dropped;
};

struct ifreq_stats {
	struct ifreq_stream_stats receive;
	struct ifreq_stream_stats send;
	uint32_t	collisions;
};

struct ifreq {
	char			ifr_name[IF_NAMESIZE];
	union {
		struct sockaddr ifr_addr;
		struct sockaddr ifr_dstaddr;
		struct sockaddr ifr_broadaddr;
		struct sockaddr ifr_mask;
		struct ifreq_stats ifr_stats;
		struct route_entry ifr_route;
		int			ifr_flags;
		int			ifr_index;
		int			ifr_metric;
		int			ifr_mtu;
		int			ifr_media;
		int			ifr_type;
		int			ifr_reqcap;
		int			ifr_count;
		uint8_t*	ifr_data;
	};
};

/* used with SIOC_IF_ALIAS_ADD, SIOC_IF_ALIAS_GET, SIOC_ALIAS_SET */
struct ifaliasreq {
	char			ifra_name[IF_NAMESIZE];
	int				ifra_index;
	struct sockaddr_storage ifra_addr;
	union {
		struct sockaddr_storage ifra_broadaddr;
		struct sockaddr_storage ifra_destination;
	};
	struct sockaddr_storage ifra_mask;
	uint32_t		ifra_flags;
};


/* interface flags */
#define IFF_UP				0x0001
#define IFF_BROADCAST		0x0002	/* valid broadcast address */
#define IFF_LOOPBACK		0x0008
#define IFF_POINTOPOINT		0x0010	/* point-to-point link */
#define IFF_NOARP			0x0040	/* no address resolution */
#define IFF_AUTOUP			0x0080	/* auto dial */
#define IFF_PROMISC			0x0100	/* receive all packets */
#define IFF_ALLMULTI		0x0200	/* receive all multicast packets */
#define IFF_SIMPLEX			0x0800	/* doesn't receive own transmissions */
#define IFF_LINK			0x1000	/* has link */
#define IFF_AUTO_CONFIGURED	0x2000
#define IFF_CONFIGURING		0x4000
#define IFF_MULTICAST		0x8000	/* supports multicast */

/* interface alias flags */
#define IFAF_AUTO_CONFIGURED	0x0001	/* has been automatically configured */
#define IFAF_CONFIGURING		0x0002	/* auto configuration in progress */


/* used with SIOCGIFCOUNT, and SIOCGIFCONF */
struct ifconf {
	int				ifc_len;	/* size of buffer */
	union {
		void*		ifc_buf;
		struct ifreq* ifc_req;
		int			ifc_value;
	};
};

/* Macro that returns the size of a single address within a SIOCGIFCONF buffer;
   it looks like this because of compatibility with other platforms. */
#define _SIZEOF_ADDR_IFREQ(request) \
	(IF_NAMESIZE + (request).ifr_addr.sa_len > (int)sizeof(struct ifreq) \
		? IF_NAMESIZE + (request).ifr_addr.sa_len : sizeof(struct ifreq))


/* POSIX definitions follow */

struct if_nameindex {
	unsigned		if_index;	/* positive interface index */
	char*			if_name;	/* interface name, ie. "loopback" */
};


#ifdef __cplusplus
extern "C" {
#endif

unsigned if_nametoindex(const char* name);
char* if_indextoname(unsigned interfaceIndex, char* nameBuffer);
struct if_nameindex* if_nameindex(void);
void if_freenameindex(struct if_nameindex* interfaceArray);

#ifdef __cplusplus
}
#endif


#endif	/* _NET_IF_H */
