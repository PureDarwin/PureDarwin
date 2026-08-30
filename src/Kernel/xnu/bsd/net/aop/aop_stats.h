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

#ifndef _NET_AOP_STATS_H_
#define _NET_AOP_STATS_H_

/* ip stats definitions */
#define AOP_IP_STATS_TABLE(X)       \
	/* Input stats */      \
	X(AOP_IP_STATS_TOTAL,               "TotalRcvd",    "\t%llu total packet received\n")       \
	X(AOP_IP_STATS_BADSUM,              "BadCsum",      "\t\t%llu bad header checksum\n")       \
	X(AOP_IP_STATS_TOOSMALL,            "DataTooSmall", "\t\t%llu with size smaller than minimum\n")\
	X(AOP_IP_STATS_TOOSHORT,            "PktTooShort",  "\t\t%llu with data size < data length\n")      \
	X(AOP_IP_STATS_ADJ,                 "TotalAdj",     "\t\t%llu with data size > data length\n")      \
	X(AOP_IP_STATS_TOOLONG,             "TooLong",      "\t\t%llu with ip length > max ip packet size\n")       \
	X(AOP_IP_STATS_BADHLEN,             "BadHdrLen",    "\t\t%llu with header length < data size\n")    \
	X(AOP_IP_STATS_BADLEN,              "BadLen",       "\t\t%llu with data length < header length\n")  \
	X(AOP_IP_STATS_BADOPTIONS,          "BadOptions",   "\t\t%llu with bad options\n")  \
	X(AOP_IP_STATS_BADVERS,             "BadVer",       "\t\t%llu with incorrect version number\n")     \
	X(AOP_IP_STATS_FRAGMENTS,           "FragRcvd",     "\t\t%llu fragment received\n") \
	X(AOP_IP_STATS_FRAGDROPPED,         "FragDrop",     "\t\t\t%llu dropped (dup or out of space)\n")   \
	X(AOP_IP_STATS_FRAGTIMEOUT,         "FragTimeO",    "\t\t\t%llu dropped after timeout\n")   \
	X(AOP_IP_STATS_REASSEMBLED,         "Reassembled",  "\t\t\t%llu reassembled ok\n")  \
	X(AOP_IP_STATS_DELIVERED,           "Delivered",    "\t\t%llu packet for this host\n")      \
	X(AOP_IP_STATS_NOPROTO,             "UnkwnProto",   "\t\t%llu packet for unknown/unsupported protocol\n")   \
	/* Output stats */      \
	X(AOP_IP_STATS_LOCALOUT,            "LocalOut",     "\t%llu packet sent from this host\n")  \
	X(AOP_IP_STATS_ODROPPED,            "DropNoBuf",    "\t\t%llu output packet dropped due to no bufs, etc.\n")        \
	X(AOP_IP_STATS_NOROUTE,             "NoRoute",      "\t\t%llu output packet discarded due to no route\n")   \
	X(AOP_IP_STATS_FRAGMENTED,          "Fragmented",   "\t\t%llu output datagram fragmented\n")        \
	X(AOP_IP_STATS_OFRAGMENTS,          "OutFraged",    "\t\t%llu fragment created\n")  \
	X(AOP_IP_STATS_CANTFRAG,            "CantFrag",     "\t\t%llu datagram that can't be fragmented\n") \
	X(__AOP_IP_STATS_MAX,               "",             "end of ip stats")

