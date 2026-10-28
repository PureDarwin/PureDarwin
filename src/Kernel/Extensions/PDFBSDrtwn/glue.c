// SPDX-License-Identifier: MIT
// rtwn's usb attachment on the fbsdkpi usb shim, the radio sits on net80211

#include <sys/bus.h>

extern driver_t *DRIVER_MODULE_NAME(rtwn_usb, uhub);

HAIKU_FBSD_WLAN_DRIVERS_GLUE(rtwn);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES | FBSD_WLAN);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// a usb device has no interrupt line of its own, transfers complete through the usb shim
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 0;
}

void
__haiku_init_hardware()
{
	driver_t *drivers[] = {
		DRIVER_MODULE_NAME(rtwn_usb, uhub),
		NULL
	};
	_fbsd_init_hardware_uhub(drivers);
}
