/*
 * Copyright 2006-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef NETINET_TCP_H
#define NETINET_TCP_H


#include <endian.h>
#include <stdint.h>


struct tcphdr {
	uint16_t	th_sport;	/* source port */
	uint16_t	th_dport;	/* destination port */
	uint32_t	th_seq;
	uint32_t	th_ack;

#if BIG_ENDIAN
	uint8_t		th_off : 4;	/* data offset */
	uint8_t		th_x2 : 4;	/* unused */
#else
	uint8_t		th_x2 : 4;
	uint8_t		th_off : 4;
#endif
	uint8_t		th_flags;
	uint16_t	th_win;
	uint16_t	th_sum;
	uint16_t	th_urp;		/* end of urgent data offset */
} _PACKED;

#if 0
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_ECE	0x40
#define	TH_CWR	0x80

#define	TCPOPT_EOL				0
#define	TCPOPT_NOP				1
#define	TCPOPT_MAXSEG			2
#define	TCPOPT_WINDOW			3
#define	TCPOPT_SACK_PERMITTED	4
#define	TCPOPT_SACK				5
#define	TCPOPT_TIMESTAMP		8
#define	TCPOPT_SIGNATURE		19

#define	MAX_TCPOPTLEN			40
	/* absolute maximum TCP options length */

#define	TCPOLEN_MAXSEG			4
#define	TCPOLEN_WINDOW			3
#define	TCPOLEN_SACK			8
#define	TCPOLEN_SACK_PERMITTED	2
#define	TCPOLEN_TIMESTAMP		10
#define	TCPOLEN_TSTAMP_APPA		(TCPOLEN_TIMESTAMP + 2)
	/* see RFC 1323, appendix A */
#define	TCPOLEN_SIGNATURE		18

#define TCPOPT_TSTAMP_HDR \
    (TCPOPT_NOP << 24 | TCPOPT_NOP << 16 | TCPOPT_TIMESTAMP << 8 | TCPOLEN_TIMESTAMP)

#define TCP_MSS					536
#define TCP_MAXWIN				65535
#define TCP_MAX_WINSHIFT		14

#endif

/* options that can be set using setsockopt() and level IPPROTO_TCP */

#define	TCP_NODELAY				0x01
	/* don't delay sending smaller packets */
#define	TCP_MAXSEG				0x02
	/* set maximum segment size */
#define TCP_NOPUSH				0x04
	/* don't use TH_PUSH */
#define TCP_NOOPT				0x08
	/* don't use any TCP options */

#endif	/* NETINET_TCP_H */
