/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _USB_SPEC_H
#define _USB_SPEC_H


#include <SupportDefs.h>


/* Request types (target/direction) for send_request() */
#define USB_REQTYPE_DEVICE_IN				0x80
#define USB_REQTYPE_DEVICE_OUT				0x00
#define USB_REQTYPE_INTERFACE_IN			0x81
#define USB_REQTYPE_INTERFACE_OUT			0x01
#define USB_REQTYPE_ENDPOINT_IN				0x82
#define USB_REQTYPE_ENDPOINT_OUT			0x02
#define USB_REQTYPE_OTHER_IN				0x83
#define USB_REQTYPE_OTHER_OUT				0x03

/* Request types for send_request() */
#define USB_REQTYPE_STANDARD				0x00
#define USB_REQTYPE_CLASS					0x20
#define USB_REQTYPE_VENDOR					0x40
#define USB_REQTYPE_RESERVED				0x60
#define USB_REQTYPE_MASK					0x9f

/* Standard request values for send_request() */
#define USB_REQUEST_GET_STATUS				0
#define USB_REQUEST_CLEAR_FEATURE			1
#define USB_REQUEST_SET_FEATURE				3
#define USB_REQUEST_SET_ADDRESS				5
#define USB_REQUEST_GET_DESCRIPTOR			6
#define USB_REQUEST_SET_DESCRIPTOR			7
#define USB_REQUEST_GET_CONFIGURATION		8
#define USB_REQUEST_SET_CONFIGURATION		9
#define USB_REQUEST_GET_INTERFACE			10
#define USB_REQUEST_SET_INTERFACE			11
#define USB_REQUEST_SYNCH_FRAME				12

/* Used by {set|get}_descriptor() */
#define USB_DESCRIPTOR_DEVICE				0x01
#define USB_DESCRIPTOR_CONFIGURATION		0x02
#define USB_DESCRIPTOR_STRING				0x03
#define USB_DESCRIPTOR_INTERFACE			0x04
#define USB_DESCRIPTOR_ENDPOINT				0x05

/* USB 3.x descriptors types */
#define USB_DESCRIPTOR_BOS									0x0f
#define USB_DESCRIPTOR_DEVICE_CAPABILITY					0x10
#define USB_DESCRIPTOR_ENDPOINT_SS_COMPANION				0x30
#define USB_DESCRIPTOR_ISOCHRONOUS_ENDPOINT_SS_COMPANION	0x31

/* conventional class-specific descriptors */
#define USB_DESCRIPTOR_CS_DEVICE			0x21
#define USB_DESCRIPTOR_CS_CONFIGURATION		0x22
#define USB_DESCRIPTOR_CS_STRING			0x23
#define USB_DESCRIPTOR_CS_INTERFACE			0x24
#define USB_DESCRIPTOR_CS_ENDPOINT			0x25

/* Used by {set|clear}_feature() */
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP	1
#define USB_FEATURE_ENDPOINT_HALT			0

#define USB_ENDPOINT_ATTR_CONTROL			0x00
#define USB_ENDPOINT_ATTR_ISOCHRONOUS		0x01
#define USB_ENDPOINT_ATTR_BULK				0x02
#define USB_ENDPOINT_ATTR_INTERRUPT			0x03
#define USB_ENDPOINT_ATTR_MASK				0x03

/* Synchronization - isochrnous endpoints only */
#define USB_ENDPOINT_ATTR_NO_SYNCHRONIZE	0x00
#define USB_ENDPOINT_ATTR_ASYNCRONOUS		0x04
#define USB_ENDPOINT_ATTR_ADAPTIVE_SYNCHRO	0x08
#define USB_ENDPOINT_ATTR_SYNCHRONOUS		0x0C
#define USB_ENDPOINT_ATTR_SYNCHRONIZE_MASK	0x0C

/* Usage Type - isochrnous endpoints only */
#define USB_ENDPOINT_ATTR_DATA_USAGE		0x00
#define USB_ENDPOINT_ATTR_FEEDBACK_USAGE	0x10
#define USB_ENDPOINT_ATTR_IMPLICIT_USAGE	0x20
#define USB_ENDPOINT_ATTR_USAGE_MASK		0x30

/* Direction */
#define USB_ENDPOINT_ADDR_DIR_IN			0x80
#define USB_ENDPOINT_ADDR_DIR_OUT			0x00
#define USB_ENDPOINT_ADDR_DIR_MASK			0x80


typedef struct usb_device_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint16	usb_version;
	uint8	device_class;
	uint8	device_subclass;
	uint8	device_protocol;
	uint8	max_packet_size_0;
	uint16	vendor_id;
	uint16	product_id;
	uint16	device_version;
	uint8	manufacturer;
	uint8	product;
	uint8	serial_number;
	uint8	num_configurations;
} _PACKED usb_device_descriptor;

typedef struct usb_configuration_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint16	total_length;
	uint8	number_interfaces;
	uint8	configuration_value;
	uint8	configuration;
	uint8	attributes;
	uint8	max_power;
} _PACKED usb_configuration_descriptor;

typedef struct usb_interface_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint8	interface_number;
	uint8	alternate_setting;
	uint8	num_endpoints;
	uint8	interface_class;
	uint8	interface_subclass;
	uint8	interface_protocol;
	uint8	interface;
} _PACKED usb_interface_descriptor;

typedef struct usb_endpoint_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint8	endpoint_address;
	uint8	attributes;
	uint16	max_packet_size;
	uint8	interval;
} _PACKED usb_endpoint_descriptor;

typedef struct usb_endpoint_ss_companion_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint8	max_burst;
	uint8	attributes;
	uint16	bytes_per_interval;
} _PACKED usb_endpoint_ss_companion_descriptor;

typedef struct usb_isochronous_endpoint_ss_companion_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint8	max_burst;
	uint8	attributes;
	uint32	bytes_per_interval;
} _PACKED usb_isochronous_endpoint_ss_companion_descriptor;

typedef struct usb_string_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uchar	string[1];
} _PACKED usb_string_descriptor;

typedef struct usb_generic_descriptor {
	uint8	length;
	uint8	descriptor_type;
	uint8	data[1];
} _PACKED usb_generic_descriptor;

typedef union usb_descriptor {
	usb_generic_descriptor			generic;
	usb_device_descriptor			device;
	usb_interface_descriptor		interface;
	usb_endpoint_descriptor			endpoint;
	usb_configuration_descriptor	configuration;
	usb_string_descriptor			string;
} usb_descriptor;


#endif	/* _USB_SPEC_H */
