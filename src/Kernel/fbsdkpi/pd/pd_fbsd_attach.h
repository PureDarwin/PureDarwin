// SPDX-License-Identifier: MIT
// what the xnu side calls to attach and run a FreeBSD network driver
// opaque pointers and fixed-width types only, both sides include it
#ifndef PD_FBSD_ATTACH_H
#define PD_FBSD_ATTACH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// brings up mbufs, callouts, taskqueues and sysinits once per kext
int pd_fbsd_init(void);

// probes and attaches the drivers of this kext on an IOPCIDevice
// returns the ifnet the driver created, or NULL
void *pd_fbsd_attach_pci(void *pcidev);
// the same on a usb interface, usbdev is the handle pdusb_open returned
void *pd_fbsd_attach_usb(void *usbdev);
void pd_fbsd_detach(void *ifp);

// the same for drivers that create no ifnet, returns the attached device_t
void *pd_fbsd_attach_pci_device(void *pcidev);
void pd_fbsd_shutdown_device(void *device);
// a device-tree device: the nub (for interrupts), its device tree node and its
// register windows with the parent buses' ranges applied
#define PD_FBSD_MAX_MEM 8
struct pd_fbsd_platform {
	void		*nub;
	void		*node;
	int		nmem;
	uint64_t	mem_phys[PD_FBSD_MAX_MEM];
	uint64_t	mem_size[PD_FBSD_MAX_MEM];
	int		nirq;
};
// probes the linux platform drivers of this kext against the node, returns the
// attached struct device or NULL
void *pd_fbsd_attach_platform_device(const struct pd_fbsd_platform *pd);

// ties an ifnet to its iokit controller for receive and link callbacks
void pd_fbsd_set_cookie(void *ifp, void *cookie);

int pd_fbsd_get_lladdr(void *ifp, uint8_t mac[6]);
int pd_fbsd_up(void *ifp);
int pd_fbsd_down(void *ifp);
int pd_fbsd_transmit(void *ifp, const void *frame, size_t len);
int pd_fbsd_set_promisc(void *ifp, int on);
int pd_fbsd_set_allmulti(void *ifp, int on);
int pd_fbsd_add_multi(void *ifp, const uint8_t mac[6]);
int pd_fbsd_get_link(void *ifp, int *up, uint64_t *baudrate);
uint32_t pd_fbsd_get_mtu(void *ifp);

// media, described as speed in mbit (0 for autoselect) and duplex, with the
// driver's own media word kept so it can be selected again
struct pd_fbsd_medium {
	uint32_t	speed;
	int32_t		full_duplex;
	int32_t		word;
};
int pd_fbsd_media_list(void *ifp, struct pd_fbsd_medium *media, int max);
int pd_fbsd_set_media(void *ifp, int32_t word);
int pd_fbsd_active_media(void *ifp, struct pd_fbsd_medium *active);

// copies a received chain the xnu side got through pdos_net_input_chain
int pd_fbsd_mbuf_copydata(void *chain, void *buffer, size_t max);

#ifdef __cplusplus
}
#endif

#endif
