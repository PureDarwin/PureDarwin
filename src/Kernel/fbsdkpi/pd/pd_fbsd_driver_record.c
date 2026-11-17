// SPDX-License-Identifier: MIT
// the record a driver hands the shared layer, from the names haiku's glue macros defined in it
#include <sys/param.h>
#include <sys/kernel.h>
#include <compat/sys/haiku-module.h>

#include "pd_fbsd_driver.h"
#include "pd_firmware.h"

// the xnu side of this driver, in PDFBSDEthernet.cpp
void pdos_net_input(void *cookie, const void *frame, size_t len);
void pdos_net_input_chain(void *cookie, void *chain);
void pdos_net_link(void *cookie, int up, uint64_t baudrate);
// a driver without firmware has no table
extern const struct pd_firmware_image pd_firmware_images[] __attribute__((weak));
extern const int pd_firmware_image_count __attribute__((weak));

// lkpi wifi drivers take these from pd_lkpi_wlan.c, the rest from HAIKU_FBSD_DRIVERS_GLUE
status_t init_wlan_stack(void);
void uninit_wlan_stack(void);
status_t start_wlan(device_t dev);
status_t stop_wlan(device_t dev);

SET_DECLARE(__freebsd_sysinit, struct sysinit);
SET_DECLARE(__freebsd_sysuninit, struct sysinit);

static struct pd_fbsd_driver sRecord;

struct pd_fbsd_driver *
pd_fbsd_driver_record(void)
{
	if (sRecord.name == NULL) {
		sRecord.name = gDriverName;
		sRecord.requirements = __haiku_driver_requirements;
		sRecord.init_hardware = __haiku_init_hardware;
		sRecord.select_miibus = (void *(*)(void *))__haiku_select_miibus_driver;
		sRecord.disable_intr = (int (*)(void *))__haiku_disable_interrupts;
		sRecord.reenable_intr = (void (*)(void *))__haiku_reenable_interrupts;
		sRecord.init_wlan_stack = init_wlan_stack;
		sRecord.uninit_wlan_stack = uninit_wlan_stack;
		sRecord.start_wlan = (int (*)(void *))start_wlan;
		sRecord.stop_wlan = (int (*)(void *))stop_wlan;
		sRecord.net_input = pdos_net_input;
		sRecord.net_input_chain = pdos_net_input_chain;
		sRecord.net_link = pdos_net_link;
		sRecord.firmware_images = pd_firmware_images;
		sRecord.firmware_count = &pd_firmware_image_count != NULL ? pd_firmware_image_count : 0;
		sRecord.sysinit_begin = (void **)SET_BEGIN(__freebsd_sysinit);
		sRecord.sysinit_end = (void **)SET_LIMIT(__freebsd_sysinit);
		sRecord.sysuninit_begin = (void **)SET_BEGIN(__freebsd_sysuninit);
		sRecord.sysuninit_end = (void **)SET_LIMIT(__freebsd_sysuninit);
	}
	return &sRecord;
}
