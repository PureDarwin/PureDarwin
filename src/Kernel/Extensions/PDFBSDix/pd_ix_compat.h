// pd_ix_compat.h - freebsd kpi bits ix uses that the haiku compat layer lacks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_IX_COMPAT_H
#define PD_IX_COMPAT_H


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
