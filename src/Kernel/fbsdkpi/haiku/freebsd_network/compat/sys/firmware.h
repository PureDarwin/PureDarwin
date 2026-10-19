/*
 * Copyright 2009-2025, Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_FIRMWARE_H_
#define _FBSD_COMPAT_SYS_FIRMWARE_H_


struct firmware {
	const char*		name;
	const void*		data;
	size_t			datasize;
	unsigned int	version;
};


#define	FIRMWARE_GET_NOWARN	0x0001

const struct firmware* firmware_get(const char*);
const struct firmware* firmware_get_flags(const char*, int flags);


#define FIRMWARE_UNLOAD		0x0001

void firmware_put(const struct firmware*, int flags);


#endif /* _FBSD_COMPAT_SYS_FIRMWARE_H_ */
