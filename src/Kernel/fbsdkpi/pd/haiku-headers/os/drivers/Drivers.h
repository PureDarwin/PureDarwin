/*
 * Copyright 2002-2013, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DRIVERS_DRIVERS_H
#define _DRIVERS_DRIVERS_H


#include <sys/types.h>
#include <sys/uio.h>

#include <BeBuild.h>
#include <Select.h>
#include <SupportDefs.h>


#ifdef __cplusplus
extern "C" {
#endif

/* These hooks are how the kernel accesses legacy devices */
typedef status_t (*device_open_hook)(const char *name, uint32 flags,
	void **cookie);
typedef status_t (*device_close_hook)(void *cookie);
typedef status_t (*device_free_hook)(void *cookie);
typedef status_t (*device_control_hook)(void *cookie, uint32 op, void *data,
	size_t len);
typedef status_t  (*device_read_hook)(void *cookie, off_t position, void *data,
	size_t *numBytes);
typedef status_t  (*device_write_hook)(void *cookie, off_t position,
	const void *data, size_t *numBytes);
typedef status_t (*device_select_hook)(void *cookie, uint8 event, uint32 ref,
	selectsync *sync);
typedef status_t (*device_deselect_hook)(void *cookie, uint8 event,
	selectsync *sync);
typedef status_t (*device_readv_hook)(void *cookie, off_t position,
	const iovec *vec, size_t count, size_t *_numBytes);
typedef status_t (*device_writev_hook)(void *cookie, off_t position,
	const iovec *vec, size_t count, size_t *_numBytes);

#define	B_CUR_DRIVER_API_VERSION	2

/* Legacy driver device hooks */
typedef struct {
	device_open_hook		open;			/* called to open the device */
	device_close_hook		close;			/* called to close the device */
	device_free_hook		free;			/* called to free the cookie */
	device_control_hook		control;		/* called to control the device */
	device_read_hook		read;			/* reads from the device */
	device_write_hook		write;			/* writes to the device */
	device_select_hook		select;			/* start select */
	device_deselect_hook	deselect;		/* stop select */
	device_readv_hook	readv;
		/* scatter-gather read from the device */
	device_writev_hook	writev;
		/* scatter-gather write to the device */
} device_hooks;

/* Driver functions needed to be exported by legacy drivers */
status_t		init_hardware(void);
const char**	publish_devices(void);
device_hooks*	find_device(const char* name);
status_t		init_driver(void);
void			uninit_driver(void);

extern int32	api_version;

enum {
	B_GET_DEVICE_SIZE = 1,			/* get # bytes - returns size_t in *data */
	B_SET_DEVICE_SIZE,				/* set # bytes - passes size_t in *data */
	B_SET_NONBLOCKING_IO,			/* set to non-blocking i/o */
	B_SET_BLOCKING_IO,				/* set to blocking i/o */
	B_GET_READ_STATUS,				/* check if can read w/o blocking */
									/* returns bool in *data */
	B_GET_WRITE_STATUS,				/* check if can write w/o blocking */
									/* returns bool in *data */
	B_GET_GEOMETRY,					/* get info about device geometry */
									/* returns struct geometry in *data */
	B_GET_DRIVER_FOR_DEVICE,		/* get the path of the executable serving */
									/* that device */
	B_GET_PARTITION_INFO,			/* get info about a device partition */
									/* returns struct partition_info in *data */
	B_SET_PARTITION,				/* obsolete, will be removed */
	B_FORMAT_DEVICE,				/* low-level device format */
	B_EJECT_DEVICE,					/* eject the media if supported */
	B_GET_ICON,						/* return device icon (see struct below) */
	B_GET_BIOS_GEOMETRY,			/* get info about device geometry */
									/* as reported by the bios */
									/*   returns struct geometry in *data */
	B_GET_MEDIA_STATUS,				/* get status of media. */
									/* return status_t in *data: */
									/* B_NO_ERROR: media ready */
									/* B_DEV_NO_MEDIA: no media */
									/* B_DEV_NOT_READY: device not ready */
									/* B_DEV_MEDIA_CHANGED: media changed */
									/*  since open or last B_GET_MEDIA_STATUS */
									/* B_DEV_MEDIA_CHANGE_REQUESTED: user */
									/*  pressed button on drive */
									/* B_DEV_DOOR_OPEN: door open */
	B_LOAD_MEDIA,					/* load the media if supported */
	B_GET_BIOS_DRIVE_ID,			/* get bios id for this device */
	B_SET_UNINTERRUPTABLE_IO,		/* prevent cntl-C from interrupting i/o */
	B_SET_INTERRUPTABLE_IO,			/* allow cntl-C to interrupt i/o */
	B_FLUSH_DRIVE_CACHE,			/* flush drive cache */
	B_GET_PATH_FOR_DEVICE,			/* get the absolute path of the device */
	B_GET_ICON_NAME,				/* get an icon name identifier */
	B_GET_VECTOR_ICON,				/* retrieves the device's vector icon */
	B_GET_DEVICE_NAME,				/* get name, string buffer */
	B_TRIM_DEVICE,					/* trims blocks, see fs_trim_data */
	B_GET_DEVICE_SERIAL_NUMBER,		/* get SN (or any other UUID), string buffer */

