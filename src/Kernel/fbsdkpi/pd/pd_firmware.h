// SPDX-License-Identifier: MIT
// one firmware image linked into a driver kext
#ifndef PD_FIRMWARE_H
#define PD_FIRMWARE_H

struct pd_firmware_image {
	const char			*name;
	const unsigned char	*start;
	const unsigned char	*end;
};

#endif