/* ipv6 stats definitions */
#define AOP_IP6_STATS_TABLE(X)      \
	/* Input Stats */       \
	X(AOP_IP6_STATS_TOTAL,              "TotalRcvd",    "\t%llu total packet received\n")       \
	X(AOP_IP6_STATS_TOOSMALL,           "DataTooSmall", "\t\t%llu with size smaller than minimum\n")    \
	X(AOP_IP6_STATS_TOOSHORT,           "PktTooShort",  "\t\t%llu with data size < data length\n")      \
	X(AOP_IP6_STATS_ADJ,                "TotalAdj",     "\t\t%llu with data size > data length\n")      \
	X(AOP_IP6_STATS_BADOPTIONS,         "BadOptions",   "\t\t%llu with bad options\n")  \
	X(AOP_IP6_STATS_BADVERS,            "BadVer",       "\t\t%llu with incorrect version number\n")     \
	X(AOP_IP6_STATS_FRAGMENTS,          "FrafRcvd",     "\t\t%llu fragment received\n") \
	X(AOP_IP6_STATS_FRAGDROPPED,        "FragDrop",     "\t\t\t%llu dropped (dup or out of space)\n")   \
	X(AOP_IP6_STATS_FRAGTIMEOUT,        "FragTimeO",    "\t\t\t%llu dropped after timeout\n")   \
	X(AOP_IP6_STATS_FRAGOVERFLOW,       "FragOverFlow", "\t\t\t%llu exceeded limit\n")  \
	X(AOP_IP6_STATS_REASSEMBLED,        "FragReassembled","\t\t\t%llu reassembled ok\n")                \
	X(AOP_IP6_STATS_DELIVERED,          "Delivered",    "\t\t%llu packet for this host\n")      \
	X(AOP_IP6_STATS_TOOMANYHDR,         "TooManyHdr",   "\t\t%llu packet discarded due to too may headers\n")   \
	/* Output stats */      \
	X(AOP_IP6_STATS_LOCALOUT,           "LocalOut",     "\t%llu packet sent from this host\n")  \
	X(AOP_IP6_STATS_ODROPPED,           "DropNoBuf",    "\t\t%llu output packet dropped due to no bufs, etc.\n")        \
	X(AOP_IP6_STATS_NOROUTE,            "NoRoute",      "\t\t%llu output packet discarded due to no route\n")   \
	X(AOP_IP6_STATS_FRAGMENTED,         "Fragmented",   "\t\t%llu output datagram fragmented\n")        \
	X(AOP_IP6_STATS_OFRAGMENTS,         "OutFraged",    "\t\t%llu fragment created\n")  \
	X(AOP_IP6_STATS_CANTFRAG,           "CantFrag",     "\t\t%llu datagram that can't be fragmented\n")\
	X(__AOP_IP6_STATS_MAX,              "",             "end of ipv6 stats")

