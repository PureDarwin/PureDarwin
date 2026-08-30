/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bpf.h	8.1 (Berkeley) 6/10/93
 *	@(#)bpf.h	1.34 (LBL)     6/16/96
 *
 * $FreeBSD: src/sys/net/bpf.h,v 1.21.2.3 2001/08/01 00:23:13 fenner Exp $
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2006 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#ifndef _NET_BPF_PRIVATE_H_
#define _NET_BPF_PRIVATE_H_

#include <net/bpf.h>

#if !defined(DRIVERKIT)
#include <net/if_var.h>
#include <uuid/uuid.h>

struct bpf_setup_args {
	uuid_t  bsa_uuid;
	char    bsa_ifname[IFNAMSIZ];
};

#ifdef KERNEL_PRIVATE
/*
 * LP64 version of bpf_program.  all pointers
 * grow when we're dealing with a 64-bit process.
 * WARNING - keep in sync with bpf_program
 */
struct bpf_program64 {
	u_int           bf_len;
	user64_addr_t   bf_insns __attribute__((aligned(8)));
};

struct bpf_program32 {
	u_int           bf_len;
	user32_addr_t   bf_insns;
};
#endif /* KERNEL_PRIVATE */

struct bpf_comp_stats {
	uint64_t bcs_total_read; /* number of packets read from device */
	uint64_t bcs_total_size; /* total size of filtered packets */
	uint64_t bcs_total_hdr_size; /* total header size of captured packets */
	uint64_t bcs_count_no_common_prefix; /* count of packets not compressible */
	uint64_t bcs_count_compressed_prefix; /* count of compressed packets */
	uint64_t bcs_total_compressed_prefix_size; /* total size of compressed data */
	uint64_t bcs_max_compressed_prefix_size; /* max compressed data size */
};

#ifdef KERNEL_PRIVATE
#define BIOCSETF64      _IOW('B',103, struct bpf_program64)
#define BIOCSETF32      _IOW('B',103, struct bpf_program32)
#define BIOCSRTIMEOUT64 _IOW('B',109, struct user64_timeval)
#define BIOCSRTIMEOUT32 _IOW('B',109, struct user32_timeval)
#define BIOCGRTIMEOUT64 _IOR('B',110, struct user64_timeval)
#define BIOCGRTIMEOUT32 _IOR('B',110, struct user32_timeval)
#endif /* KERNEL_PRIVATE */
#define BIOCGETTC       _IOR('B', 122, int)
#define BIOCSETTC       _IOW('B', 123, int)
#define BIOCSEXTHDR     _IOW('B', 124, u_int)
#define BIOCGIFATTACHCOUNT      _IOWR('B', 125, struct ifreq)
#ifdef KERNEL_PRIVATE
#define BIOCSETFNR64    _IOW('B',126, struct bpf_program64)
#define BIOCSETFNR32    _IOW('B',126, struct bpf_program32)
#endif /* KERNEL_PRIVATE */
#define BIOCGWANTPKTAP  _IOR('B', 127, u_int)
#define BIOCSWANTPKTAP  _IOWR('B', 127, u_int)
#define BIOCSHEADDROP   _IOW('B', 128, int)
#define BIOCGHEADDROP   _IOR('B', 128, int)
#define BIOCSTRUNCATE   _IOW('B', 129, u_int)
#define BIOCGETUUID     _IOR('B', 130, uuid_t)
#define BIOCSETUP       _IOW('B', 131, struct bpf_setup_args)
#define BIOCSPKTHDRV2   _IOW('B', 132, int)
#define BIOCGPKTHDRV2   _IOW('B', 133, int)
#define BIOCGHDRCOMP    _IOR('B', 134, int)
#define BIOCSHDRCOMP    _IOW('B', 135, int)
#define BIOCGHDRCOMPSTATS    _IOR('B', 136, struct bpf_comp_stats)
#define BIOCGHDRCOMPON  _IOR('B', 137, int)
#define BIOCGDIRECTION  _IOR('B', 138, int)
#define BIOCSDIRECTION  _IOW('B', 139, int)
#define BIOCSWRITEMAX   _IOW('B', 140, u_int)
#define BIOCGWRITEMAX   _IOR('B', 141, u_int)
#define BIOCGBATCHWRITE _IOR('B', 142, int)
#define BIOCSBATCHWRITE _IOW('B', 143, int)
#define BIOCGNOTSTAMP   _IOR('B', 144, int)
#define BIOCSNOTSTAMP   _IOW('B', 145, int)
#define BIOCGDVRTIN     _IOR('B', 146, int)
#define BIOCSDVRTIN     _IOW('B', 146, int)

/*
 * This structure must be a multiple of 4 bytes.
 * It includes padding and spare fields that we can use later if desired.
 */
struct bpf_hdr_ext {
	struct BPF_TIMEVAL bh_tstamp;   /* time stamp */
	bpf_u_int32     bh_caplen;      /* length of captured portion */
	bpf_u_int32     bh_datalen;     /* original length of packet */
	u_short         bh_hdrlen;      /* length of bpf header */
	u_char          bh_complen;
	u_char          bh_flags;
#define BPF_HDR_EXT_FLAGS_DIR_IN        0x00
#define BPF_HDR_EXT_FLAGS_DIR_OUT       0x01
#ifdef BSD_KERNEL_PRIVATE
#define BPF_HDR_EXT_FLAGS_TCP           0x02
#define BPF_HDR_EXT_FLAGS_UDP           0x04
#endif /* BSD_KERNEL_PRIVATE */
	pid_t           bh_pid;         /* process PID */
	char            bh_comm[MAXCOMLEN + 1]; /* process command */
	u_char          bh_pktflags;
#define BPF_PKTFLAGS_TCP_REXMT  0x01
#define BPF_PKTFLAGS_START_SEQ  0x02
#define BPF_PKTFLAGS_LAST_PKT   0x04
#define BPF_PKTFLAGS_WAKE_PKT   0x08
#define BPF_PKTFLAGS_ULPN       0x10
	uint16_t        bh_trace_tag;
	bpf_u_int32     bh_svc;         /* service class */
	bpf_u_int32     bh_flowid;      /* kernel reserved; 0 in userland */
	bpf_u_int32     bh_unsent_bytes; /* unsent bytes at interface */
	bpf_u_int32     bh_unsent_snd; /* unsent bytes at socket buffer */
	bpf_u_int32     bh_comp_gencnt; /* unsent bytes at socket buffer */
};

#define BPF_HDR_EXT_HAS_TRACE_TAG 1
#define BPF_HDR_EXT_HAS_COMP_GENCNT 1

/*
 * External representation of the bpf descriptor
 */
struct xbpf_d {
	uint32_t        bd_structsize;  /* Size of this structure. */
	int32_t         bd_dev_minor;
	int32_t         bd_sig;
	uint32_t        bd_slen;
	uint32_t        bd_hlen;
	uint32_t        bd_bufsize;
	pid_t           bd_pid;

	uint8_t         bd_promisc;
	uint8_t         bd_immediate;
	uint8_t         bd_hdrcmplt;
	uint8_t         bd_async;

	uint8_t         bd_headdrop;
	uint8_t         bd_direction;
	uint8_t         bh_compreq;
	uint8_t         bh_compenabled;

	uint8_t         bd_exthdr;
	uint8_t         bd_trunc;
	uint8_t         bd_pkthdrv2;
	uint8_t         bd_batch_write : 1;
	uint8_t         bd_divert_in : 1;
	uint8_t         bd_padding : 6;

	uint64_t        bd_rcount;
	uint64_t        bd_dcount;
	uint64_t        bd_fcount;
	uint64_t        bd_wcount;
	uint64_t        bd_wdcount;

	char            bd_ifname[IFNAMSIZ];

	uint64_t        bd_comp_count;
	uint64_t        bd_comp_size;

	uint32_t        bd_scnt;        /* number of packets in store buffer */
	uint32_t        bd_hcnt;        /* number of packets in hold buffer */

	uint64_t        bd_read_count;
	uint64_t        bd_fsize;
};

#ifndef bd_seesent
/*
 * Code compatibility workaround so that old versions of network_cmds will continue to build
 * even if netstat -B shows an incorrect value.
 */
#define bd_seesent bd_direction
#endif /* bd_seesent */

#define _HAS_STRUCT_XBPF_D_ 2

struct bpf_comp_hdr {
	struct BPF_TIMEVAL bh_tstamp;   /* time stamp */
	bpf_u_int32     bh_caplen;      /* length of captured portion */
	bpf_u_int32     bh_datalen;     /* original length of packet */
	u_short         bh_hdrlen;      /* length of bpf header (this struct
	                                 *  plus alignment padding) */
	u_char          bh_complen;     /* data portion compressed */
	u_char          bh_padding;     /* data portion compressed */
};

#define HAS_BPF_HDR_COMP 1
#define BPF_HDR_COMP_LEN_MAX 255

/*
 * Packet tap directions
 */
#define BPF_D_NONE      0x0     /* See no packet (for writing only) */
#define BPF_D_IN        0x1     /* See incoming packets */
#define BPF_D_OUT       0x2     /* See outgoing packets */
#define BPF_D_INOUT     0x3     /* See incoming and outgoing packets */

#endif /* !defined(DRIVERKIT) */

/*
 * For Apple private usage
 */
#define DLT_USER0_APPLE_INTERNAL        DLT_USER0       /* rdar://12019509 */
#define DLT_USER1_APPLE_INTERNAL        DLT_USER1       /* rdar://12019509 */
#define DLT_PKTAP                       DLT_USER2       /* rdar://11779467 */
#define DLT_USER3_APPLE_INTERNAL        DLT_USER3       /* rdar://19614531 */
#define DLT_USER4_APPLE_INTERNAL        DLT_USER4       /* rdar://19614531 */

#if !defined(DRIVERKIT)
#ifdef KERNEL_PRIVATE
#define BPF_MIN_PKT_SIZE 40
#define PORT_DNS 53
#define PORT_BOOTPS 67
#define PORT_BOOTPC 68
#define PORT_ISAKMP 500
#define PORT_ISAKMP_NATT 4500   /* rfc3948 */

#define BPF_T_MICROTIME         0x0000  /* The default */
#define BPF_T_NONE              0x0003

/* Forward declerations */
struct ifnet;
struct mbuf;

#define BPF_PACKET_TYPE_MBUF    0
#if SKYWALK
#define BPF_PACKET_TYPE_PKT     1
#include <skywalk/os_skywalk.h>
#endif /* SKYWALK */

struct bpf_packet {
	int     bpfp_type;
	void *__sized_by(bpfp_header_length) bpfp_header; /* optional */
	size_t  bpfp_header_length;
	union {
		struct mbuf     *bpfpu_mbuf;
		void *          bpfpu_ptr;
#if SKYWALK
		kern_packet_t   bpfpu_pkt;
#define bpfp_pkt        bpfp_u.bpfpu_pkt
#endif /* SKYWALK */
	} bpfp_u;
#define bpfp_mbuf       bpfp_u.bpfpu_mbuf
#define bpfp_ptr        bpfp_u.bpfpu_ptr
	size_t  bpfp_total_length;      /* length including optional header */
};

extern int      bpf_validate(const struct bpf_insn *__counted_by(len), int len);
extern void     bpfdetach(struct ifnet *);
extern void     bpfilterattach(int);
extern u_int    bpf_filter(const struct bpf_insn *__counted_by(pc_len), u_int pc_len,
    u_char *__sized_by(sizeof(struct bpf_packet)), u_int wirelen, u_int);
#endif /* KERNEL_PRIVATE */

#endif /* !defined(DRIVERKIT) */

#ifdef KERNEL
#if SKYWALK
/*!
 *       @function bpf_tap_packet_in
 *       @discussion Call this function when your interface receives a
 *               packet. This function will check if any bpf devices need a
 *               a copy of the packet.
 *       @param interface The interface the packet was received on.
 *       @param dlt The data link type of the packet.
 *       @param packet The packet received.
 *       @param header An optional pointer to a header that will be prepended.
 *       @param header_len If the header was specified, the length of the header.
 */
extern void bpf_tap_packet_in(ifnet_t interface, u_int32_t dlt,
    kern_packet_t packet, void *__sized_by(header_len) header, size_t header_len);

/*!
 *       @function bpf_tap_packet_out
 *       @discussion Call this function when your interface transmits a
 *               packet. This function will check if any bpf devices need a
 *               a copy of the packet.
 *       @param interface The interface the packet was or will be transmitted on.
 *       @param dlt The data link type of the packet.
 *       @param packet The packet received.
 *       @param header An optional pointer to a header that will be prepended.
 *       @param header_len If the header was specified, the length of the header.
 */
extern void bpf_tap_packet_out(ifnet_t interface, u_int32_t dlt,
    kern_packet_t packet, void *__sized_by(header_len) header, size_t header_len);

#endif /* SKYWALK */
#endif /* KERNEL */

#endif /* _NET_BPF_PRIVATE_H_ */
