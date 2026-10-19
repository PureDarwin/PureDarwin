/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Augustin Cavalier <waddlesplash>
 */
#ifndef _FBSD_COMPAT_USB_HAIKU_H_
#define _FBSD_COMPAT_USB_HAIKU_H_

#include <sys/cdefs.h>

/* Default USB configuration */
#define	USB_HAVE_UGEN 1
#define	USB_HAVE_DEVCTL 1
#define	USB_HAVE_BUSDMA 1
#define	USB_HAVE_COMPAT_LINUX 0
#define	USB_HAVE_USER_IO 1
#define	USB_HAVE_MBUF 1
#define	USB_HAVE_TT_SUPPORT 1
#define	USB_HAVE_POWERD 1
#define	USB_HAVE_MSCTEST 1
#define	USB_HAVE_MSCTEST_DETACH 1
#define	USB_HAVE_PF 1
#define	USB_HAVE_ROOT_MOUNT_HOLD 1
#define	USB_HAVE_ID_SECTION 0
#define	USB_HAVE_PER_BUS_PROCESS 1
#define	USB_HAVE_FIXED_ENDPOINT 0
#define	USB_HAVE_FIXED_IFACE 0
#define	USB_HAVE_FIXED_CONFIG 0
#define	USB_HAVE_FIXED_PORT 0
#define	USB_HAVE_DISABLE_ENUM 1

/* define zero ticks callout value */
#define	USB_CALLOUT_ZERO_TICKS 1

#define	USB_TD_GET_PROC(td) (td)->td_proc
#define	USB_PROC_GET_GID(td) (td)->p_pgid

#if (!defined(USB_HOST_ALIGN)) || (USB_HOST_ALIGN <= 0)
/* Use default value. */
#undef USB_HOST_ALIGN
#if defined(__arm__) || defined(__mips__) || defined(__powerpc__)
#define USB_HOST_ALIGN	32		/* Arm and MIPS need at least this much, if not more */
#else
#define	USB_HOST_ALIGN    8		/* bytes, must be power of two */
#endif
#endif
/* Sanity check for USB_HOST_ALIGN: Verify power of two. */
#if ((-USB_HOST_ALIGN) & USB_HOST_ALIGN) != USB_HOST_ALIGN
#error "USB_HOST_ALIGN is not power of two."
#endif
#define	USB_FS_ISOC_UFRAME_MAX 4	/* exclusive unit */
#define	USB_BUS_MAX 256			/* units */
#define	USB_MAX_DEVICES 128		/* units */
#define	USB_CONFIG_MAX 65535		/* bytes */
#define	USB_IFACE_MAX 32		/* units */
#define	USB_FIFO_MAX 128		/* units */
#define	USB_MAX_EP_STREAMS 8		/* units */
#define	USB_MAX_EP_UNITS 32		/* units */
#define	USB_MAX_PORTS 255		/* units */

#define	USB_MAX_FS_ISOC_FRAMES_PER_XFER (120)	/* units */
#define	USB_MAX_HS_ISOC_FRAMES_PER_XFER (8*120)	/* units */

#define	USB_HUB_MAX_DEPTH	5
#define	USB_EP0_BUFSIZE		1024	/* bytes */
#define	USB_CS_RESET_LIMIT	20	/* failures = 20 * 50 ms = 1sec */

#define	USB_MAX_AUTO_QUIRK	8	/* maximum number of dynamic quirks */

#define USB_IN_POLLING_MODE_FUNC() 0

typedef uint32_t usb_timeout_t;		/* milliseconds */
typedef uint32_t usb_frlength_t;	/* bytes */
typedef uint32_t usb_frcount_t;		/* units */
typedef uint32_t usb_size_t;		/* bytes */
typedef uint32_t usb_ticks_t;		/* system defined */
typedef uint16_t usb_power_mask_t;	/* see "USB_HW_POWER_XXX" */
typedef uint16_t usb_stream_t;		/* stream ID */

#endif // _FBSD_COMPAT_USB_HAIKU_H_