/* tcp stats definitions */
#define AOP_TCP_STATS_TABLE(X)      \
	/* Output stats */      \
	X(AOP_TCP_STATS_SNDTOTAL,           "SndTotalPkt",  "\t%llu packet sent\n") \
	X(AOP_TCP_STATS_SNDPACK,            "SndTotalDP",   "\t\t%llu data packet") \
	X(AOP_TCP_STATS_SNDBYTE,            "SndDataByte",  " (%llu byte)\n")       \
	X(AOP_TCP_STATS_SNDREXMITPACK,      "SndDPktReXmt", "\t\t%llu data packet retransmitted")   \
	X(AOP_TCP_STATS_SNDREXMITBYTE,      "SndDByteReXmt"," (%llu byte)\n")       \
	X(AOP_TCP_STATS_MTURESENT,          "MTUReSnd",     "\t\t%llu resend initiated by MTU discovery\n") \
	X(AOP_TCP_STATS_SNDACKS,            "SndAck",       "\t\t%llu ack-only packet")     \
	X(AOP_TCP_STATS_DELACK,             "DelayAck",     " (%llu delayed)\n")    \
	X(AOP_TCP_STATS_SNDURG,             "SndURG",       "\t\t%llu URG only packet\n")   \
	X(AOP_TCP_STATS_SNDPROBE,           "SndWinProb",   "\t\t%llu window probe packet\n")       \
	X(AOP_TCP_STATS_SNDWINUP,           "SndWinUpd",    "\t\t%llu window update packet\n")      \
	X(AOP_TCP_STATS_SNDCTRL,            "SndCtlPkt",    "\t\t%llu control packet\n")    \
	X(AOP_TCP_STATS_SYNCHALLENGE,       "SYNChallenge", "\t\t%llu challenge ACK sent due to unexpected SYN\n")  \
	X(AOP_TCP_STATS_RSTCHALLENGE,       "RSTChallenge", "\t\t%llu challenge ACK sent due to unexpected RST\n")  \
        \
	/* Input stats */       \
	X(AOP_TCP_STATS_RCVTOTAL,           "RcvTotalPkt",  "\t%llu packet received\n")     \
	X(AOP_TCP_STATS_RCVACKPACK,         "RcvAckPkt",    "\t\t%llu ack") \
	X(AOP_TCP_STATS_RCVACKBYTE,         "RcvAckByte",   " (for %llu byte)\n")   \
	X(AOP_TCP_STATS_RCVDUPACK,          "RcvDupAck",    "\t\t%llu duplicate ack\n")     \
	X(AOP_TCP_STATS_RCVACKTOOMUCH,      "RcvAckUnSnd",  "\t\t%llu ack for unsent data\n")       \
	X(AOP_TCP_STATS_RCVPACK,            "RcvPktInSeq",  "\t\t%llu packet received in-sequence") \
	X(AOP_TCP_STATS_RCVBYTE,            "RcvBInSeq",    " (%llu byte)\n")       \
	X(AOP_TCP_STATS_RCVDUPPACK,         "RcvDupPkt",    "\t\t%llu completely duplicate packet") \
	X(AOP_TCP_STATS_RCVDUPBYTE,         "RcvDupByte",   " (%llu byte)\n")       \
	X(AOP_TCP_STATS_PAWSDROP,           "PAWSDrop",     "\t\t%llu old duplicate packet\n")      \
	X(AOP_TCP_STATS_RCVMEMDROP,         "RcvMemDrop",   "\t\t%llu received packet dropped due to low memory\n") \
	X(AOP_TCP_STATS_RCVPARTDUPPACK,     "RcvDupData",   "\t\t%llu packet with some dup. data")  \
	X(AOP_TCP_STATS_RCVPARTDUPBYTE,     "RcvPDupByte",  " (%llu byte duped)\n") \
	X(AOP_TCP_STATS_RCVOOPACK,          "RcvOOPkt",     "\t\t%llu out-of-order packet") \
	X(AOP_TCP_STATS_RCVOOBYTE,          "RcvOOByte",    " (%llu byte)\n")               \
	X(AOP_TCP_STATS_RCVPACKAFTERWIN,    "RcvAftWinPkt", "\t\t%llu packet of data after window") \
	X(AOP_TCP_STATS_RCVBYTEAFTERWIN,    "RcvAftWinByte"," (%llu byte)\n")               \
	X(AOP_TCP_STATS_RCVWINPROBE,        "RcvWinProbPkt","\t\t%llu window probe\n")              \
	X(AOP_TCP_STATS_RCVWINUPD,          "RcvWinUpdPkt", "\t\t%llu window update packet\n")              \
	X(AOP_TCP_STATS_RCVAFTERCLOSE,      "RcvAftCloPkt", "\t\t%llu packet received after close\n")       \
	X(AOP_TCP_STATS_BADRST,             "BadRST",       "\t\t%llu bad reset\n") \
	X(AOP_TCP_STATS_RCVBADSUM,          "RcvBadCsum",   "\t\t%llu discarded for bad checksum\n")        \
	X(AOP_TCP_STATS_RCVBADOFF,          "RcvBadOff",    "\t\t%llu discarded for bad header offset field\n")     \
	X(AOP_TCP_STATS_RCVSHORT,           "RcvTooShort",  "\t\t%llu discarded because packet too short\n")        \
	X(AOP_TCP_STATS_CONNATTEMPT,        "ConnInit",     "\t\t%llu discarded because packet too short\n")        \
        \
	/* Connection stats */  \
	X(AOP_TCP_STATS_CONNECTS,           "ConnEst",      "\t%llu connection established (including accepts)\n")  \
	X(AOP_TCP_STATS_CLOSED,             "ConnClosed",   "\t%llu connection closed")     \
	X(AOP_TCP_STATS_DROPS,              "ConnDrop",     " (including %llu drop)\n")     \
	X(AOP_TCP_STATS_RTTUPDATED,         "RTTUpdated",   "\t%llu segment updated rtt")   \
	X(AOP_TCP_STATS_SEGSTIMED,          "RTTTimed",     " (of %llu attempt)\n") \
	X(AOP_TCP_STATS_REXMTTIMEO,         "ReXmtTO",      "\t%llu retransmit timeout\n")  \
	X(AOP_TCP_STATS_TIMEOUTDROP,        "DropTO",       "\t\t%llu connection dropped by rexmit timeout\n")      \
	X(AOP_TCP_STATS_RXTFINDROP,         "ReXmtFINDrop", "\t\t%llu connection dropped after retransmitting FIN\n")       \
	X(AOP_TCP_STATS_PERSISTTIMEO,       "PersistTO",    "\t%llu persist timeout\n")                     \
	X(AOP_TCP_STATS_PERSISTDROP,        "PersisStateTO","\t\t%llu connection dropped by persist timeout\n")     \
	X(AOP_TCP_STATS_KEEPTIMEO,          "KATO",         "\t%llu keepalive timeout\n")   \
	X(AOP_TCP_STATS_KEEPPROBE,          "KAProbe",      "\t\t%llu keepalive probe sent\n")      \
	X(AOP_TCP_STATS_KEEPDROPS,          "KADrop",       "\t\t%llu connection dropped by keepalive\n")   \
        \
	/* SACK/RACK related stats */        \
	X(AOP_TCP_STATS_SACK_RECOVERY_EPISODE,      "SACKRecEpi",   "\t%llu SACK recovery episode\n")       \
	X(AOP_TCP_STATS_SACK_REXMITS,               "SACKReXmt",    "\t%llu segment rexmit in SACK recovery episodes\n")    \
	X(AOP_TCP_STATS_SACK_REXMIT_BYTES,          "SACKReXmtB",   "\t%llu byte rexmit in SACK recovery episodes\n")       \
	X(AOP_TCP_STATS_SACK_RCV_BLOCKS,            "SACKRcvBlk",   "\t%llu SACK option (SACK blocks) received\n")  \
	X(AOP_TCP_STATS_SACK_SEND_BLOCKS,           "SACKSntBlk",   "\t%llu SACK option (SACK blocks) sent\n")      \
	X(AOP_TCP_STATS_SACK_SBOVERFLOW,            "SACKSndBlkOF", "\t%llu SACK scoreboard overflow\n")    \
        \
	X(AOP_TCP_STATS_LIMITED_TXT,                "LimitedXmt",   "\t%llu limited transmit done\n")               \
	X(AOP_TCP_STATS_EARLY_REXMT,                "EarlyReXmt",   "\t%llu early retransmit done\n")               \
	X(AOP_TCP_STATS_SACK_ACKADV,                "SACKAdvAck",   "\t%llu time cumulative ack advanced along with SACK\n")        \
	X(AOP_TCP_STATS_PTO,                        "ProbTO",       "\t%llu probe timeout\n")               \
	X(AOP_TCP_STATS_RTO_AFTER_PTO,              "RTOAfProb",    "\t\t%llu time retransmit timeout triggered after probe\n")     \
	X(AOP_TCP_STATS_PROBE_IF,                   "ProbeIF",      "\t\t%llu time probe packets were sent for an interface\n")     \
	X(AOP_TCP_STATS_PROBE_IF_CONFLICT,          "ProbeIFConfl", "\t\t%llu time couldn't send probe packets for an interface\n") \
	X(AOP_TCP_STATS_TLP_RECOVERY,               "TLPFastRecvr", "\t\t%llu time fast recovery after tail loss\n")        \
	X(AOP_TCP_STATS_TLP_RECOVERLASTPKT,         "TLPRecvrLPkt", "\t\t%llu time recovered last packet \n")       \
	X(AOP_TCP_STATS_PTO_IN_RECOVERY,            "PTOInRecvr",   "\t\t%llu SACK based rescue retransmit\n")      \
        \
	/* DSACK related statistics */  \
	X(AOP_TCP_STATS_DSACK_SENT,                 "DSACKSnd",     "\t%llu time DSACK option was sent\n")  \
	X(AOP_TCP_STATS_DSACK_RECVD,                "DSACKRcv",     "\t\t%llu time DSACK option was received\n")    \
	X(AOP_TCP_STATS_DSACK_DISABLE,              "DSACKDisable", "\t\t%llu time DSACK was disabled on a connection\n")   \
	X(AOP_TCP_STATS_DSACK_BADREXMT,             "DSACKBadReXmt","\t\t%llu time recovered from bad retransmission using DSACK\n")        \
	X(AOP_TCP_STATS_DSACK_ACKLOSS,              "DSACKAckLoss", "\t\t%llu time ignored DSACK due to ack loss\n")        \
	X(AOP_TCP_STATS_DSACK_RECVD_OLD,            "DSACKRcvOld",  "\t\t%llu time ignored old DSACK options\n")    \
	X(AOP_TCP_STATS_PMTUDBH_REVERTED,           "PMTUDBHRevert","\t%llu time PMTU Blackhole detection, size reverted\n")        \
	X(AOP_TCP_STATS_DROP_AFTER_SLEEP,           "DropAPSleep",  "\t%llu connection were dropped after long sleep\n")    \
	X(__AOP_TCP_STATS_MAX,                      "",             "end of tcp stats")

