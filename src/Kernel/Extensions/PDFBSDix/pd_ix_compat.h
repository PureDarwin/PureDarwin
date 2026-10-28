// pd_ix_compat.h - freebsd kpi bits ix uses that the haiku compat layer lacks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_IX_COMPAT_H
#define PD_IX_COMPAT_H

// net/rss_config.h's hash types, the receive hash ix programs into MRQC
#define RSS_HASHTYPE_RSS_IPV4           (1 << 1)
#define RSS_HASHTYPE_RSS_TCP_IPV4       (1 << 2)
#define RSS_HASHTYPE_RSS_IPV6           (1 << 3)
#define RSS_HASHTYPE_RSS_TCP_IPV6       (1 << 4)
#define RSS_HASHTYPE_RSS_IPV6_EX        (1 << 5)
#define RSS_HASHTYPE_RSS_TCP_IPV6_EX    (1 << 6)
#define RSS_HASHTYPE_RSS_UDP_IPV4       (1 << 7)
#define RSS_HASHTYPE_RSS_UDP_IPV6       (1 << 9)
#define RSS_HASHTYPE_RSS_UDP_IPV6_EX    (1 << 10)

// no kernel rss, so a random key and the tcp and ip hashes, as igc does
#define rss_getkey(key) arc4rand((key), 40, 0)
#define rss_gethashconfig() (RSS_HASHTYPE_RSS_IPV4 | RSS_HASHTYPE_RSS_TCP_IPV4 | \
    RSS_HASHTYPE_RSS_IPV6 | RSS_HASHTYPE_RSS_TCP_IPV6)

// sys/types.h's 32.32 fixed point time, only the vf mailbox deadlines use it
typedef long long sbintime_t;
#define SBT_1S ((sbintime_t)1 << 32)

// the u8, u16 and u32 sysctls, haiku's sysctl layer never runs their handlers
#define CTLTYPE_U8  CTLTYPE_UINT
#define CTLTYPE_U16 CTLTYPE_UINT
#define CTLTYPE_U32 CTLTYPE_UINT

// net/if_media.h's bidi fiber media, absent from haiku's copy
#define IFM_10G_BX  IFM_X(118)
#define IFM_1000_BX IFM_X(119)

#endif
