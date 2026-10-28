// SPDX-License-Identifier: MIT
// usb_msctest.h for run, the mass-storage eject that switches some dongles out of their cd-rom mode
#ifndef PD_USB_MSCTEST_H
#define PD_USB_MSCTEST_H

#include <sys/errno.h>

enum {
	MSC_EJECT_STOPUNIT,
	MSC_EJECT_REZERO,
	MSC_EJECT_ZTESTOR,
	MSC_EJECT_CMOTECH,
	MSC_EJECT_HUAWEI,
	MSC_EJECT_TCT,
};

// no mode switch here, a dongle that needs it stays in its storage mode
static inline int
usb_msc_eject(struct usb_device *udev, uint8_t iface_index, int method)
{
	return (ENXIO);
}

#endif