#define AOP_UDP_STATS_TABLE(X)                                              \
	/* Input stats */       \
	X(AOP_UDP_STATS_IPACKETS,                   "RcvPkt",               "\t%llu datagram received\n")   \
	X(AOP_UDP_STATS_HDROPS,                     "HdrDrop",              "\t\t%llu with incomplete header\n")    \
	X(AOP_UDP_STATS_BADSUM,                     "BadCsum",              "\t\t%llu with bad data length field\n")        \
	X(AOP_UDP_STATS_BADLEN,                     "BadLen",               "\t\t%llu with bad checksum\n") \
	X(AOP_UDP_STATS_NOSUM,                      "NoCsum",               "\t\t%llu with no checksum\n")  \
	X(AOP_UDP_STATS_NOPORT,                     "NoPort",               "\t\t%llu dropped due to no socket\n")  \
	X(AOP_UDP_STATS_FULLSOCK,                   "FullSock",             "\t\t%llu dropped due to full socket buffers\n")        \
        \
	/* Output stats */      \
	X(AOP_UDP_STATS_OPACKETS,                   "SndPkt",               "\t%llu datagram output\n")     \
        \
	X(__AOP_UDP_STATS_MAX,                      "",                     "end of UDP stats")

#define AOP_DRIVER_STATS_TABLE(X)                                              \
	/* AOP driver stats */       \
	X(AOP_DRIVER_STATS_TXDROP,                  "TxDrop",               "\t%llu total Tx dropped\n")  \
	X(AOP_DRIVER_STATS_TXPENDING,               "TxPending",            "\t%llu total pending Tx not completed\n")  \
	X(AOP_DRIVER_STATS_RXDROP,                  "RxDrop",               "\t%llu total Rx dropped\n")  \
	X(AOP_DRIVER_STATS_RXPENDING,               "RxPending",            "\t%llu total pending Rx not completed\n")  \
	X(__AOP_DRIVER_STATS_MAX,                   "",                     "end of driver stats")

