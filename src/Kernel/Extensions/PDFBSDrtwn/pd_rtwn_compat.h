// SPDX-License-Identifier: MIT
// freebsd kpi calls rtwn uses that the haiku compat layer lacks, force-included into the driver
#ifndef PD_RTWN_COMPAT_H
#define PD_RTWN_COMPAT_H

// a u32 is an unsigned int here
#define SYSCTL_ADD_U32(ctx, parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_ADD_UINT(ctx, parent, nbr, name, access, ptr, val, descr)

// usb.h's vendor-specific interface class, rtwn's id table matches on it
#ifndef UICLASS_VENDOR
#define UICLASS_VENDOR		0xff
#define UISUBCLASS_VENDOR	0xff
#endif

#endif
