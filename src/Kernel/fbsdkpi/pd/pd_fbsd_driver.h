// SPDX-License-Identifier: MIT
// a driver's identity as the shared layer sees it: what haiku's glue macros define in each driver
#ifndef PD_FBSD_DRIVER_H
#define PD_FBSD_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

struct pd_firmware_image;

struct pd_fbsd_driver {
	const char *name;
	int requirements;
	void (*init_hardware)(void);
	void *(*select_miibus)(void *dev);
	int (*disable_intr)(void *dev);
	void (*reenable_intr)(void *dev);
	int (*init_wlan_stack)(void);
	void (*uninit_wlan_stack)(void);
	int (*start_wlan)(void *dev);
	int (*stop_wlan)(void *dev);
	// its xnu side: frames and link changes for an interface it attached
	void (*net_input)(void *cookie, const void *frame, size_t len);
	void (*net_input_chain)(void *cookie, void *chain);
	void (*net_link)(void *cookie, int up, uint64_t baudrate);
	// the firmware linked into it
	const struct pd_firmware_image *firmware_images;
	int firmware_count;
	// the driver's own SYSINIT and SYSUNINIT linker sets
	void **sysinit_begin, **sysinit_end;
	void **sysuninit_begin, **sysuninit_end;
	int initialised;
};

// per driver: built from that driver's names by pd_fbsd_driver_record.c
struct pd_fbsd_driver *pd_fbsd_driver_record(void);

// the layer: attaches run one at a time, for the driver entered
void pd_fbsd_driver_enter(struct pd_fbsd_driver *driver);
void pd_fbsd_driver_leave(void);

// the layer side of the lookup: the driver attaching now, and the one a device came from
extern struct pd_fbsd_driver *pd_fbsd_current;
struct pd_fbsd_driver *pd_fbsd_device_driver(void *dev);

#ifdef __cplusplus
}
#endif

#endif
