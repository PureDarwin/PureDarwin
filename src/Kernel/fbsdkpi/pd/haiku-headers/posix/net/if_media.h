/*
 * Copyright 2007-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NET_IF_MEDIA_H
#define _NET_IF_MEDIA_H


/* bits		usage
 * ----		-----
 * 0-4		Media subtype
 * 5-7		Media type
 * 8-15		Type specific options
 * 16-31	General options
 */

/* Media types */

#define	IFM_ETHER		0x00000020	/* Ethernet */
#define	IFM_TOKEN		0x00000040	/* Token Ring */
#define	IFM_FDDI		0x00000060	/* Fiber Distributed Data Interface */
#define	IFM_IEEE80211	0x00000080	/* Wireless IEEE 802.11 */
#define IFM_ATM			0x000000a0
#define	IFM_CARP		0x000000c0	/* Common Address Redundancy Protocol */

/* Media subtypes */

/* Ethernet */
#define IFM_AUTO		0
#define	IFM_10_T		3			/* 10Base-T - RJ45 */
#define	IFM_100_TX		6			/* 100Base-TX - RJ45 */
#define IFM_1000_SX		11			/* 1000Base-SX - Fiber Optic */
#define IFM_1000_T		16			/* 1000Base-T - RJ45 */
#define IFM_10G_LR		18			/* 10GBase-LR - Fiber 1310nm */
#define IFM_10G_SR		19			/* 10GBase-SR - Fiber 850nm */
#define IFM_10G_TWINAX		22			/* 10GBase-CR (DAC) */
#define IFM_10G_LRM		24			/* 10GBase-LRM Fiber Optic */
#define IFM_UNKNOWN		25			/* media types not defined yet */
#define IFM_10G_T		26			/* 10GBase-T - RJ45 */

/* General options */

#define	IFM_FULL_DUPLEX	0x00100000	/* Full duplex */
#define	IFM_HALF_DUPLEX	0x00200000	/* Half duplex */
#define	IFM_LOOP		0x00400000	/* hardware in loopback */
#define	IFM_ACTIVE		0x00800000	/* Media link is active */

/* Masks */

#define	IFM_NMASK		0x000000e0	/* Media type */
#define	IFM_TMASK		0x0000001f	/* Media subtype */
#define	IFM_OMASK		0x0000ff00	/* Type specific options */
#define	IFM_GMASK		0x0ff00000	/* Generic options */

/* Macros for the masks */

#define	IFM_TYPE(x)		((x) & IFM_NMASK)
#define	IFM_SUBTYPE(x)	((x) & IFM_TMASK)
#define	IFM_TYPE_OPTIONS(x)	\
						((x) & IFM_OMASK)
#define	IFM_OPTIONS(x)	((x) & (IFM_OMASK | IFM_GMASK))

#endif	/* _NET_IF_MEDIA_H */
