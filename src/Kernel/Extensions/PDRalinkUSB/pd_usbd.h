/*
 * pd_usbd.h - the parts of OpenBSD's usbdi that run(4) calls, carried out by
 * PDRalinkUSB over IOUSBFamily. Completions arrive on the USB thread and are
 * run under the 802.11 stack lock.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef PD_USBD_H
#define PD_USBD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_device;
struct usbd_interface;
struct usbd_pipe;
struct usbd_xfer;

typedef int usbd_status;
enum {
	USBD_NORMAL_COMPLETION = 0,
	USBD_IN_PROGRESS,
	USBD_PENDING_REQUESTS,
	USBD_NOT_STARTED,
	USBD_INVAL,
	USBD_NOMEM,
	USBD_CANCELLED,
	USBD_BAD_ADDRESS,
	USBD_IN_USE,
	USBD_NO_ADDR,
	USBD_SET_ADDR_FAILED,
	USBD_NO_POWER,
	USBD_TOO_DEEP,
	USBD_IOERROR,
	USBD_NOT_CONFIGURED,
	USBD_TIMEOUT,
	USBD_SHORT_XFER,
	USBD_STALLED,
	USBD_INTERRUPTED,
};

#define USBD_NO_COPY		0x01
#define USBD_SYNCHRONOUS	0x02
#define USBD_SHORT_XFER_OK	0x04
#define USBD_FORCE_SHORT_XFER	0x08
#define USBD_NO_TIMEOUT		0

typedef uint8_t uWord[2];
#define UGETW(w)	((w)[0] | ((w)[1] << 8))
#define USETW(w, v)	((w)[0] = (uint8_t)(v), (w)[1] = (uint8_t)((v) >> 8))

typedef struct {
	uint8_t		bmRequestType;
	uint8_t		bRequest;
	uWord		wValue;
	uWord		wIndex;
	uWord		wLength;
} __attribute__((__packed__)) usb_device_request_t;

#define UT_WRITE		0x00
#define UT_READ			0x80
#define UT_VENDOR		0x40
#define UT_DEVICE		0x00
#define UT_READ_VENDOR_DEVICE	(UT_READ | UT_VENDOR | UT_DEVICE)
#define UT_WRITE_VENDOR_DEVICE	(UT_WRITE | UT_VENDOR | UT_DEVICE)

typedef struct {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bEndpointAddress;
	uint8_t		bmAttributes;
	uWord		wMaxPacketSize;
	uint8_t		bInterval;
} __attribute__((__packed__)) usb_endpoint_descriptor_t;

typedef struct {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bInterfaceNumber;
	uint8_t		bAlternateSetting;
	uint8_t		bNumEndpoints;
	uint8_t		bInterfaceClass;
	uint8_t		bInterfaceSubClass;
	uint8_t		bInterfaceProtocol;
	uint8_t		iInterface;
} __attribute__((__packed__)) usb_interface_descriptor_t;

#define UE_DIR_IN		0x80
#define UE_DIR_OUT		0x00
#define UE_GET_DIR(a)		((a) & 0x80)
#define UE_CONTROL		0x00
#define UE_ISOCHRONOUS		0x01
#define UE_BULK			0x02
#define UE_INTERRUPT		0x03
#define UE_GET_XFERTYPE(a)	((a) & 0x03)

typedef void (*usbd_callback)(struct usbd_xfer *, void *, usbd_status);

usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *);
usb_endpoint_descriptor_t *usbd_interface2endpoint_descriptor(struct usbd_interface *, uint8_t);
usbd_status	usbd_do_request(struct usbd_device *, usb_device_request_t *, void *);
usbd_status	usbd_open_pipe(struct usbd_interface *, uint8_t, uint8_t, struct usbd_pipe **);
void		usbd_close_pipe(struct usbd_pipe *);
void		usbd_abort_pipe(struct usbd_pipe *);
void		usbd_clear_endpoint_stall_async(struct usbd_pipe *);
struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *);
void		usbd_free_xfer(struct usbd_xfer *);
void		*usbd_alloc_buffer(struct usbd_xfer *, uint32_t);
void		usbd_setup_xfer(struct usbd_xfer *, struct usbd_pipe *, void *, void *, uint32_t,
		    uint16_t, uint32_t, usbd_callback);
usbd_status	usbd_transfer(struct usbd_xfer *);
void		usbd_get_xfer_status(struct usbd_xfer *, void **, void **, uint32_t *, usbd_status *);
int		usbd_is_dying(struct usbd_device *);
void		usbd_delay_ms(struct usbd_device *, uint32_t);
#define usbd_ref_incr(d)	((void)0)
#define usbd_ref_decr(d)	((void)0)
#define usbd_ref_wait(d)	((void)0)

// firmware comes from the driver's Resources
int		loadfirmware(const char *, uint8_t **, size_t *);

#ifndef __cplusplus
// run's command task goes on the stack's task thread
struct usb_task {
	struct task	ut_task;
};
#define USB_TASK_TYPE_GENERIC	0
#define usb_init_task(t, f, a, ty)	task_set(&(t)->ut_task, (f), (a))
#define usb_add_task(d, t)		task_add(systq, &(t)->ut_task)
#define usb_rem_task(d, t)		task_del(systq, &(t)->ut_task)
#define usb_rem_wait_task(d, t)		task_del(systq, &(t)->ut_task)
#define usb_wait_task(d, t)		((void)0)
#endif

// what run_softc keeps of the openbsd device
struct device {
	char		dv_xname[16];
};

#ifdef __cplusplus
}
#endif

#endif
