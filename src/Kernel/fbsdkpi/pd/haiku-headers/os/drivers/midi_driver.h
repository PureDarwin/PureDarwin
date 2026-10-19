/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MIDI_DRIVER_H
#define _MIDI_DRIVER_H


#include <Drivers.h>
#include <module.h>


typedef struct {
	uint8 port_index; // 0 based
	char port_name[24];
} midi_device_port_info;


/* deprecated interface */
enum {
	B_MIDI_GET_READ_TIMEOUT = B_MIDI_DRIVER_BASE,
	B_MIDI_SET_READ_TIMEOUT,
	B_MIDI_TIMED_READ,
	B_MIDI_TIMED_WRITE,
	B_MIDI_WRITE_SYNC,
	B_MIDI_WRITE_CLEAR,
	B_MIDI_GET_READ_TIMEOUT_OLD = B_DEVICE_OP_CODES_END + 1,
	B_MIDI_SET_READ_TIMEOUT_OLD,
	B_MIDI_GET_PORT_INFO // see midi_device_port_info
};


#define B_MIDI_DEFAULT_TIMEOUT 1000000000000000LL

typedef struct {
	bigtime_t	when;
	size_t		size;
	uint8*		data;
} midi_timed_data;


#define B_MIDI_PARSER_MODULE_NAME "media/midiparser/v1"

typedef struct _midi_parser_module_info {
	module_info	minfo;
	int			(*parse)(uint32* state, uchar byte, size_t maxSize);
	int			_reserved_;
} midi_parser_module_info;


#define B_MPU_401_MODULE_NAME "generic/mpu401/v1"

enum {
	B_MPU_401_ENABLE_CARD_INT = 1,
	B_MPU_401_DISABLE_CARD_INT
};

typedef struct _generic_mpu401_module {
	module_info	minfo;
	status_t	(*create_device)(int port, void** _handle, uint32 workArounds,
					void (*interruptOp)(int32 op, void* card), void* card);
	status_t	(*delete_device)(void* handle);
	status_t	(*open_hook)(void* storage, uint32 flags, void** _cookie);
	status_t	(*close_hook)(void* cookie);
	status_t	(*free_hook)(void* cookie);
	status_t	(*control_hook)(void* cookie, uint32 op, void* data,
					size_t length);
	status_t	(*read_hook)(void* cookie, off_t pos, void* data,
					size_t* _length);
	status_t	(*write_hook)(void* cookie, off_t pos, const void* data,
					size_t* _length);
	bool		(*interrupt_hook)(void* cookie);
	int			_reserved_;
} generic_mpu401_module;


#endif	/* _MIDI_DRIVER_H */
