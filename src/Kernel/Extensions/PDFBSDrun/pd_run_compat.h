// SPDX-License-Identifier: MIT
// freebsd kpi calls run uses that the haiku compat layer lacks, force-included into the driver
#ifndef PD_RUN_COMPAT_H
#define PD_RUN_COMPAT_H

#include <sys/eventhandler.h>

// usbdi.h's event for a newly configured device, run hooks it for dongles that need an eject
struct usb_device;
struct usb_attach_arg;
typedef void (*usb_dev_configured_fn)(void *, struct usb_device *, struct usb_attach_arg *);
EVENTHANDLER_DECLARE(usb_dev_configured, usb_dev_configured_fn);

// a u32 is an unsigned int here
#ifndef SYSCTL_ADD_U32
#define SYSCTL_ADD_U32(ctx, parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_ADD_UINT(ctx, parent, nbr, name, access, ptr, val, descr)
#endif

#endif