	B_GET_NEXT_OPEN_DEVICE = 1000,	/* obsolete, will be removed */
	B_ADD_FIXED_DRIVER,				/* obsolete, will be removed */
	B_REMOVE_FIXED_DRIVER,			/* obsolete, will be removed */

	B_AUDIO_DRIVER_BASE = 8000,		/* base for codes in audio_driver.h */
	B_MIDI_DRIVER_BASE = 8100,		/* base for codes in midi_driver.h */
	B_JOYSTICK_DRIVER_BASE = 8200,	/* base for codes in joystick.h */
	B_GRAPHIC_DRIVER_BASE = 8300,	/* base for codes in graphic_driver.h */

	B_SOCKET_IO_BASE = 8900,		/* base for codes in sys/sockio.h */

	B_DEVICE_OP_CODES_END = 9999	/* end of Be-defined control ids */
};

/* B_GET_GEOMETRY data structure */
typedef struct {
	uint32	bytes_per_sector;		/* sector size in bytes */
	uint32	sectors_per_track;		/* # sectors per track */
	uint32	cylinder_count;			/* # cylinders */
	uint32	head_count;				/* # heads */
	uchar	device_type;			/* type */
	bool	removable;				/* non-zero if removable */
	bool	read_only;				/* non-zero if read only */
	bool	write_once;				/* non-zero if write-once */
	uint32	bytes_per_physical_sector;	/* physical sector size in bytes */
} device_geometry;

enum {
	B_DISK = 0,						/* Hard disks, floppy disks, etc. */
	B_TAPE,							/* Tape drives */
	B_PRINTER,						/* Printers */
	B_CPU,							/* CPU devices */
	B_WORM,							/* Write-once, read-many devices */
	B_CD,							/* CD ROMS */
	B_SCANNER,						/* Scanners */
	B_OPTICAL,						/* Optical devices */
	B_JUKEBOX,						/* Jukeboxes */
	B_NETWORK						/* Network devices */
};


/* B_GET_PARTITION_INFO data structure */
typedef struct {
	off_t	offset;					/* offset (in bytes) */
	off_t	size;					/* size (in bytes) */
	int32	logical_block_size;		/* logical block size of partition */
	int32	physical_block_size;	/* physical block size of partition */
	int32	session;				/* id of session */
	int32	partition;				/* id of partition */
	char	device[256];			/* path to the physical device */
} partition_info;


/* B_GET_DRIVER_FOR_DEVICE data structure */
typedef char	driver_path[256];


/* B_GET_ICON, and B_GET_VECTOR_ICON data structure */
typedef struct {
	int32	icon_size;
		/* B_GET_VECTOR_ICON: size of the data buffer in icon_data */
		/* B_GET_ICON: size of the icon in pixels */
	void	*icon_data;
} device_icon;


/* B_TRIM_DEVICE data structure */
typedef struct {
	uint32	range_count;
	uint64	trimmed_size;			/* filled on return */
	struct range {
		uint64	offset;				/* offset (in bytes) */
		uint64	size;
	} ranges[1];
} fs_trim_data;


#ifdef __cplusplus
}
#endif


#endif /* _DRIVERS_DRIVERS_H */