/*
 * Common stats operation and macro
 */
#define EXPAND_TO_ENUMERATION(a, b, c) a,
#define EXPAND_TO_STRING(a, b, c) b,
#define EXPAND_TO_FORMAT(a, b, c) c,

#define DEFINE_STATS_STR_FUNC(type, table)                      \
__attribute__((always_inline))                                  \
static inline const char *                                      \
type##_str(enum _##type value)                                  \
{                                                               \
	static const char *table[] = {                          \
	    table(EXPAND_TO_STRING)                             \
	};                                                      \
	return (table[value]);                                  \
}

#define DEFINE_STATS_FMT_FUNC(type, table)                      \
__attribute__((always_inline))                                  \
static inline const char *                                      \
type##_fmt(enum _##type value)                                  \
{                                                               \
	static const char *table[] = {                          \
	    table(EXPAND_TO_FORMAT)                             \
	};                                                      \
	return (table[value]);                                  \
}

#define STATS_ALIGN 16  /* align for vector instruction */

#define STATS_REGISTER(name, NAME)                      \
enum _##name { NAME##_TABLE(EXPAND_TO_ENUMERATION) };   \
struct name {                                           \
	uint64_t        _arr[__##NAME##_MAX];           \
} __attribute__((aligned(STATS_ALIGN)));                \
DEFINE_STATS_STR_FUNC(name, NAME##_TABLE)               \
DEFINE_STATS_FMT_FUNC(name, NAME##_TABLE)

/* Stats registration stub */
STATS_REGISTER(aop_ip_stats, AOP_IP_STATS);
STATS_REGISTER(aop_ip6_stats, AOP_IP6_STATS);
STATS_REGISTER(aop_tcp_stats, AOP_TCP_STATS);
STATS_REGISTER(aop_udp_stats, AOP_UDP_STATS);
STATS_REGISTER(aop_driver_stats, AOP_DRIVER_STATS);

#undef  STATS_REGISTER
#undef  DEFINE_STATS_STR_FUNC
#undef  EXPAND_TO_STRING
#undef  EXPAND_TO_ENUMERATION

#define NET_AOP_PROTOCOL_STATS    "net.aop.protocol_stats"
#define NET_AOP_DRIVER_STATS      "net.aop.driver_stats"
#define NET_AOP_ACTIVITY_BITMAP   "net.aop.proc_activity_bitmaps"

struct net_aop_protocol_stats {
	struct aop_ip_stats aop_ip;
	struct aop_ip6_stats aop_ip6;
	struct aop_tcp_stats aop_tcp;
	struct aop_udp_stats aop_udp;
};

struct net_aop_global_stats {
	struct net_aop_protocol_stats aop_proto_stats;
	struct aop_driver_stats aop_driver;
}__attribute__((aligned(64)));

struct aop_activity_bitmap {
	/*
	 * `start` maintains the start time of the
	 * bitmap. The value is set based on
	 * mach_continuous_time().
	 */
	uint64_t start;
	/*
	 * AOP maintains a larger bitmap to track
	 * state when AP goes to sleep. A bitmap of
	 * size 8 allows tracking network activity for
	 * more than 60 mins.
	 */
	uint64_t bitmap[8];
};

#define AOP_MAX_PROC_BUNDLE_ID_LEN 256
struct aop_proc_activity_bitmap {
	char proc_bundle_id[AOP_MAX_PROC_BUNDLE_ID_LEN];
	struct aop_activity_bitmap wifi_bitmap;
	struct aop_activity_bitmap cell_bitmap;
};

#endif /*_NET_AOP_STATS_H_*/
